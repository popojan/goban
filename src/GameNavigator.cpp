#include "GameNavigator.h"
#include "GobanModel.h"
#include "player.h"
#include <spdlog/spdlog.h>
#include <algorithm>

void GameNavigator::setPassMessage(const Move& passMove) {
    model.state.msg = (passMove.col == Color::BLACK)
        ? GameState::BLACK_PASS : GameState::WHITE_PASS;
    size_t moveNum = model.game.getViewPosition();
    model.state.passVariationLabel = std::to_string(moveNum);
}

void GameNavigator::applyTsumegoHint() {
    if (model.tsumegoMode && model.state.comment.empty()
        && model.game.getViewPosition() == 0) {
        model.state.comment = (model.state.colorToMove == Color::BLACK)
            ? model.tsumegoHintBlack : model.tsumegoHintWhite;
    }
}

/// Re-enter Finished when navigation lands on the end of a game that carries a
/// result. A user who has pressed Start is playing on from here, and their
/// intent wins over the record's result — hence the Playing check.
void GameNavigator::restoreFinishedStateAtEnd() {
    if (model.phase() == GamePhase::Playing) return;
    if (!model.game.isAtEndOfNavigation() || !model.game.hasGameResult()) return;
    model.endGame(model.game.isResignationResult()
        ? GameState::RESIGNATION : GameState::DOUBLE_PASS);
}

/// Show the end of a finished game: territory when the position needs scoring,
/// the recorded result otherwise. Must run *after* notifyBoardChange(), whose
/// updateStones() would overwrite the territory flag.
void GameNavigator::showEndOfGameResult() {
    if (model.game.shouldShowTerritory()) {
        model.board.toggleTerritoryAuto(true);
        // processScoring() will set CALCULATING_SCORE then the result message
    } else if (model.phase() == GamePhase::Finished) {
        auto resultMsg = model.game.getResultMessage();
        if (resultMsg != GameState::NONE) {
            model.state.msg = resultMsg;
        }
    }
}

void GameNavigator::syncStateAfterNavigation() {
    // Update state from current SGF node (caller handles msg, game-over, territory, board notify)
    model.state.colorToMove = model.game.getColorToMove();
    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();
    applyTsumegoHint();
}

GameNavigator::GameNavigator(GobanModel& model, CoachProvider getCoach,
                             ActivePlayersProvider getActivePlayers, ObserverList& observers,
                             SyncEngineCallback syncEngine)
    : model(model), getCoach(std::move(getCoach)), getActivePlayers(std::move(getActivePlayers)),
      syncEngine(std::move(syncEngine)), gameObservers(observers)
{
}

void GameNavigator::syncEngines(const Move& move, Engine* coach, bool isUndo) const {
    // Sync ALL engines (invariant: all engines stay at same position)
    for (auto player : getActivePlayers()) {
        if (!player->isTypeOf(Player::ENGINE)) continue;
        if (player == reinterpret_cast<Player*>(coach)) continue;

        // Both results used to be discarded, which is how an engine quietly
        // stopped tracking the cursor: one refused `undo` somewhere in a run of
        // them, no word about it, and every later question put to that engine
        // was answered for a different position. CLAUDE.md's "check return
        // values from operations that can fail" exists for exactly this.
        const bool ok = isUndo ? player->undo() : player->play(move);
        if (!ok) {
            spdlog::warn("navigation: [{}] refused {} — it is no longer at the "
                         "same position as the coach, and what it says next will "
                         "be about a different board",
                         player->getName(), isUndo ? "undo" : move.toString());
        }
    }
}

void GameNavigator::notifyBoardChange(const Board& result) const {
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) { observer->onBoardChange(result); });
}

void GameNavigator::notifyBoardChangeWithMove(const Board& result, const Move& move) const {
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result, move](GameObserver* observer) {
            observer->onBoardChange(result);
            observer->onStonePlaced(move);
        });
}

