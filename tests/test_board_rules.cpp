// Regression tests for the Go rules implemented in src/Board.cpp: group
// detection, liberty counting, captures, suicide, ko, plus the GTP/SGF
// conversions the rules code is fed from.
//
// findGroup()/countLiberties()/removeGroup() are private, so they are exercised
// through isValidMove() and applyMoveWithCaptures(), which is also how the
// application uses them.
#include <doctest/doctest.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "Board.h"

namespace {

// ---------------------------------------------------------------------------
// Diagram helpers
//
// Diagrams are written with the highest row first, the way a board is drawn:
// line 0 is row (size - 1) and the last line is row 0. 'X' is black, 'O' is
// white, '.' is empty. Spaces are ignored so rows can be spaced out.
// ---------------------------------------------------------------------------

Color colorOf(char c) {
    if (c == 'X') return Color::BLACK;
    if (c == 'O') return Color::WHITE;
    return Color::EMPTY;
}

char charOf(const Color& c) {
    if (c == Color::BLACK) return 'X';
    if (c == Color::WHITE) return 'O';
    return '.';
}

std::string strip(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c != ' ') out.push_back(c);
    }
    return out;
}

std::vector<std::string> emptyDiagram(int size) {
    return std::vector<std::string>(static_cast<size_t>(size),
                                    std::string(static_cast<size_t>(size), '.'));
}

// Places setup stones directly (no capture or legality processing).
void setup(Board& board, const std::vector<std::string>& diagram) {
    const int size = board.getSize();
    REQUIRE(static_cast<int>(diagram.size()) == size);
    for (int i = 0; i < size; ++i) {
        const std::string row = strip(diagram[i]);
        REQUIRE(static_cast<int>(row.size()) == size);
        for (int col = 0; col < size; ++col) {
            board[Position(col, size - 1 - i)].stone = colorOf(row[static_cast<size_t>(col)]);
        }
    }
}

void put(Board& board, const Color& color, const std::vector<Position>& positions) {
    for (const auto& pos : positions) {
        board[pos].stone = color;
    }
}

// Renders the logical board. Deliberately does not use Board::operator<<,
// which is tested separately.
std::string render(const Board& board) {
    const int size = board.getSize();
    std::string out;
    for (int row = size - 1; row >= 0; --row) {
        for (int col = 0; col < size; ++col) {
            out.push_back(charOf(board[Position(col, row)].stone));
        }
        out.push_back('\n');
    }
    return out;
}

std::string expected(const std::vector<std::string>& diagram) {
    std::string out;
    for (const auto& row : diagram) {
        out += strip(row);
        out.push_back('\n');
    }
    return out;
}

// Plays a move the way the game does: legality check first, then apply.
// Returns the number of captured stones, or -1 if the move was rejected.
int playMove(Board& board, int col, int row, const Color& color) {
    const Position pos(col, row);
    if (!board.isValidMove(pos, color)) return -1;
    return board.applyMoveWithCaptures(Move(pos, color));
}

// Position has no toString(), only a stream operator.
std::string gtp(const Position& pos) {
    std::ostringstream oss;
    oss << pos;
    return oss.str();
}

int countInfluence(const Board& board, const Color& color) {
    int count = 0;
    for (int col = 0; col < board.getSize(); ++col) {
        for (int row = 0; row < board.getSize(); ++row) {
            if (board[Position(col, row)].influence == color) ++count;
        }
    }
    return count;
}

// replayMoves() logs a warning when it rejects a move; silence it where that
// rejection is the behaviour under test.
struct QuietLog {
    spdlog::level::level_enum saved = spdlog::default_logger()->level();
    QuietLog() { spdlog::set_level(spdlog::level::off); }
    ~QuietLog() { spdlog::set_level(saved); }
};

}  // namespace

// ---------------------------------------------------------------------------
// Board state basics
// ---------------------------------------------------------------------------

TEST_CASE("a fresh board is empty at every supported size") {
    for (int size : {Board::MIN_BOARD, 13, Board::MAX_BOARD}) {
        CAPTURE(size);
        Board board(size);

        CHECK(board.getSize() == size);
        CHECK(board.stonesOnBoard(Color::BLACK) == 0);
        CHECK(board.stonesOnBoard(Color::WHITE) == 0);
        CHECK(board.capturedCount(Color::BLACK) == 0);
        CHECK(board.capturedCount(Color::WHITE) == 0);
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
        CHECK(render(board) == expected(emptyDiagram(size)));

        Position minPos, maxPos;
        CHECK_FALSE(board.stoneBounds(minPos, maxPos));
    }
}

