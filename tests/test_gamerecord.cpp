// Tests for GameRecord — SGF game record creation, navigation and persistence.
//
// GameRecord treats the SGF tree as the single source of truth, so nearly every
// behaviour here is observable through hand-written fixtures in tests/data/ plus
// save/reload round trips. The invariants pinned below are the ones documented
// in CLAUDE.md ("SGF Game Record Consistency" and "Game State") plus the ones
// implied by the bug-fix commits in the git log.
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "GameRecord.h"

#ifndef GOBAN_TEST_DATA_DIR
#define GOBAN_TEST_DATA_DIR "tests/data"
#endif

namespace {

// GameRecord logs heavily at info level; silence it so test output is readable.
// Called from every fixture rather than from a static initialiser so that other
// test translation units are not affected before their cases run.
void quietLogging() {
    static bool done = false;
    if (!done) {
        spdlog::set_level(spdlog::level::off);
        done = true;
    }
}

std::string fixture(const std::string& name) {
    std::filesystem::path p(GOBAN_TEST_DATA_DIR);
    p /= name;
    REQUIRE_MESSAGE(std::filesystem::exists(p),
        "fixture not found: " << p.string()
        << " (run the test binary from the repository root)");
    return p.string();
}

std::string todayStamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    tm = *std::localtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

/// Scratch directory plus a GameRecord whose daily-session file points inside
/// it. The session file name carries today's date so that initGame() never
/// takes its "day changed" branch (which would write to ./games and touch
/// UserSettings), and it does not exist, so appendGameToDocument() starts from
/// a fresh document instead of merging the user's real session file.
struct Session {
    std::filesystem::path dir;
    GameRecord rec;

    Session() {
        quietLogging();
        // A counter plus a unique_path-style retry keeps this portable: getpid()
        // is POSIX-only, and these tests should stay runnable on Windows.
        static int counter = 0;
        const auto base = std::filesystem::temp_directory_path();
        do {
            std::ostringstream name;
            name << "goban_gamerecord_test_" << counter++;
            dir = base / name.str();
        } while (std::filesystem::exists(dir));
        std::filesystem::create_directories(dir);
        rec.setDefaultFileName(path(todayStamp() + ".sgf"));
    }

    ~Session() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    std::string path(const std::string& name) const { return (dir / name).string(); }
};

std::string readAll(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

Move blackAt(int col, int row) { return Move(Position(col, row), Color::BLACK); }
Move whiteAt(int col, int row) { return Move(Position(col, row), Color::WHITE); }

/// Walk forward along the main line n times using the public navigation API.
void forward(GameRecord& rec, int n) {
    for (int i = 0; i < n; ++i) {
        REQUIRE(rec.hasNextMove());
        REQUIRE(rec.navigateToChild(rec.getNextMove()));
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// initGame and header properties
// ---------------------------------------------------------------------------

TEST_CASE("initGame establishes an empty game with the requested header") {
    Session s;
    s.rec.initGame(13, 7.5f, 0, "Alice", "Bob");

    CHECK(s.rec.getBoardSize() == 13);
    CHECK(s.rec.moveCount() == 0);
    CHECK(s.rec.getLoadedMovesCount() == 0);
    CHECK(s.rec.getPlayerNames().first == "Alice");
    CHECK(s.rec.getPlayerNames().second == "Bob");
    CHECK(s.rec.isNavigating());          // a game object exists
    CHECK_FALSE(s.rec.hasPreviousMove());
    CHECK_FALSE(s.rec.hasNextMove());
    CHECK(s.rec.isAtEndOfNavigation());
    CHECK_FALSE(s.rec.hasGameResult());
    CHECK_FALSE(s.rec.hasUnsavedChanges());
    CHECK_FALSE(s.rec.hasNewMoves());
    CHECK(s.rec.getColorToMove() == Color::BLACK);
    CHECK(s.rec.lastMove() == Move::INVALID);
    CHECK(s.rec.getComment().empty());
    CHECK(s.rec.getMarkup().empty());
    CHECK(s.rec.getVariations().empty());
    CHECK_FALSE(s.rec.isGameFinished());
    CHECK_FALSE(s.rec.isMainLineFinished());
    CHECK_FALSE(s.rec.isAtFinishedGame());
    CHECK_FALSE(s.rec.shouldShowTerritory());
    CHECK(s.rec.getResultMessage() == GameState::NONE);
}

TEST_CASE("a default-constructed GameRecord has no game and a 1x1 board size") {
    quietLogging();
    GameRecord rec;
    CHECK_FALSE(rec.isNavigating());
    CHECK(rec.moveCount() == 0);
    CHECK(rec.getLoadedGameCount() == 0);
    CHECK(rec.getLoadedGameIndex() == -1);
    CHECK_FALSE(rec.hasLoadedExternalDoc());
    CHECK(rec.getPlayerNames().first == "Black");
    CHECK(rec.getPlayerNames().second == "White");
    // SgfcBoardSize defaults to the SGF minimum of 1x1. Callers must call
    // initGame()/loadFromSGF() before recording moves or the SGF coordinates
    // will be computed against the wrong board size.
    CHECK(rec.getBoardSize() == 1);
}

// Regression: the null-game fallback in move() used to create a game without a
// board size, leaving libsgfc++'s 1x1 default. Position::toSgf(1) then produced
// an off-board point and the library threw straight out of move(), which has no
// try/catch, and therefore out of GobanModel::onGameMove() on the game thread.
//
// Reachable at startup: a record is only created in the no-SGF branch, so if a
// restored session's SGF has moved or been corrupted the load fails while the
// board stays playable, and the first move lands here.
TEST_CASE("move() before initGame creates a usable game") {
    quietLogging();
    GameRecord rec;
    CHECK_NOTHROW(rec.move(blackAt(3, 15)));
    CHECK(rec.getBoardSize() == 19);
    CHECK(rec.moveCount() == 1);
}

TEST_CASE("initGame with a handicap makes white move first") {
    Session s;
    s.rec.initGame(19, 0.5f, 2, "Student", "Teacher");
    // HA > 0 and no AW setup => standard handicap, white plays first.
    CHECK(s.rec.getColorToMove() == Color::WHITE);
}

TEST_CASE("updatePlayers and updateKomi rewrite the root properties") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "Alice", "Bob");
    s.rec.updatePlayers("Carol", "Dave");
    CHECK(s.rec.getPlayerNames().first == "Carol");
    CHECK(s.rec.getPlayerNames().second == "Dave");

    s.rec.updateKomi(7.5f);
    s.rec.move(blackAt(4, 4));
    const std::string out = s.path("komi.sgf");
    s.rec.saveAs(out);

    GameRecord reloaded;
    GameRecord::SGFGameInfo info;
    REQUIRE(reloaded.loadFromSGF(out, info));
    CHECK(info.komi == doctest::Approx(7.5f));
    CHECK(info.blackPlayer == "Carol");
    CHECK(info.whitePlayer == "Dave");
}

// ---------------------------------------------------------------------------
// move / undo / moveCount
// ---------------------------------------------------------------------------

TEST_CASE("move and undo walk the tree depth up and down") {
    Session s;
    s.rec.initGame(19, 6.5f, 0, "B", "W");

    s.rec.move(blackAt(3, 15));   // dd
    CHECK(s.rec.moveCount() == 1);
    CHECK(s.rec.hasNewMoves());
    CHECK(s.rec.hasUnsavedChanges());
    CHECK(s.rec.lastMove() == Move::NORMAL);
    CHECK(s.rec.lastMove().col == Color::BLACK);
    CHECK(s.rec.lastMove().pos == Position(3, 15));
    CHECK(s.rec.getColorToMove() == Color::WHITE);
    CHECK(s.rec.hasPreviousMove());

    s.rec.move(whiteAt(15, 3));   // pp
    CHECK(s.rec.moveCount() == 2);
    CHECK(s.rec.getColorToMove() == Color::BLACK);
    CHECK(s.rec.secondLastMove().pos == Position(3, 15));

    s.rec.undo();
    CHECK(s.rec.moveCount() == 1);
    CHECK(s.rec.lastMove().pos == Position(3, 15));
    // The undone move is still in the tree — undo only moves the cursor.
    CHECK(s.rec.hasNextMove());
    CHECK(s.rec.getNextMove().pos == Position(15, 3));

    s.rec.undo();
    CHECK(s.rec.moveCount() == 0);
    CHECK_FALSE(s.rec.hasPreviousMove());

    // Undo at the root must be a no-op, not a crash or an underflow.
    s.rec.undo();
    s.rec.undo();
    CHECK(s.rec.moveCount() == 0);
    CHECK(s.rec.getLoadedMovesCount() == 2);
}

TEST_CASE("moveCount counts pass moves (regression: pass move numbering)") {
    // Commit 26d1d63 "fix pass move numbering" relies on moveCount() including
    // the pass that was just recorded, because GobanModel derives
    // state.passVariationLabel from it after calling game.move().
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");

    s.rec.move(blackAt(4, 4));
    s.rec.move(whiteAt(2, 2));
    CHECK(s.rec.moveCount() == 2);

    s.rec.move(Move(Move::PASS, Color::BLACK));
    CHECK(s.rec.moveCount() == 3);            // the pass is move #3
    CHECK(s.rec.lastMove() == Move::PASS);
    CHECK(s.rec.getColorToMove() == Color::WHITE);

    s.rec.move(Move(Move::PASS, Color::WHITE));
    CHECK(s.rec.moveCount() == 4);
    CHECK(s.rec.isGameFinished());            // double pass finishes the game
}

TEST_CASE("lastStoneMove and lastStoneMoveIndex skip trailing passes") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    s.rec.move(whiteAt(2, 2));
    s.rec.move(blackAt(6, 6));
    s.rec.move(Move(Move::PASS, Color::WHITE));
    s.rec.move(Move(Move::PASS, Color::BLACK));

    CHECK(s.rec.moveCount() == 5);
    CHECK(s.rec.lastMove() == Move::PASS);

    Move stone = s.rec.lastStoneMove();
    CHECK(stone == Move::NORMAL);
    CHECK(stone.pos == Position(6, 6));
    CHECK(stone.col == Color::BLACK);

    auto [m, idx] = s.rec.lastStoneMoveIndex();
    CHECK(m.pos == Position(6, 6));
    CHECK(idx == 3);    // third move overall, two passes after it

    CHECK(s.rec.countStoneMoves(Color::BLACK) == 2);
    CHECK(s.rec.countStoneMoves(Color::WHITE) == 1);
}

TEST_CASE("resign records RE on the root without adding a node") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    s.rec.move(whiteAt(2, 2));