bool GameNavigator::navigateBack() {
    spdlog::debug("navigateBack: isNavigating={}, hasPrev={}, phase={}, moveCount={}",
        model.game.isNavigating(), model.game.hasPreviousMove(),
        phaseName(model.phase()), model.game.moveCount());

    if (!model.game.isNavigating() || !model.game.hasPreviousMove()) {
        spdlog::debug("navigateBack: cannot navigate (isNavigating={}, hasPrev={})",
            model.game.isNavigating(), model.game.hasPreviousMove());
        return false;
    }

    Engine* coach = getCoach();
    if (!coach) {
        spdlog::warn("navigateBack: no coach engine available");
        return false;
    }

    NavigationGuard guard(navigationInProgress);

    // Capture the move we're about to undo (for pass message display)
    Move undoneMove = model.game.lastMove();

    // Undo one move (including passes - step through them individually)
    if (!coach->undo()) {
        spdlog::warn("navigateBack: coach->undo() failed");
        return false;
    }

    syncEngines(Move(), coach, true);
    model.game.undo();

    // Hide territory unconditionally when navigating back (stale after undo)
    model.board.showTerritory = false;
    model.board.showTerritoryAuto = false;

    // Pause active play - user is reviewing. Explicit "Start" needed to resume.
    // enterReview() rather than pause(): the position shown is no longer the end
    // of the game, so a finished game must stop being finished.
    model.enterReview();

    // Show pass message if the move at the current position is a pass
    Move currentMove = model.game.lastMove();
    if (currentMove == Move::PASS) {
        setPassMessage(currentMove);
    } else {
        model.state.msg = GameState::NONE;
    }

    syncStateAfterNavigation();

    Board result(model.game.getBoardSize());
    buildBoardFromSGF(result);
    notifyBoardChange(result);

    spdlog::debug("navigateBack: success, undid {}, now at move {}/{}, colorToMove={}",
        undoneMove.toString(),
        model.game.getViewPosition(), model.game.getLoadedMovesCount(),
        model.state.colorToMove == Color::BLACK ? "B" : "W");

    return true;
}

bool GameNavigator::navigateForward() {
    if (!model.game.isNavigating()) {
        spdlog::debug("navigateForward: not in navigation mode");
        return false;
    }

    // Get variations from SGF tree (children of current node)
    auto variations = model.game.getVariations();
    spdlog::debug("navigateForward: found {} variations", variations.size());

    NavigationGuard guard(navigationInProgress);

    if (variations.empty()) {
        return false;
    }

    // Play first variation (main line) - even if multiple variations exist
    Engine* coach = getCoach();
    if (!coach) {
        spdlog::error("navigateForward: no coach engine!");
        return false;
    }

    Move nextMove = variations[0];
    bool isPass = (nextMove == Move::PASS);

    spdlog::info("navigateForward: playing move {} (color={})",
        nextMove.toString(), nextMove.col == Color::BLACK ? "B" : "W");
    if (!coach->play(nextMove)) {
        spdlog::warn("navigateForward: coach->play() failed for move {} - engine out of sync?",
            nextMove.toString());
        return false;
    }

    syncEngines(nextMove, coach, false);

    // Navigate to child in SGF tree (navigateToChild moves currentNode)
    // If child doesn't exist, create a new branch
    if (!model.game.navigateToChild(nextMove)) {
        // Creating new branch - this creates SGF node and moves currentNode
        model.game.move(nextMove);
    }

    // Set message for pass moves, clear for stone moves
    if (isPass) {
        setPassMessage(nextMove);
    } else {
        model.state.msg = GameState::NONE;
    }

    syncStateAfterNavigation();

    restoreFinishedStateAtEnd();

    Board boardResult(model.game.getBoardSize());
    buildBoardFromSGF(boardResult);
    notifyBoardChangeWithMove(boardResult, nextMove);

    return true;
}

GameNavigator::VariationResult GameNavigator::navigateToVariation(const Move& move, bool promote) {
    VariationResult result;
    spdlog::debug("navigateToVariation: entry, move={}", move.toString());

    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToVariation: not in navigation mode");
        return result;
    }

    Engine* coach = getCoach();
    if (!coach) {
        spdlog::error("navigateToVariation: no coach engine!");
        return result;
    }

    NavigationGuard guard(navigationInProgress);

    // Play the move on the coach
    if (!coach->play(move)) {
        spdlog::warn("navigateToVariation: coach->play() failed for move {}", move.toString());
        return result;
    }

    syncEngines(move, coach, false);

    // Navigate to the child node in the SGF tree
    // If child doesn't exist, create a new branch
    result.newBranch = !model.game.navigateToChild(move, promote);
    if (result.newBranch) {
        spdlog::debug("navigateToVariation: creating new branch");
        if (promote && model.game.hasGameResult()) {
            // Finished game: create a fresh copy to preserve the historical record
            model.game.branchFromFinishedGame(move);
        } else {
            // In-progress game or non-promoted branch: modify tree in place
            model.game.move(move, promote);
            if (promote) {
                model.game.promoteCurrentPathToMainLine();
            }
        }
        // Clear game over state - we're continuing the game in a new branch
        // Skip for non-promoted branches (e.g. tsumego wrong moves) to stay in navigation mode.
        // Kibitz (space key) handles its own model.start() independently.
        if (promote) {
            model.start();
        }
    } else {
        spdlog::debug("navigateToVariation: following existing branch");
    }

    // Set message for pass moves, clear for stone moves
    if (move == Move::PASS) {
        setPassMessage(move);
    } else {
        model.state.msg = GameState::NONE;
    }

    syncStateAfterNavigation();

    Board boardResult(model.game.getBoardSize());
    buildBoardFromSGF(boardResult);
    notifyBoardChangeWithMove(boardResult, move);

    result.success = true;
    spdlog::debug("navigateToVariation: done, now at move {}/{}, colorToMove={}, newBranch={}",
        model.game.getViewPosition(), model.game.getLoadedMovesCount(),
        model.state.colorToMove == Color::BLACK ? "B" : "W",
        result.newBranch);

    return result;
}

