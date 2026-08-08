// Smoke test: proves goban_core links and runs without a graphics context.
#include <doctest/doctest.h>

#include "Board.h"

TEST_CASE("core links and Board constructs without a GL context") {
    Board board(19);
    CHECK(board.getSize() == 19);
}

TEST_CASE("Position SGF coordinates round-trip") {
    // SGF rows count from the top, board rows from the bottom, so the
    // conversion is not symmetric and is worth pinning down.
    const int size = 19;
    for (int col = 0; col < size; ++col) {
        for (int row = 0; row < size; ++row) {
            Position p(col, row);
            Position back = Position::fromSgf(p.toSgf(size), size);
            CHECK(back.col() == col);
            CHECK(back.row() == row);
        }
    }
}
