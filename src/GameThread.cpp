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
        Player* p = playerToMove.load();
        if (p != nullptr && p->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            p->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
        }
    });

    // Initialize navigator with callbacks to access GameThread resources
    navigator = std::make_unique<GameNavigator>(
        model,
        [this]() { return currentCoach(); },
        [this]() -> std::vector<Player*> {
            // Return ALL engines (coach is handled separately in syncEngines)
            return playerManager->getPlayers();
        },
        gameObservers,
        [this](Engine* eng) -> bool { return syncEngineToPosition(eng); }
    );
}

GameThread::~GameThread() {
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

// Set for the lifetime of gameLoop(). There is exactly one game loop, so a
// thread_local flag is enough and avoids atomics on a hot path.
static thread_local bool t_isGameThread = false;

bool GameThread::isOnGameThread() {
    return t_isGameThread;
}

std::string GameThread::thinkingPlayerName() const {
    Player* p = playerToMove.load();
    if (p != nullptr && p->isTypeOf(Player::ENGINE)) {
        return p->getName();
    }
    return {};
}

bool GameThread::runWhenEngineFree(std::function<void()> task, std::string* busyEngine) {
    if (busyEngine) busyEngine->clear();

    // Nothing to wait for: no engine is mid-genmove, so the caller's thread may
    // safely stop the loop and do the work itself, as it always has.
    const std::string busy = thinkingPlayerName();
    if (busy.empty() || isOnGameThread()) {
        task();
        return true;
    }

    if (busyEngine) *busyEngine = busy;
    {
        std::lock_guard<std::mutex> lock(deferredMutex);
        // Coalesce: these actions all discard the current game, so only the
        // most recent request matters.
        if (deferredTask) {
            spdlog::info("Replacing a pending deferred action with a newer one");
        }
        deferredTask = std::move(task);
    }
    deferredPending = true;
    // Makes the game loop drop the move it is about to receive — it belongs to
    // a position the deferred action is about to replace.
    navQueueCV.notify_one();
    spdlog::info("Deferred action until {} finishes thinking", busy);
    return false;
}

void GameThread::processDeferredTask() {
    if (!deferredPending.load()) return;

    std::function<void()> task;
    {
        std::lock_guard<std::mutex> lock(deferredMutex);
        task.swap(deferredTask);
    }
    if (!task) {
        deferredPending = false;
        return;
    }

    // Drop queued navigation: it refers to the game about to be replaced, and
    // replaying it against the new one lands at an arbitrary position. The UI
    // path used to get this for free from interrupt(), which clears the queue —
    // but interrupt() is a no-op on this thread, so it must be done explicitly.
    {
        std::lock_guard<std::mutex> lock(navQueueMutex);
        if (!navQueue.empty()) {
            spdlog::info("Discarding {} queued navigation command(s): the game is "
                         "being replaced", navQueue.size());
            std::queue<NavCommand> empty;
            navQueue.swap(empty);
        }
    }

    spdlog::info("Running deferred action on the game thread");
    task();
    // Cleared only once the work is done, so that hasDeferredTask() — and
    // therefore any caller waiting for quiescence — stays true for the whole
    // duration, not just while the task sits in the queue.
    deferredPending = false;
    deferredDone = true;
}

bool GameThread::takeDeferredTaskDone() {
    return deferredDone.exchange(false);
}

bool GameThread::interrupt(int timeoutMs) {
    // Called from the game loop itself (a deferred action): the loop is between
    // iterations, so no move is in flight and nothing needs stopping. Joining
    // here would deadlock on self-join, and setting interruptRequested would
    // terminate the loop the caller still needs.
    if (isOnGameThread()) {
        spdlog::debug("interrupt: already on the game thread, nothing to stop");
        return true;
    }

    spdlog::debug("interrupt: thread={}", thread ? "exists" : "null");
    spdlog::default_logger()->flush();
    if (!thread) {
        return true;
    }

    // Only a *running* loop can be asked to stop. Writing Stopping
    // unconditionally would be the mirror of the hazard gameLoop() guards
    // against: on a loop that has already exited it would claim the thread is
    // still alive, and the timeout poll below would then spin out its full
    // deadline and report failure for a thread that was ready to join.
    // Reachable after a timed-out interrupt whose loop later exited on its own.
    {
        LoopState expected = LoopState::Running;
        loop.compare_exchange_strong(expected, LoopState::Stopping);
    }
    {
        std::lock_guard<std::mutex> lock(navQueueMutex);
        std::queue<NavCommand> empty;
        navQueue.swap(empty);
    }
    navQueueCV.notify_one();
    // Unblocks a human player waiting on its condition variable. An engine
    // blocked in a GTP read is NOT unblocked by this — hence the timeout.
    playLocalMove(Move(Move::INTERRUPT, model.state.colorToMove));

    if (timeoutMs >= 0) {
        // Poll rather than wait on a condition variable: the game loop holds
        // playerMutex around parts of its turn handling, and blocking on that
        // here would risk a deadlock for the sake of a 10 ms poll.
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(timeoutMs);
        while (loop.load() != LoopState::Stopped
               && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (loop.load() != LoopState::Stopped) {
            spdlog::warn("interrupt: game loop still running after {} ms "
                         "(engine is probably mid-genmove); not joining",
                         timeoutMs);
            // The state stays Stopping, so the loop exits when genmove
            // returns; run() joins the finished thread before starting again.
            return false;
        }
    }

    spdlog::debug("interrupt: joining thread...");
    spdlog::default_logger()->flush();
    thread->join();
    spdlog::debug("interrupt: thread joined");
    thread.reset();
    // Joined, so the loop is definitively down. The old code left
    // interruptRequested set here until something else happened to clear it.
    loop = LoopState::Stopped;
    return true;
}

void GameThread::shutdown() {
    // Drop any deferred game-discarding action: the engines it would drive are
    // about to be killed, and the user is quitting rather than starting a game.
    {
        std::lock_guard<std::mutex> lock(deferredMutex);
        deferredTask = nullptr;
    }
    deferredPending = false;

    // Signal the game thread to stop (non-blocking), then kill engine processes
    // to unblock any stuck GTP commands, then join the thread.
    spdlog::debug("shutdown: signaling interrupt");
    spdlog::default_logger()->flush();
    if (thread) {
        LoopState expected = LoopState::Running;
        loop.compare_exchange_strong(expected, LoopState::Stopping);
        navQueueCV.notify_one();
    }
    // Kill engine processes — unblocks game thread if stuck in GTP I/O.
    spdlog::debug("shutdown: terminating engine processes");
    spdlog::default_logger()->flush();
    for (auto* player : playerManager->getPlayers()) {
        if (player->isTypeOf(Player::ENGINE)) {
            spdlog::debug("shutdown: terminating {}", player->getName());
            spdlog::default_logger()->flush();
            dynamic_cast<GtpEngine*>(player)->terminateProcess();
            spdlog::debug("shutdown: terminated {}", player->getName());
            spdlog::default_logger()->flush();
        }
    }
    // Now join — game thread exits fast since pipes are dead and interrupt is set.
    spdlog::debug("shutdown: joining game thread");
    spdlog::default_logger()->flush();
    interrupt();
    spdlog::debug("shutdown: complete");
    spdlog::default_logger()->flush();
}

void GameThread::removeSgfPlayers() const {
    playerManager->removeSgfPlayers();
}

bool GameThread::clearGame(int boardSize, float komi, int handicap) {

    // Reset to Match mode on new game
    gameMode = GameMode::MATCH;

    // Only sync coach engine (needed for fixed_handicap computation).
    // All other engines are synced lazily on the game thread when they
    // first need to play, keeping the UI thread responsive.
    Engine* coach = currentCoach();
    if (coach) {
        coach->boardsize(boardSize);
        coach->clear();
        coach->komi(komi);
    }

    // Clear stale setup stones from previous game (e.g. tsumego with white stones)
    // before lazy sync uses them. setFixedHandicap() will set setupBlackStones below.
    model.setupBlackStones.clear();
    model.setupWhiteStones.clear();

    // Non-coach engines are synced on the game thread, by the loop started at
    // the end of this function.
    engineSync = EngineSync::Unsynced;

    // Notify observers of board size (renders board immediately)
    std::for_each(
        gameObservers.begin(), gameObservers.end(),
        [boardSize](GameObserver* observer){
            observer->onBoardSized(boardSize);
        }
    );

    setKomi(komi);
    setFixedHandicap(handicap);
    // The loop is deliberately *not* started here — see startSyncingNewGame(),
    // which the caller invokes once the model holds the new record.
    return true;

}

void GameThread::startSyncingNewGame() {
    // Sync now, while the user is still looking at an empty board, rather than
    // billing it to their first move.
    //
    // This path used to leave the engines Unsynced with the loop stopped, so
    // the replay began only when a click called start() + run() — and on a CPU
    // KataGo, rebuilding for a new board size is seconds. The player changed
    // the board size, sat thinking, reached for a stone, and *then* paid. The
    // SGF load path has always done it this way instead (see loadSGF: "start
    // game thread early"); there was never a principle behind the difference.
    //
    // **Separate from clearGame() because of what the sync reads.** It replays
    // whatever record the model currently holds, and newGameNow() installs the
    // empty one *after* clearGame() returns. Starting the loop inside
    // clearGame() therefore raced the replay against createNewRecord(): the
    // game thread could still see the finished game and play every one of its
    // stones into the engines, which then held a position the cleared board no
    // longer showed. The next move landed on an occupied point and came back
    // "illegal move". A fast mock won that race; GNU Go lost it.
    //
    // isRunning() first, not run() alone: this path also runs *on* the game
    // thread when a discarding action was deferred past a genmove, and run()
    // takes playerMutex, which that path may already hold.
    //
    // The precondition is checked rather than trusted. Getting it wrong is
    // invisible from the UI — the board draws empty either way — and only
    // surfaces later as the engine refusing a legal-looking move, which is a
    // long way from the cause. A fresh record has no moves; a handicap places
    // setup stones, not moves, so this holds for those too.
    if (model.snapshot()->moveCount > 0) {
        spdlog::error("startSyncingNewGame: the previous record is still "
                      "installed ({} moves) — the engines would be replayed "
                      "the game being discarded; not starting the sync",
                      model.snapshot()->moveCount);
        return;
    }
    if (!isRunning()) {
        run();
    }
}

void GameThread::setKomi(float komi) {
    std::for_each(
        gameObservers.begin(), gameObservers.end(),
        [komi](GameObserver* observer){observer->onKomiChange(komi);}
    );
    // Engine komi is set during lazy sync (syncEngineToPosition) or
    // explicitly by clearGame for the coach engine.
}

size_t GameThread::getActivePlayer(int which) const {
    return playerManager->getActivePlayer(which);
}

size_t GameThread::activatePlayer(int which, size_t newIndex) {
    // All engines stay in sync after initial sync, so no special handling needed
    return playerManager->activatePlayer(which, newIndex);
}

bool GameThread::setFixedHandicap(int handicap) {
    if (model.phase() == GamePhase::Playing) {
        return false;
    }

    Engine* coach = currentCoach();
    if (!coach) {
        spdlog::error("setFixedHandicap: no coach engine!");
        return false;
    }

    std::vector<Position> stones;
    if (handicap >= 2) {
        // Handicap games use 0.5 komi by convention
        model.state.komi = 0.5f;
        coach->komi(0.5f);
        model.game.updateKomi(0.5f);
        UserSettings::instance().setKomi(0.5f);
        if (!coach->fixed_handicap(handicap, stones)) {
            return setFixedHandicap(0);  // Fall back to no handicap
        }
        // Non-coach engines get handicap stones during lazy sync
        model.state.colorToMove = Color::WHITE;
    }

    model.state.handicap = handicap;
    // Place handicap stones locally instead of using showboard()
    for (const auto& pos : stones) {
        model.board.updateStone(pos, Color::BLACK);
    }
    model.board.positionNumber += 1;

    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&stones](GameObserver* observer) { observer->onHandicapChange(stones); });

    // Notify view so handicap stones are rendered
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [this](GameObserver* observer) { observer->onBoardChange(model.board); });

    return true;
}
void GameThread::run() {
    // The loop cannot start itself: it is, by definition, already running. This
    // is reachable — a discarding action deferred past a genmove runs
    // newGameNow() / finalizeGameLoad() *on* the game thread, and both end by
    // asking for the loop. Every line below is wrong there, and the join is
    // fatal: std::thread::join() on one's own thread throws
    // "Resource deadlock avoided", which nothing catches, so the process
    // aborts. The mirror of interrupt()'s isOnGameThread() no-op, and the
    // reason this is a guard here rather than at each call site is that the
    // call sites cannot all know which thread they are on.
    if (isOnGameThread()) {
        spdlog::debug("run: already on the game thread, nothing to start");
        return;
    }

    std::unique_lock<std::mutex> lock(playerMutex);

    // If thread exists and finished, join it first before starting new one
    if (thread && thread->joinable() && loop.load() == LoopState::Stopped) {
        spdlog::debug("run: joining finished thread before starting new one");
        lock.unlock();
        thread->join();
        lock.lock();
    }

    // Don't start if already running
    if (loop.load() != LoopState::Stopped) {
        spdlog::debug("run: thread already running, skipping");
        return;
    }

    thread = std::make_unique<std::thread>(&GameThread::gameLoop, this);
    engineStarted.wait(lock, [this]() { return loop.load() != LoopState::Stopped; });
}