    s.rec.move(Move(Move::RESIGN, Color::BLACK));
    CHECK(s.rec.moveCount() == 2);            // resignation is not a move node
    CHECK(s.rec.hasGameResult());
    CHECK(s.rec.isResignationResult());
    CHECK(s.rec.isMainLineFinished());        // resignation counts as finished
    CHECK(s.rec.isAtFinishedGame());
    // Black resigned => white wins => RE[W+R] => "black resigned" message.
    CHECK(s.rec.getResultMessage() == GameState::BLACK_RESIGNED);
    // No territory display on a resignation.
    CHECK_FALSE(s.rec.shouldShowTerritory());

    // isGameFinished() only ever inspects the current node's B/W property, and
    // resignation is stored in RE on the root, so it stays false here. The
    // "resign" half of the CLAUDE.md description of isGameFinished() is
    // covered by isAtFinishedGame() instead.
    CHECK_FALSE(s.rec.isGameFinished());
}

TEST_CASE("resigning mid-tree is refused, so a reviewed game keeps its result") {
    // A resignation writes RE on the root and adds no node, so it cannot
    // describe a branch the way a stone or a pass can. Applied while reviewing,
    // it used to relabel the whole game: rewind a game White resigned, press
    // Resign as Black, and RE flipped to W+R while the recorded line still
    // ended in White resigning. The saved SGF then contradicted itself.
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    s.rec.move(whiteAt(2, 2));
    s.rec.move(blackAt(6, 6));
    s.rec.move(Move(Move::RESIGN, Color::WHITE));      // white resigns => RE[B+R]
    REQUIRE(s.rec.getResultMessage() == GameState::WHITE_RESIGNED);

    // Rewind into the game: the cursor now has a continuation ahead of it.
    s.rec.undo();
    s.rec.undo();
    REQUIRE(s.rec.moveCount() == 1);
    REQUIRE_FALSE(s.rec.isAtEndOfNavigation());

    s.rec.move(Move(Move::RESIGN, Color::BLACK));      // would have written W+R

    CHECK(s.rec.getResultMessage() == GameState::WHITE_RESIGNED);   // untouched
    CHECK(s.rec.moveCount() == 1);                                  // no node added
    CHECK(s.rec.getLoadedMovesCount() == 3);                        // tree intact
}

TEST_CASE("resigning at the end of a branch is still allowed") {
    // The guard is about having a continuation ahead, not about the game
    // already having a result. Rewind, play a new line, resign in it.
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    s.rec.move(whiteAt(2, 2));
    s.rec.undo();
    s.rec.move(whiteAt(7, 7));                 // a branch; now at its end
    REQUIRE(s.rec.isAtEndOfNavigation());

    s.rec.move(Move(Move::RESIGN, Color::BLACK));
    CHECK(s.rec.getResultMessage() == GameState::BLACK_RESIGNED);
    CHECK(s.rec.isResignationResult());
}

TEST_CASE("finalizeGame writes a score result and never overwrites a resignation") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));

    s.rec.finalizeGame(3.5f);
    CHECK(s.rec.hasGameResult());
    CHECK_FALSE(s.rec.isResignationResult());
    CHECK(s.rec.getResultMessage() == GameState::BLACK_WON);

    s.rec.finalizeGame(-2.5f);
    CHECK(s.rec.getResultMessage() == GameState::WHITE_WON);

    s.rec.move(Move(Move::RESIGN, Color::WHITE));   // RE[B+R]
    CHECK(s.rec.isResignationResult());
    CHECK(s.rec.getResultMessage() == GameState::WHITE_RESIGNED);

    s.rec.finalizeGame(9.5f);                       // must not clobber +R
    CHECK(s.rec.isResignationResult());
    CHECK(s.rec.getResultMessage() == GameState::WHITE_RESIGNED);
}

TEST_CASE("annotate appends to the current node's comment") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    s.rec.annotate("first");
    CHECK(s.rec.getComment() == "first ");
    s.rec.annotate("second");
    CHECK(s.rec.getComment() == "first second ");
}

// ---------------------------------------------------------------------------
// save -> load round trip
// ---------------------------------------------------------------------------

TEST_CASE("save/load round trip preserves header, moves and comments") {
    Session s;
    s.rec.initGame(13, 7.5f, 0, "Alice", "Bob");
    s.rec.move(blackAt(3, 9));
    s.rec.annotate("opening");
    s.rec.move(whiteAt(9, 3));
    s.rec.move(Move(Move::PASS, Color::BLACK));
    s.rec.move(whiteAt(6, 6));
    s.rec.annotate("tenuki");

    const std::string out = s.path("roundtrip.sgf");
    s.rec.saveAs(out);
    CHECK_FALSE(s.rec.hasUnsavedChanges());
    REQUIRE(std::filesystem::exists(out));

    GameRecord reloaded;
    GameRecord::SGFGameInfo info;
    REQUIRE(reloaded.loadFromSGF(out, info));

    CHECK(info.boardSize == 13);
    CHECK(info.komi == doctest::Approx(7.5f));
    CHECK(info.handicap == 0);
    CHECK(info.blackPlayer == "Alice");
    CHECK(info.whitePlayer == "Bob");
    CHECK(reloaded.getBoardSize() == 13);
    CHECK(reloaded.moveCount() == 4);
    CHECK(reloaded.getLoadedMovesCount() == 4);
    // annotate() appends a trailing space; SGFC trims trailing whitespace from
    // SimpleText values on write, so the reloaded comment has none.
    CHECK(reloaded.getComment() == "tenuki");
    CHECK(reloaded.lastMove().pos == Position(6, 6));
    CHECK(reloaded.lastMove().col == Color::WHITE);
    CHECK(reloaded.countStoneMoves(Color::BLACK) == 1);
    CHECK(reloaded.countStoneMoves(Color::WHITE) == 2);

    // Replay the whole main line and compare against what we recorded.
    std::vector<Move> replayed;
    reloaded.replay([&](const Move& m) { replayed.push_back(m); });
    REQUIRE(replayed.size() == 4);
    CHECK(replayed[0].col == Color::BLACK);
    CHECK(replayed[0].pos == Position(3, 9));
    CHECK(replayed[1].col == Color::WHITE);
    CHECK(replayed[1].pos == Position(9, 3));
    CHECK(replayed[2] == Move::PASS);
    CHECK(replayed[2].col == Color::BLACK);
    CHECK(replayed[3].pos == Position(6, 6));
}