TEST_CASE("clear() resets stones, captures and ko") {
    Board board(19);
    setup(board, [] {
        auto d = emptyDiagram(19);
        d[0][0] = 'X';   // A19
        d[18][18] = 'O'; // T1
        return d;
    }());
    REQUIRE(playMove(board, 1, 18, Color::WHITE) == 0);  // no capture, just a stone

    // Establish a genuine ko, so that clear() has one to reset. White's lone
    // stone at (3,4) is in atari with its only liberty at (4,4); Black takes
    // there and the capturing stone is itself a lone stone in atari, which is
    // exactly the condition for simple ko.
    put(board, Color::BLACK, {Position(3, 5), Position(2, 4), Position(3, 3)});
    put(board, Color::WHITE, {Position(3, 4), Position(4, 5), Position(4, 3), Position(5, 4)});
    REQUIRE(playMove(board, 4, 4, Color::BLACK) == 1);
    REQUIRE(board.capturedCount(Color::WHITE) == 1);
    REQUIRE(board.getKoPosition() == Position(3, 4));

    board.clear(9);

    CHECK(board.getSize() == 9);
    CHECK(board.capturedCount(Color::BLACK) == 0);
    CHECK(board.capturedCount(Color::WHITE) == 0);
    CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
    CHECK(board.stonesOnBoard(Color::BLACK) == 0);
    CHECK(board.stonesOnBoard(Color::WHITE) == 0);
}

// ---------------------------------------------------------------------------
// Captures
// ---------------------------------------------------------------------------

TEST_CASE("capturing a single stone") {
    Board board(9);

    SUBCASE("in the centre (four liberties)") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            "....X....",
            "...XOX...",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(playMove(board, 4, 3, Color::BLACK) == 1);
        CHECK(render(board) == expected({
            ".........",
            ".........",
            ".........",
            "....X....",
            "...X.X...",
            "....X....",
            ".........",
            ".........",
            ".........",
        }));
        // capturedCount(WHITE) is the number of white stones taken off.
        CHECK(board.capturedCount(Color::WHITE) == 1);
        CHECK(board.capturedCount(Color::BLACK) == 0);
        CHECK(board.stonesOnBoard(Color::WHITE) == 0);
        CHECK(board.stonesOnBoard(Color::BLACK) == 4);
        // No ko: the capturing stone has four liberties, so White retaking at
        // E5 could not repeat the position — it would simply be suicide. Simple
        // ko requires the capturing stone to be a lone stone left in atari.
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
        CHECK_FALSE(board.isValidMove(Position(4, 4), Color::WHITE));  // suicide
    }

    SUBCASE("on the edge (three liberties)") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            "X........",
            "O........",
            "X........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(playMove(board, 1, 4, Color::BLACK) == 1);
        CHECK(board[Position(0, 4)].stone == Color::EMPTY);
        CHECK(board.capturedCount(Color::WHITE) == 1);
        // Not a ko for the same reason: retaking A5 would be suicide.
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
        CHECK_FALSE(board.isValidMove(Position(0, 4), Color::WHITE));
    }

    SUBCASE("in the corner (two liberties)") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "OX.......",
        });
        CHECK(playMove(board, 0, 1, Color::BLACK) == 1);
        CHECK(board[Position(0, 0)].stone == Color::EMPTY);
        CHECK(board.capturedCount(Color::WHITE) == 1);
        // Not a ko: retaking the corner would be suicide.
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
        CHECK_FALSE(board.isValidMove(Position(0, 0), Color::WHITE));
    }
}

