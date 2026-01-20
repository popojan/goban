#include "GameThread.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <cmath>

GameThread::GameThread(GobanModel &m) :
        model(m), thread(nullptr),
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
    if (thread) {
        interruptRequested = true;
        playLocalMove(Move(Move::INTERRUPT, model.state.colorToMove));
        thread->join();
        thread.reset();
    }
}

/** \brief Clears the board and resets both komi and handicap.
 *
 * @param boardSize
 */
void GameThread::removeSgfPlayers() {
    // Remove players with SGF_PLAYER type (created during SGF loading)
    // Iterate backwards to avoid index shifting issues
    for (int i = static_cast<int>(players.size()) - 1; i >= 0; --i) {
        if (players[i]->isTypeOf(Player::SGF_PLAYER)) {
            spdlog::info("Removing SGF player '{}' at index {}", players[i]->getName(), i);
            delete players[i];
            players.erase(players.begin() + i);
        }
    }

    // Reset active players to defaults (human for black, coach for white)
    // These indices should be valid after removing SGF players
    activePlayer[0] = human;  // Black
    activePlayer[1] = coach;  // White
    numPlayers = players.size();

    spdlog::debug("removeSgfPlayers: {} players remaining, activePlayer=[{}, {}]",
        numPlayers, activePlayer[0], activePlayer[1]);
}

bool GameThread::clearGame(int boardSize, float komi, int handicap) {

    /*this->boardSize = boardSize;
    this->komi = komi;
    this->handicap = handicap;*/

    // Reset to Match mode on new game
    gameMode = GameMode::MATCH;
    waitingForGenmove = false;
    lastMoveSource = MoveSource::NONE;

    bool success = true;

    for (auto player : players) {
        success &= player->boardsize(boardSize);
        success &= player->clear();
    }

    // Always notify observers of board size, even if engine failed
    // This ensures the board renders correctly regardless of engine state
    std::for_each(
        gameObservers.begin(), gameObservers.end(),
        [boardSize](GameObserver* observer){
            observer->onBoardSized(boardSize);
        }
    );

    if (!success) {
        spdlog::warn("Engine initialization failed, but board will still render");
    }

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
        if (!coach) {
            spdlog::error("setFixedHandicap: no coach engine!");
            return false;
        }
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
                        success &= player->play(Move(stone, Color::BLACK));
                }
                model.state.colorToMove = Color::WHITE;
            }
        } else {
            success = true;
        }
        if(success) {
            std::for_each(
                    gameObservers.begin(), gameObservers.end(),
                    [&stones](GameObserver* observer){observer->onHandicapChange(stones);}
            );
        }
        model.state.handicap = handicap;
        model.board.copyStateFrom(coach->showboard());
    }
    return success;
}
void GameThread::run() {
    std::unique_lock<std::mutex> lock(playerMutex);

    // If thread exists and finished, join it first before starting new one
    if (thread && thread->joinable() && !hasThreadRunning) {
        spdlog::debug("run: joining finished thread before starting new one");
        lock.unlock();
        thread->join();
        lock.lock();
    }

    // Don't start if already running
    if (hasThreadRunning) {
        spdlog::debug("run: thread already running, skipping");
        return;
    }

    model.start();
    spdlog::debug("construct thread {}", (bool)model);
    thread = std::make_unique<std::thread>(&GameThread::gameLoop, this);
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

void GameThread::syncOtherEngines(const Move& move, Player* player, Engine* coach,
                                   Engine* kibitzEngine, bool kibitzed, bool doubleUndo) {
    for (auto p : players) {
        if (p != reinterpret_cast<Player*>(coach) && p != player && (!kibitzed || p != kibitzEngine)) {
            spdlog::debug("syncOtherEngines: syncing player {}", p->getName());
            if (move == Move::UNDO) {
                undo(p, doubleUndo);
            } else {
                p->play(move);
            }
        }
    }
}

void GameThread::notifyMoveComplete(Engine* coach, const Move& move,
                                     Engine* kibitzEngine, bool kibitzed,
                                     const std::string& engineComments) {
    const Board& result = coach->showboard();

    std::ostringstream comment;
    comment << engineComments;
    if (kibitzed) {
        comment << GameRecord::eventNames[GameRecord::KIBITZ_MOVE] << kibitzEngine->getName();
    }

    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result, move, &comment](GameObserver* observer) {
            observer->onGameMove(move, comment.str());
            observer->onBoardChange(result);
        });
}