TEST_CASE("non-ASCII player names and comments survive a round trip") {
    Session s;
    s.rec.initGame(19, 6.5f, 0, u8"黑棋", u8"Bílý");
    s.rec.move(blackAt(3, 15));
    s.rec.annotate(u8"překvapení");

    const std::string out = s.path(u8"české_partie.sgf");
    s.rec.saveAs(out);
    REQUIRE(std::filesystem::exists(out));

    GameRecord reloaded;
    GameRecord::SGFGameInfo info;
    REQUIRE(reloaded.loadFromSGF(out, info));
    CHECK(info.blackPlayer == u8"黑棋");
    CHECK(info.whitePlayer == u8"Bílý");
    CHECK(reloaded.getComment() == u8"překvapení");
}

TEST_CASE("saveAs writes nothing when no move has been recorded") {
    Session s;
    s.rec.initGame(19, 6.5f, 0, "Alice", "Bob");
    const std::string out = s.path("empty_game.sgf");
    s.rec.saveAs(out);
    // The document is only created when the first move is appended, so an
    // untouched game produces no file at all.
    CHECK_FALSE(std::filesystem::exists(out));
}

TEST_CASE("saveAs rotates backups of an existing file") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));

    const std::string out = s.path("backup.sgf");
    s.rec.saveAs(out);
    CHECK_FALSE(std::filesystem::exists(out + ".bak"));

    s.rec.move(whiteAt(2, 2));
    s.rec.saveAs(out);
    CHECK(std::filesystem::exists(out + ".bak"));
    CHECK_FALSE(std::filesystem::exists(out + ".bak.old"));

    s.rec.move(blackAt(6, 6));
    s.rec.saveAs(out);
    CHECK(std::filesystem::exists(out + ".bak.old"));
}

TEST_CASE("resigned games survive a round trip as a resignation") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    s.rec.move(whiteAt(2, 2));
    s.rec.move(Move(Move::RESIGN, Color::WHITE));   // white resigns => B+R

    const std::string out = s.path("resigned.sgf");
    s.rec.saveAs(out);
    CHECK(readAll(out).find("RE[B+R]") != std::string::npos);

    GameRecord reloaded;
    GameRecord::SGFGameInfo info;
    REQUIRE(reloaded.loadFromSGF(out, info));
    CHECK(reloaded.hasGameResult());
    CHECK(reloaded.isResignationResult());
    CHECK(reloaded.isAtFinishedGame());
    CHECK(info.gameResult.IsValid);
    CHECK(info.gameResult.GameResultType == LibSgfcPlusPlus::SgfcGameResultType::BlackWin);
    CHECK(info.gameResult.WinType == LibSgfcPlusPlus::SgfcWinType::WinByResignation);
}

// ---------------------------------------------------------------------------
// loadFromSGF
// ---------------------------------------------------------------------------

TEST_CASE("loadFromSGF reads a single game and lands on the last move") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("simple.sgf"), info));

    CHECK(info.boardSize == 19);
    CHECK(info.komi == doctest::Approx(6.5f));
    CHECK(info.handicap == 0);
    CHECK(info.blackPlayer == "Alice");
    CHECK(info.whitePlayer == "Bob");
    CHECK(info.setupBlackStones.empty());
    CHECK(info.setupWhiteStones.empty());

    CHECK(rec.getBoardSize() == 19);
    CHECK(rec.moveCount() == 4);
    CHECK(rec.getLoadedMovesCount() == 4);
    CHECK(rec.isAtEndOfNavigation());
    CHECK(rec.hasPreviousMove());
    CHECK_FALSE(rec.hasNextMove());
    CHECK(rec.lastMove().col == Color::WHITE);
    CHECK(rec.lastMove().pos == Position(15, 15));   // pd
    CHECK(rec.getColorToMove() == Color::BLACK);
    CHECK(rec.getPlayerNames().first == "Alice");
    CHECK_FALSE(rec.hasNewMoves());
    CHECK_FALSE(rec.hasUnsavedChanges());
    CHECK(rec.getLoadedFilePath() == fixture("simple.sgf"));
    CHECK(rec.hasLoadedExternalDoc());
    CHECK(rec.getLoadedGameCount() == 1);
    CHECK(rec.getLoadedGameIndex() == 0);
}

TEST_CASE("startAtRoot leaves the cursor before the first move") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("simple.sgf"), info, 0, /*startAtRoot=*/true));

    CHECK(rec.moveCount() == 0);
    CHECK(rec.getLoadedMovesCount() == 4);
    CHECK_FALSE(rec.hasPreviousMove());
    CHECK(rec.hasNextMove());
    CHECK_FALSE(rec.isAtEndOfNavigation());
    CHECK(rec.lastMove() == Move::INVALID);
    CHECK(rec.getColorToMove() == Color::BLACK);
    CHECK(rec.getNextMove().pos == Position(3, 15));   // dd

    forward(rec, 4);
    CHECK(rec.moveCount() == 4);
    CHECK(rec.isAtEndOfNavigation());
}

TEST_CASE("loadFromSGF honours gameIndex on a multi-game file") {
    quietLogging();
    const std::string file = fixture("two_games.sgf");

    SUBCASE("first game") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        REQUIRE(rec.loadFromSGF(file, info, 0));
        CHECK(info.boardSize == 9);
        CHECK(info.komi == doctest::Approx(0.5f));
        CHECK(info.blackPlayer == "FirstBlack");
        CHECK(rec.moveCount() == 2);
        CHECK(rec.getLoadedGameCount() == 2);
        CHECK(rec.getLoadedGameIndex() == 0);
    }

    SUBCASE("second game") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        REQUIRE(rec.loadFromSGF(file, info, 1));
        CHECK(info.boardSize == 13);
        CHECK(info.komi == doctest::Approx(5.5f));
        CHECK(info.blackPlayer == "SecondBlack");
        CHECK(info.whitePlayer == "SecondWhite");
        CHECK(rec.moveCount() == 3);
        CHECK(rec.getLoadedGameIndex() == 1);
    }

    SUBCASE("index -1 means last game") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        REQUIRE(rec.loadFromSGF(file, info, -1));
        CHECK(info.boardSize == 13);
        CHECK(rec.getLoadedGameIndex() == 1);
    }

    SUBCASE("out of range index fails cleanly") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        CHECK_FALSE(rec.loadFromSGF(file, info, 2));
        CHECK_FALSE(rec.isNavigating());     // nothing was loaded
    }

    SUBCASE("switchToGame cycles within the loaded document") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        REQUIRE(rec.loadFromSGF(file, info, 0));
        REQUIRE(rec.switchToGame(1, info));
        CHECK(info.boardSize == 13);
        CHECK(rec.getBoardSize() == 13);
        CHECK(rec.moveCount() == 3);
        CHECK(rec.getLoadedGameIndex() == 1);
        CHECK_FALSE(rec.switchToGame(5, info));
        CHECK_FALSE(rec.switchToGame(-1, info));
        CHECK(rec.getLoadedGameIndex() == 1);   // unchanged after failure
    }
}

TEST_CASE("peekBoardSize reads the last game's board size without a full load") {
    quietLogging();
    CHECK(GameRecord::peekBoardSize(fixture("simple.sgf")) == 19);
    CHECK(GameRecord::peekBoardSize(fixture("two_games.sgf")) == 13);  // last game
    CHECK(GameRecord::peekBoardSize(fixture("handicap.sgf")) == 9);
    CHECK(GameRecord::peekBoardSize(fixture("no_properties.sgf")) == 19);  // default
    CHECK(GameRecord::peekBoardSize("does/not/exist.sgf") == -1);
    CHECK(GameRecord::peekBoardSize(fixture("malformed.sgf")) == -1);
}

TEST_CASE("readFileContent / writeFileContent handle non-ASCII paths") {
    Session s;
    const std::string p = s.path(u8"日本の囲碁.sgf");
    REQUIRE(GameRecord::writeFileContent(p, "(;FF[4]GM[1]SZ[9];B[ee])"));
    auto content = GameRecord::readFileContent(p);
    REQUIRE(content.has_value());
    CHECK(*content == "(;FF[4]GM[1]SZ[9];B[ee])");
    CHECK_FALSE(GameRecord::readFileContent(s.path("missing.sgf")).has_value());
}

