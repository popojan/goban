#include "GameThread.h"
#include <fstream>

GameThread::GameThread(GobanModel &m) :
        model(m), thread(0), colorToMove(Color::BLACK), interruptRequested(false), hasThreadRunning(false),
        playerToMove(0), human(0), numPlayers(0), activePlayer({0, 0})
        /*, showTerritory(false)*/ {
    console = spdlog::get("console");
    std::string list("./config/engines.enabled");
    loadEngines(list);
}

void GameThread::reset() {
    colorToMove = Color::BLACK;
    interruptRequested = false;
    hasThreadRunning = false;
    playerToMove = 0;
}

GameThread::~GameThread() {
    std::unique_lock<std::mutex> lock(mutex);
    interrupt();
    for(auto it = players.begin(); it != players.end(); ++it) {
        delete *it;
    }
    players.clear();
}

size_t GameThread::addEngine(Engine* engine) {
    engines.push_back(engine);
    players.push_back(engine);
    return players.size() - 1;
}

size_t GameThread::addPlayer(Player* player) {
    players.push_back(player);
    return players.size() - 1;
}

Engine* GameThread::currentCoach() {
    for(auto eit = engines.begin(); eit != engines.end(); ++eit) {
        if((*eit)->hasRole(Player::COACH)) return *eit;
    }
    return 0;
}
Player* GameThread::currentPlayer() {
    int roleToMove = colorToMove == Color::BLACK ? Player::BLACK : Player::WHITE;
    for(auto pit = players.begin(); pit != players.end(); ++pit) {
        if((*pit)->hasRole(roleToMove)) return *pit;
    }
    return 0;
}

void GameThread::setRole(size_t playerIndex, int role, bool add) {
    if(players.size() > playerIndex) {
        std::unique_lock<std::mutex> lock(mutex);
        Player* player = players[playerIndex];
        player->setRole(role, add);
        console->debug("Player[{}] newType = [human = {}, computer = {}] newRole = [black = {}, whsite = {}]",
                playerIndex,
                player->isTypeOf(Player::HUMAN),
                player->isTypeOf(Player::ENGINE),
                player->hasRole(Player::BLACK),
                player->hasRole(Player::WHITE)
        );
        if(!add && playerToMove != 0 && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            playerToMove->suggestMove(Move(Move::INTERRUPT, colorToMove));
        }
    }
}
void GameThread::changeTurn() {
    model.state.colorToMove = colorToMove = Color::other(colorToMove);
    console->debug("changeTurn = {}", colorToMove.toString());
}
void GameThread::interrupt() {
    if(thread != 0) {
        interruptRequested = true;
        playLocalMove(Move(Move::INTERRUPT, colorToMove));
        thread->join();
        delete thread;
        thread = 0;
    }
}

void GameThread::clearGame(int boardSize) {
    model.newGame(boardSize);
    for (auto pit = players.begin(); pit != players.end(); ++pit) {
        Player* player = *pit;
        player->boardsize(boardSize);
    }
    players[sgf]->clear();
    setKomi(model.komi);
    setFixedHandicap(model.handicap);
    colorToMove = Color::BLACK;
}

void GameThread::setKomi(float komi) {
    if (!model.started) {
        console->debug("setting komi {}", komi);
        model.komi = komi;
        for (auto pit = players.begin(); pit != players.end(); ++pit) {
            Player* player = *pit;
            player->komi(komi);
        }
    }

}

int GameThread::getActivePlayer(int which) {
    return activePlayer[which];
}

int GameThread::activatePlayer(int which) {
    std::lock_guard<std::mutex> lock(playerMutex);
    std::size_t oldp = activePlayer[which];
    std::size_t newp = (oldp + 1) %  numPlayers;
    activePlayer[which] = newp;
    int role = which == 0 ? Player::BLACK : Player::WHITE;
    if(newp == sgf) {
        clearGame(model.getBoardSize());
    } else if (oldp == sgf) {
        players[oldp]->clear();
    }
    setRole(oldp, role, false);
    setRole(newp, role, true);
    if(players[oldp]->getRole() == Player::SPECTATOR && !players[oldp]->isTypeOf(Player::HUMAN)) {
        players[oldp]->clear();
        console->debug("Clearing board");
    }
    if(newp == sgf || oldp == sgf) {
        console->debug("newp == sgf");
        int role = which == 0 ? Player::WHITE : Player::BLACK;
        std::size_t oldp = activePlayer[1-which];
        activePlayer[1-which] = newp;
        setRole(oldp, role, false);
        setRole(newp, role, true);
        if(players[oldp]->getRole() == Player::SPECTATOR && !players[oldp]->isTypeOf(Player::HUMAN)) {
            players[oldp]->clear();
            console->debug("Clearing board");
        }
    }
    if(!((players[newp]->getRole() & (Player::COACH)) || players[newp]->isTypeOf(Player::HUMAN))) {
        console->debug("Replaying history");
        for(auto it = model.history.begin(); it != model.history.end(); ++it) {
           players[newp]->play(*it);
        }
        console->debug("Needs replaying history size={} for player with role={}",
                       model.history.size(), players[newp]->getRole());
    }
    return newp;
}

