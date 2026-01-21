#ifndef GAMETHREAD_H
#define GAMETHREAD_H

#include <thread>
#include <mutex>
#include <atomic>
#include <string>

#include "player.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "Board.h"
#include "Configuration.h"

extern std::shared_ptr<Configuration> config;

/** \brief Game mode determining player interaction behavior
 */
enum class GameMode {
    MATCH,      ///< Default - strict turn alternation, assigned roles
    ANALYSIS    ///< Sabaki-like - human plays either color, AI responds based on move source
};

/** \brief Source of a move for Analysis mode behavior
 *
 * Only used in Analysis mode to determine who plays next:
 * - NONE: Initial state or after mode switch - human plays next
 * - HUMAN: User clicked on board - AI will auto-respond
 * - KIBITZ: Kibitz engine suggested - AI waits for explicit genmove (Space)
 * - AI: AI just played - human plays next
 *
 * Not used in Match mode (player selection based on assigned roles).
 */
enum class MoveSource {
    NONE,       ///< Initial/reset state - human plays next
    HUMAN,      ///< User clicked on board - AI will auto-respond
    KIBITZ,     ///< Kibitz engine suggested - AI waits for explicit genmove
    AI          ///< AI's own genmove - human plays next
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

    void setRole(size_t playerIndex, int role, bool add = false);

    std::string getName(size_t id) { return players[id]->getName(); }

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
    void triggerGenmove();
    bool isWaitingForGenmove() const { return waitingForGenmove; }
    void setAiVsAi(bool enabled);
    bool isAiVsAi() const { return aiVsAiMode; }

    void loadEngines(std::shared_ptr<Configuration> config);

	size_t activatePlayer(int which, int delta = 1);

	size_t getActivePlayer(int which);

	[[nodiscard]] Move getLocalMove(const Position& coord) const;
    [[nodiscard]] Move getLocalMove(Move::Special move) const;
	void reset();

    void addGameObserver(GameObserver* pobserver) {
        gameObservers.push_back(pobserver);
    }

    bool undo(Player * engine, bool doubleUndo);

    // Navigation methods for SGF replay
    bool navigateBack();   // Undo one move during navigation
    bool navigateForward();  // Play next move during navigation
    bool navigateToVariation(const Move& move);  // Navigate to specific variation
    bool navigateToStart();  // Go to beginning of game
    bool navigateToEnd();    // Go to end of game (main line)

    std::vector<Player*> getPlayers() { return players;}

    bool loadSGF(const std::string& fileName, int gameIndex = 0);

private:
    // Undo processing result
    struct UndoResult {
        bool success = false;
        bool doubleUndo = false;
    };

    UndoResult processUndo(Engine* coach);
    void syncOtherEngines(const Move& move, Player* player, Engine* coach, Engine* kibitzEngine, bool kibitzed, bool doubleUndo);
    void notifyMoveComplete(Engine* coach, const Move& move, Engine* kibitzEngine, bool kibitzed, const std::string& engineComments);
    void setHandicapStones(const std::vector<Position>& stones);
    std::vector<GameObserver*> gameObservers;
    GobanModel& model;
    std::vector<Engine*> engines; //engines know the rules
    std::vector<Player*> players; //all players including engines, humans, spectators
    std::unique_ptr<std::thread> thread;
    std::mutex mutex2;
    std::atomic<bool> interruptRequested{false};
    std::atomic<bool> hasThreadRunning{false};
    Player* playerToMove;
	std::size_t human, sgf, coach, kibitz;
	std::size_t numPlayers;
	std::array<std::size_t, 2> activePlayer;
    mutable std::mutex playerMutex;
    std::condition_variable engineStarted;

    // Analysis mode state
    GameMode gameMode = GameMode::MATCH;
    bool waitingForGenmove = false;
    bool aiVsAiMode = false;
    MoveSource lastMoveSource = MoveSource::NONE;
    std::condition_variable genmoveTriggered;

    // Navigation synchronization - prevents genmove during active navigation
    std::atomic<bool> navigationInProgress{false};
};

#endif // GAMETHREAD_H