// ---------------------------------------------------------------------------
// Malformed input
// ---------------------------------------------------------------------------

TEST_CASE("malformed and empty SGF input fails cleanly") {
    quietLogging();

    SUBCASE("garbage text") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        CHECK_FALSE(rec.loadFromSGF(fixture("malformed.sgf"), info));
        CHECK_FALSE(rec.isNavigating());
        // Querying a record that failed to load must not crash.
        CHECK(rec.moveCount() == 0);
        CHECK(rec.getVariations().empty());
        CHECK(rec.getComment().empty());
        CHECK(rec.getMarkup().empty());
        CHECK(rec.lastMove() == Move::INVALID);
        CHECK(rec.getColorToMove() == Color::BLACK);
        CHECK_FALSE(rec.hasGameResult());
        CHECK_FALSE(rec.hasNextMove());
        CHECK_FALSE(rec.hasPreviousMove());
        CHECK_FALSE(rec.isBadMove());
        CHECK_FALSE(rec.isOnBadMovePath());
        CHECK(rec.getTreePath().length == 0);
        CHECK_FALSE(rec.navigateToTreePath(3, {0}));
        Board b(19);
        Position ko;
        rec.buildBoardFromMoves(b, ko);   // must not crash
        CHECK(b.stonesOnBoard(Color::BLACK) == 0);
    }

    SUBCASE("empty file") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        CHECK_FALSE(rec.loadFromSGF(fixture("empty.sgf"), info));
        CHECK_FALSE(rec.isNavigating());
    }

    SUBCASE("missing file") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        CHECK_FALSE(rec.loadFromSGF("no/such/file.sgf", info));
        CHECK_FALSE(rec.isNavigating());
    }

    SUBCASE("a failed load leaves a previously loaded game intact") {
        GameRecord rec;
        GameRecord::SGFGameInfo info;
        REQUIRE(rec.loadFromSGF(fixture("simple.sgf"), info));
        CHECK_FALSE(rec.loadFromSGF("no/such/file.sgf", info));
        CHECK(rec.moveCount() == 4);
        CHECK(rec.getBoardSize() == 19);
    }
}

TEST_CASE("setup stones outside the board do not corrupt the board array") {
    // AB[sa]/AB[ss] name points on a 19x19 grid but the game declares SZ[9].
    // Whatever the parser does with them, GameRecord must not write outside the
    // logical board when rebuilding the position.
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    const bool loaded = rec.loadFromSGF(fixture("bad_setup.sgf"), info, 0, true);
    if (!loaded) {
        // Acceptable: the parser rejected the file outright.
        CHECK_FALSE(rec.isNavigating());
        return;
    }
    CHECK(info.boardSize == 9);
    for (const auto& p : info.setupBlackStones) {
        CHECK(p.col() < 9);
        CHECK(p.row() < 9);
    }
    Board board(9);
    Position ko;
    rec.buildBoardFromMoves(board, ko);
    CHECK(board.stonesOnBoard(Color::BLACK) <= 2);
    CHECK(board.stonesOnBoard(Color::WHITE) == 1);
}

TEST_CASE("extractGameInfo falls back to defaults for missing properties") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("no_properties.sgf"), info));
    CHECK(info.boardSize == 19);
    CHECK(info.komi == doctest::Approx(6.5f));
    CHECK(info.handicap == 0);
    CHECK(info.blackPlayer == "Black");
    CHECK(info.whitePlayer == "White");
    CHECK_FALSE(info.gameResult.IsValid);
    CHECK(rec.moveCount() == 2);
    // aa is the top-left corner: col 0, row boardSize-1.
    std::vector<Move> path;
    rec.replay([&](const Move& m) { path.push_back(m); });
    REQUIRE(path.size() == 2);
    CHECK(path[0].pos == Position(0, 18));
    CHECK(path[1].pos == Position(1, 17));
}

// ---------------------------------------------------------------------------
// Handicap and setup stones
// ---------------------------------------------------------------------------

TEST_CASE("handicap SGF exposes AB stones and makes white move first") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("handicap.sgf"), info, 0, /*startAtRoot=*/true));

    CHECK(info.boardSize == 9);
    CHECK(info.handicap == 2);
    CHECK(info.blackPlayer == "Student");
    REQUIRE(info.setupBlackStones.size() == 2);
    CHECK(info.setupWhiteStones.empty());
    // AB[cg] -> col 2, row 9-6-1 = 2 ; AB[gc] -> col 6, row 6
    std::vector<Position> stones = info.setupBlackStones;
    std::sort(stones.begin(), stones.end(),
              [](const Position& a, const Position& b) { return a.col() < b.col(); });
    CHECK(stones[0] == Position(2, 2));
    CHECK(stones[1] == Position(6, 6));

    // Standard handicap convention: HA without AW means white starts.
    CHECK(rec.getColorToMove() == Color::WHITE);
    CHECK_FALSE(GameRecord::isTsumego(info, rec.getLoadedMovesCount()));

    Board board(9);
    Position ko;
    rec.buildBoardFromMoves(board, ko);
    CHECK(board.stonesOnBoard(Color::BLACK) == 2);
    CHECK(board.stonesOnBoard(Color::WHITE) == 0);
    CHECK(board[Position(2, 2)].stone == Color::BLACK);
    CHECK(board[Position(6, 6)].stone == Color::BLACK);
}

TEST_CASE("setHandicapStones adds AB to the root exactly once") {
    Session s;
    s.rec.initGame(9, 0.5f, 2, "Student", "Teacher");
    s.rec.setHandicapStones({Position(2, 2), Position(6, 6)});
    s.rec.setHandicapStones({Position(4, 4)});   // must be ignored, AB exists
    s.rec.setHandicapStones({});                 // empty is a no-op

    s.rec.move(whiteAt(4, 4));
    const std::string out = s.path("handicap_out.sgf");
    s.rec.saveAs(out);

    GameRecord reloaded;
    GameRecord::SGFGameInfo info;
    REQUIRE(reloaded.loadFromSGF(out, info, 0, /*startAtRoot=*/true));
    CHECK(info.handicap == 2);
    REQUIRE(info.setupBlackStones.size() == 2);
    CHECK(reloaded.getColorToMove() == Color::WHITE);

    Board board(9);
    Position ko;
    reloaded.buildBoardFromMoves(board, ko);
    CHECK(board.stonesOnBoard(Color::BLACK) == 2);
}

TEST_CASE("tsumego SGF exposes both AB and AW and honours PL") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("tsumego.sgf"), info, 0, /*startAtRoot=*/true));

    CHECK(info.boardSize == 9);
    CHECK(info.setupBlackStones.size() == 4);
    CHECK(info.setupWhiteStones.size() == 5);
    CHECK(GameRecord::isTsumego(info, rec.getLoadedMovesCount()));
    // PL[B] wins over any other heuristic.
    CHECK(rec.getColorToMove() == Color::BLACK);

    Board board(9);
    Position ko;
    rec.buildBoardFromMoves(board, ko);
    CHECK(board.stonesOnBoard(Color::BLACK) == 4);
    CHECK(board.stonesOnBoard(Color::WHITE) == 5);
}

TEST_CASE("AB is decoded against SZ even when AB is written first") {
    // extractGameInfo() decodes AB/AW points with Position::fromSgf(pt,
    // gameInfo.boardSize), and boardSize is only correct once the SZ property
    // has been seen. If the SGF writes AB before SZ, a naive single pass over
    // GetProperties() in document order would decode the stones against the
    // default board size of 19 and place them off a 9x9 board.
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("ab_before_sz.sgf"), info, 0, /*startAtRoot=*/true));
    CHECK(info.boardSize == 9);
    REQUIRE(info.setupBlackStones.size() == 2);
    for (const auto& p : info.setupBlackStones) {
        CHECK(p.col() >= 0);
        CHECK(p.col() < 9);
        CHECK(p.row() >= 0);
        CHECK(p.row() < 9);
    }
    std::vector<Position> stones = info.setupBlackStones;
    std::sort(stones.begin(), stones.end(),
              [](const Position& a, const Position& b) { return a.col() < b.col(); });
    CHECK(stones[0] == Position(2, 2));
    CHECK(stones[1] == Position(6, 6));
}

TEST_CASE("isTsumego needs both colours of setup stones") {
    GameRecord::SGFGameInfo info;
    info.setupBlackStones = {Position(0, 0)};
    CHECK_FALSE(GameRecord::isTsumego(info, 3));          // handicap-like, AB only
    info.setupWhiteStones = {Position(1, 1)};
    CHECK(GameRecord::isTsumego(info, 3));
    CHECK(GameRecord::isTsumego(info, 50));
    CHECK_FALSE(GameRecord::isTsumego(info, 51));         // too long for a problem
}