TEST_CASE("capturing a multi-stone group") {
    Board board(9);

    SUBCASE("three stones in a row") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            "...XXX...",
            "..XOOOX..",
            "...XX....",
            ".........",
            ".........",
            ".........",
        });
        CHECK(playMove(board, 5, 3, Color::BLACK) == 3);
        CHECK(render(board) == expected({
            ".........",
            ".........",
            ".........",
            "...XXX...",
            "..X...X..",
            "...XXX...",
            ".........",
            ".........",
            ".........",
        }));
        CHECK(board.capturedCount(Color::WHITE) == 3);
        // More than one stone taken, so there is no ko.
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
    }

    SUBCASE("an L-shape wrapped around the corner") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "X........",
            "O........",
            "OOX......",
        });
        CHECK(playMove(board, 1, 1, Color::BLACK) == 3);
        CHECK(render(board) == expected({
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "X........",
            ".X.......",
            "..X......",
        }));
        CHECK(board.capturedCount(Color::WHITE) == 3);
    }

    SUBCASE("a plus shape, leaving an unconnected stone alone") {
        // The lone white stone at C6 only touches the plus diagonally, so it
        // must survive. This pins down findGroup()'s connectivity.
        setup(board, {
            ".........",
            ".........",
            "....X....",
            "..OXOX...",
            "..XOOOX..",
            "...XOX...",
            ".........",
            ".........",
            ".........",
        });
        CHECK(playMove(board, 4, 2, Color::BLACK) == 5);
        CHECK(render(board) == expected({
            ".........",
            ".........",
            "....X....",
            "..OX.X...",
            "..X...X..",
            "...X.X...",
            "....X....",
            ".........",
            ".........",
        }));
        CHECK(board.capturedCount(Color::WHITE) == 5);
        CHECK(board.stonesOnBoard(Color::WHITE) == 1);
    }

    SUBCASE("a ring is not captured while its eye is still empty") {
        setup(board, {
            ".........",
            ".........",
            "...XXX...",
            "..XOOOX..",
            "..XO.OX..",
            "..XOOOX..",
            "...X.X...",
            ".........",
            ".........",
        });
        // Filling the last outside liberty leaves the eye at E5, so the ring
        // survives; only filling the eye takes it.
        CHECK(playMove(board, 4, 2, Color::BLACK) == 0);
        CHECK(board.stonesOnBoard(Color::WHITE) == 8);
        CHECK(board.isValidMove(Position(4, 4), Color::BLACK));
        CHECK(playMove(board, 4, 4, Color::BLACK) == 8);
        CHECK(board.capturedCount(Color::WHITE) == 8);
        CHECK(render(board) == expected({
            ".........",
            ".........",
            "...XXX...",
            "..X...X..",
            "..X.X.X..",
            "..X...X..",
            "...XXX...",
            ".........",
            ".........",
        }));
    }

    SUBCASE("diagonally touching stones are separate groups") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".X.......",
            "X........",
        });
        // A2 is not connected to B2, so filling A1's liberties one at a time
        // must not take B2 with it.
        CHECK(playMove(board, 1, 0, Color::WHITE) == 0);
        CHECK(playMove(board, 0, 1, Color::WHITE) == 1);
        CHECK(board[Position(0, 0)].stone == Color::EMPTY);
        CHECK(board[Position(1, 1)].stone == Color::BLACK);
        CHECK(board.capturedCount(Color::BLACK) == 1);
        CHECK(board.capturedCount(Color::WHITE) == 0);
    }
}

TEST_CASE("one move can capture several distinct groups") {
    Board board(9);

    SUBCASE("two single stones on the edge") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "XXX......",
            "O.OX.....",
        });
        CHECK(playMove(board, 1, 0, Color::BLACK) == 2);
        CHECK(render(board) == expected({
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "XXX......",
            ".X.X.....",
        }));
        CHECK(board.capturedCount(Color::WHITE) == 2);
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
    }

    SUBCASE("two two-stone groups") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "X.X......",
            "OXOX.....",
            "O.OX.....",
        });
        CHECK(playMove(board, 1, 0, Color::BLACK) == 4);
        CHECK(render(board) == expected({
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "X.X......",
            ".X.X.....",
            ".X.X.....",
        }));
        CHECK(board.capturedCount(Color::WHITE) == 4);
    }

    SUBCASE("four groups around a single point") {
        setup(board, {
            ".........",
            ".........",
            "....X....",
            "...XOX...",
            "..XO.OX..",
            "...XOX...",
            "....X....",
            ".........",
            ".........",
        });
        CHECK(playMove(board, 4, 4, Color::BLACK) == 4);
        CHECK(board.capturedCount(Color::WHITE) == 4);
        CHECK(board.stonesOnBoard(Color::WHITE) == 0);
        CHECK(board.stonesOnBoard(Color::BLACK) == 9);
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
    }
}

// ---------------------------------------------------------------------------
// Suicide
// ---------------------------------------------------------------------------

