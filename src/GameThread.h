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

    GameThread(GobanModel &model);
    ~GameThread();

    size_t addEngine(Engine* engine);

    size_t addPlayer(Player* player);

    Engine* currentCoach();
    Engine* currentKibitz();

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
    void playKibitzMove();

    void loadEngines(std::shared_ptr<Configuration> config);

	int activatePlayer(int which, int delta = 1);

	int getActivePlayer(int which);

	Move getLocalMove(const Position& coord);
    Move getLocalMove(Move::Special move);
	void reset();

    void addGameObserver(GameObserver* pobserver) {
        gameObservers.push_back(pobserver);
    }

    bool undo(Player * engine, bool doubleUndo);

    std::vector<Engine*> getEngines() { return engines;}
    std::vector<Player*> getPlayers() { return players;}

    void unloadEngines() {
        for(auto& engine: engines) {
            dynamic_cast<GtpEngine*>(engine)->issueCommand("quit");
        }
    }

private:
    std::vector<GameObserver*> gameObservers;
    GobanModel& model;
    std::vector<Engine*> engines; //engines know the rules
    std::vector<Player*> players; //all players including engines, humans, spectators
    std::thread* thread;
    std::mutex mutex2;
    bool interruptRequested, hasThreadRunning;
    Player* playerToMove;
	std::size_t human, sgf, coach, kibitz;
	std::size_t numPlayers;
	std::array<std::size_t, 2> activePlayer;
    std::mutex playerMutex;
    std::condition_variable engineStarted;
    int komi;
    int boardSize;
    int handicap;
 //public:
//	bool showTerritory;
};

#endif // GAMETHREAD_H