GameThread::UndoResult GameThread::processUndo(Engine* coach) {
    UndoResult result;

    bool bothHumans = players[activePlayer[0]]->isTypeOf(Player::HUMAN)
            && players[activePlayer[1]]->isTypeOf(Player::HUMAN);

    // Single undo if: both humans OR in Analysis mode OR reviewing (not at end of game)
    // hasNextMove() = true means we're stepped back in history, reviewing moves
    bool singleUndo = bothHumans || (gameMode == GameMode::ANALYSIS) || model.game.hasNextMove();

    spdlog::debug("processUndo: singleUndo={}, moveCount={}, isNavigating={}, bothHumans={}, gameMode={}",
        singleUndo, model.game.moveCount(), model.game.isNavigating(), bothHumans, static_cast<int>(gameMode));

    if (singleUndo && model.game.moveCount() > 0) {
        result.success = coach->undo();
        spdlog::debug("processUndo: coach->undo() returned {}", result.success);
        model.game.undo();
    } else if (currentPlayer()->isTypeOf(Player::HUMAN) && model.game.moveCount() > 1) {
        // Double undo in Match mode: undo AI's response + human's move
        result.success = coach->undo();
        result.success &= coach->undo();
        model.game.undo();
        model.game.undo();
        model.changeTurn();
        result.doubleUndo = true;
    }

    // In Analysis mode, after undo, human should play next (not AI)
    if (result.success && gameMode == GameMode::ANALYSIS) {
        lastMoveSource = MoveSource::NONE;
        spdlog::debug("processUndo: Analysis mode undo complete, human plays next");
    }

    return result;
}