TEST_CASE("suicide is rejected") {
    Board board(9);

    SUBCASE("single stone into a surrounded centre point") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            "....O....",
            "...O.O...",
            "....O....",
            ".........",
            ".........",
            ".........",
        });
        CHECK_FALSE(board.isValidMove(Position(4, 4), Color::BLACK));
        // The same point is of course fine for the surrounding colour.
        CHECK(board.isValidMove(Position(4, 4), Color::WHITE));
    }

    SUBCASE("single stone into a surrounded edge point") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            "O........",
            ".O.......",
            "O........",
            ".........",
            ".........",
            ".........",
        });
        CHECK_FALSE(board.isValidMove(Position(0, 4), Color::BLACK));
    }

    SUBCASE("single stone into a surrounded corner point") {
        setup(board, {
            ".......O.",
            "........O",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "O........",
            ".O.......",
        });
        CHECK_FALSE(board.isValidMove(Position(0, 0), Color::BLACK));
        CHECK_FALSE(board.isValidMove(Position(8, 8), Color::BLACK));
    }

    SUBCASE("connecting two groups that are both in atari") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "O........",
            "XO.......",
            ".XO......",
        });
        // A1 would join A2 and B1 into a group with no liberty at all.
        CHECK_FALSE(board.isValidMove(Position(0, 0), Color::BLACK));
    }

    SUBCASE("legal when it connects to a group that still has liberties") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "X........",
            ".O.......",
        });
        CHECK(board.isValidMove(Position(0, 0), Color::BLACK));
        CHECK(playMove(board, 0, 0, Color::BLACK) == 0);
        CHECK(board[Position(0, 0)].stone == Color::BLACK);
    }

    SUBCASE("an occupied point is never valid") {
        setup(board, {
            ".........",
            ".........",
            ".........",
            ".........",
            "....X....",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK_FALSE(board.isValidMove(Position(4, 4), Color::BLACK));
        CHECK_FALSE(board.isValidMove(Position(4, 4), Color::WHITE));
    }
}

TEST_CASE("a surrounded point is legal when playing there captures") {
    Board board(9);
    // Every neighbour of E5 is white, but the E4 stone is in atari, so black
    // may play E5 and take it.
    setup(board, {
        ".........",
        ".........",
        ".........",
        "....O....",
        "...O.O...",
        "...XOX...",
        "....X....",
        ".........",
        ".........",
    });
    REQUIRE(board.isValidMove(Position(4, 4), Color::BLACK));
    CHECK(board.applyMoveWithCaptures(Move(Position(4, 4), Color::BLACK)) == 1);
    CHECK(board.capturedCount(Color::WHITE) == 1);
    CHECK(board[Position(4, 3)].stone == Color::EMPTY);
    // That capture is a genuine ko: white retaking at E4 would restore the
    // previous position exactly.
    CHECK(board.getKoPosition() == Position(4, 3));
    CHECK_FALSE(board.isValidMove(Position(4, 3), Color::WHITE));
}

// ---------------------------------------------------------------------------
// Ko
// ---------------------------------------------------------------------------

TEST_CASE("ko: immediate recapture is rejected") {
    Board board(9);
    // The textbook ko shape. White plays E5 and takes D5; black may not
    // answer at D5 straight away.
    setup(board, {
        ".........",
        ".........",
        ".........",
        "...OX....",
        "..OX.X...",
        "...OX....",
        ".........",
        ".........",
        ".........",
    });
    REQUIRE(playMove(board, 4, 4, Color::WHITE) == 1);
    REQUIRE(board.capturedCount(Color::BLACK) == 1);
    REQUIRE(board.getKoPosition() == Position(3, 4));

    CHECK_FALSE(board.isValidMove(Position(3, 4), Color::BLACK));

    SUBCASE("the ban lifts after a move elsewhere") {
        REQUIRE(playMove(board, 0, 0, Color::BLACK) == 0);
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));

        CHECK(board.isValidMove(Position(3, 4), Color::BLACK));
        CHECK(playMove(board, 3, 4, Color::BLACK) == 1);
        CHECK(board.capturedCount(Color::WHITE) == 1);
        // ... and now the ban applies the other way round.
        CHECK(board.getKoPosition() == Position(4, 4));
        CHECK_FALSE(board.isValidMove(Position(4, 4), Color::WHITE));
    }

    SUBCASE("the ban lifts after a pass") {
        CHECK(board.applyMoveWithCaptures(Move(Move::PASS, Color::BLACK)) == 0);
        CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
        CHECK(board.isValidMove(Position(3, 4), Color::BLACK));
    }

    SUBCASE("the ko point stays banned until something else is played") {
        CHECK_FALSE(board.isValidMove(Position(3, 4), Color::BLACK));
        CHECK(board.getKoPosition() == Position(3, 4));
    }
}

// Regression test for a fixed ko-detection bug (src/Board.cpp).
//
// applyMoveWithCaptures() used to set koPosition whenever *exactly one* stone
// was captured, without checking that the capturing stone is itself a lone
// stone with a single liberty. In a snapback the capture takes one stone but
// the capturing group is larger, so the recapture is not a repetition and Go
// rules allow it. The board here is the standard snapback: black has thrown a
// stone in at A1, white takes it at B1, and black plays A1 again to take four
// stones.
TEST_CASE("snapback: retaking a bigger group at the same point is legal") {
    Board board(9);
    setup(board, {
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "XXX......",
        "OOOX.....",
        "X.X......",
    });

    // White takes the throw-in stone at A1.
    REQUIRE(playMove(board, 1, 0, Color::WHITE) == 1);
    REQUIRE(board.capturedCount(Color::BLACK) == 1);
    // The white group A2-B2-C2-B1 now has A1 as its only liberty, so A1 is not
    // a ko point at all.
    CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
    CHECK(board.isValidMove(Position(0, 0), Color::BLACK));

    SUBCASE("the capture itself works once legality is out of the way") {
        CHECK(board.applyMoveWithCaptures(Move(Position(0, 0), Color::BLACK)) == 4);
        CHECK(board.capturedCount(Color::WHITE) == 4);
        CHECK(board.stonesOnBoard(Color::WHITE) == 0);
        CHECK(render(board) == expected({
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            "XXX......",
            "...X.....",
            "X.X......",
        }));
    }
}

