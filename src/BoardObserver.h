#pragma once

#include <Board.h>

struct BoardObserver {
    virtual void onBoardChange(const Board& board) {}
    //void onGameEnd() {}
};