void GameThread::gameLoop() {
    interruptRequested = false;
    while (model && !interruptRequested) {
        Engine* coach = currentCoach();
        Engine* kibitzEngine = currentKibitz();
        Player* player = currentPlayer();

        // Analysis mode: determine player based on last move source
        if (gameMode == GameMode::ANALYSIS && !aiVsAiMode) {
            // If waiting for genmove trigger (after kibitz move), wait here
            if (waitingForGenmove) {
                spdlog::debug("Analysis mode: waiting for genmove trigger (Space key)");
                // Use timeout-based wait to avoid deadlock with playLocalMove
                // This allows periodic checking without holding mutex continuously
                while (waitingForGenmove && !interruptRequested && gameMode == GameMode::ANALYSIS) {
                    std::unique_lock<std::mutex> waitLock(playerMutex);
                    genmoveTriggered.wait_for(waitLock, std::chrono::milliseconds(100));
                }
                if (interruptRequested || gameMode != GameMode::ANALYSIS) continue;
            }

            // Determine who plays this turn based on last move source
            if (lastMoveSource == MoveSource::HUMAN || lastMoveSource == MoveSource::KIBITZ) {
                // Human (click or kibitz) just played → kibitz engine responds
                player = reinterpret_cast<Player*>(kibitzEngine);
            } else {
                // NONE (fresh start) or AI just responded → human plays
                player = players[human];
            }
        }

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
            //cancel blocking wait for input if human player
            player->suggestMove(Move(Move::INVALID, model.state.colorToMove));
            //blocking wait for move - do NOT hold mutex during this wait
            //to keep UI responsive (especially important for slow engines like KataGo loading models)
            Move move = player->genmove(model.state.colorToMove);
            if(move == Move::KIBITZED) {
                if (kibitzEngine) {
                    move = kibitzEngine->genmove(model.state.colorToMove);
                    kibitzed = true;
                } else {
                    spdlog::warn("Kibitz requested but no kibitz engine available");
                    move = Move(Move::INVALID, model.state.colorToMove);
                }
            }
            spdlog::debug("MOVE to {}, valid = {}", move.toString(), (bool)move);
            playerToMove = nullptr;
            //lock mutex after genmove returns, before processing the move result
            lock.lock();
            locked = true;
            //TODO no direct access to model but via observers
            if (move == Move::INTERRUPT) {
                // Mode switch or player change - skip this iteration and re-evaluate
                spdlog::debug("INTERRUPT received, re-evaluating game state");
                continue;
            }
            else if (move == Move::UNDO) {
                auto undoResult = processUndo(coach);
                success = undoResult.success;
                doubleUndo = undoResult.doubleUndo;
            }
            else if (move) {
                // coach plays
                success = player == coach
                          || (kibitzed && kibitzEngine == coach)
                          || move == Move::RESIGN
                          || coach->play(move);
            }
            if(success) {
                // Collect engine comments for annotation
                std::ostringstream engineComments;
                for (auto p : players) {
                    if (p->isTypeOf(Player::ENGINE)) {
                        std::string engineMsg(dynamic_cast<GtpEngine*>(p)->lastError());
                        if (!engineMsg.empty()) {
                            engineComments << engineMsg << " (" << p->getName() << ") ";
                        }
                    }
                }

                // Sync other engines (not for resign)
                if (!(move == Move::RESIGN)) {
                    syncOtherEngines(move, player, coach, kibitzEngine, kibitzed, doubleUndo);
                }

                // Notify observers
                notifyMoveComplete(coach, move, kibitzEngine, kibitzed, engineComments.str());
            }

            //update territory if shown

            bool influence = model.board.showTerritory
                && (success || move == Move::INTERRUPT);

            if(influence) {
                bool finalized = model.state.reason == GameState::DOUBLE_PASS;
                const Board& result(
                    coach->showterritory(finalized, model.game.lastStoneMove().col)
                );
                std::for_each(
                    gameObservers.begin(), gameObservers.end(),
                    [&result](GameObserver* observer){observer->onBoardChange(result);}
                );
            }

            if(model.isGameOver) {
                break;
            }

            // Track move source for Analysis mode
            if (gameMode == GameMode::ANALYSIS && success && move != Move::INTERRUPT && move != Move::UNDO) {
                if (player->isTypeOf(Player::ENGINE)) {
                    lastMoveSource = MoveSource::AI;
                    spdlog::debug("Analysis mode: AI played, human's turn next");
                } else if (kibitzed) {
                    lastMoveSource = MoveSource::KIBITZ;
                    waitingForGenmove = true;
                    spdlog::debug("Analysis mode: Kibitz played, waiting for Space to trigger AI");
                } else {
                    lastMoveSource = MoveSource::HUMAN;
                    spdlog::debug("Analysis mode: Human played, AI will auto-respond");
                }
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
    spdlog::debug("playLocalMove: move={}, playerToMove={}", move.toString(), playerToMove ? "set" : "null");
    if(playerToMove) playerToMove->suggestMove(move);
}

void GameThread::playKibitzMove() {
    std::unique_lock<std::mutex> lock(playerMutex);
    Move kibitzed(Move::KIBITZED, model.state.colorToMove);
    if(playerToMove) playerToMove->suggestMove(kibitzed);
}

bool GameThread::navigateBack() {
    if (!model.game.isNavigating() || !model.game.hasPreviousMove()) {
        spdlog::debug("navigateBack: cannot navigate (isNavigating={}, hasPrev={})",
            model.game.isNavigating(), model.game.hasPreviousMove());
        return false;
    }

    Engine* coach = currentCoach();
    if (!coach) return false;

    // Capture the move we're about to undo (for pass message display)
    Move undoneMove = model.game.lastMove();

    // Undo one move (including passes - step through them individually)
    if (!coach->undo()) {
        spdlog::warn("navigateBack: coach->undo() failed");
        return false;
    }

    // Keep all engines in sync for continuation
    for (auto player : players) {
        if (player != reinterpret_cast<Player*>(coach)) {
            player->undo();
        }
    }

    model.game.undo();

    // Hide territory display when navigating (stale after undo)
    model.board.toggleTerritoryAuto(false);

    // Keep colorToMove in sync with SGF tree position
    model.state.colorToMove = model.game.getColorToMove();

    // Update comment from current SGF node
    model.state.comment = model.game.getComment();

    // Show pass message if we just undid a pass (shows the state we're viewing)
    if (undoneMove == Move::PASS) {
        model.state.msg = (undoneMove.col == Color::BLACK)
            ? GameState::BLACK_PASS
            : GameState::WHITE_PASS;
    } else {
        model.state.msg = GameState::NONE;
    }

    const Board& result = coach->showboard();
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) { observer->onBoardChange(result); });
    spdlog::debug("navigateBack: success, undid {}, now at move {}/{}, colorToMove={}",
        undoneMove.toString(),
        model.game.getViewPosition(), model.game.getLoadedMovesCount(),
        model.state.colorToMove == Color::BLACK ? "B" : "W");

    return true;
}

