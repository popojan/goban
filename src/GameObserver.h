/** \file
 *  \brief The notification interface from the game thread to model and view.
 *
 * Exactly two observers are ever registered, both in ElementGame's constructor:
 * GobanModel, which writes the record and publishes the snapshot, and
 * GobanView, which repaints and plays sounds. Every callback runs **on the game
 * thread**, so an implementation must not touch RmlUi and must not block.
 *
 * `onBoardChange()` is the funnel — moves, all four navigations, SGF load, game
 * switch, scoring and handicap all pass through it, which is why the snapshot is
 * published there. Anything that changes what the UI displays without going
 * through it has to publish for itself.
 */
#pragma once

#include <Board.h>

class GameObserver {

public:
    virtual ~GameObserver() = default;

    /// Visual/audio event: a stone was placed on the board (sound, overlay update)
    /// Called during both gameplay and navigation
    virtual void onStonePlaced(const Move& move) {}

    /// Game state event: a move was played (history, turn change, game-over detection)
    /// Only called during actual gameplay, NOT during navigation
    virtual void onGameMove(const Move& move, const std::string& comment) {}

    virtual void onKomiChange(float newKomi) {}
    virtual void onHandicapChange(const std::vector<Position>& newHandicapStones) {}
    virtual void onBoardSized(int newBoardSize) {}
    virtual void onBoardChange(const Board& board) {}
    virtual void onPlayerChange(int which, const std::string& name) {}
    //void onGameEnd() {}
};


