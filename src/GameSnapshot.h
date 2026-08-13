#ifndef GOBAN_GAMESNAPSHOT_H
#define GOBAN_GAMESNAPSHOT_H

#include "GameState.h"

#include <cstddef>
#include <string>

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

    int boardSize = 0;

    /// Where the position came from. Empty when the game lives in the daily
    /// session document rather than an external file.
    std::string sgfFile;
    int gameIndex = 0;
};

#endif // GOBAN_GAMESNAPSHOT_H