bool GameThread::isRunning() const { return loop.load() != LoopState::Stopped; }

bool GameThread::isThinking() const {
    // Only block for engine thinking, not for human waiting for input
    Player* p = playerToMove.load();
    return p != nullptr && p->isTypeOf(Player::ENGINE);
}

bool GameThread::analysisMayRun() const {
    if (isThinking() || isSyncingEngines() || hasPendingNavigation()) return false;
    return !(isRunning() && isCurrentPlayerEngine());
}

bool GameThread::hasPendingNavigation() const {
    {
        std::lock_guard<std::mutex> lock(navQueueMutex);
        // Queued, or popped and still running — see processNavigationQueue().
        if (!navQueue.empty() || navInFlight.load() > 0) return true;
    }
    return navigator && navigator->isNavigating();
}

bool GameThread::hasQueuedNavigation() const {
    std::lock_guard<std::mutex> lock(navQueueMutex);
    return !navQueue.empty();
}

bool GameThread::humanToMove() const {
    // Use playerToMove if set, otherwise fall back to actual current player
    // (playerToMove is nullptr before game loop sets it, but we still need to check)
    const Player* player = playerToMove.load();
    if (!player && playerManager) {
        player = playerManager->currentPlayer(model.state.colorToMove);
    }
    return player && player->isTypeOf(Player::HUMAN);
}
void GameThread::syncOtherEngines(const Move& move, const Player* player, const Engine* coach,
                                   const Engine* kibitzEngine, bool kibitzed) const {
    // Sync ALL engines to keep them in sync (invariant: all engines at same position)
    for (auto* p : playerManager->getPlayers()) {
        if (!p->isTypeOf(Player::ENGINE)) continue;

        // Skip: coach (already has the move), move maker, kibitz if it made the move
        if (p == reinterpret_cast<const Player*>(coach)) continue;
        if (p == player) continue;
        if (kibitzed && p == kibitzEngine) continue;

        spdlog::debug("syncOtherEngines: syncing engine {}", p->getName());
        if (!p->play(move)) {
            spdlog::error("syncOtherEngines: {} rejected move {} — engine out of sync!",
                p->getName(), move.toString());
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

    // Preserve territory display flags set by onGameMove (e.g. double pass
    // enables showTerritory).  The fresh result board has showTerritory=false,
    // and updateStones would overwrite the model's flag back to false.
    result.showTerritory = model.board.showTerritory;
    result.showTerritoryAuto = model.board.showTerritoryAuto;

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
                if (engineComments.tellp() > 0) engineComments << "\n";
                engineComments << engineMsg;
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

    // Territory scoring is handled in the game loop idle section —
    // isGameOver + showTerritory flags trigger it on the next iteration.
}

void GameThread::gameLoop() {
    t_isGameThread = true;
    // Announce the loop is up before the first iteration, and only from
    // Stopped. Doing it inside the loop (as `hasThreadRunning = true` did) would
    // now be a hazard rather than a redundancy: one enum carries both bits, so
    // an unconditional write to Running would silently swallow a Stopping that
    // arrived between the loop test and the write. It also closes a latent
    // deadlock — an interrupt landing before the first iteration used to leave
    // run() waiting on engineStarted forever.
    {
        LoopState expected = LoopState::Stopped;
        loop.compare_exchange_strong(expected, LoopState::Running);
    }
    engineStarted.notify_all();

    while (loop.load() == LoopState::Running) {

        // Game-discarding actions (new game, load, switch game) requested while
        // an engine was thinking run here: the loop is between moves and owns
        // the engine pipes, so it is the only safe place for them.
        processDeferredTask();

        processNavigationQueue();
        if (stopRequested()) break;

        // Anything but Playing means no genmove: Setup and Paused are waiting
        // on the user, Finished is done. (This used to read `!model ||
        // isGameOver`, whose second half the phase makes visibly redundant —
        // Finished is not Playing.)
        // If engines are synced (live game end), score and wait for nav commands.
        // If not synced (loaded game), fall through to initial sync which
        // syncs the coach first, then scores.
        if (model.phase() != GamePhase::Playing) {
            if (engineSync.load() == EngineSync::Synced) {
                processScoring();
                waitForCommandOrTimeout(100);
                continue;
            }
        }

        Engine* coach = currentCoach();

        // Initial sync: sync all engines to current position after load/new game.
        // Runs AFTER nav queue so queued tree path navigation executes first
        // (navigateToTreePath syncs the coach internally).
        if (engineSync.load() != EngineSync::Synced) {
            // Visible to other threads for as long as the replay takes, so a
            // caller waiting for quiescence waits for it. Always left below,
            // failure included, so it cannot strand anyone.
            engineSync = EngineSync::Syncing;

            // 1. Sync coach first (fast, enables scoring)
            if (coach) {
                spdlog::info("Initial sync: syncing coach {} to position", coach->getName());
                syncEngineToPosition(coach);
            }

            // 2. Score if game is finished (coach now ready)
            if (model.phase() == GamePhase::Finished) {
                processScoring();
            }

            // 3. Sync remaining engines
            for (auto* p : playerManager->getPlayers()) {
                if (stopRequested()) break;
                if (p->isTypeOf(Player::ENGINE) && p != coach) {
                    spdlog::info("Initial sync: syncing engine {} to position", p->getName());
                    syncEngineToPosition(static_cast<Engine*>(p));
                }
            }
            engineSync = EngineSync::Synced;

            // Re-evaluate from the top. The "!model || isGameOver" test above
            // ran while the engines were still unsynced, so it deliberately did
            // not take its early-out — falling through now would call genmove
            // on a game that is paused or already finished. For a loaded SGF
            // the active player is a LocalHumanPlayer, whose genmove() blocks
            // on a condition variable forever, which silently wedged the loop:
            // queued navigation was never drained, and isThinking() reported
            // false because the stuck player is not an engine.
            continue;
        }

        Engine* kibitzEngine = currentKibitz();
        Player* player = currentPlayer();

        std::unique_lock<std::mutex> lock(playerMutex, std::defer_lock);
        bool locked = false;

        // Skip genmove if navigation operation is in progress (atomic flag)
        if (navigator->isNavigating()) {
            spdlog::debug("Game loop: waiting for navigation to complete");
            waitForCommandOrTimeout(50);
            continue;
        }
        // Navigation back/home calls model.pause(), blocking genmove via !model check above.
        // No separate navigatingHistory check needed.

        if (gameMode == GameMode::ANALYSIS && coach && !stopRequested()) {
            // Analysis mode: human plays either color, engine responds to human moves
            Player* humanPlayer = playerManager->getPlayers()[playerManager->getHumanIndex()];
            spdlog::debug("Game loop: Analysis mode, waiting for human move (color={})",
                model.state.colorToMove.toString());

            // Atomically read and clear queuedMove under lock
            Move suggestedMove;
            {
                std::unique_lock<std::mutex> qLock(playerMutex);
                suggestedMove = queuedMove;
                queuedMove = Move(Move::INVALID, model.state.colorToMove);
                playerToMove = humanPlayer;
            }

            // To humanPlayer, which is what genmove() below blocks on. This read
            // `player` — the player assigned to the colour to move — so whenever
            // that was an engine, a queued move was handed to an object nobody
            // was waiting on and silently lost, while the human blocked for a
            // move that had already been made.
            humanPlayer->suggestMove(suggestedMove);
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

            // Release lock before processSuccessfulMove — it may block on
            // territory scoring (GTP final_status_list), and the UI thread
            // needs playerMutex for isThinking()/humanToMove().
            playerToMove = nullptr;
            lock.unlock();
            locked = false;

            if (success) {
                processSuccessfulMove(move, humanPlayer, coach, kibitzEngine, wasKibitz);

                // Human-originated move: engine (kibitz) auto-responds
                if (!wasKibitz && model.phase() != GamePhase::Finished && kibitzEngine) {
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

            if (model.phase() == GamePhase::Finished) {
                continue;
            }

            if (success)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

        } else if(coach && player && !stopRequested()) {
            // Match mode: strict player roles
            spdlog::debug("Game loop: Match mode, calling genmove for {} (player={})",
                model.state.colorToMove.toString(), player->getName());

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

            // An engine cannot be interrupted mid-genmove (GTP has no portable
            // abort), so a move can still arrive after the loop was asked to
            // stop — typically because the user is loading another game. That
            // move belongs to a position nobody is looking at any more, so drop
            // it rather than playing it into a record that is about to be
            // replaced. Only the human path gets the INTERRUPT sentinel above.
            if (shouldDiscardMove()) {
                spdlog::info("Discarding {} from {}: the current game is being "
                             "replaced or the loop is stopping",
                             move.toString(), player->getName());
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

            // Release lock before processSuccessfulMove — it may block on
            // territory scoring (GTP final_status_list), and the UI thread
            // needs playerMutex for isThinking()/humanToMove().
            playerToMove = nullptr;
            lock.unlock();
            locked = false;

            if(success) {
                processSuccessfulMove(move, player, coach, kibitzEngine, kibitzed);
            }

            if(model.phase() == GamePhase::Finished) {
                continue;
            }

            if(success && move != Move::INTERRUPT)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        // Clear playerToMove while still holding lock - signals processing complete
        playerToMove = nullptr;
        if(locked) lock.unlock();

        // Debug: log if we fell through without entering any mode
        if (gameMode != GameMode::ANALYSIS && !(coach && player)) {
            static int fallCount = 0;
            if (++fallCount % 100 == 1) {
                spdlog::debug("Game loop: fell through (gameMode={}, coach={}, player={})",
                    gameMode == GameMode::MATCH ? "Match" : "Analysis",
                    coach ? coach->getName() : "null",
                    player ? player->getName() : "null");
            }
        }
        waitForCommandOrTimeout(50);
    }
    spdlog::debug("gameLoop: exiting");
    loop = LoopState::Stopped;
}

void GameThread::playLocalMove(const Move& move) {
    std::unique_lock<std::mutex> lock(playerMutex);
    Player* p = playerToMove.load();
    spdlog::debug("playLocalMove: move={}, playerToMove={}", move.toString(), p ? "set" : "null");
    if (p) {
        p->suggestMove(move);
    } else if (model.phase() == GamePhase::Playing || move == Move::INTERRUPT) {
        // RESIGN used to be queued from any phase, which is how a resignation
        // issued while reviewing survived to fire on the next start, against a
        // position nobody was looking at. GobanControl::canResign() now refuses
        // it outright, so the exemption only kept the hazard alive.
        queuedMove = move;
    }
}

void GameThread::playKibitzMove() {
    std::unique_lock<std::mutex> lock(playerMutex);
    Move kibitzed(Move::KIBITZED, model.state.colorToMove);
    Player* p = playerToMove.load();
    if (p) {
        p->suggestMove(kibitzed);
    } else if (model.phase() == GamePhase::Playing) {
        // The same fallback playLocalMove() has, and for the same reason. Nobody
        // is blocked in genmove between two moves: the loop clears playerToMove
        // and then sleeps 500 ms before the next iteration. A kibitz request
        // arriving in that window used to be dropped on the floor, so pressing
        // Kibitz just after a move did nothing at all — silently, and often,
        // since half a second is exactly how long a user takes to reach for it.
        queuedMove = kibitzed;
    }
}

void GameThread::navigateBack() {
    std::lock_guard<std::mutex> lock(navQueueMutex);
    navQueue.push({NavCommand::BACK});
    wakeGameThread();
}

void GameThread::navigateForward() {
    std::lock_guard<std::mutex> lock(navQueueMutex);
    navQueue.push({NavCommand::FORWARD});
    wakeGameThread();
}

void GameThread::navigateToVariation(const Move& move, bool promote) {
    std::lock_guard<std::mutex> lock(navQueueMutex);
    navQueue.push({NavCommand::TO_VARIATION, move, promote});
    wakeGameThread();
}

void GameThread::requestKibitzNav() {
    std::lock_guard<std::mutex> lock(navQueueMutex);
    navQueue.push({NavCommand::KIBITZ_NAV});
    wakeGameThread();
}

void GameThread::navigateToStart() {
    std::lock_guard<std::mutex> lock(navQueueMutex);
    navQueue.push({NavCommand::TO_START});
    wakeGameThread();
}

void GameThread::navigateToEnd() {
    std::lock_guard<std::mutex> lock(navQueueMutex);
    navQueue.push({NavCommand::TO_END});
    wakeGameThread();
}

void GameThread::navigateToTreePath(int pathLength, const std::vector<int>& branchChoices) {
    std::lock_guard<std::mutex> lock(navQueueMutex);
    NavCommand cmd;
    cmd.type = NavCommand::TO_TREE_PATH;
    cmd.pathLength = pathLength;
    cmd.branchChoices = branchChoices;
    navQueue.push(std::move(cmd));
    wakeGameThread();
}

void GameThread::waitForCommandOrTimeout(int ms) {
    std::unique_lock<std::mutex> lock(navQueueMutex);
    navQueueCV.wait_for(lock, std::chrono::milliseconds(ms),
        [this]() { return !navQueue.empty() || stopRequested(); });
}

void GameThread::wakeGameThread() {
    navQueueCV.notify_one();
    Player* p = playerToMove.load();
    if (p && p->isTypeOf(Player::LOCAL | Player::HUMAN)) {
        p->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
    }
}

void GameThread::processScoring() {
    // territoryFailed is the third term, and it is what stops this being called
    // ten times a second forever once scoring has genuinely failed. See Board.
    if (!model.board.showTerritory || model.board.territoryReady || model.board.territoryFailed)
        return;

    Engine* coach = currentCoach();
    if (!coach) return;

    model.state.msg = GameState::CALCULATING_SCORE;

    Board result(model.game.getBoardSize());
    Position koPosition;
    model.game.buildBoardFromMoves(result, koPosition);
    const bool scored = coach->applyTerritory(result);

    if (stopRequested()) return;

    // The coach shaded territory but could not put a number on it. Another
    // engine may be able to — but only one that is actually at this position.
    //
    // This is the guard that was missing. The initial sync syncs the coach
    // (step 1), scores (step 2), then syncs everything else (step 3), so asking
    // the others here reaches them at whatever position they still held. For an
    // engine that has just started, that is an empty board it was never told
    // about, and the answer is not merely wrong: a CPU KataGo asked to score an
    // empty 19x19 board blocks until the command timeout, with the game thread
    // stuck inside the sync block and the UI showing "Calculating score…" the
    // whole time. Skipping it here costs nothing — the loop calls us again once
    // engineSync reaches Synced, and then the fallback is both safe and useful.
    if (!scored && result.showTerritory && engineSync.load() == EngineSync::Synced) {
        for (auto* player : playerManager->getPlayers()) {
            if (stopRequested()) return;
            if (player == coach || !player->isTypeOf(Player::ENGINE)) continue;
            auto* gtpEngine = dynamic_cast<GtpEngine*>(player);
            if (!gtpEngine) continue;

            spdlog::info("Coach [{}] could not score; asking [{}] for final_score",
                coach->getName(), gtpEngine->getName());
            if (const std::optional<float> altScore = gtpEngine->final_score()) {
                spdlog::info("Using [{}] score: {:.1f}", gtpEngine->getName(), *altScore);
                result.score = *altScore;
                result.territoryReady = true;
                break;
            }
        }
    }

    if (result.territoryReady) {
        model.state.scoreDelta = result.score;
        // Update message to show winner (replaces CALCULATING_SCORE)
        model.state.msg = (result.score > 0) ? GameState::BLACK_WON : GameState::WHITE_WON;
    } else if (engineSync.load() == EngineSync::Synced) {
        // Every engine was at the right position and none could score it, so
        // this position is not going to score. Latch it, or the guard above
        // never becomes true and we retry on every loop iteration.
        result.territoryFailed = true;
        model.state.msg = GameState::SCORING_FAILED;
        model.state.scoringError = "No engine could score this position";
        spdlog::warn("Scoring failed with all engines synced; not retrying for this position");
    } else {
        // Mid-sync: leave both flags clear and say nothing yet. The loop comes
        // back here once the engines are synced.
        spdlog::debug("Scoring deferred: engines not synced yet");
        return;
    }

    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) { observer->onBoardChange(result); });
}

void GameThread::processNavigationQueue() {
    /// Keeps a popped command visible to hasPendingNavigation() until it has
    /// actually finished, exception paths included.
    struct InFlightGuard {
        std::atomic<int>& count;
        ~InFlightGuard() { --count; }
    };

    while (true) {
        NavCommand cmd;
        {
            std::lock_guard<std::mutex> lock(navQueueMutex);
            if (navQueue.empty()) return;
            cmd = std::move(navQueue.front());
            navQueue.pop();
            // Claim the command before the lock drops. GameNavigator raises its
            // own NavigationGuard, but only once it is past its guard clauses —
            // so between the pop and that point the command was invisible to
            // hasPendingNavigation(), and a caller polling for quiescence in
            // that window read the board before navigation had applied. That is
            // exactly the "queued navigation" half of the isIdle() invariant.
            ++navInFlight;
        }
        InFlightGuard guard{navInFlight};
        executeNavCommand(cmd);
    }
}

void GameThread::executeNavCommand(const NavCommand& cmd) {
    switch (cmd.type) {
        case NavCommand::BACK: {
            bool success = navigator->navigateBack();
            // Update tsumego feedback on back-navigation
            if (success && model.tsumegoMode) {
                if (model.game.isOnBadMovePath()) {
                    model.state.msg = GameState::TSUMEGO_WRONG;
                } else {
                    model.state.msg = GameState::NONE;
                }
            }
            break;
        }
        case NavCommand::FORWARD: {
            bool success = navigator->navigateForward();
            // If at scored game end, set flags — scoring handled in game loop idle section
            if (success && model.game.isAtEndOfNavigation()
                && model.game.shouldShowTerritory()) {
                model.board.toggleTerritoryAuto(true);
            }
            // Update tsumego feedback on forward-navigation
            if (success && model.tsumegoMode) {
                if (model.game.isOnBadMovePath()) {
                    model.state.msg = GameState::TSUMEGO_WRONG;
                } else {
                    model.state.msg = GameState::NONE;
                }
            }
            break;
        }
        case NavCommand::TO_START:
            navigator->navigateToStart();
            break;
        case NavCommand::TO_END:
            navigator->navigateToEnd();
            break;
        case NavCommand::TO_VARIATION: {
            auto varResult = navigator->navigateToVariation(cmd.move, cmd.promote);

            // Tsumego logic: infer BM marking from context
            if (varResult.success && model.tsumegoMode) {
                if (varResult.newBranch && !model.game.isOnBadMovePath()) {
                    // New branch from correct tree → first wrong move
                    model.game.markBadMove();
                }

                if (model.game.isOnBadMovePath()) {
                    model.state.msg = GameState::TSUMEGO_WRONG;
                } else if (model.game.isAtEndOfNavigation()) {
                    model.state.msg = GameState::TSUMEGO_SOLVED;
                } else if (model.game.hasNextMove()) {
                    // Correct path: auto-play opponent's response from SGF
                    navigator->navigateForward();
                    if (model.game.isAtEndOfNavigation()) {
                        model.state.msg = GameState::TSUMEGO_SOLVED;
                    }
                }
            }

            // In Analysis mode, auto-respond with kibitz engine after human variation
            if (varResult.success && gameMode == GameMode::ANALYSIS) {
                Engine* kibitz = currentKibitz();
                Engine* coach = currentCoach();
                if (kibitz && coach && model.phase() != GamePhase::Finished) {
                    // All engines are synced after initial sync
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
        case NavCommand::KIBITZ_NAV: {
            // Every way out of here used to be a bare `break`. A kibitz that
            // did nothing was therefore indistinguishable from one that was
            // never requested, in the log as much as on screen — which cost two
            // rounds of guessing at a bug report. Say why.
            Engine* kibitz = currentKibitz();
            Engine* coach = currentCoach();
            if (!kibitz || !coach) {
                spdlog::warn("kibitz: no {} engine is loaded, so there is nothing "
                             "to ask", !kibitz ? "kibitz" : "coach");
                break;
            }

            const Color responseColor = model.state.colorToMove;
            spdlog::info("kibitz: asking [{}] for a {} move at the cursor",
                         kibitz->getName(), responseColor.toString());

            // Announce it the way an ordinary genmove is announced. A kibitz
            // asked from the navigation queue never set this, so `isThinking()`
            // stayed false for its whole duration: the toolbar did not grey, no
            // engine was named, navigation was not blocked, and nothing on
            // screen distinguished "KataGo is thinking" from "the button did
            // nothing". On a CPU backend that silence lasts tens of seconds, and
            // it is the whole of what a bug report described as "nothing
            // happened" — twice.
            //
            // Setting it is also correct beyond the display: an engine is in
            // fact busy, so ADR-0001's refusals and the analysis overlay's yield
            // both want to know.
            struct ThinkingScope {
                std::atomic<Player*>& slot;
                ~ThinkingScope() { slot = nullptr; }
            } scope{playerToMove};
            playerToMove = kibitz;

            Move response = kibitz->genmove(responseColor);
            if (!response || response == Move::RESIGN) {
                spdlog::warn("kibitz: [{}] offered no move for {} — it answered {}",
                             kibitz->getName(), responseColor.toString(),
                             response.toString());
                break;
            }

            // The coach owns the authoritative board, so it has to accept the
            // move before it can be recorded. A rejection means the two engines
            // are not at the same position — navigation syncs them move by move
            // and ignores every failure while doing it, so a single refused
            // `undo` earlier leaves exactly this state. Resync both from the
            // record and ask again rather than dropping the request on the
            // floor, which is what it did before.
            if (kibitz != coach && !coach->play(response)) {
                spdlog::warn("kibitz: [{}] suggested {} but the coach [{}] refused "
                             "it — resyncing both to the current position and "
                             "retrying", kibitz->getName(), response.toString(),
                             coach->getName());
                if (!syncEngineToPosition(coach) || !syncEngineToPosition(kibitz)) {
                    spdlog::error("kibitz: could not resync the engines; the "
                                  "suggestion is lost");
                    break;
                }
                response = kibitz->genmove(responseColor);
                if (!response || response == Move::RESIGN || !coach->play(response)) {
                    spdlog::error("kibitz: [{}] still cannot place a {} move the "
                                  "coach accepts", kibitz->getName(),
                                  responseColor.toString());
                    break;
                }
            }
            processSuccessfulMove(response, kibitz, coach, kibitz, false);
            break;
        }
        case NavCommand::TO_TREE_PATH: {
            if (!navigator->navigateToTreePath(cmd.pathLength, cmd.branchChoices)) {
                spdlog::warn("navigateToTreePath failed on game thread - clearing session state");
                UserSettings::instance().clearSessionState();
            }
            break;
        }
    }

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
        Player* p = playerToMove.load();
        if (p != nullptr && p->isTypeOf(Player::LOCAL | Player::HUMAN)) {
            p->suggestMove(Move(Move::INTERRUPT, model.state.colorToMove));
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

bool GameThread::areBothPlayersEngines() const {
    return playerManager->areBothPlayersEngines();
}

bool GameThread::isCurrentPlayerEngine() const {
    // Get color to move (0 = black, 1 = white)
    int colorIndex = (model.state.colorToMove == Color::BLACK) ? 0 : 1;
    size_t playerIdx = playerManager->getActivePlayer(colorIndex);
    auto& players = playerManager->getPlayers();
    if (playerIdx >= players.size()) {
        return false;
    }
    return players[playerIdx]->isTypeOf(Player::ENGINE);
}

void GameThread::loadEngines(const std::shared_ptr<Configuration> conf) const {
    playerManager->loadEngines(conf);
}

void GameThread::loadEnginesParallel(std::shared_ptr<Configuration> conf,
                                      const std::string& sgfPath,
                                      std::function<void()> onFirstEngineReady,
                                      int gameIndex,
                                      bool startAtRoot) {
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
        // Registered before the thread starts, so the indicator names every
        // engine from the first frame rather than only those whose thread has
        // been scheduled. The name is what the user configured; falling back to
        // the command keeps an unnamed bot from showing as a blank.
        const std::string botName = botConfigs[i].value("name", botConfigs[i].value("command", "engine"));
        playerManager->beginLoading(botName);

        threads.emplace_back([this, &botConfigs, i, botName, &mtx, &cv, &firstReadyEngine, &enginesLoaded, &loadedEngines]() {
            Engine* engine = playerManager->loadSingleEngine(botConfigs[i]);
            // Whether or not it loaded: a failure is reported through the log,
            // and leaving a failed engine "still loading" forever would leave
            // the indicator up for the session.
            playerManager->finishLoading(botName);

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

    // Wait for first engine to be ready (or all engines to fail)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&firstReadyEngine, &enginesLoaded, totalEngines]() {
            return firstReadyEngine != nullptr || enginesLoaded >= totalEngines;
        });
    }

    if (!firstReadyEngine) {
        spdlog::error("All engines failed to load!");
        // Join threads before returning
        for (auto& t : threads) {
            t.join();
        }
        playerManager->loadHumanPlayers(conf);
        return;
    }

    // Load SGF with first ready engine (stones appear now!)
    if (!sgfPath.empty() && firstReadyEngine) {
        spdlog::info("Loading SGF with first engine: {} (gameIndex={}, startAtRoot={})", sgfPath, gameIndex, startAtRoot);
        loadSGFWithEngine(sgfPath, firstReadyEngine, gameIndex, startAtRoot);

        // Wait for coach engine (usually GNU Go — loads fast, often already ready).
        // Needed for tree path navigation and scoring on the game thread.
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]() { return currentCoach() != nullptr; });
        }

        // Start game thread early — processes any queued navigation (tree path,
        // etc.) and scoring with the coach, without waiting for slow engines.
        // finalizeGameLoad() will skip run() since the thread is already running.
        // Must mark Unsynced BEFORE run() — otherwise the game thread sees
        // stale true from previous game, skips initial sync, and processScoring
        // runs with an unsynced coach (wrong position → final_status_list fails).
        engineSync = EngineSync::Unsynced;
        run();
    } else {
        // No SGF - wait for coach engine specifically (it handles handicap/scoring)
        Engine* coach = nullptr;
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this, &coach]() {
                coach = currentCoach();
                return coach != nullptr;
            });
        }

        auto& settings = UserSettings::instance();
        int boardSize = settings.getBoardSize();
        float komi = settings.getKomi();

        // Set up coach engine with saved board size/komi
        coach->boardsize(boardSize);
        coach->komi(komi);
        coach->clear();

        // Update model state with saved settings
        spdlog::debug("Setting model.state.komi = {} from settings", komi);
        model.state.komi = komi;
        model.state.handicap = settings.getHandicap();
        model.game.updateKomi(komi);  // Sync game record with actual komi

        std::for_each(gameObservers.begin(), gameObservers.end(),
            [boardSize](GameObserver* observer) { observer->onBoardSized(boardSize); });

        // Coach board is already clear — setFixedHandicap sends fixed_handicap
        setFixedHandicap(settings.getHandicap());
        model.createNewRecord();
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
    finalizeGameLoad(firstReadyEngine, sgfWasLoaded);

    spdlog::info("All engines loaded and synced");
}

Move GameThread::getLocalMove(const Position& coord) const {
    return {coord, model.state.colorToMove};
}

Move GameThread::getLocalMove(const Move::Special move) const {
    return {move, model.state.colorToMove};
}

bool GameThread::loadSGF(const std::string& fileName, int gameIndex, bool startAtRoot) {
    // Use unified loading path - same as startup
    Engine* coach = currentCoach();

    if (!loadSGFWithEngine(fileName, coach, gameIndex, startAtRoot)) {
        return false;
    }

    // Sync remaining engines and finalize (player matching, game mode, run thread)
    // Pass coach as already-synced engine
    finalizeGameLoad(coach);

    return true;
}

bool GameThread::switchGame(int gameIndex, bool startAtRoot) {
    // Switch to a different game within the already-loaded SGF document.
    // Unlike loadSGF(), no file I/O, no auto-save, no player removal.
    interrupt();
    model.pause();

    GameRecord::SGFGameInfo gameInfo;
    if (!model.game.switchToGame(gameIndex, gameInfo, startAtRoot)) {
        return false;
    }

    if (!applyLoadedGame(gameInfo, nullptr)) {
        return false;
    }

    // Sync remaining engines (kibitz, etc.) but don't re-match players
    finalizeGameLoad(currentCoach(), false);

    spdlog::info("Switched to game {} (board: {}, moves: {})",
        gameIndex + 1, gameInfo.boardSize, model.game.moveCount());
    return true;
}

bool GameThread::autoPlayTsumegoSetup() {
    // Some tsumego SGFs use PL to mark "side to keep alive" rather than "side to move".
    // Per the SGF spec, PL means "player to move first". When the first child move's color
    // contradicts PL, auto-play that move as setup so the solver starts with the PL color.
    if (!model.game.hasNextMove()) return false;
    Color plColor = model.game.getColorToMove();
    Move nextMove = model.game.getNextMove();
    if (nextMove.col == plColor) return false;

    spdlog::info("Tsumego setup: auto-playing {} move (PL={}) as setup",
        nextMove.col == Color::BLACK ? "B" : "W",
        plColor == Color::BLACK ? "B" : "W");
    navigateForward();  // Async — hint applied in executeNavCommand after FORWARD completes
    return true;
}

bool GameThread::syncCoachToCurrentPosition() {
    Engine* coach = currentCoach();
    if (!coach) {
        spdlog::warn("syncCoachToCurrentPosition: no coach engine available");
        return false;
    }
    return syncEngineToPosition(coach);
}

bool GameThread::syncEngineToPosition(Engine* engine, int* syncedMoves) {
    // Core method: sync one engine to current game position
    if (!engine || stopRequested()) return false;

    int boardSize = model.getBoardSize();
    float komi = model.state.komi;
    spdlog::debug("syncEngineToPosition: {} boardSize={} komi={} setupB={} setupW={} moves={}",
        engine->getName(), boardSize, komi,
        model.setupBlackStones.size(), model.setupWhiteStones.size(),
        model.game.moveCount());

    if (!engine->boardsize(boardSize)) {
        spdlog::error("syncEngineToPosition: {} failed boardsize({})", engine->getName(), boardSize);
        return false;
    }
    if (!engine->clear()) {
        spdlog::error("syncEngineToPosition: {} failed clear_board", engine->getName());
        return false;
    }
    engine->komi(komi);

    // Replay setup stones (AB: black, AW: white) — fail early on invalid stones
    for (const auto& stone : model.setupBlackStones) {
        if (!engine->play(Move(stone, Color::BLACK))) {
            spdlog::error("syncEngineToPosition: {} rejected setup black stone {} — aborting",
                engine->getName(), stone.toSgf(boardSize));
            return false;
        }
    }
    for (const auto& stone : model.setupWhiteStones) {
        if (!engine->play(Move(stone, Color::WHITE))) {
            spdlog::error("syncEngineToPosition: {} rejected setup white stone {} — aborting",
                engine->getName(), stone.toSgf(boardSize));
            return false;
        }
    }

    // Replay all moves — stop on first failure (fail early)
    int moveNum = 0;
    bool replayFailed = false;
    model.game.replay([&](const Move& move) {
        if (replayFailed) return;
        ++moveNum;
        if (!engine->play(move)) {
            spdlog::error("syncEngineToPosition: {} rejected move #{} {} — aborting replay",
                engine->getName(), moveNum, move.toString());
            model.state.scoringError = "Illegal move #" + std::to_string(moveNum)
                + " " + move.toString();
            replayFailed = true;
        }
    });
    if (syncedMoves) *syncedMoves = replayFailed ? moveNum - 1 : moveNum;
    return !replayFailed;
}

void GameThread::finalizeLoadedGame(Engine* /* engine */, const GameRecord::SGFGameInfo& gameInfo) {
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

    // Actual scoring happens in the game loop idle section
    if (endedWithPasses) {
        model.board.toggleTerritoryAuto(true);
    }
    model.endGame(endedByResignation ? GameState::RESIGNATION : GameState::DOUBLE_PASS);

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
                playerManager->activatePlayer(which, i);
                return;
            }
        }

        // No match found - create a temporary SGF player (acts as human)
        auto* sgfPlayer = new LocalHumanPlayer(sgfName);
        sgfPlayer->addType(Player::SGF_PLAYER);
        size_t newIndex = addPlayer(sgfPlayer);
        playerManager->activatePlayer(which, newIndex);
        spdlog::info("Created SGF player '{}' at index {}", sgfName, newIndex);
    };

    spdlog::info("Matching SGF players - Black='{}', White='{}'", blackPlayer, whitePlayer);
    matchPlayerName(blackPlayer, 0);
    matchPlayerName(whitePlayer, 1);
}

