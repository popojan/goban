#include "GameThread.h"
#include "UserSettings.h"
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
    // Place handicap stones locally instead of using showboard()
    for (const auto& pos : stones) {
        model.board.updateStone(pos, Color::BLACK);
    }
    model.board.positionNumber += 1;

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
        return playerToMove->isTypeOf(Player::HUMAN);
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
    std::ostringstream comment;
    comment << engineComments;
    if (kibitzed) {
        comment << GameRecord::eventNames[GameRecord::KIBITZ_MOVE] << kibitzEngine->getName();
    }

    // First notify observers of the move (this adds the move to game record)
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [move, &comment](GameObserver* observer) {
            observer->onGameMove(move, comment.str());
        });

    // Now build board from SGF using local capture logic (engine-independent)
    // This must happen AFTER onGameMove adds the move to the game record
    Board result(model.game.getBoardSize());
    Position koPosition;
    model.game.buildBoardFromMoves(result, koPosition);

    // Notify observers of the board state
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) {
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

    // 4. Update territory display if enabled (double pass detected)
    if (model.board.showTerritory) {
        // Build board locally and apply territory from coach (engine-independent)
        Board result(model.game.getBoardSize());
        Position koPosition;
        model.game.buildBoardFromMoves(result, koPosition);
        coach->applyTerritory(result);
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

            // Atomically read and clear queuedMove under lock
            Move suggestedMove;
            {
                std::unique_lock<std::mutex> qLock(playerMutex);
                suggestedMove = queuedMove;
                queuedMove = Move(Move::INVALID, model.state.colorToMove);
                playerToMove = humanPlayer;
            }

            player->suggestMove(suggestedMove);
            Move move = humanPlayer->genmove(model.state.colorToMove);

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

            // Atomically read and clear queuedMove under lock
            Move suggestedMove;
            {
                std::unique_lock<std::mutex> qLock(playerMutex);
                suggestedMove = queuedMove;
                queuedMove = Move(Move::INVALID, model.state.colorToMove);
                playerToMove = player;
            }

            player->suggestMove(suggestedMove);
            Move move = player->genmove(model.state.colorToMove);

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
                    // Build board locally and apply territory (engine-independent)
                    Board territory(model.game.getBoardSize());
                    Position koPosition;
                    model.game.buildBoardFromMoves(territory, koPosition);
                    coach->applyTerritory(territory);
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

void GameThread::loadEnginesParallel(std::shared_ptr<Configuration> conf,
                                      const std::string& sgfPath,
                                      std::function<void()> onFirstEngineReady) {
    auto bots = conf->data.find("bots");
    if (bots == conf->data.end()) {
        spdlog::warn("No bots configured");
        playerManager->loadHumanPlayers(conf);
        return;
    }

    // Collect enabled bot configs
    std::vector<nlohmann::json> botConfigs;
    for (auto it = bots->begin(); it != bots->end(); ++it) {
        if (it->value("enabled", 1) && !it->value("command", "").empty()) {
            botConfigs.push_back(*it);
        }
    }

    if (botConfigs.empty()) {
        spdlog::warn("No enabled bots configured");
        playerManager->loadHumanPlayers(conf);
        return;
    }

    // Synchronization state
    std::mutex mtx;
    std::condition_variable cv;
    Engine* firstReadyEngine = nullptr;
    std::atomic<int> enginesLoaded{0};
    int totalEngines = static_cast<int>(botConfigs.size());
    std::vector<Engine*> loadedEngines(totalEngines, nullptr);

    spdlog::info("Loading {} engines in parallel", totalEngines);

    // Spawn a thread for each engine
    std::vector<std::thread> threads;
    for (int i = 0; i < totalEngines; ++i) {
        threads.emplace_back([this, &botConfigs, i, &mtx, &cv, &firstReadyEngine, &enginesLoaded, &loadedEngines]() {
            Engine* engine = playerManager->loadSingleEngine(botConfigs[i]);

            std::lock_guard<std::mutex> lock(mtx);
            loadedEngines[i] = engine;
            if (engine && !firstReadyEngine) {
                firstReadyEngine = engine;
                spdlog::info("First engine ready: {}", engine->getName());
            }
            enginesLoaded++;
            cv.notify_all();
        });
    }

    // Wait for first engine to be ready
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&firstReadyEngine]() { return firstReadyEngine != nullptr; });
    }

    // Load SGF with first ready engine (stones appear now!)
    if (!sgfPath.empty() && firstReadyEngine) {
        spdlog::info("Loading SGF with first engine: {}", sgfPath);
        loadSGFWithEngine(sgfPath, firstReadyEngine, -1);
    } else if (firstReadyEngine) {
        // No SGF - apply saved game settings
        auto& settings = UserSettings::instance();
        int boardSize = settings.getBoardSize();
        float komi = settings.getKomi();

        firstReadyEngine->boardsize(boardSize);
        firstReadyEngine->komi(komi);
        firstReadyEngine->clear();

        // Update model state with saved settings
        spdlog::debug("Setting model.state.komi = {} from settings", komi);
        model.state.komi = komi;
        model.state.handicap = settings.getHandicap();

        std::for_each(gameObservers.begin(), gameObservers.end(),
            [boardSize](GameObserver* observer) { observer->onBoardSized(boardSize); });
    }

    // Notify that first engine is ready (for UI update)
    if (onFirstEngineReady) {
        onFirstEngineReady();
    }

    // Wait for all engines
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&enginesLoaded, totalEngines]() { return enginesLoaded >= totalEngines; });
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    // Load human players
    playerManager->loadHumanPlayers(conf);

    // Sync remaining engines to game state
    spdlog::info("Syncing {} engines to game state", totalEngines);
    bool sgfWasLoaded = !sgfPath.empty();
    syncRemainingEngines(firstReadyEngine, sgfWasLoaded);

    spdlog::info("All engines loaded and synced");
}

