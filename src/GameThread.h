#ifndef GAMETHREAD_H
#define GAMETHREAD_H

#include <thread>
#include <mutex>
#include <string>

#include "player.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "Board.h"

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

    void clearGame(int boardSize);

    void setKomi(float komi);

    bool setFixedHandicap(int handicap);

    void run();

    bool isRunning();
    bool isOver();

    void gameLoop();

    bool humanToMove();

    void playLocalMove(const Move& move);
    Color getCurrentColor();

    int boardChanged(Board&);

    void toggleTerritory(int jak = -1);

	void invalidateBoard();

	void loadEngines(const std::string& path);

	int activatePlayer(int which, int delta = 1);

	int getActivePlayer(int which);

	Move getLocalMove(const Position& coord);

	void init();
	void reset();

    void addGameObserver(GameObserver* pobserver) {
        gobservers.push_back(pobserver);
    }

    void addBoardObserver(BoardObserver* pobserver) {
        bobservers.push_back(pobserver);
    }

private:
	std::shared_ptr<spdlog::logger> console;
    std::vector<GameObserver*> gobservers;
    std::vector<BoardObserver*> bobservers;
    GobanModel& model;
    std::vector<Engine*> engines; //engines know the rules
    std::vector<Player*> players; //all players including engines, humans, spectators
    std::thread* thread;
    std::mutex mutex2;
    std::condition_variable cvPlayer;
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
