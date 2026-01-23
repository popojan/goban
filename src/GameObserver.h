#pragma once

#include <Board.h>

class GameObserver {

public:
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