bool GameThread::navigateForward() {
    if (!model.game.isNavigating()) {
        spdlog::debug("navigateForward: not in navigation mode");
        return false;
    }

    // Get variations from SGF tree (children of current node)
    auto variations = model.game.getVariations();
    spdlog::debug("navigateForward: found {} variations", variations.size());

    if (variations.empty()) {
        // Already at end - show territory on this "extra" forward press
        spdlog::debug("navigateForward: at end, showing territory");
        Engine* coach = currentCoach();
        if (coach) {
            model.board.toggleTerritoryAuto(true);
            model.state.msg = GameState::NONE;  // Clear pass message
            const Board& result = coach->showterritory(true, model.game.lastStoneMove().col);
            std::for_each(gameObservers.begin(), gameObservers.end(),
                [&result](GameObserver* observer) {
                    observer->onBoardChange(result);
                });
            return true;  // We did something - trigger UI update
        }
        spdlog::debug("navigateForward: no variations available, no coach");
        return false;
    }

    // Play first variation (main line) - even if multiple variations exist
    // User can click on variation markers to choose alternatives
    Engine* coach = currentCoach();
    if (!coach) {
        spdlog::error("navigateForward: no coach engine!");
        return false;
    }

    Move nextMove = variations[0];
    bool isPass = (nextMove == Move::PASS);

    spdlog::debug("navigateForward: playing move {}", nextMove.toString());
    if (!coach->play(nextMove)) {
        spdlog::warn("navigateForward: coach->play() failed for move {}", nextMove.toString());
        return false;
    }

    // Keep all engines in sync for continuation
    for (auto player : players) {
        if (player != reinterpret_cast<Player*>(coach)) {
            player->play(nextMove);
        }
    }

    // Navigate to child in SGF tree (navigateToChild moves currentNode)
    // If child doesn't exist, create a new branch
    if (!model.game.navigateToChild(nextMove)) {
        // Creating new branch - this creates SGF node and moves currentNode
        model.game.move(nextMove);
    }

    // Set message for pass moves, clear for stone moves
    if (isPass) {
        model.state.msg = (nextMove.col == Color::BLACK)
            ? GameState::BLACK_PASS
            : GameState::WHITE_PASS;
        spdlog::debug("navigateForward: showing pass message for {}", nextMove.col == Color::BLACK ? "Black" : "White");
    } else {
        model.state.msg = GameState::NONE;
    }

    // Keep colorToMove in sync with SGF tree position
    model.state.colorToMove = model.game.getColorToMove();

    // Update comment from current SGF node
    model.state.comment = model.game.getComment();

    // Show board update (territory display deferred to next forward press at end)
    const Board& result = coach->showboard();
    spdlog::debug("navigateForward: notifying {} observers, colorToMove={}",
        gameObservers.size(), model.state.colorToMove == Color::BLACK ? "B" : "W");
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result, nextMove](GameObserver* observer) {
            observer->onBoardChange(result);
            observer->onStonePlaced(nextMove);
        });
    spdlog::debug("navigateForward: done");

    return true;
}

