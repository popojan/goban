#include "GameThread.h"
#include <fstream>
#include <nlohmann/json.hpp>

GameThread::GameThread(GobanModel &m) :
        model(m), thread(nullptr), interruptRequested(false), hasThreadRunning(false),
        playerToMove(0), human(0), sgf(0), coach(0), numPlayers(0), activePlayer({0, 0})
{
    loadEngines(config);
}

void GameThread::reset() {
    interruptRequested = false;
    hasThreadRunning = false;
    playerToMove = 0;
}

GameThread::~GameThread() {
    std::unique_lock<std::mutex> lock(mutex2);
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
    int roleToMove = model.state.colorToMove == Color::BLACK ? Player::BLACK : Player::WHITE;
    for(auto pit = players.begin(); pit != players.end(); ++pit) {
        if((*pit)->hasRole(roleToMove)) return *pit;
    }
    return 0;
}

void GameThread::setRole(size_t playerIndex, int role, bool add) {
    if(players.size() > playerIndex) {
        std::unique_lock<std::mutex> lock(mutex2);
        Player* player = players[playerIndex];
        player->setRole(role, add);
        spdlog::debug("Player[{}] newType = [human = {}, computer = {}] newRole = [black = {}, whsite = {}]",
            playerIndex,
            player->isTypeOf(Player::HUMAN),
            player->isTypeOf(Player::ENGINE),
            player->hasRole(Player::BLACK),
            player->hasRole(Player::WHITE)
        );
        if(!add && playerToMove != 0 && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            playerToMove->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
        }
        std::for_each(
            gameObservers.begin(), gameObservers.end(),
            [player, role](GameObserver* observer){
                observer->onPlayerChange(role, player->getName());
            }
        );

    }
}

void GameThread::interrupt() {
    if(thread != 0) {
        interruptRequested = true;
        playLocalMove(Move(Move::INTERRUPT, model.state.colorToMove));
        thread->join();
        delete thread;
        thread = 0;
    }
}

/** \brief Clears the board and resets both komi and handicap.
 *
 * @param boardSize
 */
bool GameThread::clearGame(int boardSize, float komi, int handicap) {

    bool success = true;

    for (auto pit = players.begin(); pit != players.end(); ++pit) {
        Player* player = *pit;
        success &= player->boardsize(boardSize);
        success &= player->clear();
    }
    if(! success)
        return false;

    std::for_each(
        gameObservers.begin(), gameObservers.end(),
        [boardSize](GameObserver* observer){
            observer->onBoardSized(boardSize);
        }
    );
    setKomi(komi);
    setFixedHandicap(handicap);
    return success;

}

void GameThread::setKomi(float komi) {
    std::for_each(
        gameObservers.begin(), gameObservers.end(),
        [komi](GameObserver* observer){observer->onKomiChange(komi);}
    );
    for (auto pit = players.begin(); pit != players.end(); ++pit) {
        Player* player = *pit;
        player->komi(komi);
    }
}

int GameThread::getActivePlayer(int which) {
    return activePlayer[which];
}

int GameThread::activatePlayer(int which, int delta) {
    std::lock_guard<std::mutex> lock(playerMutex);

    std::size_t oldp = activePlayer[which];
    std::size_t newp = (oldp + delta) %  numPlayers;

    activePlayer[which] = newp;

    int role = which == 0 ? Player::BLACK : Player::WHITE;

    setRole(oldp, role, false);
    setRole(newp, role, true);

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
            coach->clear();
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
                        player->clear();
                        for (auto pit = stones.begin(); pit != stones.end(); ++pit)
                            success &= player->play(Move(*pit, Color::BLACK));
                    }

                }
                model.state.colorToMove = Color::WHITE;
            }
        }
        else {
            success = true;
        }
        model.state.handicap = handicap;
		model.board.copyStateFrom(coach->showboard());
	}
	return success;
}
void GameThread::run() {
    model.start();
    thread = new std::thread(&GameThread::gameLoop, this);
    while(!hasThreadRunning || !playerToMove);
}

bool GameThread::isRunning() { return hasThreadRunning;}

bool GameThread::humanToMove() {
    std::unique_lock<std::mutex> lock(playerMutex);
    if(playerToMove) {
        return currentPlayer()->isTypeOf(Player::HUMAN);
    }
    return false;
}
bool GameThread::undo(Player * engine, bool doubleUndo) {
    bool success = engine->undo();
    if(success && doubleUndo) {
        success &= engine->undo();
    }
    if (!success) {
        spdlog::debug("Undo not supported. Replaying game from the beginning.");
        success = engine->clear() || engine->boardsize(model.getBoardSize());
        model.game.replay(
            [&](const Move &move) {
                success &= engine->play(move);
            }
        );
    }
    return success;
}

