#ifndef GAMETHREAD_H
#define GAMETHREAD_H

#include <thread>
#include <mutex>
#include <string>

#include "player.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "Board.h"
#include "Configuration.h"

extern std::shared_ptr<Configuration> config;

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

    bool clearGame(int boardSize, float komi, int handicap);

    void setKomi(float komi);

    bool setFixedHandicap(int handicap);

    void run();

    bool isRunning() const;

    void gameLoop();

    bool humanToMove();

    void playLocalMove(const Move& move);
    void playKibitzMove();

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

    std::vector<Player*> getPlayers() { return players;}

    bool loadSGF(const std::string& fileName);

private:
    void setHandicapStones(const std::vector<Position>& stones);
    std::vector<GameObserver*> gameObservers;
    GobanModel& model;
    std::vector<Engine*> engines; //engines know the rules
    std::vector<Player*> players; //all players including engines, humans, spectators
    std::thread* thread;
    std::mutex mutex2;
    volatile bool interruptRequested, hasThreadRunning;
    Player* playerToMove;
	std::size_t human, sgf, coach, kibitz;
	std::size_t numPlayers;
	std::array<std::size_t, 2> activePlayer;
    std::mutex playerMutex;
    std::condition_variable engineStarted;
};

#endif // GAMETHREAD_H