bool GameThread::navigateToVariation(const Move& move) {
    spdlog::debug("navigateToVariation: entry, move={}", move.toString());

    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToVariation: not in navigation mode");
        return false;
    }

    Engine* coach = currentCoach();
    if (!coach) {
        spdlog::error("navigateToVariation: no coach engine!");
        return false;
    }

    // Play the move on the coach
    if (!coach->play(move)) {
        spdlog::warn("navigateToVariation: coach->play() failed for move {}", move.toString());
        return false;
    }

    // Keep all engines in sync for continuation
    for (auto player : players) {
        if (player != reinterpret_cast<Player*>(coach)) {
            player->play(move);
        }
    }

    // Navigate to the child node in the SGF tree (navigateToChild moves currentNode)
    // If child doesn't exist, create a new branch
    bool newBranch = !model.game.navigateToChild(move);
    if (newBranch) {
        spdlog::debug("navigateToVariation: creating new branch");
        model.game.move(move);
        // Clear game over flag - we're continuing the game in a new branch
        model.isGameOver = false;
    } else {
        spdlog::debug("navigateToVariation: following existing branch");
    }

    // Keep colorToMove in sync with SGF tree position
    model.state.colorToMove = model.game.getColorToMove();

    // Update comment from current SGF node
    model.state.comment = model.game.getComment();

    // For new branches, switch to Analysis mode for flexible SGF editing
    if (newBranch) {
        if (gameMode == GameMode::MATCH) {
            spdlog::info("navigateToVariation: switching to Analysis mode for SGF editing");
            setGameMode(GameMode::ANALYSIS);
        }
        // Mark as human move so AI auto-responds, clear any waiting state
        lastMoveSource = MoveSource::HUMAN;
        waitingForGenmove = false;
        spdlog::debug("navigateToVariation: Analysis mode - AI should auto-respond, isRunning={}",
            isRunning());
    }

    // Notify observers first (before starting game loop to avoid race)
    const Board& result = coach->showboard();
    Move moveCopy = move;
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result, moveCopy](GameObserver* observer) {
            observer->onBoardChange(result);
            observer->onStonePlaced(moveCopy);
        });

    spdlog::debug("navigateToVariation: done, now at move {}/{}, colorToMove={}, needsGameLoop={}",
        model.game.getViewPosition(), model.game.getLoadedMovesCount(),
        model.state.colorToMove == Color::BLACK ? "B" : "W",
        newBranch && !isRunning());

    // Don't start game loop here - let caller do it to avoid race condition
    return true;
}

bool GameThread::navigateToStart() {
    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToStart: not in navigation mode");
        return false;
    }

    Engine* coach = currentCoach();
    if (!coach) return false;

    bool success = false;
    while (model.game.hasPreviousMove()) {
        if (!coach->undo()) break;

        // Keep all engines in sync
        for (auto player : players) {
            if (player != reinterpret_cast<Player*>(coach)) {
                player->undo();
            }
        }
        model.game.undo();
        success = true;
    }

    if (success) {
        model.board.toggleTerritoryAuto(false);
        model.state.colorToMove = model.game.getColorToMove();
        model.state.comment = model.game.getComment();
        model.state.msg = GameState::NONE;

        const Board& result = coach->showboard();
        std::for_each(gameObservers.begin(), gameObservers.end(),
            [&result](GameObserver* observer) { observer->onBoardChange(result); });
        spdlog::debug("navigateToStart: at beginning, colorToMove={}",
            model.state.colorToMove == Color::BLACK ? "B" : "W");
    }
    return success;
}

