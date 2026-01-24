#include "GameThread.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <cmath>

GameThread::GameThread(GobanModel &m) :
        model(m), thread(nullptr), playerToMove(nullptr)
{
    // Initialize player manager
    playerManager = std::make_unique<PlayerManager>(gameObservers);

    // Set interrupt callback for player manager
    playerManager->setInterruptCallback([this]() {
        if (playerToMove != nullptr && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            playerToMove->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
        }
    });

    // Initialize navigator with callbacks to access GameThread resources
    navigator = std::make_unique<GameNavigator>(
        model,
        [this]() { return currentCoach(); },
        playerManager->getPlayers(),
        gameObservers
    );
}

void GameThread::reset() {
    interruptRequested = false;
    hasThreadRunning = false;
    playerToMove = nullptr;
}

GameThread::~GameThread() {
    std::unique_lock<std::mutex> lock(mutex2);
    interrupt();
    // PlayerManager destructor handles player cleanup
}

size_t GameThread::addEngine(Engine* engine) const {
    return playerManager->addEngine(engine);
}

size_t GameThread::addPlayer(Player* player) const {
    return playerManager->addPlayer(player);
}

Engine* GameThread::currentCoach() const {
    return playerManager->currentCoach();
}

Engine* GameThread::currentKibitz() const {
    return playerManager->currentKibitz();
}

Player* GameThread::currentPlayer() const {
    return playerManager->currentPlayer(model.state.colorToMove);
}

void GameThread::interrupt() {
    if (thread) {
        interruptRequested = true;
        navQueueCV.notify_one();
        playLocalMove(Move(Move::INTERRUPT, model.state.colorToMove));
        thread->join();
        thread.reset();
    }
}

void GameThread::removeSgfPlayers() const {
    playerManager->removeSgfPlayers();
}

bool GameThread::clearGame(int boardSize, float komi, int handicap) {

    /*this->boardSize = boardSize;
    this->komi = komi;
    this->handicap = handicap;*/

    // Reset to Match mode on new game
    gameMode = GameMode::MATCH;

    bool success = true;

    for (auto player : playerManager->getPlayers()) {
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
    for (auto player : playerManager->getPlayers()) {
        player->komi(komi);
    }
}

size_t GameThread::getActivePlayer(int which) const {
    return playerManager->getActivePlayer(which);
}

size_t GameThread::activatePlayer(int which, size_t newIndex) const {
    return playerManager->activatePlayer(which, newIndex);
}

bool GameThread::setFixedHandicap(int handicap) {
    if (model.started) {
        return false;
    }

    Engine* coach = currentCoach();
    if (!coach) {
        spdlog::error("setFixedHandicap: no coach engine!");
        return false;
    }

    std::vector<Position> stones;
    if (handicap >= 2) {
        int boardSize = model.board.getSize();
        coach->boardsize(boardSize);
        coach->clear();
        if (!coach->fixed_handicap(handicap, stones)) {
            return setFixedHandicap(0);  // Fall back to no handicap
        }
        applyHandicapStonesToEngines(stones, coach);
    }

    model.state.handicap = handicap;
    model.board.copyStateFrom(coach->showboard());

    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&stones](GameObserver* observer) { observer->onHandicapChange(stones); });

    return true;
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

    thread = std::make_unique<std::thread>(&GameThread::gameLoop, this);
    engineStarted.wait(lock);
}

bool GameThread::isRunning() const { return hasThreadRunning;}

bool GameThread::isThinking() const {
    std::unique_lock<std::mutex> lock(playerMutex);
    // Only block for engine thinking, not for human waiting for input
    return playerToMove != nullptr && playerToMove->isTypeOf(Player::ENGINE);
}

