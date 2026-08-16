/** \file
 *  \brief The game loop — the only thread allowed to speak GTP while a game runs.
 *
 * Owns the game thread, and through it the `GameRecord` inside `GobanModel`: no
 * other thread may touch that tree while the loop is running (ADR-0006).
 * Delegates player lifecycle to PlayerManager and tree walking to
 * GameNavigator, and fans every position change out to the GameObserver list.
 *
 * The UI thread talks to it in one direction only, and never blocks — moves via
 * `playLocalMove()`, navigation via a queue, game-replacing actions via
 * `runWhenEngineFree()`. It must never call into RmlUi, and it must never be
 * joined from its own thread, which is why `interrupt()` is a deliberate no-op
 * there. A genmove in flight cannot be aborted; ADR-0001 explains why a
 * UI-thread wait on one freezes the whole application.
 */
#ifndef GAMETHREAD_H
#define GAMETHREAD_H

#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <memory>
#include <queue>
#include <functional>
#include <condition_variable>

#include "player.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "GameNavigator.h"
#include "PlayerManager.h"
#include "Board.h"
#include "Configuration.h"

extern std::shared_ptr<Configuration> config;

/** \brief Navigation command queued for execution on the game thread */
struct NavCommand {
    enum Type { BACK, FORWARD, TO_START, TO_END, TO_VARIATION, KIBITZ_NAV, TO_TREE_PATH };
    Type type;
    Move move;  // For TO_VARIATION
    bool promote = true;  // For TO_VARIATION: promote to main line
    int pathLength = 0;                 // For TO_TREE_PATH
    std::vector<int> branchChoices;     // For TO_TREE_PATH
};

/** \brief Game mode determining player interaction behavior
 */
enum class GameMode {
    MATCH,      ///< Default - strict turn alternation, assigned roles
    ANALYSIS    ///< Sabaki-like - human plays either color, AI responds based on move source
};

/** \brief Whether the engines match the game record — ADR-0002 step 4.
 *
 * Replaces the `enginesSynced` bool. The distinction the bool could not make is
 * between work *pending* and work *happening*: after a new game the record has
 * moved and nothing is synced, but the game loop is stopped and will stay
 * stopped until the user plays. Treating that as "busy" would hang any caller
 * waiting for quiescence, whereas an in-progress replay genuinely is busy.
 */
/** \brief The game loop's own lifecycle — ADR-0002 step 4.
 *
 * Replaces the `hasThreadRunning` / `interruptRequested` pair. `Stopping` is
 * the state the two encoded jointly and neither named: the thread is still
 * alive, because an engine mid-genmove cannot be aborted, but it will exit as
 * soon as that call returns.
 *
 * `deferredPending` is deliberately *not* folded in. A deferred task can be
 * queued while the loop runs perfectly happily, and it never means "stopping" —
 * it is work waiting, not lifecycle. The two only meet in one derived question,
 * shouldDiscardMove().
 */
enum class LoopState {
    Stopped,   ///< No game loop. The thread may exist but has returned.
    Running,   ///< The loop is turning.
    Stopping   ///< Asked to stop; still alive until the current call returns.
};

enum class EngineSync {
    Unsynced,  ///< Engines are behind the record. Nobody may be working on it.
    Syncing,   ///< The game thread is replaying the record into engines now.
    Synced     ///< Every engine sits at the record's current position.
};


/** \brief Background thread responsible for rules enforcing
 *
 */
class GameThread
{
public:

    explicit GameThread(GobanModel &model);
    ~GameThread();

    size_t addEngine(Engine* engine) const;

    size_t addPlayer(Player* player) const;

    Engine* currentCoach() const;
    Engine* currentKibitz() const;

    Player* currentPlayer() const;

    std::string getName(size_t id) const { return playerManager->getName(id); }