bool GameThread::navigateToEnd() {
    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToEnd: not in navigation mode");
        return false;
    }

    Engine* coach = currentCoach();
    if (!coach) return false;

    bool playedMoves = false;
    Move lastMove;

    // Play all moves on main line (first child at each branch)
    while (true) {
        auto variations = model.game.getVariations();
        if (variations.empty()) break;

        Move nextMove = variations[0];  // Always take main line (first variation)
        spdlog::debug("navigateToEnd: playing {}", nextMove.toString());

        if (!coach->play(nextMove)) {
            spdlog::warn("navigateToEnd: coach->play failed for {}", nextMove.toString());
            break;
        }

        // Keep all engines in sync
        for (auto player : players) {
            if (player != reinterpret_cast<Player*>(coach)) {
                player->play(nextMove);
            }
        }

        if (!model.game.navigateToChild(nextMove)) {
            spdlog::warn("navigateToEnd: navigateToChild failed, this shouldn't happen");
            break;  // Don't create new moves, something is wrong
        }
        lastMove = nextMove;
        playedMoves = true;
    }

    // Always show result at end (whether we played moves or were already there)
    model.state.colorToMove = model.game.getColorToMove();
    model.state.comment = model.game.getComment();
    model.state.msg = GameState::NONE;

    // Show territory at end
    model.board.toggleTerritoryAuto(true);
    const Board& result = coach->showterritory(true, model.game.lastStoneMove().col);
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) { observer->onBoardChange(result); });

    spdlog::debug("navigateToEnd: at end, move {}/{}, colorToMove={}, playedMoves={}",
        model.game.getViewPosition(), model.game.getLoadedMovesCount(),
        model.state.colorToMove == Color::BLACK ? "B" : "W", playedMoves);

    return true;  // Always return true - we're at the end now
}

bool GameThread::setGameMode(GameMode mode) {
    std::unique_lock<std::mutex> lock(playerMutex);

    // Don't allow Analysis mode for human-human matches (no AI to respond)
    if (mode == GameMode::ANALYSIS) {
        bool bothHumans = players[activePlayer[0]]->isTypeOf(Player::HUMAN)
                && players[activePlayer[1]]->isTypeOf(Player::HUMAN);
        if (bothHumans) {
            spdlog::info("Analysis mode not available for human-human matches");
            return false;
        }
    }

    if (gameMode != mode) {
        gameMode = mode;
        spdlog::info("Game mode changed to: {}", mode == GameMode::MATCH ? "Match" : "Analysis");
        // Reset waiting state when switching modes
        if (mode == GameMode::MATCH) {
            waitingForGenmove = false;
            aiVsAiMode = false;
        }
        // Reset lastMoveSource so human plays first after mode switch
        lastMoveSource = MoveSource::NONE;
        // Interrupt any blocking human player so game loop re-evaluates with new mode
        if (playerToMove != nullptr && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            playerToMove->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
        }
        // Also wake up if waiting for genmove trigger
        genmoveTriggered.notify_all();
        return true;
    }
    return false;  // No change (already in requested mode)
}

void GameThread::triggerGenmove() {
    std::unique_lock<std::mutex> lock(playerMutex);
    if (waitingForGenmove) {
        spdlog::debug("Triggering genmove - AI will respond");
        waitingForGenmove = false;
        genmoveTriggered.notify_all();
    }
}

