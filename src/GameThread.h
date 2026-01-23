#ifndef GAMETHREAD_H
#define GAMETHREAD_H

#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <memory>
#include <queue>
#include <future>
#include <condition_variable>

#include "player.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "GameNavigator.h"
#include "PlayerManager.h"
#include "Board.h"
#include "Configuration.h"

extern std::shared_ptr<Configuration> config;

/** \brief Result of a navigation command executed on the game thread */
struct NavResult {
    bool success = false;
    bool newBranch = false;
};

/** \brief Navigation command queued for execution on the game thread */
struct NavCommand {
    enum Type { BACK, FORWARD, TO_START, TO_END, TO_VARIATION };
    Type type;
    Move move;  // For TO_VARIATION
    std::shared_ptr<std::promise<NavResult>> resultPromise;
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

    size_t addEngine(Engine* engine);

    size_t addPlayer(Player* player);

    Engine* currentCoach();
    Engine* currentKibitz();

    Player* currentPlayer();

    std::string getName(size_t id) { return playerManager->getName(id); }

    void interrupt();

    // Check if genmove is in progress (engine is thinking)
    bool isThinking() const;

    bool clearGame(int boardSize, float komi, int handicap);

    void removeSgfPlayers();  // Remove temporary players created from SGF loading

    void setKomi(float komi);

    bool setFixedHandicap(int handicap);

    void run();

    bool isRunning() const;

    void gameLoop();

    bool humanToMove();

    void playLocalMove(const Move& move);
    void playKibitzMove();

    // Analysis mode support
    bool setGameMode(GameMode mode);  // Returns true if mode change succeeded
    GameMode getGameMode() const { return gameMode; }
    void setAiVsAi(bool enabled);
    bool isAiVsAi() const { return aiVsAiMode; }

    void loadEngines(std::shared_ptr<Configuration> config);

	size_t activatePlayer(int which, size_t newIndex);

	size_t getActivePlayer(int which);

	[[nodiscard]] Move getLocalMove(const Position& coord) const;
    [[nodiscard]] Move getLocalMove(Move::Special move) const;
	void reset();

    void addGameObserver(GameObserver* pobserver) {
        gameObservers.push_back(pobserver);
    }

    // Navigation methods for SGF replay
    bool navigateBack();   // Undo one move during navigation
    bool navigateForward();  // Play next move during navigation
    bool navigateToVariation(const Move& move);  // Navigate to specific variation
    bool navigateToStart();  // Go to beginning of game
    bool navigateToEnd();    // Go to end of game (main line)

    std::vector<Player*> getPlayers() { return playerManager->getPlayers(); }

    bool loadSGF(const std::string& fileName, int gameIndex = 0);

private:
    void syncOtherEngines(const Move& move, Player* player, Engine* coach, Engine* kibitzEngine, bool kibitzed);
    void notifyMoveComplete(Engine* coach, const Move& move, Engine* kibitzEngine, bool kibitzed, const std::string& engineComments);
    void setHandicapStones(const std::vector<Position>& stones);
    void applyHandicapStonesToEngines(const std::vector<Position>& stones, Engine* coach);
    std::vector<GameObserver*> gameObservers;
    GobanModel& model;
    std::unique_ptr<std::thread> thread;
    std::mutex mutex2;
    std::atomic<bool> interruptRequested{false};
    std::atomic<bool> hasThreadRunning{false};
    Player* playerToMove;
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
    std::mutex navQueueMutex;
    std::condition_variable navQueueCV;

    void processNavigationQueue();
    NavResult executeNavCommand(const NavCommand& cmd);
    void wakeGameThread();
    void waitForCommandOrTimeout(int ms);
};

#endif // GAMETHREAD_H
