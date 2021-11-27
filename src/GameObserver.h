#pragma once

#include <Board.h>

class GameObserver {

public:
    virtual void onGameMove(const Move& move, const std::string& comment) {}
    virtual void onKomiChange(float newKomi) {}
    virtual void onHandicapChange(const std::vector<Position>& newHandicapStones) {}
    virtual void onBoardSized(int newBoardSize) {}
    virtual void onBoardChange(const Board& board) {}
    virtual void onPlayerChange(int role, const std::string& name) {}
    //void onGameEnd() {}
};


