#include "GameNavigator.h"
#include "GobanModel.h"
#include "player.h"
#include <spdlog/spdlog.h>
#include <algorithm>

GameNavigator::GameNavigator(GobanModel& model, CoachProvider getCoach,
                             ActivePlayersProvider getActivePlayers, ObserverList& observers)
    : model(model), getCoach(std::move(getCoach)), getActivePlayers(std::move(getActivePlayers)), gameObservers(observers)
{
}

void GameNavigator::syncEngines(const Move& move, Engine* coach, bool isUndo) const {
    // Only sync active players (black + white), not all available engines
    for (auto player : getActivePlayers()) {
        if (player != reinterpret_cast<Player*>(coach)) {
            if (isUndo) {
                player->undo();
            } else {
                player->play(move);
            }
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
    spdlog::debug("navigateBack: isNavigating={}, hasPrev={}, isGameOver={}, moveCount={}",
        model.game.isNavigating(), model.game.hasPreviousMove(),
        model.isGameOver.load(), model.game.moveCount());

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

    // Clear game-over state when navigating away from the end of a finished game.
    // This allows resuming play (pass, new moves) in Analysis mode.
    if (model.isGameOver) {
        model.start();
    }

    // Keep colorToMove in sync with SGF tree position
    model.state.colorToMove = model.game.getColorToMove();

    // Update comment/markup from current SGF node
    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();

    // Show pass message if we just undid a pass (shows the state we're viewing)
    if (undoneMove == Move::PASS) {
        model.state.msg = (undoneMove.col == Color::BLACK)
            ? GameState::BLACK_PASS
            : GameState::WHITE_PASS;
    } else {
        model.state.msg = GameState::NONE;
    }

    // Build board from SGF (local capture logic, no engine dependency)
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
        model.state.msg = (nextMove.col == Color::BLACK)
            ? GameState::BLACK_PASS
            : GameState::WHITE_PASS;
        spdlog::debug("navigateForward: showing pass message for {}", nextMove.col == Color::BLACK ? "Black" : "White");
    } else {
        model.state.msg = GameState::NONE;
    }

    // Keep colorToMove in sync with SGF tree position
    model.state.colorToMove = model.game.getColorToMove();

    // Update comment/markup from current SGF node
    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();

    // Restore game-over state if we've reached the end of a finished game
    if (model.game.isAtEndOfNavigation() && model.game.hasGameResult()) {
        model.isGameOver = true;
    }

    // Build board from SGF (local capture logic, no engine dependency)
    Board boardResult(model.game.getBoardSize());
    buildBoardFromSGF(boardResult);
    spdlog::debug("navigateForward: notifying {} observers, colorToMove={}",
        gameObservers.size(), model.state.colorToMove == Color::BLACK ? "B" : "W");
    notifyBoardChangeWithMove(boardResult, nextMove);
    spdlog::debug("navigateForward: done");

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

    // Keep colorToMove in sync with SGF tree position
    model.state.colorToMove = model.game.getColorToMove();

    // Update comment/markup from current SGF node
    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();

    // Build board from SGF (local capture logic, no engine dependency)
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
    spdlog::debug("navigateToStart: isNavigating={}, hasPrev={}, isGameOver={}, moveCount={}",
        model.game.isNavigating(), model.game.hasPreviousMove(),
        model.isGameOver.load(), model.game.moveCount());

    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToStart: not in navigation mode");
        return false;
    }

    Engine* coach = getCoach();
    if (!coach) {
        spdlog::warn("navigateToStart: no coach engine available");
        return false;
    }

    NavigationGuard guard(navigationInProgress);

    bool success = false;
    while (model.game.hasPreviousMove()) {
        if (!coach->undo()) break;

        syncEngines(Move(), coach, true);
        model.game.undo();
        success = true;
    }

    if (success) {
        model.board.showTerritory = false;
        model.board.showTerritoryAuto = false;

        // Clear game-over state when navigating away from end
        if (model.isGameOver) {
            model.start();
        }

        model.state.colorToMove = model.game.getColorToMove();
        model.state.comment = model.game.getComment();
        model.state.markup = model.game.getMarkup();
        model.state.msg = GameState::NONE;

        // Build board from SGF (local capture logic, no engine dependency)
        Board result(model.game.getBoardSize());
        buildBoardFromSGF(result);
        notifyBoardChange(result);
        spdlog::debug("navigateToStart: at beginning, colorToMove={}",
            model.state.colorToMove == Color::BLACK ? "B" : "W");
    }
    return success;
}

bool GameNavigator::navigateToEnd() {
    if (!model.game.isNavigating()) {
        spdlog::debug("navigateToEnd: not in navigation mode");
        return false;
    }

    Engine* coach = getCoach();
    if (!coach) return false;

    NavigationGuard guard(navigationInProgress);

    bool playedMoves = false;

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
        playedMoves = true;
    }

    // Always show result at end (whether we played moves or were already there)
    model.state.colorToMove = model.game.getColorToMove();
    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();
    model.state.msg = GameState::NONE;

    // Always build stones from SGF (local capture logic, reliable)
    Board boardResult(model.game.getBoardSize());
    buildBoardFromSGF(boardResult);

    // Restore game-over state if at end of a finished game
    if (model.game.isAtEndOfNavigation() && model.game.hasGameResult()) {
        model.isGameOver = true;
    }

    notifyBoardChange(boardResult);

    // Set territory flag after notifyBoardChange — updateStones would overwrite it
    if (model.game.shouldShowTerritory()) {
        model.board.toggleTerritoryAuto(true);
    }

    spdlog::debug("navigateToEnd: at end, move {}/{}, colorToMove={}, playedMoves={}",
        model.game.getViewPosition(), model.game.getLoadedMovesCount(),
        model.state.colorToMove == Color::BLACK ? "B" : "W", playedMoves);

    return true;  // Always return true - we're at the end now
}

void GameNavigator::buildBoardFromSGF(Board& outBoard) const {
    Position koPosition;
    model.game.buildBoardFromMoves(outBoard, koPosition);
    spdlog::debug("buildBoardFromSGF: built board from SGF, koPosition=({},{})",
        koPosition.col(), koPosition.row());
}
