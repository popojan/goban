#include "GameThread.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <cmath>

GameThread::GameThread(GobanModel &m) :
        model(m), thread(nullptr), interruptRequested(false), hasThreadRunning(false),
        playerToMove(nullptr), human(0), sgf(0), coach(0), kibitz(0), numPlayers(0), activePlayer({0, 0})
{

}

void GameThread::reset() {
    interruptRequested = false;
    hasThreadRunning = false;
    playerToMove = nullptr;
}

GameThread::~GameThread() {
    std::unique_lock<std::mutex> lock(mutex2);
    interrupt();
    for(auto & player : players) {
        delete player;
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
    for(auto & engine : engines) {
        if(engine->hasRole(Player::COACH)) return engine;
    }
    return nullptr;
}

Engine* GameThread::currentKibitz() {
    for(auto & engine : engines) {
        if(engine->hasRole(Player::KIBITZ)) return engine;
    }
    return nullptr;
}

Player* GameThread::currentPlayer() {
    int roleToMove = model.state.colorToMove == Color::BLACK ? Player::BLACK : Player::WHITE;
    for(auto & player : players) {
        if(player->hasRole(roleToMove)) return player;
    }
    return nullptr;
}

void GameThread::setRole(size_t playerIndex, int role, bool add) {
    if(players.size() > playerIndex) {
        std::unique_lock<std::mutex> lock(mutex2);
        Player* player = players[playerIndex];
        player->setRole(role, add);
        spdlog::debug("Player[{}] newType = [human = {}, computer = {}] newRole = [black = {}, white = {}]",
            playerIndex,
            player->isTypeOf(Player::HUMAN),
            player->isTypeOf(Player::ENGINE),
            player->hasRole(Player::BLACK),
            player->hasRole(Player::WHITE)
        );
        if(!add && playerToMove != nullptr && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            playerToMove->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
        }
        if(add) {
            std::for_each(
                    gameObservers.begin(), gameObservers.end(),
                    [player, role](GameObserver *observer) {
                        observer->onPlayerChange(role, player->getName());
                    }
            );
        }

    }
}

void GameThread::interrupt() {
    if(thread != nullptr) {
        interruptRequested = true;
        playLocalMove(Move(Move::INTERRUPT, model.state.colorToMove));
        thread->join();
        delete thread;
        thread = nullptr;
    }
}

/** \brief Clears the board and resets both komi and handicap.
 *
 * @param boardSize
 */
bool GameThread::clearGame(int boardSize, float komi, int handicap) {

    /*this->boardSize = boardSize;
    this->komi = komi;
    this->handicap = handicap;*/

    bool success = true;

    for (auto player : players) {
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
    for (auto player : players) {
        player->komi(komi);
    }
}

size_t GameThread::getActivePlayer(int which) {
    return activePlayer[which];
}

size_t GameThread::activatePlayer(int which, int delta) {
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
		Engine* coach = currentCoach();
        std::vector<Position> stones;
		if (handicap >= 2) {
            coach->boardsize(lastSize);
            coach->clear();
            success = coach->fixed_handicap(handicap, stones);
            if (!success) {
                return setFixedHandicap(0);
            }
            for (auto player : players) {
                if (player != coach && player->getRole() != Player::NONE) {
                    player->boardsize(lastSize);
                    player->clear();
                    for (auto & stone : stones)
                        success &= player->play(Move(stone, Color(Color::BLACK)));
                }
                model.state.colorToMove = Color(Color::WHITE);
            }
        } else {
            success = true;
        }
        if(success) {
            std::for_each(
                    gameObservers.begin(), gameObservers.end(),
                    [stones](GameObserver* observer){observer->onHandicapChange(stones);}
            );
        }
        model.state.handicap = handicap;
		model.board.copyStateFrom(coach->showboard());
	}
    return success;
}
void GameThread::run() {
    std::unique_lock<std::mutex> lock(playerMutex);
    model.start();
    spdlog::debug("construct thread {}", (bool)model);
    thread = new std::thread(&GameThread::gameLoop, this);
    engineStarted.wait(lock);
}

bool GameThread::isRunning() const { return hasThreadRunning;}

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
    interruptRequested = false;
    while (model && !interruptRequested) {
        Engine* coach = currentCoach();
        Engine* kibitz = currentKibitz();
        Player* player = currentPlayer();
        if(!hasThreadRunning) {
            hasThreadRunning = true;
            engineStarted.notify_all();
        }
        std::unique_lock<std::mutex> lock(playerMutex, std::defer_lock);
        bool locked = false;
        if(coach && player && !interruptRequested) {
            bool success = false;
            bool kibitzed = false;
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
            if(move == Move::KIBITZED) {
                move = kibitz->genmove(model.state.colorToMove);
                kibitzed = true;
            }
            spdlog::debug("MOVE to {}, valid = {}", move.toString(), (bool)move);
            playerToMove = nullptr;
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
                          || (kibitzed && kibitz == coach)
                          || move == Move::RESIGN
                          || coach->play(move);
            }
            std::ostringstream comment;
            if(success) {
                //other engines play
                if (!(move == Move::RESIGN)) {
                    for (auto p : players) {
                        if (p != reinterpret_cast<Player *>(coach) && p != player && (!kibitzed || p != kibitz)) {
                            spdlog::debug("DEBUG play iter");
                            if (move == Move::UNDO) {
                                undo(p, doubleUndo);
                            } else
                                p->play(move);
                        }
                        //compose move comment
                        if(p->isTypeOf(Player::ENGINE)) {
                            std::string engineMsg(dynamic_cast<GtpEngine*>(p)->lastError());
                            if(!engineMsg.empty())
                                comment << engineMsg << " (" << p->getName() << ") ";
                        }
                    }
                }
                //update model

                const Board& result(
                        coach->showboard()
                );
                if(kibitzed)
                    comment << GameRecord::eventNames[GameRecord::KIBITZ_MOVE] << kibitz->getName();

                std::for_each(
                    gameObservers.begin(), gameObservers.end(),
                    [result, move, &comment](GameObserver* observer) {
                        observer->onGameMove(move, comment.str());
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

            if(model.isGameOver) {
                break;
            }

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

void GameThread::playKibitzMove() {
    std::unique_lock<std::mutex> lock(playerMutex);
    Move kibitzed(Move::KIBITZED, model.state.colorToMove);
    if(playerToMove) playerToMove->suggestMove(kibitzed);
}

void GameThread::loadEngines(const std::shared_ptr<Configuration> config) {
    auto bots = config->data.find("bots");
    if(bots != config->data.end()) {
        bool hasCoach(false);
        bool hasKibitz(false);
        for(auto it = bots->begin(); it != bots->end(); ++it) {
            auto enabled = it->value("enabled", 1);
            if(enabled) {
                auto path = it->value("path", "" );
                auto name = it->value("name", "");
                auto command = it->value("command", "");
                auto parameters = it->value("parameters", "");
                auto main = it->value("main", 0);
                auto kibitz = it->value("kibitz", 0);
                auto messages = it->value("messages", nlohmann::json::array());

                int role = Player::SPECTATOR;

                if(!command.empty()) {
                    auto engine = new GtpEngine(command, parameters, path, name, messages);
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
                    if (kibitz) {
                        if(!hasKibitz) {
                            role |= Player::KIBITZ;
                            hasKibitz = true;
                            kibitz = id;
                            spdlog::info("Setting [{}] engine as trusted kibitz.",
                                         players[id]->getName());
                        } else {
                            spdlog::warn("Ignoring kibitz flag for [{}] engine, kibitz has already been set to [{}].",
                                         players[id]->getName(),
                                         players[coach]->getName());
                        }
                    }
                    setRole(id, role, true);
                }
            }
        }
        if(!hasKibitz) {
            kibitz = coach;
            spdlog::info("No kibitz set. Defaulting to [{}] coach engine.",
                         players[kibitz]->getName());
            players[coach]->setRole(players[coach]->getRole() | Player::KIBITZ);
        }
    }

    sgf = -1;

    auto humans = config->data.find("humans");
    if(humans != config->data.end()) {
        for(auto & it : *humans) {
            human = addPlayer(new LocalHumanPlayer(it));
        }
    } else {
        human = addPlayer(new LocalHumanPlayer("Human"));
    }

    numPlayers = human + 1;
    activePlayer[0] = human;
    activePlayer[1] = coach;
}

Move GameThread::getLocalMove(const Position& coord) const {
    return {coord, model.state.colorToMove};
}

Move GameThread::getLocalMove(const Move::Special move) const {
    return {move, model.state.colorToMove};
}

bool GameThread::loadSGF(const std::string& fileName, int gameIndex) {
    //std::unique_lock<std::mutex> lock(mutex2);
    
    // Auto-save current game if it has moves before replacing it
    if (model.game.moveCount() > 0) {
        try {
            model.game.saveAs(""); // Use default auto-generated filename
            spdlog::info("Auto-saved current game with {} moves before loading SGF", model.game.moveCount());
        } catch (const std::exception& ex) {
            spdlog::warn("Failed to auto-save current game before SGF loading: {}", ex.what());
        }
    }
    
    GameRecord::SGFGameInfo gameInfo;
    
    if (!model.game.loadFromSGF(fileName, gameInfo, gameIndex)) {
        return false;
    }

    interrupt();
    
    if (sgf == static_cast<size_t>(-1)) {
        auto sgfPlayer = new SGFPlayer("SGF Player");
        sgf = addPlayer(sgfPlayer);
    }
    
    auto sgfPlayer = dynamic_cast<SGFPlayer*>(players[sgf]);
    if (!sgfPlayer) {
        spdlog::error("Failed to get SGF player");
        return false;
    }
    
    if (!clearGame(gameInfo.boardSize, gameInfo.komi, gameInfo.handicap)) {
        spdlog::error("Failed to clear game for SGF loading");
        return false;
    }

    if (!gameInfo.handicapStones.empty()) {
        setHandicapStones(gameInfo.handicapStones);
    }

    Engine* coach = currentCoach();
    if (coach) {
        model.game.replay([&](const Move& move) {
            coach->play(move);
            for (auto player : players) {
                if (player != coach) {
                    player->play(move);
                }
            }
        });
        
        const Board& result = coach->showboard();
        std::for_each(
            gameObservers.begin(), gameObservers.end(),
            [result](GameObserver* observer) {
                observer->onBoardChange(result);
            }
        );
    }

    if (model.game.moveCount() > 0) {
        model.state.colorToMove = Color(model.game.lastMove().col == Color::BLACK
            ? Color::WHITE : Color::BLACK);
    } else if (gameInfo.handicap > 0) {
        model.state.colorToMove = Color(Color::WHITE);
    } else {
        model.state.colorToMove = Color(Color::BLACK);
    }

    model.pause();
    
    // Check if the loaded game is finished and trigger final scoring
    if (gameInfo.gameResult.IsValid && 
        (gameInfo.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::BlackWin ||
         gameInfo.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::WhiteWin ||
         gameInfo.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::Draw)) {
        
        spdlog::info("Loaded SGF contains finished game, triggering final scoring");
        
        // Check if we ended with consecutive passes (typical for finished games)
        bool endedWithPasses = false;
        if (model.game.moveCount() >= 2) {
            const Move& lastMove = model.game.lastMove();
            const Move& secondLastMove = model.game.secondLastMove();
            endedWithPasses = (lastMove == Move::PASS && secondLastMove == Move::PASS);
        }
        
        // Check if the game ended by resignation (from SGF result, not from moves)
        bool endedByResignation = (gameInfo.gameResult.WinType == LibSgfcPlusPlus::SgfcWinType::WinByResignation);
        
        // Trigger final scoring for finished games (both double pass and resignation)
        if (coach && (endedWithPasses || endedByResignation)) {
            // Set the game state reason but don't set 'over' yet to avoid breaking the game loop
            model.state.reason = endedByResignation ? GameState::RESIGNATION : GameState::DOUBLE_PASS;

            {}
            model.board.toggleTerritoryAuto(true);
            const Board& result = coach->showterritory(endedWithPasses, model.state.colorToMove);

            // Do not update observers with the final board state
            /*std::for_each(
                gameObservers.begin(), gameObservers.end(),
                [result](GameObserver* observer) {
                    observer->onBoardChange(result);
                }
            );*/
            
            // Compare coach's calculated score with SGF result
            if (endedWithPasses) {
                float coachScore = coach->final_score();
                float sgfScore = 0.0f;
                bool sgfScoreValid = false;
                
                // Extract score from SGF result if it's a win with score
                if (gameInfo.gameResult.WinType == LibSgfcPlusPlus::SgfcWinType::WinWithScore) {
                    sgfScore = gameInfo.gameResult.Score;
                    if (gameInfo.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::WhiteWin) {
                        sgfScore = -sgfScore; // Convert to coach's convention (negative = white wins)
                    }
                    sgfScoreValid = true;
                }
                
                if (sgfScoreValid) {
                    float scoreDifference = std::abs(coachScore - sgfScore);
                    if (scoreDifference > 0.1f) {
                        spdlog::warn("Score discrepancy detected: Coach calculated {:.1f}, SGF shows {:.1f} (difference: {:.1f}). "
                                   "This may be due to different scoring rules or SGF error.",
                                   coachScore, sgfScore, scoreDifference);
                    } else {
                        spdlog::info("Score verification: Coach {:.1f}, SGF {:.1f} - scores match", coachScore, sgfScore);
                    }
                }
                model.state.adata.delta = coachScore;
            }

            // Now safely set the game as over after all operations are complete
            model.isGameOver = true;
            model.board.copyStateFrom(result);
            model.board.positionNumber += 1;

            model.state.capturedBlack = model.board.capturedCount(Color::BLACK);
            model.state.capturedWhite = model.board.capturedCount(Color::WHITE);

            if (endedByResignation) {
                // For resigned games, create a resignation move based on SGF game result
                Color resigningPlayer = Color((gameInfo.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::BlackWin)
                                        ? Color::WHITE : Color::BLACK);
                Move resignationMove(Move::RESIGN, resigningPlayer);
                model.result(resignationMove, model.state.adata);
            } else {
                model.result(model.game.lastMove(), model.state.adata);
            }

            if (endedByResignation) {
                spdlog::info("Final scoring completed for resigned game (showing territory influence)");
            } else {
                spdlog::info("Final scoring completed for loaded SGF game");
            }
        }
    }
    
    spdlog::info("SGF file [{}] (game index {}) loaded successfully. Board size: {}, Komi: {}, Handicap: {}, Moves: {}",
                 fileName, gameIndex, gameInfo.boardSize, gameInfo.komi, gameInfo.handicap, model.game.moveCount());
    
    return true;
}

void GameThread::setHandicapStones(const std::vector<Position>& stones) {
    Engine* coach = currentCoach();
    if (coach && !stones.empty()) {
        coach->boardsize(model.getBoardSize());
        coach->clear();
        
        for (const auto& stone : stones) {
            coach->play(Move(stone, Color(Color::BLACK)));
        }
        
        for (auto player : players) {
            if (player != coach) {
                player->boardsize(model.getBoardSize());
                player->clear();
                for (const auto& stone : stones) {
                    player->play(Move(stone, Color(Color::BLACK)));
                }
            }
        }
        
        model.state.colorToMove = Color(Color::WHITE);
        
        const Board& result = coach->showboard();
        std::for_each(
            gameObservers.begin(), gameObservers.end(),
            [result](GameObserver* observer) {
                observer->onBoardChange(result);
            }
        );
        
        std::for_each(
            gameObservers.begin(), gameObservers.end(),
            [stones](GameObserver* observer) {
                observer->onHandicapChange(stones);
            }
        );
    }
}
