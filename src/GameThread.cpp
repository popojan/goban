#include "GameThread.h"
#include <fstream>
 
GameThread::GameThread(GobanModel &m) :
        game(m), thread(0), colorToMove(Color::BLACK), interruptRequested(false), hasThreadRunning(false),
        playerToMove(0), human(0), numPlayers(0), activePlayer({0, 0}),
        showTerritory(false) {
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
    std::unique_lock<std::mutex> lock(mutex);
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
        std::cerr << playerIndex
            << "newType = [human = " << (int)player->isTypeOf(Player::HUMAN) << ", computer = " << (int)player->isTypeOf(Player::ENGINE) << "] "
            << "newRole = [black = " << (int)player->hasRole(Player::BLACK) << ", white = " << (int)player->hasRole(Player::WHITE) << "]"
            << std::endl;
        if(!add && playerToMove != 0 && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            playerToMove->suggestMove(Move(Move::INTERRUPT, colorToMove));
        }
    }
}
void GameThread::changeTurn() {
    game.state.colorToMove = colorToMove = Color::other(colorToMove);
    std::cerr << "changeTurn = " << colorToMove << std::endl;
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
    game.newGame(boardSize);
    for (auto pit = players.begin(); pit != players.end(); ++pit) {
        Player* player = *pit;
        player->boardsize(boardSize);
    }
    players[sgf]->clear();
    setKomi(game.komi);
    setFixedHandicap(game.handicap);
    colorToMove = Color::BLACK;
    lastMove = Move(Move::INVALID, Color::EMPTY);
}

void GameThread::setKomi(float komi) {
    if (!game.started) {
        std::cerr << "setting komi " << komi << std::endl;
        game.komi = komi;
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
        clearGame(game.getBoardSize());
    } else if (oldp == sgf) {
        players[oldp]->clear();
    }
    setRole(oldp, role, false);
    setRole(newp, role, true);
    if(players[oldp]->getRole() == Player::SPECTATOR && !players[oldp]->isTypeOf(Player::HUMAN)) {
        players[oldp]->clear();
        std::cerr << "Clearing board" << std::endl;
    }
    if(newp == sgf || oldp == sgf) {
        std::cerr << "newp == sgf" << std::endl;
        int role = which == 0 ? Player::WHITE : Player::BLACK;
        std::size_t oldp = activePlayer[1-which];
        activePlayer[1-which] = newp;
        setRole(oldp, role, false);
        setRole(newp, role, true);
        if(players[oldp]->getRole() == Player::SPECTATOR && !players[oldp]->isTypeOf(Player::HUMAN)) {
            players[oldp]->clear();
            std::cerr << "Clearing board" << std::endl;
        }
    }
    if(!((players[newp]->getRole() & (Player::COACH)) || players[newp]->isTypeOf(Player::HUMAN))) {
        std::cerr << "Replaying history" << std::endl;
        for(auto it = game.history.begin(); it != game.history.end(); ++it) {
           players[newp]->play(*it); 
        }
    }
    std::cerr << "Needs replaying history " << game.history.size() << "  " <<  players[newp]->getRole() << std::endl;
    
    return newp;
}

bool GameThread::setFixedHandicap(int handicap) {
    bool success = false;
    if (!game.started) {
        int lastSize = game.board.getSize();
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
            }
        }
        else {
            if (handicap == 1) {
                colorToMove = Color::WHITE;
            }
            success = true;
        }
		game.handicap = handicap;
		game.board.copyStateFrom(coach->showboard());
	}
	return success;
}
void GameThread::run() {
    game.start();
    thread = new std::thread(&GameThread::gameLoop, this);
    while(!hasThreadRunning && !playerToMove);
}

bool GameThread::isRunning() { return hasThreadRunning;}

bool GameThread::getResult(GameState::Result& ret) {
    if (game.over && !hasThreadRunning) {
        game.result(ret, lastMove);
        if(thread) {
            thread->join();
            delete thread;
            thread = 0;
        }
        return true;
    }
    return false;
}

void GameThread::toggleTerritory(int jak){
    showTerritory = jak < 0 ? !showTerritory : (jak > 0);
    //showTerritoryAuto = false;
    if (showTerritory && playerToMove != 0 && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
        playerToMove->suggestMove(Move(Move::INTERRUPT, colorToMove));
    }
}