TEST_CASE("FF[3] setup node before the first move is treated as the root") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("ff3_setup.sgf"), info, 0, /*startAtRoot=*/true));

    // AB/AW live on the child node, not on the root: extractGameInfo must scan
    // through to the effective root to find them.
    CHECK(info.setupBlackStones.size() == 2);
    CHECK(info.setupWhiteStones.size() == 2);
    CHECK(info.boardSize == 19);

    // The setup node counts as the root, so depth is 0 and there is no
    // "previous move" to navigate back to.
    CHECK(rec.moveCount() == 0);
    CHECK_FALSE(rec.hasPreviousMove());
    CHECK(rec.hasNextMove());
    CHECK(rec.getLoadedMovesCount() == 2);
    // PL[2] (FF[3] numeric form) means white to move.
    CHECK(rec.getColorToMove() == Color::WHITE);

    Board board(19);
    Position ko;
    rec.buildBoardFromMoves(board, ko);
    CHECK(board.stonesOnBoard(Color::BLACK) == 2);
    CHECK(board.stonesOnBoard(Color::WHITE) == 2);
}

// ---------------------------------------------------------------------------
// Board reconstruction
// ---------------------------------------------------------------------------

TEST_CASE("buildBoardFromMoves applies captures") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("capture.sgf"), info));
    REQUIRE(rec.moveCount() == 3);

    Board board(9);
    Position ko;
    rec.buildBoardFromMoves(board, ko);

    // B[ba] W[aa] B[ab] captures the lone white corner stone at aa = (0, 8).
    CHECK(board.stonesOnBoard(Color::BLACK) == 2);
    CHECK(board.stonesOnBoard(Color::WHITE) == 0);
    CHECK(board[Position(0, 8)].stone == Color::EMPTY);
    // Not a ko: the capturing stone at ab keeps three liberties, so White
    // retaking the corner would be suicide rather than a repetition.
    CHECK_FALSE(static_cast<bool>(ko));

    // captured = stones played - stones remaining
    CHECK(rec.countStoneMoves(Color::WHITE) == 1);
    CHECK(rec.countStoneMoves(Color::WHITE) - board.stonesOnBoard(Color::WHITE) == 1);

    // Rewinding one move puts the white stone back.
    rec.undo();
    Board earlier(9);
    rec.buildBoardFromMoves(earlier, ko);
    CHECK(earlier.stonesOnBoard(Color::WHITE) == 1);
    CHECK(earlier.stonesOnBoard(Color::BLACK) == 1);
}

// ---------------------------------------------------------------------------
// Variations and branching
// ---------------------------------------------------------------------------

TEST_CASE("getVariations lists every child of the current node") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("variations.sgf"), info, 0, /*startAtRoot=*/true));

    CHECK(rec.getLoadedMovesCount() == 4);   // main line: dd pp dp pd
    CHECK(rec.getVariations().size() == 1);

    forward(rec, 2);                          // at W[pp]
    auto vars = rec.getVariations();
    REQUIRE(vars.size() == 3);
    CHECK(vars[0].pos == Position(3, 3));     // dp  (main line)
    CHECK(vars[1].pos == Position(15, 15));   // pd
    CHECK(vars[2].pos == Position(9, 9));     // jj
    for (const auto& v : vars) CHECK(v.col == Color::BLACK);
    CHECK(rec.getNextMove().pos == Position(3, 3));   // first child is main line

    SUBCASE("navigating into a side variation") {
        REQUIRE(rec.navigateToChild(vars[1]));
        CHECK(rec.moveCount() == 3);
        CHECK(rec.lastMove().pos == Position(15, 15));
        CHECK(rec.getNextMove().pos == Position(3, 3));   // W[dp] follows
        CHECK_FALSE(rec.isAtEndOfNavigation());
    }

    SUBCASE("navigateToChild rejects a move that is not a child") {
        CHECK_FALSE(rec.navigateToChild(blackAt(0, 0)));
        CHECK(rec.moveCount() == 2);   // cursor unchanged
    }

    SUBCASE("a dead-end variation ends navigation") {
        REQUIRE(rec.navigateToChild(vars[2]));   // B[jj]
        CHECK(rec.isAtEndOfNavigation());
        CHECK(rec.getVariations().empty());
        CHECK(rec.getNextMove() == Move::INVALID);
    }
}

TEST_CASE("promoteToMainLine is inert until the user has added moves") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("variations.sgf"), info, 0, /*startAtRoot=*/true));
    forward(rec, 2);
    auto vars = rec.getVariations();
    REQUIRE(vars.size() == 3);

    // navigateToChild only reorders children when gameHasNewMoves is set, so
    // simply reviewing a variation of a loaded game leaves the main line alone.
    REQUIRE(rec.navigateToChild(vars[1], /*promoteToMainLine=*/true));
    CHECK(rec.getLoadedMovesCount() == 4);
    rec.undo();
    CHECK(rec.getNextMove().pos == Position(3, 3));   // dp still first

    // promoteCurrentPathToMainLine() is the explicit way to do it.
    REQUIRE(rec.navigateToChild(vars[1]));
    rec.promoteCurrentPathToMainLine();
    rec.undo();
    CHECK(rec.getNextMove().pos == Position(15, 15));   // pd promoted
    CHECK(rec.getVariations()[0].pos == Position(15, 15));
}

TEST_CASE("move() inserts a new branch as the main line and drops a stale result") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("double_pass.sgf"), info, 0, /*startAtRoot=*/true));
    REQUIRE(rec.hasGameResult());

    // Branching at a node that already has children invalidates RE, because RE
    // describes the main line only.
    rec.move(blackAt(0, 0));
    CHECK_FALSE(rec.hasGameResult());
    CHECK(rec.moveCount() == 1);
    CHECK(rec.hasNewMoves());
    rec.undo();
    CHECK(rec.getVariations()[0].pos == Position(0, 0));   // new branch is first
    CHECK(rec.getLoadedMovesCount() == 1);
}

TEST_CASE("move(insertAsFirst=false) appends without changing the main line") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("simple.sgf"), info, 0, /*startAtRoot=*/true));

    rec.move(blackAt(0, 0), /*insertAsFirst=*/false);
    CHECK(rec.moveCount() == 1);
    rec.undo();
    auto vars = rec.getVariations();
    REQUIRE(vars.size() == 2);
    CHECK(vars[0].pos == Position(3, 15));   // original dd is still main line
    CHECK(vars[1].pos == Position(0, 0));
    CHECK(rec.getLoadedMovesCount() == 4);
}

TEST_CASE("a pass and the A1 corner are distinct variations") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("pass_vs_corner.sgf"), info, 0, /*startAtRoot=*/true));
    forward(rec, 2);

    auto vars = rec.getVariations();
    REQUIRE(vars.size() == 2);
    CHECK(vars[0] == Move::PASS);
    CHECK(vars[1] == Move::NORMAL);
    CHECK(vars[1].pos == Position(0, 0));   // ai on a 9x9 board is A1
    // Move(PASS, col) leaves pos default-constructed at (0, 0), so a pass and a
    // stone played on A1 carry the same Position.
    CHECK(vars[0].pos == vars[1].pos);

    // Navigating to the pass works because it is the first child.
    REQUIRE(rec.navigateToChild(vars[0]));
    CHECK(rec.getComment() == "pass variation");
}

// SUSPECTED PRODUCTION BUG — see report.
//
// GameRecord::navigateToChild() (GameRecord.cpp:1222) matches a child with
//     childMove->pos == targetMove.pos && childMove->col == targetMove.col
// and never compares the move kind. Move(Move::PASS, col) leaves `pos` at its
// default (0, 0), which is the A1 corner, so at a node that has both a pass
// variation and an A1 variation the first of the two always wins regardless of
// which one was asked for.
//
// Reachable through GameNavigator::navigateToVariation(), i.e. a board click on
// A1 while reviewing a game whose tree contains a pass at that node: the click
// silently follows the pass branch instead. The mirror case (asking for the pass
// and landing on the A1 stone) happens when the A1 child comes first.
//
// Fix shape: compare the move kind too, e.g. add
//     (*childMove == Move::PASS) == (targetMove == Move::PASS)
// to the predicate (getVariations() already returns correctly typed Moves).
TEST_CASE("navigateToChild distinguishes a pass from an A1 stone") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("pass_vs_corner.sgf"), info, 0, /*startAtRoot=*/true));
    forward(rec, 2);

    // Asking for the A1 stone move must not land on the pass variation.
    REQUIRE(rec.navigateToChild(blackAt(0, 0)));
    CHECK(rec.lastMove() == Move::NORMAL);                      // actual: PASS
    CHECK(rec.getComment() == "A1 corner variation");            // actual: "pass variation"
}

