#include "GameNavigator.h"
#include "GobanModel.h"
#include "player.h"
#include <spdlog/spdlog.h>
#include <algorithm>

GameNavigator::GameNavigator(GobanModel& model, CoachProvider getCoach,
                             const PlayerList& players, ObserverList& observers)
    : model(model), getCoach(std::move(getCoach)), players(players), gameObservers(observers)
{
}

void GameNavigator::syncEngines(const Move& move, Engine* coach, bool isUndo) {
    for (auto player : players) {
        if (player != reinterpret_cast<Player*>(coach)) {
            if (isUndo) {
                player->undo();
            } else {
                player->play(move);
            }
        }
    }
}

void GameNavigator::notifyBoardChange(const Board& result) {
    std::for_each(gameObservers.begin(), gameObservers.end(),
        [&result](GameObserver* observer) { observer->onBoardChange(result); });
}

void GameNavigator::notifyBoardChangeWithMove(const Board& result, const Move& move) {
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

    // Hide territory display when navigating (stale after undo)
    model.board.toggleTerritoryAuto(false);

    // Keep colorToMove in sync with SGF tree position
    model.state.colorToMove = model.game.getColorToMove();

    // Update comment from current SGF node
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

    const Board& result = coach->showboard();
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
        // At end of branch - only show territory if game is actually finished
        if (model.game.isGameFinished()) {
            spdlog::debug("navigateForward: at end of finished game, showing territory");
            Engine* coach = getCoach();
            if (coach) {
                model.board.toggleTerritoryAuto(true);
                model.state.msg = GameState::NONE;  // Clear pass message
                const Board& result = coach->showterritory(true, model.game.lastStoneMove().col);
                notifyBoardChange(result);
                return true;  // We did something - trigger UI update
            }
        }
        spdlog::debug("navigateForward: at end of branch, nothing to do");
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

    spdlog::debug("navigateForward: playing move {}", nextMove.toString());
    if (!coach->play(nextMove)) {
        spdlog::warn("navigateForward: coach->play() failed for move {}", nextMove.toString());
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

    // Update comment from current SGF node
    model.state.comment = model.game.getComment();
    model.state.markup = model.game.getMarkup();

    // Show board update
    const Board& result = coach->showboard();
    spdlog::debug("navigateForward: notifying {} observers, colorToMove={}",
        gameObservers.size(), model.state.colorToMove == Color::BLACK ? "B" : "W");
    notifyBoardChangeWithMove(result, nextMove);
    spdlog::debug("navigateForward: done");

    return true;
}

GameNavigator::VariationResult GameNavigator::navigateToVariation(const Move& move) {
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
    result.newBranch = !model.game.navigateToChild(move);
    if (result.newBranch) {
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
    model.state.markup = model.game.getMarkup();

    // Notify observers
    const Board& boardResult = coach->showboard();
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
        model.board.toggleTerritoryAuto(false);
        model.state.colorToMove = model.game.getColorToMove();
        model.state.comment = model.game.getComment();
        model.state.markup = model.game.getMarkup();
        model.state.msg = GameState::NONE;

        const Board& result = coach->showboard();
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

        syncEngines(nextMove, coach, false);

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
    model.state.markup = model.game.getMarkup();
    model.state.msg = GameState::NONE;

    // Only show territory if at a finished game position (resign or double pass)
    const Board& result = model.game.isGameFinished()
        ? (model.board.toggleTerritoryAuto(true), coach->showterritory(true, model.game.lastStoneMove().col))
        : coach->showboard();
    notifyBoardChange(result);

    spdlog::debug("navigateToEnd: at end, move {}/{}, colorToMove={}, playedMoves={}",
        model.game.getViewPosition(), model.game.getLoadedMovesCount(),
        model.state.colorToMove == Color::BLACK ? "B" : "W", playedMoves);

    return true;  // Always return true - we're at the end now
}