// Regression test for the user-visible consequence of the ko bug above.
//
// replayMoves() rebuilds the board for every SGF position
// (GameRecord::buildBoardFromMoves), and it aborts the replay at the first move
// it believes to be a ko violation. With the old ko heuristic, a game record
// containing a snapback was reconstructed only up to the snapback — and
// buildBoardFromMoves ignored the return value, so the truncation was silent.
TEST_CASE("replaying a snapback rebuilds the whole sequence") {
    QuietLog quiet;
    Board board(9);
    setup(board, {
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "XXX......",
        "OOOX.....",
        "X.X......",
    });

    const std::vector<Move> moves = {
        Move(Position(1, 0), Color::WHITE),  // takes the throw-in
        Move(Position(0, 0), Color::BLACK),  // snapback, takes four stones
    };
    CHECK(board.replayMoves(moves) == 2);
    CHECK(board.stonesOnBoard(Color::WHITE) == 0);
}

// ---------------------------------------------------------------------------
// Liberties at the edges and in the corners
// ---------------------------------------------------------------------------

TEST_CASE("edge and corner stones have exactly the liberties they should") {
    for (int size : {Board::MIN_BOARD, 13, Board::MAX_BOARD}) {
        CAPTURE(size);
        const int last = size - 1;
        const int mid = size / 2;

        // Each entry is a lone white stone plus the neighbours black has to
        // fill; only the final stone may capture.
        struct Case {
            const char* what;
            Position target;
            std::vector<Position> neighbours;
        };
        const std::vector<Case> cases = {
            {"bottom-left corner", Position(0, 0), {Position(1, 0), Position(0, 1)}},
            {"top-right corner", Position(last, last), {Position(last - 1, last), Position(last, last - 1)}},
            {"left edge", Position(0, mid), {Position(0, mid - 1), Position(0, mid + 1), Position(1, mid)}},
            {"top edge", Position(mid, last), {Position(mid - 1, last), Position(mid + 1, last), Position(mid, last - 1)}},
            {"centre", Position(mid, mid), {Position(mid - 1, mid), Position(mid + 1, mid),
                                            Position(mid, mid - 1), Position(mid, mid + 1)}},
        };

        for (const auto& c : cases) {
            CAPTURE(c.what);
            Board board(size);
            put(board, Color::WHITE, {c.target});

            for (size_t i = 0; i + 1 < c.neighbours.size(); ++i) {
                CAPTURE(i);
                CHECK(playMove(board, c.neighbours[i].col(), c.neighbours[i].row(), Color::BLACK) == 0);
                CHECK(board[c.target].stone == Color::WHITE);
            }
            const Position& last_ = c.neighbours.back();
            CHECK(playMove(board, last_.col(), last_.row(), Color::BLACK) == 1);
            CHECK(board[c.target].stone == Color::EMPTY);
            CHECK(board.capturedCount(Color::WHITE) == 1);
        }
    }
}

TEST_CASE("points outside a smaller board are not neighbours") {
    // ord() always strides by MAX_BOARD, so a 9x9 board still has storage for
    // the points of a 19x19 one. A stale stone in that storage (e.g. left over
    // from a larger game) must not act as a neighbour or a liberty.
    Board board(9);
    put(board, Color::WHITE, {Position(4, 8)});   // E9, on the top edge
    put(board, Color::BLACK, {Position(4, 9)});   // outside the 9x9 board

    REQUIRE(playMove(board, 3, 8, Color::BLACK) == 0);
    REQUIRE(playMove(board, 5, 8, Color::BLACK) == 0);
    // Three liberties for a top-edge stone, so this is the capturing move.
    CHECK(playMove(board, 4, 7, Color::BLACK) == 1);
    CHECK(board[Position(4, 8)].stone == Color::EMPTY);
    CHECK(board.capturedCount(Color::WHITE) == 1);
}

// ---------------------------------------------------------------------------
// Capture accounting
// ---------------------------------------------------------------------------

TEST_CASE("capture counts accumulate per colour") {
    Board board(9);
    setup(board, {
        "OOX......",
        "X........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "OX.....OX",
    });

    CHECK(board.capturedCount(Color::WHITE) == 0);
    CHECK(board.capturedCount(Color::BLACK) == 0);

    // Black takes A1.
    CHECK(playMove(board, 0, 1, Color::BLACK) == 1);
    CHECK(board.capturedCount(Color::WHITE) == 1);
    CHECK(board.capturedCount(Color::BLACK) == 0);

    // White takes the black stone in the opposite corner.
    CHECK(playMove(board, 8, 1, Color::WHITE) == 1);
    CHECK(board.capturedCount(Color::WHITE) == 1);
    CHECK(board.capturedCount(Color::BLACK) == 1);

    // Black takes the two-stone group A9-B9.
    CHECK(playMove(board, 1, 7, Color::BLACK) == 2);
    CHECK(board.capturedCount(Color::WHITE) == 3);
    CHECK(board.capturedCount(Color::BLACK) == 1);
    CHECK_FALSE(static_cast<bool>(board.getKoPosition()));
}

