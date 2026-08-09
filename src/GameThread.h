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

    /// Stops the game loop and joins it.
    ///
    /// The loop can be blocked inside a player's genmove(), which for an engine
    /// is a blocking GTP read: nothing short of the engine replying will return
    /// from it. A plain join() therefore freezes the caller for as long as the
    /// engine thinks — which froze the whole UI when an SGF was opened during a
    /// slow engine's move.
    ///
    /// timeoutMs < 0 waits indefinitely (the historical behaviour, used on the
    /// paths that must succeed). A non-negative timeout gives up and returns
    /// false instead of blocking; the loop still exits once genmove returns,
    /// and run() joins the finished thread before starting a new one.
    bool interrupt(int timeoutMs = -1);

    /// Longest the UI is willing to stall waiting for the game loop to stop
    /// before refusing an action outright.
    static constexpr int INTERRUPT_TIMEOUT_MS = 1500;

    /// Forceful shutdown: kill all engine processes (unblocks game thread), then interrupt.
    void shutdown();

    // Check if genmove is in progress (engine is thinking)
    bool isThinking() const;

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

    void removeSgfPlayers() const;  // Remove temporary players created from SGF loading

    void setKomi(float komi);

    bool setFixedHandicap(int handicap);

    void run();

    bool isRunning() const;

    void gameLoop();

    bool humanToMove() const;

    void playLocalMove(const Move& move);
    void playKibitzMove() const;

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
	void reset();

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

    bool loadSGF(const std::string& fileName, int gameIndex = 0, bool startAtRoot = false);

    bool loadSGFWithEngine(const std::string& fileName, Engine* engine = nullptr, int gameIndex = 0, bool startAtRoot = false);
    bool switchGame(int gameIndex, bool startAtRoot = false);  // Switch game within loaded SGF doc
    bool autoPlayTsumegoSetup();  // Auto-play first move if it contradicts PL (non-standard tsumego convention)
    bool syncEngineToPosition(Engine* engine, int* syncedMoves = nullptr);  // Sync one engine to current game state (returns false on failure)
    bool syncCoachToCurrentPosition();  // Sync coach engine to current game tree position (for session restoration)
    void finalizeGameLoad(Engine* alreadySynced = nullptr, bool matchPlayers = true);  // Mark engines for lazy sync, match players, start game thread

private:
    void syncOtherEngines(const Move& move, const Player* player, const Engine* coach, const Engine* kibitzEngine, bool kibitzed) const;
    void notifyMoveComplete(Engine* coach, const Move& move, Engine* kibitzEngine, bool kibitzed, const std::string& engineComments);
    void setHandicapStones(const std::vector<Position>& stones);
    void applyHandicapStonesToEngines(const std::vector<Position>& stones, const Engine* coach) const;

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
    std::mutex mutex2;
    std::atomic<bool> interruptRequested{false};
    std::atomic<bool> hasThreadRunning{false};
    std::atomic<bool> enginesSynced{true};  // All engines synced to current position
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

    void processNavigationQueue();
    void executeNavCommand(const NavCommand& cmd);
    void processScoring();
    void wakeGameThread();
    void waitForCommandOrTimeout(int ms);
};

#endif // GAMETHREAD_H