void GameThread::setAiVsAi(bool enabled) {
    std::unique_lock<std::mutex> lock(playerMutex);
    aiVsAiMode = enabled;
    spdlog::info("AI vs AI mode: {}", enabled ? "enabled" : "disabled");
    if (enabled && waitingForGenmove) {
        // If we're waiting for genmove and AI vs AI is enabled, trigger it
        waitingForGenmove = false;
        genmoveTriggered.notify_all();
    }
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

    // Remove any SGF players from previous load
    removeSgfPlayers();

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
            [&result](GameObserver* observer) {
                observer->onBoardChange(result);
            }
        );
    }

    if (model.game.moveCount() > 0) {
        model.state.colorToMove = Color(model.game.lastMove().col == Color::BLACK
            ? Color::WHITE : Color::BLACK);
        
        // Set pass message if last move was a pass (needed for proper UI display)
        const Move& lastMove = model.game.lastMove();
        if (lastMove == Move::PASS) {
            model.state.msg = (lastMove.col == Color::BLACK) 
                ? GameState::BLACK_PASS 
                : GameState::WHITE_PASS;
        }
    } else if (gameInfo.handicap > 0) {
        model.state.colorToMove = Color::WHITE;
    } else {
        model.state.colorToMove = Color::BLACK;
    }

    // Set initial comment from current SGF node
    model.state.comment = model.game.getComment();
    spdlog::info("loadSGF: initial comment = '{}'", model.state.comment.substr(0, 50));

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

        if (!endedWithPasses && !endedByResignation && gameInfo.gameResult.IsValid) {
            endedWithPasses = true;
        }

        // Trigger final scoring for finished games (both double pass and resignation)
        if (coach && (endedWithPasses || endedByResignation)) {
            // Set the game state reason but don't set 'over' yet to avoid breaking the game loop
            model.state.reason = endedByResignation ? GameState::RESIGNATION : GameState::DOUBLE_PASS;

            model.board.toggleTerritoryAuto(true);
            const Board& result = coach->showterritory(endedWithPasses, model.game.lastStoneMove().col);

            // Do not update observers with the final board state
            /*std::for_each(
                gameObservers.begin(), gameObservers.end(),
                [&result](GameObserver* observer) {
                    observer->onBoardChange(result);
                }
            );*/
            
            // Compare coach's calculated score with SGF result
            if (endedWithPasses) {
                float coachScore = result.score;  // Already calculated by showterritory()
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
                model.state.scoreDelta = coachScore;
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
                model.result(resignationMove);
            } else {
                model.result(model.game.lastMove());
            }

            if (endedByResignation) {
                spdlog::info("Final scoring completed for resigned game (showing territory influence)");
            } else {
                spdlog::info("Final scoring completed for loaded SGF game");
            }
        }
    }
    
    // Match SGF player names to loaded players (engines)
    // If a name matches an engine exactly, activate that engine; otherwise create a temporary SGF player
    auto matchPlayerName = [this](const std::string& sgfName, int role) {
        int which = (role == Player::BLACK) ? 0 : 1;
        spdlog::info("matchPlayerName: sgfName='{}', role={}, which={}, current activePlayer[{}]={}",
            sgfName, role, which, which, activePlayer[which]);

        // Search for matching player name (skip SGF_PLAYER types from previous loads)
        for (size_t i = 0; i < players.size(); i++) {
            spdlog::debug("  checking player[{}]='{}' type={}", i, players[i]->getName(),
                players[i]->isTypeOf(Player::SGF_PLAYER) ? "SGF" : "normal");
            if (players[i]->getName() == sgfName && !players[i]->isTypeOf(Player::SGF_PLAYER)) {
                // Found matching engine - activate it
                activePlayer[which] = i;
                spdlog::info("Matched SGF player '{}' to existing player at index {}", sgfName, i);
                std::for_each(gameObservers.begin(), gameObservers.end(),
                    [role, &sgfName](GameObserver* observer) {
                        observer->onPlayerChange(role, sgfName);
                    });
                return;
            }
        }

        // No match - create a temporary SGF player
        auto* sgfPlayer = new LocalHumanPlayer(sgfName);
        sgfPlayer->addType(Player::SGF_PLAYER);  // Mark as SGF player for cleanup
        size_t newIndex = addPlayer(sgfPlayer);
        activePlayer[which] = newIndex;

        spdlog::info("Created SGF player '{}' at index {}, activePlayer[{}] now = {}",
            sgfName, newIndex, which, activePlayer[which]);
        std::for_each(gameObservers.begin(), gameObservers.end(),
            [role, &sgfName](GameObserver* observer) {
                observer->onPlayerChange(role, sgfName);
            });
    };

    spdlog::info("loadSGF: matching players - Black='{}', White='{}', players.size={}",
        gameInfo.blackPlayer, gameInfo.whitePlayer, players.size());
    matchPlayerName(gameInfo.blackPlayer, Player::BLACK);
    matchPlayerName(gameInfo.whitePlayer, Player::WHITE);
    numPlayers = players.size();  // Update numPlayers after adding SGF players
    spdlog::info("loadSGF: after matching - activePlayer=[{}, {}], players.size={}",
        activePlayer[0], activePlayer[1], players.size());

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
            coach->play(Move(stone, Color::BLACK));
        }
        
        for (auto player : players) {
            if (player != coach) {
                player->boardsize(model.getBoardSize());
                player->clear();
                for (const auto& stone : stones) {
                    player->play(Move(stone, Color::BLACK));
                }
            }
        }
        
        model.state.colorToMove = Color::WHITE;
        
        const Board& result = coach->showboard();
        std::for_each(
            gameObservers.begin(), gameObservers.end(),
            [&result](GameObserver* observer) {
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