Move GameThread::getLocalMove(const Position& coord) const {
    return {coord, model.state.colorToMove};
}

Move GameThread::getLocalMove(const Move::Special move) const {
    return {move, model.state.colorToMove};
}

bool GameThread::loadSGF(const std::string& fileName, int gameIndex) {
    // Use unified loading path - same as startup
    Engine* coach = currentCoach();

    if (!loadSGFWithEngine(fileName, coach, gameIndex)) {
        return false;
    }

    // Sync remaining engines and finalize (player matching, game mode, run thread)
    // Pass coach as already-synced engine
    syncRemainingEngines(coach);

    return true;
}

void GameThread::syncEngineToPosition(Engine* engine) {
    // Core method: sync one engine to current game position
    if (!engine) return;

    int boardSize = model.getBoardSize();
    float komi = model.state.komi;
    spdlog::debug("syncEngineToPosition: {} boardSize={} komi={}", engine->getName(), boardSize, komi);

    engine->boardsize(boardSize);
    engine->clear();
    engine->komi(komi);

    // Replay handicap stones
    for (const auto& stone : model.handicapStones) {
        engine->play(Move(stone, Color::BLACK));
    }

    // Replay all moves
    model.game.replay([&](const Move& move) {
        engine->play(move);
    });
}

void GameThread::finalizeLoadedGame(Engine* engine, const GameRecord::SGFGameInfo& gameInfo) {
    // Check if game ended (double pass or resignation)
    bool endedWithPasses = false;
    if (model.game.moveCount() >= 2) {
        const Move& lastMove = model.game.lastMove();
        const Move& secondLastMove = model.game.secondLastMove();
        endedWithPasses = (lastMove == Move::PASS && secondLastMove == Move::PASS);
    }

    bool endedByResignation = (gameInfo.gameResult.WinType == LibSgfcPlusPlus::SgfcWinType::WinByResignation);

    if (!endedWithPasses && !endedByResignation && gameInfo.gameResult.IsValid) {
        endedWithPasses = true;
    }

    if (!endedWithPasses && !endedByResignation) {
        return;  // Game not finished
    }

    // Set the game state for finished game
    model.state.reason = endedByResignation ? GameState::RESIGNATION : GameState::DOUBLE_PASS;

    if (engine) {
        model.board.toggleTerritoryAuto(true);
        // Build board locally and apply territory (engine-independent)
        Board result(model.game.getBoardSize());
        Position koPosition;
        model.game.buildBoardFromMoves(result, koPosition);

        if (endedWithPasses) {
            engine->applyTerritory(result);

            // Score verification for scored games
            float coachScore = result.score;
            if (gameInfo.gameResult.WinType == LibSgfcPlusPlus::SgfcWinType::WinWithScore) {
                float sgfScore = gameInfo.gameResult.Score;
                if (gameInfo.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::WhiteWin) {
                    sgfScore = -sgfScore;
                }
                float scoreDifference = std::abs(coachScore - sgfScore);
                if (scoreDifference > 0.1f) {
                    spdlog::warn("Score discrepancy: Coach {:.1f}, SGF {:.1f}", coachScore, sgfScore);
                }
            }
            model.state.scoreDelta = coachScore;
        }

        // Set isGameOver BEFORE onBoardChange so territory display logic works
        model.isGameOver = true;

        // Notify all observers of board change (triggers territory display via onBoardChange)
        std::for_each(gameObservers.begin(), gameObservers.end(),
            [&result](GameObserver* observer) {
                observer->onBoardChange(result);
            });
    } else {
        model.isGameOver = true;
    }

    if (endedByResignation) {
        bool blackWon = (gameInfo.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::BlackWin);
        model.state.winner = blackWon ? Color::BLACK : Color::WHITE;
        Color resigningPlayer = blackWon ? Color::WHITE : Color::BLACK;
        Move resignationMove(Move::RESIGN, resigningPlayer);
        model.result(resignationMove);
        spdlog::info("Loaded SGF: game ended by resignation");
    } else {
        model.result(model.game.lastMove());
        spdlog::info("Loaded SGF: game ended with scoring");
    }
}

