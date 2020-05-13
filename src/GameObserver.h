#pragma once

#include <Board.h>

struct GameObserver {
    virtual void onGameMove(const Move& move) {}
    virtual void onKomiChange(float newKomi) {}
    virtual void onHandicapChange(int newHandicap) {}
    virtual void onBoardSized(int newBoardSize) {}
    virtual void onBoardChange(const Board& board) {}
    virtual void onPlayerChange(int role, const std::string& name) {}
    //void onGameEnd() {}
};


