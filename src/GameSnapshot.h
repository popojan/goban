/** \file
 *  \brief The UI thread's immutable view of the game record (ADR-0006).
 *
 * Plain data, computed by whoever owns the record and published for readers.
 * Produced by `GobanModel::publishSnapshot()`, consumed through
 * `GobanModel::snapshot()`. The struct comment below records why locking the
 * tree was not the answer.
 */
#ifndef GOBAN_GAMESNAPSHOT_H
#define GOBAN_GAMESNAPSHOT_H

#include "Board.h"
#include "GameState.h"

#include <cstddef>
#include <string>
#include <vector>

/** \brief Everything the UI thread needs to know about the game record, as
 *  plain data, computed by whoever owns the record and published for readers.
 *
 * `GameRecord` is an SGF tree that the game thread mutates freely — moves,
 * navigation, branch promotion, loading — and its const accessors take no lock.
 * Reading it from the UI thread is therefore a data race, and not a theoretical
 * one: routing `shouldShowTerritory()` through `GobanControl::uiInputs()`, which
 * runs every frame, segfaulted in `SgfcProperty`'s destructor about one run in
 * six, because `isGameFinished()` builds and destroys a vector of
 * `shared_ptr<ISgfcProperty>` per node.
 *
 * The answer is not to lock the tree. `GameRecord::saveAs()` writes the whole
 * SGF to disk under its mutex and autosave runs after moves, so a UI thread that
 * locked to read `moveCount()` every frame would stall on disk I/O once per
 * save. Instead the owner computes this struct once per position change and
 * publishes it; the UI reads a snapshot and never touches the tree.
 *
 * That also removes a cost, not just a race. `moveCount()` walks to the root and
 * `getLoadedMovesCount()` walks the entire main line — the UI was paying both
 * every frame, at a price proportional to the length of the game.
 *
 * See ADR-0006.
 */
struct GameSnapshot {
    size_t moveCount     = 0;   ///< Tree depth from the root to the cursor.
    size_t viewPosition  = 0;   ///< Same value; kept separate because the two
                                ///< names mean different things to a reader.
    size_t mainLineMoves = 0;   ///< Total moves on the main line.
    size_t variations    = 0;   ///< Children of the current node.

    bool navigating = false;
    bool atEnd      = true;     ///< No continuation ahead of the cursor.
    bool hasResult  = false;    ///< The record carries an RE property.

    /// The position is the end of a game decided on points. False for a
    /// resignation, which records a result without ever scoring the board.
    /// Mirrors GameRecord::shouldShowTerritory(); decides whether Territory is
    /// offered, which is why the UI needs it every frame.
    bool scoredEnd = false;

    GameState::Message resultMessage = GameState::NONE;

    // --- Stage 2: what the command handlers read ----------------------------
    // boardClick(), `pass` and keyPress() all consult the record to decide what
    // a click or a key means before handing the work to the game thread. They
    // run on user input rather than every frame, so the window is narrower than
    // uiInputs() had — but a click landing while the engine plays its move is
    // exactly the case, and it is not rare.

    /// Whose turn it is at the cursor, for the move a click would create.
    Color colorToMove = Color::BLACK;

    /// Children of the current node: the moves already recorded from here. A
    /// click matching one of these follows it instead of branching. `variations`
    /// above is the count of these, kept separate because scenarios assert on it.
    std::vector<Move> variationMoves;

    /// Some node between the root and the cursor is marked BM. Tsumego uses it
    /// to decide whether the player is on a dead branch.
    bool onBadMovePath = false;

    /// The cursor sits at a game-ending position — a resignation, a double
    /// pass, or the end of a line carrying a result.
    bool atFinishedGame = false;

    /// Games in the loaded SGF collection, for prev_game / next_game.
    size_t loadedGameCount = 0;

    // --- Stage 3: what the annotation display reads --------------------------
    // Written by the game thread in GameNavigator::syncStateAfterNavigation()
    // (and by applyTsumegoHint, which is why this is published from
    // model.state rather than re-read from the record — the hint is part of
    // what the user sees). Read by ElementGame::OnUpdate()'s message tail and
    // by GobanView::updateNavigationOverlay().
    //
    // ElementGame guarded its read with the atomic `positionNumber`, which is a
    // correct message-passing edge and makes the write *visible*. It does not
    // make it exclusive: a second navigation while the UI copies the string or
    // walks the vector still races, and for a std::string or std::vector that is
    // a use-after-free rather than a stale value. Publishing them makes the
    // reader's copy immutable, which settles both halves.
    std::string comment;
    std::vector<BoardMarkup> markup;

    // --- Stage 5: the last two GameState strings the UI read directly --------
    // Same argument as `comment` above, and the same writers: both are set by
    // the game thread immediately before the notify that lands in
    // onBoardChange(), so publishing them here costs nothing and removes the
    // last per-frame copy of a std::string the game thread may be reassigning.
    // `state = GameState()` in onBoardSized() reassigns every one of them at
    // once, which is the sharpest version of that race.
    std::string scoringError;         ///< Detail behind GameState::SCORING_FAILED.
    std::string passVariationLabel;   ///< "11b" — the move number a pass gets.

    int boardSize = 0;

    /// Where the position came from. Empty when the game lives in the daily
    /// session document rather than an external file.
    std::string sgfFile;
    int gameIndex = 0;

    // --- Stage 4: what the board overlays read -------------------------------
    // GobanView::updateLastMoveOverlay() and updateNavigationOverlay() run
    // inside Render() — the UI thread, once per repaint. They were the last
    // readers walking the tree: moveCount(), lastStoneMoveIndex(),
    // isNavigating(), getVariations() and getViewPosition(), the last of which
    // allocates a vector of shared_ptr<ISgfcNode> per child on every frame that
    // redraws. Same hazard as the rest of ADR-0006, on the hottest path in the
    // program.

    /// The most recent stone actually placed on the path to the cursor, and its
    /// move number. INVALID when the position has no stone move behind it — a
    /// game at the root, or one whose only moves are passes. Passes are skipped
    /// deliberately: the marker belongs on a stone, and the number shown is the
    /// stone's index, not the tree depth.
    Move lastStoneMove{Move::INVALID, Color::EMPTY};
    size_t lastStoneMoveNumber = 0;

    // --- Stage 5: what the analysis thread reads (ADR-0007) ------------------
    // Continuous analysis follows the review cursor, so it needs the position
    // itself and not merely facts about it. Published here for the same reason
    // as everything above — the analysis thread is a third reader that must
    // never walk the tree — and consumed by AnalysisService, which compares
    // `positionId` and diffs `pathMoves` against what it last sent to its
    // engine.

    /// Bumped on every publish. The analysis thread compares only this, so a
    /// position change is one integer comparison rather than a vector compare.
    /// Wrapping is not a concern at one increment per board change.
    unsigned long long positionId = 0;

    /// Every move from the root to the cursor, in order — `GameRecord::
    /// getPathFromRoot()`. This is what makes the analysis engine reachable in
    /// one `undo` or one `play` when the user presses an arrow key: the service
    /// takes the common prefix with the path it last sent.
    std::vector<Move> pathMoves;

    /// The rest of what putting an engine at this position needs, mirroring
    /// `GameThread::syncEngineToPosition()`. A change in any of these forces a
    /// full reset rather than an incremental diff.
    float komi = 0.0f;
    std::vector<Position> setupBlack;
    std::vector<Position> setupWhite;
};

#endif // GOBAN_GAMESNAPSHOT_H