void GameThread::matchSgfPlayers() {
    auto [blackPlayer, whitePlayer] = model.game.getPlayerNames();

    auto matchPlayerName = [this](const std::string& sgfName, int which) {
        auto players = playerManager->getPlayers();
        spdlog::debug("matchPlayerName: sgfName='{}', which={}", sgfName, which);

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

    spdlog::info("Matching SGF players - Black='{}', White='{}'", blackPlayer, whitePlayer);
    matchPlayerName(blackPlayer, 0);
    matchPlayerName(whitePlayer, 1);
}

bool GameThread::loadSGFWithEngine(const std::string& fileName, Engine* engine, int gameIndex) {
    // Load SGF and sync specified engine (or coach if none specified)
    // Call syncRemainingEngines() later when other engines are ready

    removeSgfPlayers();

    // Auto-save current game if it has moves
    if (model.game.moveCount() > 0) {
        try {
            model.game.saveAs("");
            spdlog::info("Auto-saved current game with {} moves before loading SGF", model.game.moveCount());
        } catch (const std::exception& ex) {
            spdlog::warn("Failed to auto-save current game: {}", ex.what());
        }
    }

    GameRecord::SGFGameInfo gameInfo;
    if (!model.game.loadFromSGF(fileName, gameInfo, gameIndex)) {
        return false;
    }

    interrupt();
    model.pause();

    // Set up model state from SGF
    model.state.komi = gameInfo.komi;
    model.state.handicap = gameInfo.handicap;
    gameMode = GameMode::MATCH;

    // Handle handicap stones in model
    if (!gameInfo.handicapStones.empty()) {
        model.board.clear(gameInfo.boardSize);
        for (const auto& stone : gameInfo.handicapStones) {
            model.board.updateStone(stone, Color::BLACK);
        }
        model.game.setHandicapStones(gameInfo.handicapStones);
        model.handicapStones = gameInfo.handicapStones;
    }

    // Notify observers of board size
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&gameInfo](GameObserver* observer) {
            observer->onBoardSized(gameInfo.boardSize);
        });

    // Use provided engine or fall back to coach
    if (!engine) {
        engine = currentCoach();
    }
    if (engine) {
        syncEngineToPosition(engine);
    }

    // Build board from SGF using local capture logic (engine-independent)
    Board result(model.game.getBoardSize());
    Position koPosition;
    model.game.buildBoardFromMoves(result, koPosition);
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) {
            observer->onBoardChange(result);
        });

    // Set game state
    if (model.game.moveCount() > 0) {
        model.state.colorToMove = Color(model.game.lastMove().col == Color::BLACK
            ? Color::WHITE : Color::BLACK);
        const Move& lastMove = model.game.lastMove();
        if (lastMove == Move::PASS) {
            model.state.msg = (lastMove.col == Color::BLACK)
                ? GameState::BLACK_PASS : GameState::WHITE_PASS;
        }
    } else if (gameInfo.handicap > 0) {
        model.state.colorToMove = Color::WHITE;
    } else {
        model.state.colorToMove = Color::BLACK;
    }

    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();

    // Handle finished games (resignation or double pass)
    finalizeLoadedGame(engine, gameInfo);

    spdlog::info("SGF [{}] loaded. Board: {}, Moves: {}.",
                 fileName, gameInfo.boardSize, model.game.moveCount());

    return true;
}

void GameThread::syncRemainingEngines(Engine* alreadySynced, bool matchPlayers) {
    // Sync all engines except the one already synced

    // If no engine specified, fall back to coach
    Engine* skipEngine = alreadySynced ? alreadySynced : currentCoach();

    spdlog::info("Syncing remaining engines (board: {}, moves: {})",
                 model.getBoardSize(), model.game.moveCount());

    for (auto player : playerManager->getPlayers()) {
        if (player == skipEngine) continue;  // Already synced
        if (!player->isTypeOf(Player::ENGINE)) continue;  // Skip non-engines

        syncEngineToPosition(static_cast<Engine*>(player));
    }

    // Match SGF player names to engines (only when SGF was loaded)
    if (matchPlayers) {
        matchSgfPlayers();
    }

    // Set game mode based on result
    gameMode = model.game.hasGameResult() ? GameMode::ANALYSIS : GameMode::MATCH;

    // Start game thread for navigation
    run();

    spdlog::info("All engines synced");
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

    // Notify observers - build board locally from handicap stones
    Board result(model.getBoardSize());
    for (const auto& pos : stones) {
        result.updateStone(pos, Color::BLACK);
    }
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) { observer->onBoardChange(result); });

    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&stones](GameObserver* observer) { observer->onHandicapChange(stones); });
}