bool GameThread::applyLoadedGame(const GameRecord::SGFGameInfo& gameInfo, Engine* engine) {
    // Common logic for setting up model state after loading/switching a game.
    // Called by both loadSGFWithEngine() and switchGame().
    // Engine sync happens on game thread (finalizeGameLoad marks Unsynced).

    model.state.komi = gameInfo.komi;
    model.state.handicap = gameInfo.handicap;
    gameMode = GameMode::MATCH;

    // Copy setup stones to model (always update, even if empty, to clear old state).
    // The SGF tree already has AB/AW properties — don't call setHandicapStones() here,
    // it would duplicate them onto root for FF[3] files where setup is on a child node.
    model.setupBlackStones = gameInfo.setupBlackStones;
    model.setupWhiteStones = gameInfo.setupWhiteStones;
    if (!gameInfo.setupBlackStones.empty() || !gameInfo.setupWhiteStones.empty()) {
        model.board.clear(gameInfo.boardSize);
        for (const auto& stone : gameInfo.setupBlackStones) {
            model.board.updateStone(stone, Color::BLACK);
        }
        for (const auto& stone : gameInfo.setupWhiteStones) {
            model.board.updateStone(stone, Color::WHITE);
        }
    }

    // Notify observers of board size
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&gameInfo](GameObserver* observer) {
            observer->onBoardSized(gameInfo.boardSize);
        });

    // Engine sync deferred to game thread - just display board immediately

    // Set game state before onBoardChange — the render loop reads comment/colorToMove
    // when it detects positionNumber change, so these must be ready first.
    model.state.msg = GameState::NONE;
    model.state.colorToMove = model.game.getColorToMove();
    if (model.game.moveCount() > 0) {
        const Move& lastMove = model.game.lastMove();
        if (lastMove == Move::PASS) {
            model.state.msg = (lastMove.col == Color::BLACK)
                ? GameState::BLACK_PASS : GameState::WHITE_PASS;
            model.state.passVariationLabel = std::to_string(model.game.moveCount());
        }
    }

    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();
    navigator->applyTsumegoHint();

    // Build board from SGF using local capture logic (engine-independent)
    // This triggers onBoardChange → positionNumber increment, which the render
    // loop uses to detect changes and read the state set above.
    Board result(model.game.getBoardSize());
    Position koPosition;
    model.game.buildBoardFromMoves(result, koPosition);
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) {
            observer->onBoardChange(result);
        });

    // Handle finished games (resignation or double pass)
    // Only finalize when at the end of the game - not when viewing from root
    if (model.game.isAtEndOfNavigation()) {
        finalizeLoadedGame(engine, gameInfo);
    }

    return true;
}

