#pragma once

#include <Board.h>

struct GameObserver {
    virtual void onGameMove(const Move& move) {}
    //void onGameEnd() {}
};