    /// Stops the game loop and joins it. A no-op on the game thread itself.
    ///
    /// This waits, and it may wait a long time: the loop can be blocked inside a
    /// player's genmove(), which for an engine is a blocking GTP read that
    /// nothing but the engine replying will return from. It is therefore only
    /// safe to call once you know no engine is on move — which is precisely what
    /// `runWhenEngineFree()` establishes, under `playerMutex`, before handing a
    /// task to a caller's thread.
    ///
    /// It used to take a timeout, so a caller could give up rather than block.
    /// Nothing ever passed one: `INTERRUPT_TIMEOUT_MS` was declared and never
    /// referenced, and every call site used the default. A mitigation with no
    /// caller is worse than none, because the comment describing it reads as a
    /// guarantee — so the answer is the claim in runWhenEngineFree(), which
    /// removes the wait instead of bounding it.
    bool interrupt();

    /// Forceful shutdown: kill all engine processes (unblocks game thread), then interrupt.
    void shutdown();

    // Check if genmove is in progress (engine is thinking)
    bool isThinking() const;

    /// Whether the continuous analysis stream may run right now (ADR-0007
    /// decision 6). Read from the analysis thread; it touches no pipe.
    ///
    /// Written as more than `!isThinking()` on purpose. The game loop clears
    /// `playerToMove` *before* its 500 ms inter-move sleep, so `isThinking()` is
    /// false for that window on every move — a bare test would switch the stream
    /// on and off once per move in a bot-versus-bot match, which is the
    /// two-searches-at-once case this decision exists to prevent. Asking whether
    /// an *engine is on move in a running loop* covers the sleep as well.
    ///
    /// The loop check matters in the other direction too: a loaded game paused
    /// with an engine to move has nobody searching, and review is exactly when
    /// the numbers are worth most.
    [[nodiscard]] bool analysisMayRun() const;

    /// Configuration for the dedicated analysis process, or nullopt when no
    /// engine nominated itself. Forwarded from PlayerManager; see ADR-0007.
    [[nodiscard]] std::optional<nlohmann::json> analysisConfig() const {
        return playerManager->analysisConfig();
    }

    /// Runs an action that *discards or replaces the current game* (new game,
    /// clear, load, switch game) as soon as no engine is mid-genmove.
    ///
    /// A GTP command in flight owns the engine's pipes until it replies, and
    /// standard GTP cannot abort one, so only the game thread may wait for it.
    /// If an engine is thinking, the task is handed to the game thread, which
    /// discards the now-irrelevant move and then runs it. The caller never
    /// blocks.
    ///
    /// Actions that *preserve* the current game (navigation, stone placement)
    /// must NOT use this — their pending genmove is still valid, so they are
    /// refused while thinking instead.
    ///
    /// Returns true if the task already ran, false if it was deferred, in which
    /// case `busyEngine` is set to the name of the engine being waited for.
    /// Only one task can be pending: a newer one replaces an older, since these
    /// actions all discard the game anyway.
    bool runWhenEngineFree(std::function<void()> task, std::string* busyEngine = nullptr);

    /// Consumed by the UI thread once per completed deferred task, so it can
    /// refresh widgets it alone may touch.
    bool takeDeferredTaskDone();

    /// True while a deferred task is waiting for an engine to finish.
    [[nodiscard]] bool hasDeferredTask() const { return deferredPending.load(); }

    /// True once the loop has been asked to stop and has not yet exited. The
    /// old spelling was `interruptRequested`.
    [[nodiscard]] bool stopRequested() const { return loop.load() == LoopState::Stopping; }

    /// A move that arrived from an engine is worthless if the loop is stopping
    /// or the game is about to be replaced. The one place the loop's lifecycle
    /// and the deferred-task queue are asked a single question.
    [[nodiscard]] bool shouldDiscardMove() const {
        return stopRequested() || deferredPending.load();
    }

    /// For diagnostics and scenario assertions.
    [[nodiscard]] LoopState loopState() const { return loop.load(); }