void GameThread::gameLoop() {
    hasThreadRunning = true;
    interruptRequested = false;
    while (model && !interruptRequested) {
        Engine* coach = currentCoach();
        Player* player = currentPlayer();
        std::unique_lock<std::mutex> lock(playerMutex, std::defer_lock);
        bool locked = false;
        if(coach && player && !interruptRequested) {
            bool success = false;
            bool doubleUndo = false;
            playerToMove = player;
            if(!player->isTypeOf(Player::HUMAN)){
                lock.lock();
                locked = true;
            }
            //cancel blocking wait for input if human player
            player->suggestMove(Move(Move::INVALID, model.state.colorToMove));
            //blocking wait for move
            Move move = player->genmove(model.state.colorToMove);
            spdlog::debug("MOVE to {}, valid = {}", move.toString(), move);
            playerToMove = 0;
            if(player->isTypeOf(Player::HUMAN)){
                lock.lock();
                locked = true;
            }
            //TODO no direct access to model but via observers
            if (move == Move::UNDO) {
                bool bothHumans = players[activePlayer[0]]->isTypeOf(Player::HUMAN)
                        && players[activePlayer[1]]->isTypeOf(Player::HUMAN);
                if(bothHumans && model.game.moveCount() > 0) {
                    success  = coach->undo();
                    model.game.undo();
                } else if(currentPlayer()->isTypeOf(Player::HUMAN) && model.game.moveCount() > 1) {
                        success  = coach->undo();
                        success &= coach->undo();
                        model.game.undo();
                        model.game.undo();
                        model.changeTurn();
                        doubleUndo = true;
                }
            }
            else if (move) {
                // coach plays
                success = player == coach
                          || move == Move::RESIGN
                          || coach->play(move);
            }
            if(success) {
                //other engines play
                if (!(move == Move::RESIGN)) {
                    for (auto pit = players.begin(); pit != players.end(); ++pit) {
                        Player *p = *pit;
                        if (p != reinterpret_cast<Player *>(coach) && p != player) {
                            spdlog::debug("DEBUG play iter");
                            if (move == Move::UNDO) {
                                undo(p, doubleUndo);
                            } else
                                p->play(move);
                        }
                    }
                }
                //update model

                const Board& result(
                        coach->showboard()
                );

                std::for_each(
                    gameObservers.begin(), gameObservers.end(),
                    [result, move](GameObserver* observer) {
                        observer->onGameMove(move);
                        observer->onBoardChange(result);
                    }
                );
            }

            //update territory if shown

            bool influence = model.board.showTerritory
                && (success || move == Move::INTERRUPT);

            if(influence) {
                bool finalized = model.state.reason == GameState::DOUBLE_PASS;
                const Board& result(
                    coach->showterritory(finalized, model.state.colorToMove)
                );
                std::for_each(
                    gameObservers.begin(), gameObservers.end(),
                    [result](GameObserver* observer){observer->onBoardChange(result);}
                );
            }

            if(model.over)
                break;


            if(success && move != Move::INTERRUPT)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        if(locked) lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    hasThreadRunning = false;
}

void GameThread::playLocalMove(const Move& move) {
    std::unique_lock<std::mutex> lock(playerMutex);
    if(playerToMove) playerToMove->suggestMove(move);
}

void GameThread::loadEngines(const Configuration& config) {
    auto bots = config.data.find("bots");
    if(bots != config.data.end()) {
        bool hasCoach(false);
        for(auto it = bots->begin(); it != bots->end(); ++it) {
            auto enabled = it->value("enabled", 1);
            if(enabled) {
                auto path = it->value("path", "" );
                auto name = it->value("name", "");
                auto command = it->value("command", "");
                auto parameters = it->value("parameters", "");
                auto main = it->value("main", 0);
                auto messages = it->value("messages", nlohmann::json::array());

                int role = Player::SPECTATOR;

                if(!command.empty()) {
                    auto engine = new GtpEngine(command, parameters, path, name);
                    for(auto &&msg: messages) {
                        engine->addOutputFilter(
                                msg.value("regex", ""),
                                msg.value("output", ""),
                                msg.value("var", ""));
                    }
                    std::size_t id = addEngine(engine);

                    if (main) {
                        if(!hasCoach) {
                          role |= Player::COACH;
                          hasCoach = true;
                          coach = id;
                          spdlog::info("Setting [{}] engine as coach and referee.",
                                  players[id]->getName());
                        } else {
                          spdlog::warn("Ignoring coach flag for [{}] engine, coach has already been set to [{}].",
                                  players[id]->getName(),
                                  players[coach]->getName());
                        }
                    }

                    setRole(id, role, true);
                }
            }
        }
    }
    sgf = -1; //addPlayer(new SgfPlayer("SGF Record", "./problems/alphago-2016/3/13/Tictactoe.sgf"));

    human = addPlayer(new LocalHumanPlayer("Human"));

    numPlayers = human + 1;
    activePlayer[0] = human;
    activePlayer[1] = coach;
}

Move GameThread::getLocalMove(const Position& coord) {
    return Move(coord, model.state.colorToMove);
}
