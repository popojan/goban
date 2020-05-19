#ifndef GAMETHREAD_H
#define GAMETHREAD_H

#include <thread>
#include <mutex>
#include <string>

#include "player.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "Board.h"

/** \brief Background thread responsible for rules enforcing
 *
 */
class GameThread
{
public:

    GameThread(GobanModel &model);
    ~GameThread();

    size_t addEngine(Engine* engine);

    size_t addPlayer(Player* player);

    Engine* currentCoach();

    Player* currentPlayer();

    void setRole(size_t playerIndex, int role, bool add = false);

    std::string& getName(size_t id, Color whose) { return players[id]->getName(whose); }

    void interrupt();

    bool clearGame(int boardSize, float komi, int handicap);

    void setKomi(float komi);

    bool setFixedHandicap(int handicap);

    void run();

    bool isRunning();

    void gameLoop();

    bool humanToMove();

    void playLocalMove(const Move& move);

    void loadEngines(const std::string& path);

	int activatePlayer(int which, int delta = 1);

	int getActivePlayer(int which);

	Move getLocalMove(const Position& coord);

	void init();
	void reset();

    void addGameObserver(GameObserver* pobserver) {
        gameObservers.push_back(pobserver);
    }

    bool undo(Player * engine, bool doubleUndo);

    const std::vector<Engine*> getEngines() { return engines;}
    const std::vector<Player*> getPlayers() { return players;}

private:
	std::shared_ptr<spdlog::logger> console;
    std::vector<GameObserver*> gameObservers;
    GobanModel& model;
    std::vector<Engine*> engines; //engines know the rules
    std::vector<Player*> players; //all players including engines, humans, spectators
    std::thread* thread;
    std::mutex mutex2;
    volatile bool interruptRequested, hasThreadRunning;
    Player* playerToMove;
	std::size_t human, sgf, coach;
	std::size_t numPlayers;
	std::array<std::size_t, 2> activePlayer;
    std::mutex playerMutex;
 //public:
//	bool showTerritory;
};

#endif // GAMETHREAD_H