// ---------------------------------------------------------------------------
// replayMoves
// ---------------------------------------------------------------------------

TEST_CASE("replayMoves rebuilds a position") {
    Board board(9);
    const std::vector<Move> moves = {
        Move(Position(4, 4), Color::BLACK),
        Move(Position(4, 5), Color::WHITE),
        Move(Position(3, 4), Color::BLACK),
        Move(Move::PASS, Color::WHITE),
        Move(Position(4, 3), Color::BLACK),
        Move(Position(8, 8), Color::WHITE),
        Move(Position(5, 4), Color::BLACK),
    };
    CHECK(board.replayMoves(moves) == static_cast<int>(moves.size()));
    CHECK(board.stonesOnBoard(Color::BLACK) == 4);
    CHECK(board.stonesOnBoard(Color::WHITE) == 2);
    CHECK(board.capturedCount(Color::BLACK) == 0);
    CHECK(board.capturedCount(Color::WHITE) == 0);

    SUBCASE("and stops at a move onto an occupied point") {
        QuietLog quiet;
        Board other(9);
        const std::vector<Move> bad = {
            Move(Position(4, 4), Color::BLACK),
            Move(Position(4, 5), Color::WHITE),
            Move(Position(4, 4), Color::WHITE),  // occupied
            Move(Position(0, 0), Color::BLACK),
        };
        CHECK(other.replayMoves(bad) == 2);
        CHECK(other.stonesOnBoard(Color::BLACK) == 1);
        CHECK(other.stonesOnBoard(Color::WHITE) == 1);
        CHECK(other[Position(0, 0)].stone == Color::EMPTY);
    }

    SUBCASE("and replays captures") {
        Board other(9);
        const std::vector<Move> capture = {
            Move(Position(0, 0), Color::WHITE),
            Move(Position(1, 0), Color::BLACK),
            Move(Move::PASS, Color::WHITE),
            Move(Position(0, 1), Color::BLACK),  // takes A1
        };
        CHECK(other.replayMoves(capture) == 4);
        CHECK(other.capturedCount(Color::WHITE) == 1);
        // The capturing stone at A2 keeps three liberties, so this corner
        // capture is not a ko — retaking A1 would be suicide.
        CHECK_FALSE(static_cast<bool>(other.getKoPosition()));
        CHECK(other.stonesOnBoard(Color::WHITE) == 0);
    }
}

// ---------------------------------------------------------------------------
// copyStateFrom
// ---------------------------------------------------------------------------

TEST_CASE("copyStateFrom takes an independent snapshot") {
    Board source(13);
    // A real ko, so that the ko point is part of what gets copied.
    put(source, Color::BLACK, {Position(3, 5), Position(2, 4), Position(3, 3)});
    put(source, Color::WHITE, {Position(3, 4), Position(4, 5), Position(4, 3), Position(5, 4)});
    REQUIRE(playMove(source, 4, 4, Color::BLACK) == 1);
    REQUIRE(source.capturedCount(Color::WHITE) == 1);
    REQUIRE(source.getKoPosition() == Position(3, 4));

    Board copy(9);
    copy.copyStateFrom(source);

    CHECK(copy.getSize() == 13);
    CHECK(render(copy) == render(source));
    CHECK(copy.capturedCount(Color::WHITE) == 1);
    CHECK(copy.capturedCount(Color::BLACK) == 0);
    CHECK(copy.getKoPosition() == Position(3, 4));

    // The copy must not track later changes to the source.
    REQUIRE(playMove(source, 8, 8, Color::WHITE) == 0);
    CHECK(source[Position(8, 8)].stone == Color::WHITE);
    CHECK(copy[Position(8, 8)].stone == Color::EMPTY);
    CHECK_FALSE(static_cast<bool>(source.getKoPosition()));
    CHECK(copy.getKoPosition() == Position(3, 4));
}

// ---------------------------------------------------------------------------
// Territory
// ---------------------------------------------------------------------------