    /// True only while the game thread is actually replaying the record into
    /// the engines. Deliberately not true for EngineSync::Unsynced: that state
    /// persists with the loop stopped after a new game, so treating it as busy
    /// would stall every caller waiting for quiescence.
    [[nodiscard]] bool isSyncingEngines() const {
        return engineSync.load() == EngineSync::Syncing;
    }

    /// For diagnostics and scenario assertions.
    [[nodiscard]] EngineSync engineSyncState() const { return engineSync.load(); }

    /// Name of the engine currently thinking, or empty.
    [[nodiscard]] std::string thinkingPlayerName() const;

    /// True when called from the game loop's own thread, where stopping the
    /// loop is both unnecessary and impossible (it would join itself).
    [[nodiscard]] static bool isOnGameThread();

    /// True while any navigation command is queued or executing. Navigation is
    /// fire-and-forget from the UI thread, so isThinking() alone does not tell
    /// you whether the board has caught up; scripted runs and any other caller
    /// that needs quiescence must consult this too.
    bool hasPendingNavigation() const;

    /// Queue-only half of hasPendingNavigation(), for diagnostics.
    [[nodiscard]] bool hasQueuedNavigation() const;

    bool clearGame(int boardSize, float komi, int handicap);

    /// Begin the replay into the engines for a freshly cleared game.
    ///
    /// Call **after** the model holds the new record: the sync reads whatever
    /// record is installed, so running it while the previous game is still in
    /// place replays that one into the engines instead. Separate from
    /// `clearGame()` for exactly that reason — see its comment.
    void startSyncingNewGame();

    void removeSgfPlayers() const;  // Remove temporary players created from SGF loading

    void setKomi(float komi);

    bool setFixedHandicap(int handicap);

    void run();

    bool isRunning() const;

    void gameLoop();

    bool humanToMove() const;

    void playLocalMove(const Move& move);
    void playKibitzMove();

    // Analysis mode support
    bool setGameMode(GameMode mode);  // Returns true if mode change succeeded
    GameMode getGameMode() const { return gameMode; }
    void setAiVsAi(bool enabled);
    bool isAiVsAi() const { return aiVsAiMode; }
    bool areBothPlayersEngines() const;
    bool isCurrentPlayerEngine() const;  // Is the player to move an engine?

    void loadEngines(std::shared_ptr<Configuration> config) const;

    // Parallel engine loading with early SGF display
    // Callback is invoked when first engine is ready (for SGF loading)
    // Returns when all engines are loaded and synced
    // gameIndex: -1 = last game (default), 0+ = specific game index
    // startAtRoot: true = stay at root (for session restoration with tree path)
    void loadEnginesParallel(std::shared_ptr<Configuration> config,
                             const std::string& sgfPath,
                             std::function<void()> onFirstEngineReady,
                             int gameIndex = -1,
                             bool startAtRoot = false);

	size_t activatePlayer(int which, size_t newIndex);

	size_t getActivePlayer(int which) const;

	[[nodiscard]] Move getLocalMove(const Position& coord) const;
    [[nodiscard]] Move getLocalMove(Move::Special move) const;

    void addGameObserver(GameObserver* pobserver) {
        gameObservers.push_back(pobserver);
    }

    // Navigation methods for SGF replay (fire-and-forget, processed on game thread)
    void navigateBack();
    void navigateForward();
    void navigateToVariation(const Move& move, bool promote = true);
    void navigateToStart();
    void navigateToEnd();
    void navigateToTreePath(int pathLength, const std::vector<int>& branchChoices);  // Navigate to specific tree position (for session restore)
    void requestKibitzNav();  // Request engine move via navigation (for tsumego dead branches)

    std::vector<Player*> getPlayers() const { return playerManager->getPlayers(); }

    /// Which engine the status indicator should name while starting up. Empty
    /// once every engine has answered or failed. See PlayerManager.
    std::string engineLoadingSummary() const { return playerManager->loadingSummary(); }

    bool loadSGF(const std::string& fileName, int gameIndex = 0, bool startAtRoot = false);

