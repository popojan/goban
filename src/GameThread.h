#ifndef GAMETHREAD_H
#define GAMETHREAD_H

#include "player.h"
#include "GobanModel.h"
#include "Board.h"

#include <thread>
#include <mutex>
#include <string>

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

    void changeTurn();

    void interrupt();

    void clearGame(int boardSize);

    bool getResult(GameState::Result& ret);

    void setKomi(float komi);

    bool setFixedHandicap(int handicap);

    void run();

    bool isRunning();
    bool isOver();

    void gameLoop();

    void playLocalMove(const Move& move);
    Color getCurrentColor();

    int boardChanged(Board&);

    Move getLastMove();

    void toggleTerritory(int jak = -1);

	void invalidateBoard();

	void loadEngines(const std::string& path);

	int activatePlayer(int which);

	int getActivePlayer(int which);

	Move getLocalMove(std::pair<int, int>& coord);

	void init();
	void reset();

private:
    GobanModel& game;
    std::vector<Engine*> engines; //engines know the rules
    std::vector<Player*> players; //all players including engines, humans, spectators
    std::thread* thread;
    Color colorToMove;
    std::mutex mutex;
    std::mutex playerMutex;
    std::condition_variable cvPlayer;
    volatile bool interruptRequested, hasThreadRunning;
    Move lastMove;
    Player* playerToMove;
	std::size_t human;
	std::size_t sgf;
	std::size_t numPlayers;
	std::array<std::size_t, 2> activePlayer;
public:
	bool showTerritory;
};

#endif // GAMETHREAD_H