bool GameThread::humanToMove() const {
    std::unique_lock<std::mutex> lock(playerMutex);
    if(playerToMove) {
        return currentPlayer()->isTypeOf(Player::HUMAN);
    }
    return true;
}
void GameThread::syncOtherEngines(const Move& move, const Player* player, const Engine* coach,
                                   const Engine* kibitzEngine, bool kibitzed) const {
    for (auto p : playerManager->getPlayers()) {
        if (p != reinterpret_cast<const Player*>(coach) && p != player && (!kibitzed || p != kibitzEngine)) {
            spdlog::debug("syncOtherEngines: syncing player {}", p->getName());
            p->play(move);
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

Move GameThread::handleKibitzRequest(Move move, Engine* kibitzEngine,
                                      const Color& colorToMove, bool& wasKibitz) {
    if (move == Move::KIBITZED) {
        if (kibitzEngine) {
            move = kibitzEngine->genmove(colorToMove);
            wasKibitz = true;
        } else {
            spdlog::warn("Kibitz requested but no kibitz engine available");
            move = Move(Move::INVALID, colorToMove);
        }
    }
    return move;
}

std::string GameThread::collectEngineComments() const {
    std::ostringstream engineComments;
    for (auto p : playerManager->getPlayers()) {
        if (p->isTypeOf(Player::ENGINE)) {
            std::string engineMsg(dynamic_cast<GtpEngine*>(p)->lastError());
            if (!engineMsg.empty()) {
                engineComments << engineMsg << " (" << p->getName() << ") ";
            }
        }
    }
    return engineComments.str();
}

void GameThread::processSuccessfulMove(const Move& move, const Player* movePlayer,
                                        Engine* coach, Engine* kibitzEngine, bool wasKibitz) {
    // 1. Collect engine diagnostic comments
    std::string engineComments = collectEngineComments();

    // 2. Sync all other engines (including RESIGN - they need to know game ended)
    syncOtherEngines(move, movePlayer, coach, kibitzEngine, wasKibitz);

    // 3. Notify observers with move and comments
    notifyMoveComplete(coach, move, kibitzEngine, wasKibitz, engineComments);

    // 4. Update territory display if enabled
    if (model.board.showTerritory) {
        bool finalized = model.state.reason == GameState::DOUBLE_PASS;
        const Board& result = coach->showterritory(finalized, model.game.lastStoneMove().col);
        std::for_each(
            gameObservers.begin(), gameObservers.end(),
            [&result](GameObserver* observer) { observer->onBoardChange(result); }
        );
    }
}

void GameThread::gameLoop() {
    interruptRequested = false;
    while (!interruptRequested) {
        if(!hasThreadRunning) {
            hasThreadRunning = true;
            engineStarted.notify_all();
        }

        processNavigationQueue();
        if (interruptRequested) break;

        // When model is inactive or game is over, just wait for nav commands
        if (!model || model.isGameOver) {
            waitForCommandOrTimeout(100);
            continue;
        }

        Engine* coach = currentCoach();
        Engine* kibitzEngine = currentKibitz();
        Player* player = currentPlayer();

        std::unique_lock<std::mutex> lock(playerMutex, std::defer_lock);
        bool locked = false;

        // Skip genmove if:
        // 1. Navigation operation is in progress (atomic flag)
        // 2. Navigating through history (not at the end position)
        // This prevents AI from playing on an old board state during SGF review
        if (navigator->isNavigating()) {
            waitForCommandOrTimeout(50);
            continue;
        }
        bool navigatingHistory = model.game.isNavigating() && !model.game.isAtEndOfNavigation();
        if (navigatingHistory && gameMode == GameMode::MATCH
            && player && !player->isTypeOf(Player::HUMAN)) {
            // Match mode only: block engine genmove at historical positions.
            // In Analysis mode, human always controls — handled by the Analysis branch.
            spdlog::debug("Game loop: skipping engine genmove during navigation (view pos {}/{})",
                model.game.getViewPosition(), model.game.getLoadedMovesCount());
            waitForCommandOrTimeout(100);
            continue;
        }

        if (gameMode == GameMode::ANALYSIS && coach && !interruptRequested) {
            // Analysis mode: human plays either color, engine responds to human moves
            Player* humanPlayer = playerManager->getPlayers()[playerManager->getHumanIndex()];
            playerToMove = humanPlayer;
            player->suggestMove(queuedMove);
            Move move = humanPlayer->genmove(model.state.colorToMove);
            queuedMove = Move(Move::INVALID, model.state.colorToMove);

            bool wasKibitz = false;
            move = handleKibitzRequest(move, kibitzEngine, model.state.colorToMove, wasKibitz);

            lock.lock();
            locked = true;

            if (move == Move::INTERRUPT) {
                playerToMove = nullptr;
                continue;
            }

            bool success = false;
            if (move) {
                success = move == Move::RESIGN
                          || (wasKibitz && kibitzEngine == coach)
                          || coach->play(move);
            }

            if (success) {
                processSuccessfulMove(move, humanPlayer, coach, kibitzEngine, wasKibitz);

                // Human-originated move: engine (kibitz) auto-responds
                if (!wasKibitz && !model.isGameOver && kibitzEngine) {
                    Color responseColor = model.state.colorToMove;
                    spdlog::debug("Analysis: triggering kibitz response for {}", responseColor.toString());
                    Move response = kibitzEngine->genmove(responseColor);
                    if (response && response != Move::RESIGN) {
                        if (kibitzEngine == coach || coach->play(response)) {
                            processSuccessfulMove(response, kibitzEngine, coach, kibitzEngine, false);
                        }
                    }
                }
            }

            if (model.isGameOver) {
                playerToMove = nullptr;
                continue;
            }

            if (success)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

        } else if(coach && player && !interruptRequested) {
            // Match mode: strict player roles
            playerToMove = player;
            player->suggestMove(queuedMove);
            Move move = player->genmove(model.state.colorToMove);
            queuedMove = Move(Move::INVALID, model.state.colorToMove);

            bool kibitzed = false;
            move = handleKibitzRequest(move, kibitzEngine, model.state.colorToMove, kibitzed);

            spdlog::debug("MOVE to {}, valid = {}", move.toString(), static_cast<bool>(move));
            lock.lock();
            locked = true;

            if (move == Move::INTERRUPT) {
                spdlog::debug("INTERRUPT received, re-evaluating game state");
                playerToMove = nullptr;
                continue;
            }

            bool success = false;
            if (move) {
                success = player == coach
                          || (kibitzed && kibitzEngine == coach)
                          || move == Move::RESIGN
                          || coach->play(move);
            }

            if(success) {
                processSuccessfulMove(move, player, coach, kibitzEngine, kibitzed);
            }

            if(model.isGameOver) {
                playerToMove = nullptr;
                continue;
            }

            if(success && move != Move::INTERRUPT)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        // Clear playerToMove while still holding lock - signals processing complete
        playerToMove = nullptr;
        if(locked) lock.unlock();
        waitForCommandOrTimeout(50);
    }
    hasThreadRunning = false;
}

void GameThread::playLocalMove(const Move& move) {
    std::unique_lock<std::mutex> lock(playerMutex);
    spdlog::debug("playLocalMove: move={}, playerToMove={}", move.toString(), playerToMove ? "set" : "null");
    if (playerToMove) {
        playerToMove->suggestMove(move);
    } else if (model.started) {
        queuedMove = move;
    }
}

void GameThread::playKibitzMove() const {
    std::unique_lock<std::mutex> lock(playerMutex);
    Move kibitzed(Move::KIBITZED, model.state.colorToMove);
    if(playerToMove) playerToMove->suggestMove(kibitzed);
}

bool GameThread::navigateBack() {
    auto promise = std::make_shared<std::promise<NavResult>>();
    auto future = promise->get_future();
    {
        std::lock_guard<std::mutex> lock(navQueueMutex);
        navQueue.push({NavCommand::BACK, Move(), promise});
    }
    wakeGameThread();
    return future.get().success;
}

bool GameThread::navigateForward() {
    auto promise = std::make_shared<std::promise<NavResult>>();
    auto future = promise->get_future();
    {
        std::lock_guard<std::mutex> lock(navQueueMutex);
        navQueue.push({NavCommand::FORWARD, Move(), promise});
    }
    wakeGameThread();
    return future.get().success;
}

bool GameThread::navigateToVariation(const Move& move) {
    auto promise = std::make_shared<std::promise<NavResult>>();
    auto future = promise->get_future();
    {
        std::lock_guard<std::mutex> lock(navQueueMutex);
        navQueue.push({NavCommand::TO_VARIATION, move, promise});
    }
    wakeGameThread();
    NavResult result = future.get();
    return result.success;
}

bool GameThread::navigateToStart() {
    auto promise = std::make_shared<std::promise<NavResult>>();
    auto future = promise->get_future();
    {
        std::lock_guard<std::mutex> lock(navQueueMutex);
        navQueue.push({NavCommand::TO_START, Move(), promise});
    }
    wakeGameThread();
    return future.get().success;
}

bool GameThread::navigateToEnd() {
    auto promise = std::make_shared<std::promise<NavResult>>();
    auto future = promise->get_future();
    {
        std::lock_guard<std::mutex> lock(navQueueMutex);
        navQueue.push({NavCommand::TO_END, Move(), promise});
    }
    wakeGameThread();
    return future.get().success;
}

void GameThread::waitForCommandOrTimeout(int ms) {
    std::unique_lock<std::mutex> lock(navQueueMutex);
    navQueueCV.wait_for(lock, std::chrono::milliseconds(ms),
        [this]() { return !navQueue.empty() || interruptRequested.load(); });
}

void GameThread::wakeGameThread() {
    navQueueCV.notify_one();
    std::lock_guard<std::mutex> lock(playerMutex);
    if (playerToMove && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
        playerToMove->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
    }
}

void GameThread::processNavigationQueue() {
    while (true) {
        NavCommand cmd;
        {
            std::lock_guard<std::mutex> lock(navQueueMutex);
            if (navQueue.empty()) return;
            cmd = std::move(navQueue.front());
            navQueue.pop();
        }
        NavResult result = executeNavCommand(cmd);
        cmd.resultPromise->set_value(result);
    }
}

NavResult GameThread::executeNavCommand(const NavCommand& cmd) {
    NavResult result;
    switch (cmd.type) {
        case NavCommand::BACK:
            result.success = navigator->navigateBack();
            break;
        case NavCommand::FORWARD:
            result.success = navigator->navigateForward();
            // Territory consolidation: if at scored game end, show territory atomically
            if (result.success && model.game.isAtEndOfNavigation()
                && model.game.shouldShowTerritory()) {
                if (Engine* coach = currentCoach()) {
                    model.board.toggleTerritoryAuto(true);
                    const Board& territory = coach->showterritory(true, model.game.lastStoneMove().col);
                    navigator->notifyBoardChange(territory);
                }
            }
            break;
        case NavCommand::TO_START:
            result.success = navigator->navigateToStart();
            break;
        case NavCommand::TO_END:
            result.success = navigator->navigateToEnd();
            break;
        case NavCommand::TO_VARIATION: {
            auto varResult = navigator->navigateToVariation(cmd.move);
            result.success = varResult.success;
            result.newBranch = varResult.newBranch;
            // In Analysis mode, auto-respond with kibitz engine after human variation
            if (result.success && gameMode == GameMode::ANALYSIS) {
                Engine* kibitz = currentKibitz();
                Engine* coach = currentCoach();
                if (kibitz && coach && !model.isGameOver) {
                    Color responseColor = model.state.colorToMove;
                    spdlog::debug("Analysis nav: triggering kibitz response for {}", responseColor.toString());
                    Move response = kibitz->genmove(responseColor);
                    if (response && response != Move::RESIGN) {
                        if (kibitz == coach || coach->play(response)) {
                            processSuccessfulMove(response, kibitz, coach, kibitz, false);
                        }
                    }
                }
            }
            break;
        }
    }
    return result;
}

bool GameThread::setGameMode(GameMode mode) {
    std::unique_lock<std::mutex> lock(playerMutex);

    // Don't allow Analysis mode for human-human matches (no AI to respond)
    if (mode == GameMode::ANALYSIS) {
        if (playerManager->areBothPlayersHuman()) {
            spdlog::info("Analysis mode not available for human-human matches");
            return false;
        }
    }

    if (gameMode != mode) {
        gameMode = mode;
        spdlog::info("Game mode changed to: {}", mode == GameMode::MATCH ? "Match" : "Analysis");
        if (mode == GameMode::MATCH) {
            aiVsAiMode = false;
        }
        // Interrupt any blocking human player so game loop re-evaluates with new mode
        if (playerToMove != nullptr && playerToMove->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            playerToMove->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
        }
        return true;
    }
    return false;  // No change (already in requested mode)
}

void GameThread::setAiVsAi(bool enabled) {
    std::unique_lock<std::mutex> lock(playerMutex);
    aiVsAiMode = enabled;
    spdlog::info("AI vs AI mode: {}", enabled ? "enabled" : "disabled");
}

void GameThread::loadEngines(const std::shared_ptr<Configuration> conf) const {
    playerManager->loadEngines(conf);
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
    model.pause();  // Must pause before clearGame so onKomiChange/onHandicapChange take effect

    if (!clearGame(gameInfo.boardSize, gameInfo.komi, gameInfo.handicap)) {
        spdlog::error("Failed to clear game for SGF loading");
        return false;
    }

    // Ensure SGF values are set in model state (setFixedHandicap may have failed if no coach)
    spdlog::info("loadSGF: setting model.state from SGF - komi={}, handicap={}", gameInfo.komi, gameInfo.handicap);
    model.state.komi = gameInfo.komi;
    model.state.handicap = gameInfo.handicap;

    if (!gameInfo.handicapStones.empty()) {
        setHandicapStones(gameInfo.handicapStones);
    }

    Engine* coach = currentCoach();
    if (coach) {
        model.game.replay([&](const Move& move) {
            coach->play(move);
            for (auto player : playerManager->getPlayers()) {
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

    // Set initial comment and markup from current SGF node
    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();

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
                // For resigned games, set winner based on SGF result and create resignation move
                // BlackWin means white resigned, WhiteWin means black resigned
                bool blackWon = (gameInfo.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::BlackWin);
                model.state.winner = blackWon ? Color::BLACK : Color::WHITE;
                Color resigningPlayer = blackWon ? Color::WHITE : Color::BLACK;
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
    auto matchPlayerName = [this](const std::string& sgfName, int which) {
        auto players = playerManager->getPlayers();
        spdlog::info("matchPlayerName: sgfName='{}', which={}, current activePlayer[{}]={}",
            sgfName, which, which, playerManager->getActivePlayer(which));

        // Search for matching player name (skip SGF_PLAYER types from previous loads)
        for (size_t i = 0; i < players.size(); i++) {
            if (players[i]->getName() == sgfName && !players[i]->isTypeOf(Player::SGF_PLAYER)) {
                spdlog::info("Matched SGF player '{}' to existing player at index {}", sgfName, i);
                playerManager->setActivePlayer(which, i);
                return;
            }
        }

        // No match found - create a temporary SGF player (acts as human)
        auto* sgfPlayer = new LocalHumanPlayer(sgfName);
        sgfPlayer->addType(Player::SGF_PLAYER);
        size_t newIndex = addPlayer(sgfPlayer);
        playerManager->setActivePlayer(which, newIndex);
        spdlog::info("Created SGF player '{}' at index {}", sgfName, newIndex);
    };

    spdlog::info("loadSGF: matching players - Black='{}', White='{}', players.size={}",
        gameInfo.blackPlayer, gameInfo.whitePlayer, playerManager->getNumPlayers());
    matchPlayerName(gameInfo.blackPlayer, 0);
    matchPlayerName(gameInfo.whitePlayer, 1);
    spdlog::info("loadSGF: after matching - activePlayer=[{}, {}], players.size={}",
        playerManager->getActivePlayer(0), playerManager->getActivePlayer(1), playerManager->getNumPlayers());

    spdlog::info("SGF file [{}] (game index {}) loaded successfully. Board size: {}, Komi: {}, Handicap: {}, Moves: {}",
                 fileName, gameIndex, gameInfo.boardSize, gameInfo.komi, gameInfo.handicap, model.game.moveCount());

    // Finished games enter Analysis mode (review), unfinished enter Match mode (continue)
    gameMode = model.game.hasGameResult() ? GameMode::ANALYSIS : GameMode::MATCH;

    // Start game thread for navigation command processing
    run();

    return true;
}

void GameThread::applyHandicapStonesToEngines(const std::vector<Position>& stones, const Engine* coach) const {
    int boardSize = model.getBoardSize();
    // Sync handicap stones to active players (if they're engines and not the coach)
    for (int which = 0; which < 2; which++) {
        Player* player = playerManager->getPlayers()[playerManager->getActivePlayer(which)];
        if (player != coach && player->isTypeOf(Player::ENGINE)) {
            player->boardsize(boardSize);
            player->clear();
            for (const auto& stone : stones) {
                player->play(Move(stone, Color::BLACK));
            }
        }
    }
    model.state.colorToMove = Color::WHITE;
}

void GameThread::setHandicapStones(const std::vector<Position>& stones) {
    if (stones.empty()) {
        return;
    }

    Engine* coach = currentCoach();
    if (!coach) {
        return;
    }

    // Play stones to coach
    int boardSize = model.getBoardSize();
    coach->boardsize(boardSize);
    coach->clear();
    for (const auto& stone : stones) {
        coach->play(Move(stone, Color::BLACK));
    }

    // Sync to other engines
    applyHandicapStonesToEngines(stones, coach);

    // Notify observers
    const Board& result = coach->showboard();
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) { observer->onBoardChange(result); });

    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&stones](GameObserver* observer) { observer->onHandicapChange(stones); });
}