TEST_CASE("calculateTerritoryFromDeadStones") {
    Board board(9);
    setup(board, {
        ".........",
        ".........",
        ".........",
        ".........",
        "XXXXXXXXX",  // row 4
        ".........",  // row 3: dame between the two walls
        "OOOOOOOOO",  // row 2
        ".........",
        ".........",
    });

    SUBCASE("regions enclosed by one colour, shared regions stay neutral") {
        board.calculateTerritoryFromDeadStones({});

        CHECK(countInfluence(board, Color::BLACK) == 36);  // rows 5-8
        CHECK(countInfluence(board, Color::WHITE) == 18);  // rows 0-1
        // Row 3 touches both walls, and the walls themselves are not territory.
        CHECK(board[Position(4, 3)].influence == Color::EMPTY);
        CHECK(board[Position(0, 8)].influence == Color::BLACK);
        CHECK(board[Position(0, 0)].influence == Color::WHITE);
        CHECK(board[Position(0, 4)].influence == Color::EMPTY);
        CHECK(board[Position(0, 2)].influence == Color::EMPTY);
    }

    SUBCASE("a live stone inside a region makes it neutral") {
        put(board, Color::WHITE, {Position(4, 7)});
        board.calculateTerritoryFromDeadStones({});

        CHECK(countInfluence(board, Color::BLACK) == 0);
        CHECK(countInfluence(board, Color::WHITE) == 18);
    }

    SUBCASE("marking that stone dead hands the region over") {
        put(board, Color::WHITE, {Position(4, 7)});
        board.calculateTerritoryFromDeadStones({});
        REQUIRE(countInfluence(board, Color::BLACK) == 0);

        // Recomputing must clear the previous result, not add to it.
        board.calculateTerritoryFromDeadStones({Position(4, 7)});

        CHECK(countInfluence(board, Color::BLACK) == 36);
        CHECK(board[Position(4, 7)].influence == Color::BLACK);
        CHECK(countInfluence(board, Color::WHITE) == 18);
    }

    SUBCASE("clearTerritory undoes it") {
        board.calculateTerritoryFromDeadStones({});
        REQUIRE(countInfluence(board, Color::BLACK) == 36);
        board.clearTerritory();
        CHECK(countInfluence(board, Color::BLACK) == 0);
        CHECK(countInfluence(board, Color::WHITE) == 0);
    }
}

TEST_CASE("territory on an empty board belongs to nobody") {
    Board board(9);
    board.calculateTerritoryFromDeadStones({});
    CHECK(countInfluence(board, Color::BLACK) == 0);
    CHECK(countInfluence(board, Color::WHITE) == 0);
}

// ---------------------------------------------------------------------------
// Position, Move and Color
// ---------------------------------------------------------------------------

TEST_CASE("Position SGF round-trip at every supported size") {
    for (int size : {Board::MIN_BOARD, 13, Board::MAX_BOARD}) {
        CAPTURE(size);
        for (int col = 0; col < size; ++col) {
            for (int row = 0; row < size; ++row) {
                const Position p(col, row);
                const Position back = Position::fromSgf(p.toSgf(size), size);
                REQUIRE(back.col() == col);
                REQUIRE(back.row() == row);
            }
        }
        // SGF rows count from the top, board rows from the bottom.
        CHECK(Position(0, size - 1).toSgf(size) == "aa");
        CHECK(Position(0, 0).toSgf(size) == std::string("a") + static_cast<char>('a' + size - 1));
        CHECK(Position::fromSgf("aa", size) == Position(0, size - 1));
    }
}

TEST_CASE("Position::fromSgf rejects malformed points") {
    CHECK_FALSE(static_cast<bool>(Position::fromSgf("", 19)));
    CHECK_FALSE(static_cast<bool>(Position::fromSgf("a", 19)));
    CHECK_FALSE(static_cast<bool>(Position::fromSgf("aaa", 19)));
    // A point past the bottom of the board yields a negative row, which
    // callers do test for.
    CHECK_FALSE(static_cast<bool>(Position::fromSgf("az", 9)));

    // Regression: fromSgf() used not to check the letters against boardSize, so
    // a point past the right-hand edge came back as a "valid" Position with an
    // off-board column. Callers guard only with col()/row() >= 0 and then index
    // straight into the points array, so "za" on 9x9 was an out-of-bounds write.
    CHECK_FALSE(static_cast<bool>(Position::fromSgf("za", 9)));
}

TEST_CASE("Position GTP round-trip") {
    // GTP column letters skip 'I'.
    CHECK(gtp(Position(0, 0)) == "A1");
    CHECK(gtp(Position(7, 0)) == "H1");
    CHECK(gtp(Position(8, 0)) == "J1");
    CHECK(gtp(Position(18, 18)) == "T19");

    for (int col = 0; col < Board::MAX_BOARD; ++col) {
        for (int row = 0; row < Board::MAX_BOARD; ++row) {
            const Position p(col, row);
            std::istringstream ss(gtp(p));
            Position back;
            ss >> back;
            REQUIRE(back == p);
        }
    }
}