bool GameThread::setFixedHandicap(int handicap) {
    bool success = false;
    if (!model.started) {
        int lastSize = model.board.getSize();
        success = true;
		Engine* coach = currentCoach();
		if (handicap >= 2) {
            coach->boardsize(lastSize);
            std::vector<Position> stones;
            if (coach != 0) {
                success = coach->fixed_handicap(handicap, stones);
				if (handicap > 0 && !success) {
					return setFixedHandicap(0);
				}
            }
            if (success) {
                for (auto pit = players.begin(); pit != players.end(); ++pit) {
                    Player* player = *pit;
                    if (player != coach && player->getRole() != Player::NONE) {
                        player->boardsize(lastSize);
                        for (auto pit = stones.begin(); pit != stones.end(); ++pit)
                            success &= player->play(Move(*pit, Color::BLACK));
                    }

                }
                colorToMove = Color::WHITE;
            }
        }
        else {
            success = true;
        }
        model.handicap = handicap;
		model.board.copyStateFrom(coach->showboard());
	}
	return success;
}
void GameThread::run() {
    model.start();
    thread = new std::thread(&GameThread::gameLoop, this);
    while(!hasThreadRunning && !playerToMove);
}

bool GameThread::isRunning() { return hasThreadRunning;}

//void GameThread::toggleTerritory(int jak){
    //showTerritory = jak < 0 ? !showTerritory : (jak > 0);
    //showTerritoryAuto = false;
//    if (showTerritory && playerToMove != 0 && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
//        playerToMove->suggestMove(Move(Move::INTERRUPT, colorToMove));
//    }
//}

void GameThread::gameLoop() {
    hasThreadRunning = true;
    interruptRequested = false;
    bool prevPass = false;
    while (model && !interruptRequested) {
        Engine* coach = currentCoach();
        Player* player = currentPlayer();
        if(coach && player && !interruptRequested) {
            bool success = false;
            bool doubleUndo = false;
            //std::lock_guard<std::mutex> lock(playerMutex);
            playerToMove = player;
            //cancel blocking wait for input if human player
            player->suggestMove(Move(Move::INVALID, colorToMove));
            //blocking wait for move
            Move move = player->genmove(colorToMove);
            console->debug("MOVE to {}, valid = {}", move.toString(), move);
            playerToMove = 0;
            if (move == Move::UNDO) {
                success = coach->undo();
                changeTurn();
                if (currentPlayer()->isTypeOf(Player::ENGINE)) {
                    success = coach->undo();
                    changeTurn();
                    doubleUndo = true;
                }
            } else if (move) {
                // coach plays
                success = player == coach
                    || move == Move::RESIGN
                    || coach->play(move);
                //other engines play
                for (auto pit = players.begin(); pit != players.end(); ++pit) {
                    Player* p = *pit;
                    if (p->getRole() != Player::NONE && p->getRole() != Player::SPECTATOR
                            && p != reinterpret_cast<Player*>(coach) && p != player) {
                        console->debug("DEBUG play iter");
                        if(move == Move::UNDO) {
                            p->undo();
                            if(doubleUndo)
                               p->undo();
                        }
                        else
                            p->play(move);
                    }
                }
            }
            //update model
            if(success) {
                const Board& result = coach->showboard();
                console->debug("LOCK board");
                std::lock_guard<std::mutex> lock(mutex);
                model.update(move, result);
                changeTurn();
            }
            if (model.over || (model.board.showTerritory && (success || move == Move::INTERRUPT))) {
                const Board& newTerritory = coach->showterritory(model.state.reason == GameState::DOUBLE_PASS, Color::other(colorToMove));
                console->debug("LOCK territory");
                std::lock_guard<std::mutex> lock(mutex);
                model.update(newTerritory);
            }
            if(model.over){
                model.result(move, model.state.adata);
                if (model.state.reason == GameState::DOUBLE_PASS)
                    model.board.toggleTerritoryAuto(true);
                break;
            }
            if(success && move != Move::INTERRUPT)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    hasThreadRunning = false;
}

Color GameThread::getCurrentColor() {
    return colorToMove;
}
void GameThread::playLocalMove(const Move& move) {
    std::unique_lock<std::mutex> lock(playerMutex);
    if(playerToMove) playerToMove->suggestMove(move);
}

void GameThread::loadEngines(const std::string& dir) {
    boost::filesystem::path p(dir);

    if(boost::filesystem::is_directory(p)) {

        bool hasCouch(false);

        for (auto it = boost::filesystem::directory_iterator(p); it != boost::filesystem::directory_iterator(); ++it) {
            std::string fengine(it->path().string());
            std::ifstream fin(fengine.c_str());
            std::string exe, path, cmd, name;
            std::getline(fin, path);
            std::getline(fin, exe);
            std::getline(fin, cmd);
            std::getline(fin, name);
            int isCouch = 0;
            fin >> isCouch;
            fin.close();
            int role = Player::SPECTATOR;
            if(isCouch && !hasCouch) {
                role |= Player::COACH;
                hasCouch = true;
            };
            std::size_t id = addEngine(new GtpEngine(exe, cmd, path, name));
            setRole(id, role, true);
        }
    }
    sgf = addPlayer(new SgfPlayer("SGF Record", "./problems/alphago-2016/3/13/Tictactoe.sgf"));

    human = addPlayer(new LocalHumanPlayer("Human"));

    numPlayers = human + 1;
    activePlayer[0] = sgf;
    activePlayer[1] = human;
}

Move GameThread::getLocalMove(const Position& coord) {
    return Move(coord, colorToMove);
}