TEST_CASE("branchFromFinishedGame copies the path into a fresh unfinished game") {
    Session s;
    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("resign.sgf"), info));
    REQUIRE(s.rec.moveCount() == 3);
    REQUIRE(s.rec.hasGameResult());
    REQUIRE(s.rec.hasLoadedExternalDoc());

    s.rec.branchFromFinishedGame(whiteAt(1, 1));

    CHECK(s.rec.moveCount() == 4);
    CHECK(s.rec.lastMove().pos == Position(1, 1));
    CHECK(s.rec.lastMove().col == Color::WHITE);
    CHECK_FALSE(s.rec.hasGameResult());          // RE dropped: new game is unfinished
    CHECK(s.rec.hasNewMoves());
    CHECK(s.rec.hasUnsavedChanges());
    // The new game belongs to the daily session, not to the loaded file.
    CHECK_FALSE(s.rec.hasLoadedExternalDoc());
    CHECK(s.rec.getLoadedFilePath().empty());
    CHECK(s.rec.getNumGames() == 1);
    // Header of the original game is carried over.
    CHECK(s.rec.getPlayerNames().first == "Black");
    CHECK(s.rec.getBoardSize() == 9);

    // The copy keeps the original moves in order.
    std::vector<Move> path;
    s.rec.replay([&](const Move& m) { path.push_back(m); });
    REQUIRE(path.size() == 4);
    CHECK(path[0].pos == Position(4, 4));   // ee
    CHECK(path[1].pos == Position(2, 6));   // cc
    CHECK(path[2].pos == Position(6, 2));   // gg
    CHECK(path[3].pos == Position(1, 1));   // the new branch move

    // The fixture on disk is untouched.
    CHECK(readAll(fixture("resign.sgf")).find("RE[W+R]") != std::string::npos);
}

TEST_CASE("branchFromFinishedGame at the root produces a one-move game") {
    Session s;
    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("resign.sgf"), info, 0, /*startAtRoot=*/true));
    REQUIRE(s.rec.moveCount() == 0);

    s.rec.branchFromFinishedGame(blackAt(0, 0));
    CHECK(s.rec.moveCount() == 1);
    CHECK(s.rec.getLoadedMovesCount() == 1);
    CHECK_FALSE(s.rec.hasGameResult());
    CHECK(s.rec.lastMove().pos == Position(0, 0));
    CHECK(s.rec.getBoardSize() == 9);
}

// ---------------------------------------------------------------------------
// "Result removed if main line unfinished"
// ---------------------------------------------------------------------------

TEST_CASE("saving a modified game whose main line is unfinished drops RE") {
    Session s;
    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("double_pass.sgf"), info));
    REQUIRE(s.rec.isGameFinished());          // double pass at the end
    REQUIRE(s.rec.isMainLineFinished());
    REQUIRE(s.rec.hasGameResult());

    // Continue past the double pass: the main line no longer ends in a
    // double pass, so the recorded result is no longer meaningful.
    s.rec.move(blackAt(6, 6));
    REQUIRE(s.rec.hasGameResult());           // still present in memory...
    CHECK_FALSE(s.rec.isMainLineFinished());

    const std::string out = s.path("unfinished.sgf");
    s.rec.saveAs(out);
    // ...but saveAs enforces the invariant.
    CHECK_FALSE(s.rec.hasGameResult());
    const std::string text = readAll(out);
    CHECK(text.find("RE[") == std::string::npos);

    GameRecord reloaded;
    GameRecord::SGFGameInfo info2;
    REQUIRE(reloaded.loadFromSGF(out, info2));
    CHECK_FALSE(reloaded.hasGameResult());
    CHECK_FALSE(info2.gameResult.IsValid);
    CHECK(reloaded.moveCount() == 5);
}

TEST_CASE("a main line that still ends in a double pass keeps RE") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    s.rec.move(whiteAt(2, 2));
    s.rec.move(Move(Move::PASS, Color::BLACK));
    s.rec.move(Move(Move::PASS, Color::WHITE));
    s.rec.finalizeGame(3.5f);
    REQUIRE(s.rec.isMainLineFinished());

    const std::string out = s.path("finished.sgf");
    s.rec.saveAs(out);
    CHECK(s.rec.hasGameResult());
    CHECK(readAll(out).find("RE[B+3.5]") != std::string::npos);

    GameRecord reloaded;
    GameRecord::SGFGameInfo info;
    REQUIRE(reloaded.loadFromSGF(out, info));
    CHECK(reloaded.isGameFinished());
    CHECK(reloaded.isMainLineFinished());
    CHECK(reloaded.shouldShowTerritory());     // scored game, not a resignation
    CHECK(reloaded.getResultMessage() == GameState::BLACK_WON);
}

TEST_CASE("a resignation result survives modification because +R wins") {
    Session s;
    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("resign.sgf"), info));
    REQUIRE(s.rec.isMainLineFinished());       // RE contains +R

    s.rec.move(whiteAt(1, 1));                 // extend the main line
    CHECK(s.rec.isMainLineFinished());         // still "finished" via +R
    const std::string out = s.path("still_resigned.sgf");
    s.rec.saveAs(out);
    CHECK(s.rec.hasGameResult());
    CHECK(readAll(out).find("RE[W+R]") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Game state predicates
// ---------------------------------------------------------------------------

TEST_CASE("territory display and finished-game predicates follow the position") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;

    SUBCASE("scored double-pass game") {
        REQUIRE(rec.loadFromSGF(fixture("double_pass.sgf"), info));
        CHECK(rec.moveCount() == 4);
        CHECK(rec.lastMove() == Move::PASS);
        CHECK(rec.isGameFinished());
        CHECK(rec.isAtFinishedGame());
        CHECK(rec.shouldShowTerritory());
        CHECK(rec.getResultMessage() == GameState::BLACK_WON);

        // Territory is only shown at the finished position, not before it.
        rec.undo();
        CHECK_FALSE(rec.isGameFinished());
        CHECK_FALSE(rec.isAtEndOfNavigation());
        CHECK_FALSE(rec.shouldShowTerritory());
        CHECK_FALSE(rec.isAtFinishedGame());
    }

    SUBCASE("resigned game") {
        REQUIRE(rec.loadFromSGF(fixture("resign.sgf"), info));
        CHECK(rec.isAtEndOfNavigation());
        CHECK(rec.isAtFinishedGame());
        CHECK(rec.isResignationResult());
        CHECK_FALSE(rec.shouldShowTerritory());   // never for a resignation
        CHECK(rec.getResultMessage() == GameState::BLACK_RESIGNED);
    }

    SUBCASE("unfinished game") {
        REQUIRE(rec.loadFromSGF(fixture("simple.sgf"), info));
        CHECK(rec.isAtEndOfNavigation());
        CHECK_FALSE(rec.hasGameResult());
        CHECK_FALSE(rec.isGameFinished());
        CHECK_FALSE(rec.isAtFinishedGame());
        CHECK_FALSE(rec.shouldShowTerritory());
        CHECK(rec.getResultMessage() == GameState::NONE);
    }
}

TEST_CASE("a single pass does not finish the game") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    s.rec.move(Move(Move::PASS, Color::WHITE));
    CHECK_FALSE(s.rec.isGameFinished());
    CHECK_FALSE(s.rec.isMainLineFinished());
    s.rec.move(Move(Move::PASS, Color::BLACK));
    CHECK(s.rec.isGameFinished());
    CHECK(s.rec.isMainLineFinished());
}

// ---------------------------------------------------------------------------
// Comments, markup, bad moves
// ---------------------------------------------------------------------------