TEST_CASE("Move comparison and conversion operators") {
    SUBCASE("a default move is invalid") {
        const Move m;
        CHECK_FALSE(static_cast<bool>(m));
        CHECK(m == Move::INVALID);
        CHECK(m != Move::NORMAL);
    }

    SUBCASE("a normal move carries its point and colour") {
        const Move m(Position(3, 3), Color::BLACK);
        CHECK(static_cast<bool>(m));
        CHECK(m == Move::NORMAL);
        CHECK(m == Position(3, 3));
        CHECK_FALSE(m == Position(3, 4));
        CHECK(m == Color::BLACK);
        CHECK_FALSE(m == Color::WHITE);
        CHECK(static_cast<Position>(m) == Position(3, 3));
        CHECK(static_cast<Color>(m) == Color::BLACK);
        CHECK(m.toString() == "B D4");
    }

    SUBCASE("pass and resign count as real moves, interrupts do not") {
        CHECK(static_cast<bool>(Move(Move::PASS, Color::WHITE)));
        CHECK(static_cast<bool>(Move(Move::RESIGN, Color::BLACK)));
        CHECK_FALSE(static_cast<bool>(Move(Move::INTERRUPT, Color::BLACK)));
        CHECK_FALSE(static_cast<bool>(Move(Move::KIBITZED, Color::BLACK)));
        CHECK(Move(Move::PASS, Color::WHITE).toString() == "W PASS");
    }
}

TEST_CASE("Move::parseGtp") {
    SUBCASE("vertices") {
        const Move m = Move::parseGtp("= D4", Color::BLACK);
        CHECK(m == Move::NORMAL);
        CHECK(m == Position(3, 3));
        CHECK(m == Color::BLACK);
        CHECK(Move::parseGtp("= A1", Color::WHITE) == Position(0, 0));
        CHECK(Move::parseGtp("= J4", Color::WHITE) == Position(8, 3));
        CHECK(Move::parseGtp("= T19", Color::WHITE) == Position(18, 18));
    }

    SUBCASE("pass and resign, in either case") {
        CHECK(Move::parseGtp("= pass", Color::BLACK) == Move::PASS);
        CHECK(Move::parseGtp("= PASS", Color::BLACK) == Move::PASS);
        CHECK(Move::parseGtp("= resign", Color::WHITE) == Move::RESIGN);
        CHECK(Move::parseGtp("= RESIGN", Color::WHITE) == Move::RESIGN);
    }

    SUBCASE("failed and empty responses") {
        CHECK(Move::parseGtp("? illegal move", Color::BLACK) == Move::INVALID);
        CHECK(Move::parseGtp("", Color::BLACK) == Move::INVALID);
        CHECK(Move::parseGtp("D4", Color::BLACK) == Move::INVALID);
    }

    // SUSPECTED PRODUCTION BUG (src/Board.cpp:51)
    //
    //     if (c >= 'I') j = 7 + c - 'I'; else j = c - 'A';
    //
    // only handles upper-case column letters, yet GTP vertices are
    // case-insensitive and engines do emit lower case. "d4" comes out as
    // column 7 + 'd' - 'I' = 34, which still passes Position's validity test
    // and would be written to points[34*19+3] of a 361-point array.
    // Regression: GTP vertices are case-insensitive, but the column decoder
    // only handled upper case, so "d4" became column 7 + 'd' - 'I' = 34 and
    // still passed Position's validity test.
    SUBCASE("a lower-case vertex") {
        const Move m = Move::parseGtp("= d4", Color::BLACK);
        CHECK(m == Position(3, 3));
        CHECK_FALSE(m.pos.col() >= Board::MAX_BOARD);
    }
}

TEST_CASE("Color") {
    CHECK(Color::other(Color(Color::BLACK)) == Color::WHITE);
    CHECK(Color::other(Color(Color::WHITE)) == Color::BLACK);
    CHECK(Color::other(Color(Color::EMPTY)) == Color::EMPTY);

    CHECK(static_cast<int>(Color(Color::BLACK)) == 0);
    CHECK(static_cast<int>(Color(Color::WHITE)) == 1);
    CHECK(static_cast<int>(Color(Color::EMPTY)) == -1);

    CHECK(Color().toString() == "E");
    CHECK(Color(Color::BLACK).toString() == "B");
    CHECK(Color(Color::WHITE).toString() == "W");

    CHECK(Color() == Color::EMPTY);
    CHECK(Color(Color::BLACK) != Color(Color::WHITE));
    CHECK(Color(Color::BLACK) != Color::EMPTY);
}

// SUSPECTED PRODUCTION BUG (src/Board.cpp:36)
//
//     for (int i = board.boardSize; i != 0; --i)
//         ... board[Position(j, i)] ...
//
// iterates rows boardSize..1 instead of boardSize-1..0: the bottom row is
// never printed and a row one past the top is printed in its place. On a 19x19
// board Position(18, 19) even resolves to point 18*19+19 = 361 of a 361-point
// array, so the operator reads past the end. This test stays on 9x9 to avoid
// provoking that read.
TEST_CASE("streaming a board prints every row") {
    Board board(9);
    put(board, Color::BLACK, {Position(0, 0)});  // bottom-left
    put(board, Color::WHITE, {Position(0, 8)});  // top-left

    std::ostringstream oss;
    oss << board;
    const std::string out = oss.str();

    CHECK(std::count(out.begin(), out.end(), '\n') == 9);
    CHECK(out.find('W') != std::string::npos);
    CHECK(out.find('B') != std::string::npos);
}