void GameThread::gameLoop() {
    interruptRequested = false;
    bool prevPass = false;
    std::unique_lock<std::mutex> lock(playerMutex, std::defer_lock);
    while (game && !interruptRequested) {
        Move move;
        Engine* coach = currentCoach();
        Player* player = currentPlayer();
        hasThreadRunning = true;
        if(coach && player) {
            bool success = false;
            bool locked = false;
            bool doubleUndo = false;
            {
                if (!interruptRequested && player == currentPlayer()) {
                    playerToMove = player;
                    if(!player->isTypeOf(Player::HUMAN)){
                        lock.lock();
                        locked = true;
                    }
                    player->suggestMove(Move(Move::INVALID, colorToMove));
                    move = player->genmove(colorToMove);
                    playerToMove = 0;
                    std::cerr << "MOVE to BOOL = " << (int)(bool)move << " " << move << std::endl;
                    if(player->isTypeOf(Player::HUMAN)){
                        lock.lock();
                        locked = true;
                    }
                    if(move == Move::UNDO) {
                        success = coach->undo();
                        changeTurn();
                        if(currentPlayer()->isTypeOf(Player::ENGINE)) {
                            success = coach->undo();
                            doubleUndo = true;
                        }
                        changeTurn();
                    }
                    else {
                        success = move && (move == Move::RESIGN || player == coach || coach->play(move));
                        game.history.push_back(move);
                    }
                    if ((move == Move::PASS && prevPass) || move == Move::RESIGN) {
                        game.state.reason = move == Move::RESIGN ? GameState::RESIGNATION : GameState::DOUBLE_PASS;
                        game.over = true;
                    }
                    else if(move == Move::PASS) {
                        prevPass = true;
                        bool blackPass =  colorToMove == Color::BLACK;
                        game.state.msg = blackPass ? GameState::BLACK_PASS : GameState::WHITE_PASS;
                    }
                    else {
                        prevPass = false;
                        game.state.msg = GameState::NONE;
                    }
                }
                if(success) {
                    if(game.over) {
                        std::cerr << "Main Over! Reason " << game.state.reason << std::endl;
                        showTerritory = true;
                    }
                    else {
                        for (auto pit = players.begin(); pit != players.end(); ++pit) {
                            Player* p = *pit;
                            if (p->getRole() != Player::NONE && p->getRole() != Player::SPECTATOR
                                    && p != (Player*)coach && p != player) {
                                std::cerr << "DEBUG play iter" << std::endl;
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

                    {
                        const Board& newBoard = coach->showboard();
                        std::cerr << "LOCK board" << std::endl;
                        std::unique_lock<std::mutex> lock(mutex);
                        game.board.copyStateFrom(newBoard);
                        game.board.positionNumber += 1;
                        game.board.order += 1;
                        lastMove = move;
                        if(!doubleUndo)
                            changeTurn();
                    }
                }
                else if (!interruptRequested && move != Move::INTERRUPT) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                if (showTerritory && (success || move == Move::INTERRUPT)) {
                    if (game.state.reason == GameState::DOUBLE_PASS) {
                        const Board& newTerritory = coach->showterritory(true, Color::other(colorToMove));
                        {
                            std::cerr << "LOCK territory" << std::endl;
                            std::unique_lock<std::mutex> lock(mutex);
                            game.territory.copyStateFrom(newTerritory);
                            game.board.positionNumber += 1;
                        }
                    }
                    else {
                        std::cerr << "LOCK territory" << std::endl;
                        Board newTerritory = coach->showterritory(false, Color::other(colorToMove));
                        std::unique_lock<std::mutex> lock(mutex);
                        game.territory.copyStateFrom(newTerritory);
                        game.board.positionNumber += 1;
                    }
                }
                if (game.over == true) {
                    game.result(game.state.adata, move);
                    break;
                }
            }
            if(locked)
                lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
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

Move GameThread::getLastMove() { 
    return lastMove;
}

void GameThread::loadEngines(const std::string& dir) {
    boost::filesystem::path p(dir);

    bool hasCouch(false);

    if(boost::filesystem::is_directory(p)) {
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

Move GameThread::getLocalMove(std::pair<int, int>& coord) {
    return Move(Position(static_cast<unsigned>(coord.first), static_cast<unsigned>(coord.second)), colorToMove);
}