TEST_CASE("comments and markup are read from the current node only") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("markup.sgf"), info, 0, /*startAtRoot=*/true));

    CHECK(rec.getComment().empty());
    CHECK(rec.getMarkup().empty());

    forward(rec, 1);   // B[dd] with markup
    CHECK(rec.getComment() == "markup node");

    auto markup = rec.getMarkup();
    REQUIRE(markup.size() == 6);

    auto find = [&](MarkupType t) {
        std::vector<BoardMarkup> out;
        for (const auto& m : markup)
            if (m.type == t) out.push_back(m);
        return out;
    };
    auto labels = find(MarkupType::LABEL);
    REQUIRE(labels.size() == 2);
    CHECK(labels[0].pos == Position(15, 3));    // pp
    CHECK(labels[0].label == "A");
    CHECK(labels[1].pos == Position(16, 2));    // qq
    CHECK(labels[1].label == "B");

    REQUIRE(find(MarkupType::TRIANGLE).size() == 1);
    CHECK(find(MarkupType::TRIANGLE)[0].pos == Position(2, 16));   // cc
    REQUIRE(find(MarkupType::SQUARE).size() == 1);
    CHECK(find(MarkupType::SQUARE)[0].pos == Position(3, 16));     // dc
    REQUIRE(find(MarkupType::CIRCLE).size() == 1);
    CHECK(find(MarkupType::CIRCLE)[0].pos == Position(2, 14));     // ce
    REQUIRE(find(MarkupType::MARK).size() == 1);
    CHECK(find(MarkupType::MARK)[0].pos == Position(2, 13));       // cf

    forward(rec, 1);   // W[pp] BM[1]
    CHECK(rec.getComment().empty());
    CHECK(rec.getMarkup().empty());
}

TEST_CASE("BM marks a bad move and taints the rest of the path") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("markup.sgf"), info, 0, /*startAtRoot=*/true));

    CHECK_FALSE(rec.isBadMove());
    CHECK_FALSE(rec.isOnBadMovePath());

    forward(rec, 1);                 // B[dd]
    CHECK_FALSE(rec.isBadMove());
    CHECK_FALSE(rec.isOnBadMovePath());

    forward(rec, 1);                 // W[pp] BM[1]
    CHECK(rec.isBadMove());
    CHECK(rec.isOnBadMovePath());

    forward(rec, 1);                 // B[jj], not itself bad
    CHECK_FALSE(rec.isBadMove());
    CHECK(rec.isOnBadMovePath());    // but an ancestor is
}

TEST_CASE("markBadMove tags the current node") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B", "W");
    s.rec.move(blackAt(4, 4));
    CHECK_FALSE(s.rec.isBadMove());
    s.rec.markBadMove();
    CHECK(s.rec.isBadMove());
    CHECK(s.rec.isOnBadMovePath());
    s.rec.move(whiteAt(2, 2));
    CHECK_FALSE(s.rec.isBadMove());
    CHECK(s.rec.isOnBadMovePath());

    const std::string out = s.path("badmove.sgf");
    s.rec.saveAs(out);
    CHECK(readAll(out).find("BM[1]") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Tree path (session persistence)
// ---------------------------------------------------------------------------

TEST_CASE("getTreePath records only the choices made at branch points") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("variations.sgf"), info));   // end of main line

    auto path = rec.getTreePath();
    CHECK(path.length == 4);
    REQUIRE(path.branchChoices.size() == 1);   // one 3-way branch at W[pp]
    CHECK(path.branchChoices[0] == 0);

    SUBCASE("round trip through navigateToTreePath") {
        rec.navigateToTreePath(0, {});
        CHECK(rec.moveCount() == 0);
        REQUIRE(rec.navigateToTreePath(path.length, path.branchChoices));
        CHECK(rec.moveCount() == 4);
        CHECK(rec.lastMove().pos == Position(15, 15));   // W[pd]
        CHECK(rec.getTreePath().length == 4);
    }

    SUBCASE("a different branch choice selects a different variation") {
        REQUIRE(rec.navigateToTreePath(4, {1}));
        CHECK(rec.moveCount() == 4);
        CHECK(rec.lastMove().pos == Position(3, 3));     // W[dp] of variation 2
        CHECK(rec.lastMove().col == Color::WHITE);
        auto again = rec.getTreePath();
        CHECK(again.length == 4);
        REQUIRE(again.branchChoices.size() == 1);
        CHECK(again.branchChoices[0] == 1);
    }

    SUBCASE("too long a path fails and rewinds to the root") {
        CHECK_FALSE(rec.navigateToTreePath(4, {2}));     // B[jj] is only 3 deep
        CHECK(rec.moveCount() == 0);                     // reset to effective root
    }

    SUBCASE("an out-of-range branch choice fails and rewinds to the root") {
        CHECK_FALSE(rec.navigateToTreePath(4, {7}));
        CHECK(rec.moveCount() == 0);
    }

    SUBCASE("missing branch choices fail and rewind to the root") {
        CHECK_FALSE(rec.navigateToTreePath(4, {}));
        CHECK(rec.moveCount() == 0);
    }

    SUBCASE("a path past the end of the tree fails") {
        CHECK_FALSE(rec.navigateToTreePath(99, {0}));
        CHECK(rec.moveCount() == 0);
    }
}

TEST_CASE("undo stops at the effective root of an FF[3] file") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("ff3_setup.sgf"), info, 0, /*startAtRoot=*/true));
    REQUIRE_FALSE(rec.hasPreviousMove());   // setup node counts as the root

    // GameNavigator only calls undo() while hasPreviousMove() is true, so it can
    // never step above the setup node. Calling undo() directly does step onto
    // the true root, and must still leave a consistent, non-crashing state.
    rec.undo();
    CHECK(rec.moveCount() == 0);
    CHECK_FALSE(rec.hasPreviousMove());
    CHECK(rec.getTreePath().length == 0);

    Board board(19);
    Position ko;
    rec.buildBoardFromMoves(board, ko);
    CHECK(board.stonesOnBoard(Color::BLACK) == 2);   // setup stones still applied
    CHECK(board.stonesOnBoard(Color::WHITE) == 2);

    // At the true root the only child is the move-less setup node, which is why
    // hasNextMove() and getVariations() disagree here.
    CHECK(rec.hasNextMove());
    CHECK(rec.getNextMove() == Move::INVALID);
    CHECK(rec.getVariations().empty());
    // navigateToTreePath is the supported way back and re-anchors on the
    // effective root.
    REQUIRE(rec.navigateToTreePath(2, {}));
    CHECK(rec.moveCount() == 2);
}

TEST_CASE("tree path is relative to the effective root for FF[3] files") {
    quietLogging();
    GameRecord rec;
    GameRecord::SGFGameInfo info;
    REQUIRE(rec.loadFromSGF(fixture("ff3_setup.sgf"), info));
    // Two moves after the setup node; the setup node itself is not a step.
    auto path = rec.getTreePath();
    CHECK(path.length == 2);
    CHECK(path.branchChoices.empty());
    REQUIRE(rec.navigateToTreePath(2, {}));
    CHECK(rec.moveCount() == 2);
    REQUIRE(rec.navigateToTreePath(0, {}));
    CHECK(rec.moveCount() == 0);
    CHECK_FALSE(rec.hasPreviousMove());
}

// ---------------------------------------------------------------------------
// Session document handling
// ---------------------------------------------------------------------------

TEST_CASE("consecutive games accumulate in the session document") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B1", "W1");
    s.rec.move(blackAt(4, 4));
    CHECK(s.rec.getNumGames() == 1);

    s.rec.initGame(9, 0.5f, 0, "B2", "W2");
    CHECK(s.rec.getNumGames() == 1);      // not counted until a move is played
    s.rec.move(blackAt(2, 2));
    CHECK(s.rec.getNumGames() == 2);

    const std::string out = s.path("session.sgf");
    s.rec.saveAs(out);

    GameRecord reloaded;
    GameRecord::SGFGameInfo info;
    REQUIRE(reloaded.loadFromSGF(out, info, 0));
    CHECK(reloaded.getLoadedGameCount() == 2);
    CHECK(info.blackPlayer == "B1");
    REQUIRE(reloaded.loadFromSGF(out, info, 1));
    CHECK(info.blackPlayer == "B2");
}