bool GameNavigator::navigateToStart() {
    spdlog::debug("navigateToStart: isNavigating={}, hasPrev={}, phase={}, moveCount={}",
        model.game.isNavigating(), model.game.hasPreviousMove(),
        phaseName(model.phase()), model.game.moveCount());

    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToStart: not in navigation mode");
        return false;
    }

    if (!model.game.hasPreviousMove()) {
        spdlog::debug("navigateToStart: already at start");
        return false;
    }

    Engine* coach = getCoach();
    if (!coach) {
        spdlog::warn("navigateToStart: no coach engine available");
        return false;
    }

    NavigationGuard guard(navigationInProgress);

    // Navigate game record to root
    while (model.game.hasPreviousMove()) {
        model.game.undo();
    }

    // Sync all engines to root position via callback
    if (!syncEngine(coach)) {
        spdlog::warn("navigateToStart: coach sync failed");
        return false;
    }
    for (auto player : getActivePlayers()) {
        if (player != reinterpret_cast<Player*>(coach) && player->isTypeOf(Player::ENGINE)) {
            syncEngine(static_cast<Engine*>(player));
        }
    }

    model.board.showTerritory = false;
    model.board.showTerritoryAuto = false;

    // Pause active play - user is reviewing. Explicit "Start" needed to resume.
    model.enterReview();

    model.state.msg = GameState::NONE;
    syncStateAfterNavigation();

    Board result(model.game.getBoardSize());
    buildBoardFromSGF(result);
    notifyBoardChange(result);

    spdlog::debug("navigateToStart: success, now at move 0");
    return true;
}

bool GameNavigator::navigateToEnd() {
    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToEnd: not in navigation mode");
        return false;
    }

    Engine* coach = getCoach();
    if (!coach) return false;

    NavigationGuard guard(navigationInProgress);

    // Play all moves on main line (first child at each branch)
    while (true) {
        auto variations = model.game.getVariations();
        if (variations.empty()) break;

        Move nextMove = variations[0];  // Always take main line (first variation)
        spdlog::info("navigateToEnd: playing {} (color={})",
            nextMove.toString(), nextMove.col == Color::BLACK ? "B" : "W");

        if (!coach->play(nextMove)) {
            spdlog::warn("navigateToEnd: coach->play failed for {} - engine out of sync?",
                nextMove.toString());
            break;
        }

        syncEngines(nextMove, coach, false);

        if (!model.game.navigateToChild(nextMove)) {
            spdlog::warn("navigateToEnd: navigateToChild failed, this shouldn't happen");
            break;  // Don't create new moves, something is wrong
        }
    }

    // Always show result at end (whether we played moves or were already there)
    model.state.msg = GameState::NONE;
    syncStateAfterNavigation();

    restoreFinishedStateAtEnd();

    Board boardResult(model.game.getBoardSize());
    buildBoardFromSGF(boardResult);
    notifyBoardChange(boardResult);

    showEndOfGameResult();

    return true;  // Always return true - we're at the end now
}

void GameNavigator::buildBoardFromSGF(Board& outBoard) const {
    Position koPosition;
    model.game.buildBoardFromMoves(outBoard, koPosition);
    spdlog::debug("buildBoardFromSGF: built board from SGF, koPosition=({},{})",
        koPosition.col(), koPosition.row());
}

bool GameNavigator::navigateToTreePath(int pathLength, const std::vector<int>& branchChoices) {
    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToTreePath: not in navigation mode");
        return false;
    }

    Engine* coach = getCoach();
    if (!coach) {
        spdlog::warn("navigateToTreePath: no coach engine available");
        return false;
    }

    NavigationGuard guard(navigationInProgress);

    // Navigate the game record to the tree path
    if (!model.game.navigateToTreePath(pathLength, branchChoices)) {
        spdlog::warn("navigateToTreePath: failed to navigate (path invalid or SGF changed)");
        return false;
    }

    // Update display immediately (before engine sync, which may be slow)
    model.state.msg = GameState::NONE;
    syncStateAfterNavigation();

    restoreFinishedStateAtEnd();

    Board boardResult(model.game.getBoardSize());
    buildBoardFromSGF(boardResult);
    notifyBoardChange(boardResult);

    showEndOfGameResult();

    spdlog::info("navigateToTreePath: navigated {} steps ({} branch choices), now at move {}",
        pathLength, branchChoices.size(), model.game.getViewPosition());

    // Sync only the coach engine (needed for scoring). Other engines (kibitz, etc.)
    // are synced on-demand when they need to generate a move — avoids blocking on
    // slow engines like KataGo during startup.
    if (!syncEngine(coach)) {
        spdlog::warn("navigateToTreePath: coach sync failed");
        return false;
    }

    return true;
}