bool GameThread::loadSGFWithEngine(const std::string& fileName, Engine* engine, int gameIndex, bool startAtRoot) {
    // Load SGF and sync specified engine (or coach if none specified)
    // Call finalizeGameLoad() later when other engines are ready

    // Interrupt game loop first - it may be blocked waiting on a player's genmove().
    // Must happen before removeSgfPlayers() to avoid destroying a player mid-wait.
    //
    interrupt();
    model.pause();

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
    if (!model.game.loadFromSGF(fileName, gameInfo, gameIndex, startAtRoot)) {
        return false;
    }

    return applyLoadedGame(gameInfo, engine);
}

void GameThread::finalizeGameLoad(Engine* alreadySynced, bool matchPlayers) {
    // Finalize game load: mark for initial sync, match players, start game thread.
    // All engines will be synced on game thread (coach first for scoring).

    spdlog::info("Finalizing game load (board: {}, moves: {})",
                 model.getBoardSize(), model.game.moveCount());

    // Mark for initial sync on game thread
    engineSync = EngineSync::Unsynced;

    // Match SGF player names to engines (only when SGF was loaded)
    if (matchPlayers) {
        matchSgfPlayers();
    }

    // Set game mode based on result (but don't start yet - caller must call
    // model.start() after UI refresh to avoid race with transient player changes)
    gameMode = model.game.hasGameResult() ? GameMode::ANALYSIS : GameMode::MATCH;

    // Start game thread for navigation (loop waits at !model until started)
    run();

    spdlog::info("SGF viewing ready (engines will sync on game thread)");
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