TEST_CASE("clearSession forgets games already archived but keeps the current one") {
    Session s;
    // Two games accumulate in the daily session document.
    s.rec.initGame(9, 0.5f, 0, "B1", "W1");
    s.rec.move(blackAt(4, 4));
    REQUIRE(s.rec.getNumGames() == 1);
    s.rec.initGame(9, 0.5f, 0, "B2", "W2");
    s.rec.move(blackAt(2, 2));
    REQUIRE(s.rec.getNumGames() == 2);

    // Archiving hands the accumulated games off to the timestamped file. The
    // fresh session keeps only the game still in progress.
    s.rec.clearSession();
    CHECK(s.rec.getNumGames() == 1);

    const std::string out = s.path("cleared.sgf");
    s.rec.saveAs(out);
    REQUIRE(std::filesystem::exists(out));
    const std::string sgf = readAll(out);
    CHECK(sgf.find("PB[B2]") != std::string::npos);
    CHECK(sgf.find("PB[B1]") == std::string::npos);

    // A brand new game afterwards starts the next session cleanly.
    s.rec.initGame(9, 0.5f, 0, "B3", "W3");
    s.rec.move(blackAt(6, 6));
    CHECK(s.rec.getNumGames() == 2);
}

// Regression test for a fixed data-loss bug.
//
// clearSession() cleared `gameInDocument` but left `doc` null with nothing able
// to rebuild it: appendGameToDocument()'s only caller is move(), behind
// `if (!gameHasNewMoves)`, which is already true for a game in progress. So
// saveAsInternal() bailed out at `doc == nullptr`, silently discarding the rest
// of the game and leaving hasUnsavedChanges() stuck at true.
//
// Reachable from the "archive" command, which renames the daily session file and
// calls clearSession() — archiving mid-game lost every subsequent move.
TEST_CASE("archiving mid-game keeps recording the game in progress") {
    Session s;
    s.rec.initGame(9, 0.5f, 0, "B1", "W1");
    s.rec.move(blackAt(4, 4));
    REQUIRE(s.rec.getNumGames() == 1);

    s.rec.clearSession();
    // The diverged game re-attaches to the fresh session straight away.
    CHECK(s.rec.getNumGames() == 1);

    s.rec.move(whiteAt(2, 2));
    CHECK(s.rec.getNumGames() == 1);

    const std::string out = s.path("archived_then_continued.sgf");
    s.rec.saveAs(out);
    REQUIRE(std::filesystem::exists(out));
    CHECK_FALSE(s.rec.hasUnsavedChanges());

    // Both moves must be present, not just the one played before archiving.
    // SGF rows count from the top, so on 9x9 (2,2) is "cg", not "cc".
    const std::string sgf = readAll(out);
    CHECK(sgf.find("B[ee]") != std::string::npos);
    CHECK(sgf.find("W[cg]") != std::string::npos);
}

// The counterpart invariant, and the reason clearSession() must not simply
// force the game in: a record the player is only replaying, without diverging
// from the SGF tree, must not be copied into the new session by archiving.
TEST_CASE("archiving does not copy a game that is only being replayed") {
    Session s;
    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("simple.sgf"), info));
    REQUIRE_FALSE(s.rec.hasNewMoves());

    s.rec.clearSession();
    CHECK(s.rec.getNumGames() == 0);
    CHECK_FALSE(s.rec.hasNewMoves());
}

TEST_CASE("loading a file named after today adopts it as the daily session") {
    Session s;
    // Daily-session detection compares only the file *stem* against today's
    // date, so any file called <today>.sgf is adopted (and defaultFileName is
    // repointed at it) regardless of which directory it lives in.
    const std::string daily = s.path(todayStamp() + ".sgf");
    REQUIRE(GameRecord::writeFileContent(daily,
        "(;FF[4]GM[1]SZ[9]KM[0.5]PB[A]PW[B];B[ee];W[cc])\n"
        "(;FF[4]GM[1]SZ[9]KM[0.5]PB[C]PW[D];B[gg])\n"));

    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(daily, info, 1));
    CHECK(s.rec.getDefaultFileName() == daily);
    CHECK_FALSE(s.rec.hasLoadedExternalDoc());   // it is the session, not external
    CHECK(s.rec.getNumGames() == 2);
    CHECK(s.rec.getLoadedGameCount() == 2);
    CHECK(s.rec.getLoadedGameIndex() == 1);

    // Continuing the second game keeps both games in the session document.
    s.rec.move(whiteAt(1, 1));
    CHECK(s.rec.getNumGames() == 2);             // no duplicate append
    s.rec.saveAs(daily);

    GameRecord reloaded;
    GameRecord::SGFGameInfo info2;
    REQUIRE(reloaded.loadFromSGF(daily, info2, 0));
    CHECK(reloaded.getLoadedGameCount() == 2);
    CHECK(info2.blackPlayer == "A");
    REQUIRE(reloaded.loadFromSGF(daily, info2, 1));
    CHECK(reloaded.moveCount() == 2);            // gg plus the new white move
}

TEST_CASE("an external SGF is copied into the session as soon as it is modified") {
    Session s;
    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("simple.sgf"), info));
    REQUIRE(s.rec.hasLoadedExternalDoc());
    REQUIRE(s.rec.getNumGames() == 0);           // nothing in the session yet

    s.rec.move(blackAt(0, 0));
    CHECK(s.rec.getNumGames() == 1);

    const std::string out = s.path("from_external.sgf");
    s.rec.saveAs(out);
    REQUIRE(std::filesystem::exists(out));

    GameRecord reloaded;
    GameRecord::SGFGameInfo info2;
    REQUIRE(reloaded.loadFromSGF(out, info2));
    CHECK(reloaded.getLoadedGameCount() == 1);
    CHECK(info2.blackPlayer == "Alice");
    CHECK(reloaded.moveCount() == 5);
    // The fixture on disk is untouched.
    CHECK(readAll(fixture("simple.sgf")).find("aa") == std::string::npos);
}

TEST_CASE("modifying an external SGF clears the external reference "
          "when a session file already exists") {
    Session s;
    // Pre-existing daily session file => appendGameToDocument() reads it and
    // falls through to its AppendGame() path, which clears the external doc
    // reference as its comment promises.
    const std::string daily = s.path(todayStamp() + ".sgf");
    REQUIRE(GameRecord::writeFileContent(daily,
        "(;FF[4]GM[1]SZ[9]KM[0.5]PB[Old]PW[Game];B[ee])\n"));

    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("simple.sgf"), info));
    REQUIRE(s.rec.hasLoadedExternalDoc());

    s.rec.move(blackAt(0, 0));
    CHECK(s.rec.getNumGames() == 2);
    CHECK_FALSE(s.rec.hasLoadedExternalDoc());
    CHECK(s.rec.getLoadedFilePath().empty());
}

// SUSPECTED PRODUCTION BUG — see report.
//
// appendGameToDocument() clears loadedExternalDoc/loadedFilePath only on its
// fall-through AppendGame() path (GameRecord.cpp:686-692). The three early
// `return`s that create a brand new document — no session file on disk
// (GameRecord.cpp:663-669), session file present but invalid (:649-655), and
// read exception (:656-662) — skip that cleanup.
//
// "No session file on disk yet" is the normal case for the first game recorded
// on any given day, so after loading an external SGF and playing one move the
// record still reports hasLoadedExternalDoc() == true and a getLoadedFilePath()
// pointing at the external file, even though the game now lives in the daily
// session document.
//
// GobanControl::saveCurrentGame() (GobanControl.cpp:970-983) branches on exactly
// this flag: it writes the modified game to the daily session via saveAs(""),
// but then persists the *external* path as the session file with
// setSessionIsExternal(true). On the next launch the unmodified external file is
// restored instead of the continuation the user just played.
TEST_CASE("modifying an external SGF clears the external reference "
          "when there is no session file yet") {
    Session s;
    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("simple.sgf"), info));
    REQUIRE(s.rec.hasLoadedExternalDoc());
    REQUIRE_FALSE(std::filesystem::exists(s.rec.getDefaultFileName()));

    s.rec.move(blackAt(0, 0));
    REQUIRE(s.rec.getNumGames() == 1);            // game is in the session doc

    CHECK_FALSE(s.rec.hasLoadedExternalDoc());    // actual: still true
    CHECK(s.rec.getLoadedFilePath().empty());     // actual: still simple.sgf
}

TEST_CASE("suppressSessionCopy keeps tsumego branches out of the session") {
    Session s;
    GameRecord::SGFGameInfo info;
    REQUIRE(s.rec.loadFromSGF(fixture("tsumego.sgf"), info, 0, /*startAtRoot=*/true));
    s.rec.setSuppressSessionCopy(true);

    s.rec.move(blackAt(0, 0));
    CHECK(s.rec.getNumGames() == 0);        // never appended to the daily session
    const std::string out = s.path("tsumego_out.sgf");
    s.rec.saveAs(out);
    CHECK_FALSE(std::filesystem::exists(out));
}