    bool loadSGFWithEngine(const std::string& fileName, Engine* engine = nullptr, int gameIndex = 0, bool startAtRoot = false);
    bool switchGame(int gameIndex, bool startAtRoot = false);  // Switch game within loaded SGF doc
    bool autoPlayTsumegoSetup();  // Auto-play first move if it contradicts PL (non-standard tsumego convention)
    bool syncEngineToPosition(Engine* engine, int* syncedMoves = nullptr);  // Sync one engine to current game state (returns false on failure)
    void finalizeGameLoad(Engine* alreadySynced = nullptr, bool matchPlayers = true);  // Mark engines for lazy sync, match players, start game thread

private:
    void syncOtherEngines(const Move& move, const Player* player, const Engine* coach, const Engine* kibitzEngine, bool kibitzed) const;
    void notifyMoveComplete(Engine* coach, const Move& move, Engine* kibitzEngine, bool kibitzed, const std::string& engineComments);

    // Helper methods for game loop
    Move handleKibitzRequest(Move move, Engine* kibitzEngine, const Color& colorToMove, bool& wasKibitz);
    std::string collectEngineComments() const;
    void processSuccessfulMove(const Move& move, const Player* movePlayer, Engine* coach,
                              Engine* kibitzEngine, bool wasKibitz);

    // Apply loaded game info to model state, sync engine, build board, finalize
    bool applyLoadedGame(const GameRecord::SGFGameInfo& gameInfo, Engine* engine);

    // Helper for SGF loading - handles finished game detection and state setup
    void finalizeLoadedGame(Engine* engine, const GameRecord::SGFGameInfo& gameInfo);

    // Helper for SGF loading - matches SGF player names to engines or creates temporary players
    void matchSgfPlayers();
    std::vector<GameObserver*> gameObservers;
    GobanModel& model;
    std::unique_ptr<std::thread> thread;
    /// The one writer of the loop's lifecycle outside gameLoop() itself is
    /// interrupt()/run(); there is deliberately no second setter. `reset()` used
    /// to be one, and because it wrote Stopped unconditionally it told the
    /// truth only on the UI thread — on the game thread (a deferred discarding
    /// action) it claimed the running loop had stopped, which sent
    /// startSyncingNewGame() into run() and the game thread into joining
    /// itself. Same rule as GobanModel::transitionTo(): one writer.
    std::atomic<LoopState> loop{LoopState::Stopped};
    std::atomic<EngineSync> engineSync{EngineSync::Synced};
    std::atomic<Player*> playerToMove;
    Move queuedMove;
    mutable std::mutex playerMutex;
    std::condition_variable engineStarted;

    // Player management (extracted to separate class)
    std::unique_ptr<PlayerManager> playerManager;

    // Analysis mode state
    GameMode gameMode = GameMode::MATCH;
    bool aiVsAiMode = false;

    // Navigation (extracted to separate class)
    std::unique_ptr<GameNavigator> navigator;

    // Navigation command queue (UI thread -> game thread)
    std::queue<NavCommand> navQueue;
    mutable std::mutex navQueueMutex;
    std::condition_variable navQueueCV;
    /// Commands popped from navQueue but not yet finished. Without it a command
    /// is invisible to hasPendingNavigation() between the pop and the
    /// navigator raising its own flag. Incremented under navQueueMutex.
    std::atomic<int> navInFlight{0};

    // Deferred game-discarding action (see runWhenEngineFree).
    std::function<void()> deferredTask;
    mutable std::mutex deferredMutex;
    std::atomic<bool> deferredPending{false};
    std::atomic<bool> deferredDone{false};

    void processDeferredTask();

    /// Restarts any engine that was killed for not answering, and marks every
    /// engine unsynced so the loop replays the record into them. Game thread
    /// only: it swaps an engine's pipes. See GtpClient::revive().
    void reviveFailedEngines();

    void processNavigationQueue();
    void executeNavCommand(const NavCommand& cmd);
    void processScoring();
    void wakeGameThread();
    void waitForCommandOrTimeout(int ms);
};

#endif // GAMETHREAD_H
