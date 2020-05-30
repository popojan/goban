#ifndef GOBAN_GAMERECORD_H
#define GOBAN_GAMERECORD_H

#define LIBSGFCPLUSPLUS_STATIC_DEFINE

#include <memory>
#include "Board.h"

#include <ISgfcTreeBuilder.h>
#include <SgfcPlusPlusFactory.h>
#include <SgfcGameType.h>
#include <ISgfcGame.h>

class GameRecord {
public:

    GameRecord(): game(LibSgfcPlusPlus::SgfcPlusPlusFactory::CreateGame()) {
    }

    int moveCount() { return history.size(); }

    void undo() { history.pop_back(); }

    template <class UnaryFunction>
    void replay(UnaryFunction fMove) {
        std::for_each(history.begin(), history.end(), fMove);
    }

    const Move& lastMove() const  { return history.back(); }
    void move(const Move& move) { history.push_back(move); }
    void clear() { history.clear(); }

private:
    std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game;
    std::vector<Move> history;
};

#endif //GOBAN_GAMERECORD_H
