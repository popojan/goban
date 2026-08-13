// Tests for GameNavigator — SGF tree navigation on top of GobanModel.
//
// GameNavigator talks to engines only through the abstract Engine interface, so
// no GTP subprocess is needed here: FakeEngine below is an in-process Engine
// whose six pure virtuals are trivial and whose play()/undo() record what
// navigation asked for and can be made to fail on demand. That covers
//   * the guard clauses (refusing to navigate must leave the SGF cursor and the
//     model state untouched),
//   * the NavigationGuard lifetime — isNavigating() must be true for the whole
//     duration of an engine call and false again on every exit path, which is
//     what the "no genmove during navigation" invariant rests on,
//   * the successful navigateBack / navigateForward / navigateToStart /
//     navigateToEnd / navigateToVariation / navigateToTreePath paths, including
//     pass labelling, territory display and branch creation,
//   * the engine-independent helpers buildBoardFromSGF, notifyBoardChange,
//     notifyBoardChangeWithMove and applyTsumegoHint.
#include <doctest/doctest.h>

#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "GameNavigator.h"
#include "GobanModel.h"
#include "player.h"

#ifndef GOBAN_TEST_DATA_DIR
#define GOBAN_TEST_DATA_DIR "tests/data"
#endif

namespace {

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

/// Records the observer callbacks GameNavigator fires.
struct RecordingObserver : GameObserver {
    int boardChanges = 0;
    int stonesPlaced = 0;
    int lastBlackStones = 0;
    Move lastMove;

    void onBoardChange(const Board& board) override {
        ++boardChanges;
        lastBlackStones = board.stonesOnBoard(Color::BLACK);
    }
    void onStonePlaced(const Move& move) override {
        ++stonesPlaced;
        lastMove = move;
    }
};

/// In-process stand-in for a GTP engine.
///
/// GameNavigator only ever calls play(), undo() and (indirectly, through the
/// SyncEngineCallback) nothing else, so a plain Engine subclass is enough — no
/// subprocess, no GTP, no config. Every call is recorded, and both play() and
/// undo() can be told to fail so the "fail early" paths can be exercised.
class FakeEngine : public Engine {
public:
    explicit FakeEngine(const std::string& name = "Fake") : Engine(name) {}

    Move genmove(const Color&) override { return Move(Move::PASS, Color::BLACK); }
    std::optional<float> final_score() override { return 0.0f; }
    bool applyTerritory(Board&) override { return true; }

    bool play(const Move& move) override {
        plays.push_back(move);
        note();
        return playResult;
    }
    bool undo() override {
        ++undos;
        note();
        return undoResult;
    }
    bool clear() override { ++clears; return true; }

    std::vector<Move> plays;
    int undos = 0;
    int clears = 0;
    bool playResult = true;
    bool undoResult = true;
    /// Set by the fixture so the engine can assert that navigation is flagged
    /// as in progress while it is being driven.
    std::function<bool()> navigatingProbe;
    bool sawNavigating = true;

private:
    void note() {
        if (navigatingProbe && !navigatingProbe()) sawNavigating = false;
    }
};

/// A GobanModel plus a GameNavigator. By default there is no coach and no active
/// player, which is what the guard-clause tests need; a test that wants real
/// navigation assigns `coach` and/or pushes into `players`.
///
/// The record's daily-session file is redirected into a scratch directory whose
/// name carries today's date, so nothing here can read or write the user's real
/// games folder (and initGame() never takes its "day changed" branch).
struct Fixture {
    std::filesystem::path dir;
    GobanModel model;
    GameNavigator::ObserverList observers;
    RecordingObserver observer;
    Engine* coach = nullptr;
    std::vector<Player*> players;
    int coachRequests = 0;
    int syncCalls = 0;
    std::vector<Engine*> synced;
    bool syncResult = true;
    GameNavigator nav;

    explicit Fixture(int boardSize = 19)
        : model(boardSize),
          nav(model,
              [this]() -> Engine* { ++coachRequests; return coach; },
              [this]() { return players; },
              observers,
              [this](Engine* e) { ++syncCalls; synced.push_back(e); return syncResult; })
    {
        quietLogging();
        // getpid() is POSIX-only; a counter plus an existence check keeps these
        // tests runnable on Windows too.
        static int counter = 0;
        const auto base = std::filesystem::temp_directory_path();
        do {
            std::ostringstream name;
            name << "goban_navigator_test_" << counter++;
            dir = base / name.str();
        } while (std::filesystem::exists(dir));
        std::filesystem::create_directories(dir);
        model.game.setDefaultFileName((dir / (todayStamp() + ".sgf")).string());
        observers.push_back(&observer);
    }

    ~Fixture() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    /// Install a coach and let it observe the navigationInProgress flag.
    void useCoach(FakeEngine& engine) {
        coach = &engine;
        engine.navigatingProbe = [this]() { return nav.isNavigating(); };
    }

    bool load(const std::string& name, bool startAtRoot = false) {
        GameRecord::SGFGameInfo info;
        bool ok = model.game.loadFromSGF(fixture(name), info, 0, startAtRoot);
        if (ok) {
            model.state.komi = info.komi;
            model.state.handicap = info.handicap;
        }
        return ok;
    }

    /// Walk the main line forward with GameRecord directly (no engine needed).
    void forward(int n) {
        for (int i = 0; i < n; ++i) {
            REQUIRE(model.game.hasNextMove());
            REQUIRE(model.game.navigateToChild(model.game.getNextMove()));
        }
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// Construction and the isNavigating flag
// ---------------------------------------------------------------------------

TEST_CASE("a freshly constructed navigator is not navigating") {
    Fixture f;
    CHECK_FALSE(f.nav.isNavigating());
    CHECK(f.coachRequests == 0);
    CHECK(f.syncCalls == 0);
}

TEST_CASE("navigation is refused while the model has no game record") {
    Fixture f;
    REQUIRE_FALSE(f.model.game.isNavigating());   // no initGame / loadFromSGF yet

    CHECK_FALSE(f.nav.navigateBack());
    CHECK_FALSE(f.nav.navigateForward());
    CHECK_FALSE(f.nav.navigateToStart());
    CHECK_FALSE(f.nav.navigateToEnd());
    CHECK_FALSE(f.nav.navigateToTreePath(2, {}));
    CHECK_FALSE(f.nav.navigateToVariation(Move(Position(3, 3), Color::BLACK)).success);

    CHECK_FALSE(f.nav.isNavigating());   // the guard is released every time
    CHECK(f.observer.boardChanges == 0);
    CHECK(f.observer.stonesPlaced == 0);
    CHECK(f.syncCalls == 0);
}

// ---------------------------------------------------------------------------
// Guard clauses: no coach engine
// ---------------------------------------------------------------------------

TEST_CASE("every navigation method refuses to run without a coach engine") {
    Fixture f;
    REQUIRE(f.load("variations.sgf"));            // at the end of the main line
    const size_t before = f.model.game.moveCount();
    REQUIRE(before == 4);
    const auto pathBefore = f.model.game.getTreePath();

    SUBCASE("navigateBack") {
        CHECK_FALSE(f.nav.navigateBack());
        CHECK(f.coachRequests == 1);
    }
    SUBCASE("navigateToStart") {
        CHECK_FALSE(f.nav.navigateToStart());
        CHECK(f.coachRequests == 1);
    }
    SUBCASE("navigateToEnd") {
        CHECK_FALSE(f.nav.navigateToEnd());
        CHECK(f.coachRequests == 1);
    }
    SUBCASE("navigateToTreePath") {
        CHECK_FALSE(f.nav.navigateToTreePath(2, {}));
        CHECK(f.coachRequests == 1);
    }
    SUBCASE("navigateToVariation") {
        auto result = f.nav.navigateToVariation(Move(Position(0, 0), Color::BLACK));
        CHECK_FALSE(result.success);
        CHECK_FALSE(result.newBranch);
        CHECK(f.coachRequests == 1);
        // Crucially: no branch was created in the SGF tree.
        CHECK(f.model.game.getLoadedMovesCount() == 4);
        CHECK_FALSE(f.model.game.hasNewMoves());
    }

    // Whatever was attempted, the position, the model and the flag are intact.
    CHECK(f.model.game.moveCount() == before);
    CHECK(f.model.game.getTreePath().length == pathBefore.length);
    CHECK_FALSE(f.nav.isNavigating());
    CHECK(f.observer.boardChanges == 0);
    CHECK(f.observer.stonesPlaced == 0);
    CHECK(f.syncCalls == 0);
}

TEST_CASE("navigateBack and navigateToStart refuse before asking for a coach at the root") {
    Fixture f;
    REQUIRE(f.load("variations.sgf", /*startAtRoot=*/true));
    REQUIRE(f.model.game.moveCount() == 0);

    CHECK_FALSE(f.nav.navigateBack());
    CHECK_FALSE(f.nav.navigateToStart());
    // Both bail out on hasPreviousMove() before the coach is even requested.
    CHECK(f.coachRequests == 0);
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("navigateForward refuses at the end of the tree without asking for a coach") {
    Fixture f;
    REQUIRE(f.load("variations.sgf"));            // end of main line, no children
    REQUIRE(f.model.game.isAtEndOfNavigation());

    CHECK_FALSE(f.nav.navigateForward());
    CHECK(f.coachRequests == 0);                  // variations.empty() short-circuits
    CHECK_FALSE(f.nav.isNavigating());
    CHECK(f.model.game.moveCount() == 4);
}

TEST_CASE("navigateForward asks for a coach when a variation exists") {
    Fixture f;
    REQUIRE(f.load("variations.sgf", /*startAtRoot=*/true));
    REQUIRE_FALSE(f.model.game.getVariations().empty());

    CHECK_FALSE(f.nav.navigateForward());
    CHECK(f.coachRequests == 1);
    CHECK(f.model.game.moveCount() == 0);         // cursor did not move
    CHECK_FALSE(f.nav.isNavigating());
}

// ---------------------------------------------------------------------------
// Successful navigation with an in-process engine
// ---------------------------------------------------------------------------

TEST_CASE("navigateBack undoes one move on the record and on every engine") {
    Fixture f;
    FakeEngine coach;
    FakeEngine other;
    f.useCoach(coach);
    f.players = {&other};
    REQUIRE(f.load("simple.sgf"));
    REQUIRE(f.model.game.moveCount() == 4);
    f.model.board.showTerritory = true;
    f.model.board.showTerritoryAuto = true;
    f.model.endGame(GameState::DOUBLE_PASS);

    CHECK(f.nav.navigateBack());

    CHECK(f.model.game.moveCount() == 3);
    CHECK(f.model.game.lastMove().pos == Position(3, 3));   // dp
    CHECK(coach.undos == 1);
    CHECK(other.undos == 1);                 // every engine stays in sync
    CHECK(other.plays.empty());
    CHECK(coach.sawNavigating);               // guard held during the engine call
    CHECK_FALSE(f.nav.isNavigating());        // and released afterwards

    // Reviewing pauses play and clears the stale finished-game state.
    CHECK(f.model.phase() != GamePhase::Playing);
    CHECK(f.model.phase() != GamePhase::Finished);
    CHECK_FALSE(f.model.board.showTerritory);
    CHECK_FALSE(f.model.board.showTerritoryAuto);

    // Model state follows the new position.
    CHECK(f.model.state.msg == GameState::NONE);
    CHECK(f.model.state.colorToMove == Color::WHITE);
    CHECK(f.model.state.comment == "third move");
    CHECK(f.observer.boardChanges == 1);
    CHECK(f.observer.stonesPlaced == 0);      // navigateBack does not place stones
    CHECK(f.observer.lastBlackStones == 2);
}

TEST_CASE("navigateBack aborts without touching the record when the coach fails") {
    Fixture f;
    FakeEngine coach;
    FakeEngine other;
    coach.undoResult = false;
    f.useCoach(coach);
    f.players = {&other};
    REQUIRE(f.load("simple.sgf"));

    CHECK_FALSE(f.nav.navigateBack());
    CHECK(f.model.game.moveCount() == 4);     // fail early: nothing moved
    CHECK(coach.undos == 1);
    CHECK(other.undos == 0);                  // other engines not touched either
    CHECK(f.observer.boardChanges == 0);
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("navigateBack onto a pass restores the pass message and its number") {
    // Regression for the pass move numbering fix: the label must be the number
    // of the pass itself, counted after the cursor has moved.
    Fixture f(9);
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("double_pass.sgf"));       // ...;B[];W[]
    REQUIRE(f.model.game.moveCount() == 4);

    CHECK(f.nav.navigateBack());              // now standing on B[] (move 3)
    CHECK(f.model.game.moveCount() == 3);
    CHECK(f.model.game.lastMove() == Move::PASS);
    CHECK(f.model.state.msg == GameState::BLACK_PASS);
    CHECK(f.model.state.passVariationLabel == "3");

    CHECK(f.nav.navigateBack());              // now on W[cc] (move 2)
    CHECK(f.model.state.msg == GameState::NONE);
}

TEST_CASE("navigateForward replays the main line and labels passes") {
    Fixture f(9);
    FakeEngine coach;
    FakeEngine other;
    f.useCoach(coach);
    f.players = {&other};
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));

    CHECK(f.nav.navigateForward());
    CHECK(f.model.game.moveCount() == 1);
    CHECK(f.model.state.msg == GameState::NONE);
    REQUIRE(coach.plays.size() == 1);
    CHECK(coach.plays[0].pos == Position(4, 4));   // ee
    REQUIRE(other.plays.size() == 1);              // engines kept in sync
    CHECK(f.observer.boardChanges == 1);
    CHECK(f.observer.stonesPlaced == 1);
    CHECK(f.observer.lastMove.pos == Position(4, 4));

    CHECK(f.nav.navigateForward());                // W[cc]
    CHECK(f.nav.navigateForward());                // B[] pass, move 3
    CHECK(f.model.game.moveCount() == 3);
    CHECK(f.model.state.msg == GameState::BLACK_PASS);
    CHECK(f.model.state.passVariationLabel == "3");

    CHECK(f.nav.navigateForward());                // W[] pass, move 4
    CHECK(f.model.state.msg == GameState::WHITE_PASS);
    CHECK(f.model.state.passVariationLabel == "4");
    CHECK(f.model.game.isGameFinished());
    // End of a game that carries a result and the user has not pressed Start.
    CHECK(f.model.phase() == GamePhase::Finished);

    CHECK_FALSE(f.nav.navigateForward());          // nothing left
    CHECK(coach.plays.size() == 4);
    CHECK(coach.sawNavigating);
}

TEST_CASE("navigateForward leaves the record alone when the coach rejects the move") {
    Fixture f;
    FakeEngine coach;
    coach.playResult = false;
    f.useCoach(coach);
    REQUIRE(f.load("simple.sgf", /*startAtRoot=*/true));

    CHECK_FALSE(f.nav.navigateForward());
    CHECK(f.model.game.moveCount() == 0);
    CHECK(f.model.game.getLoadedMovesCount() == 4);   // no branch was invented
    CHECK(f.observer.boardChanges == 0);
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("navigateForward keeps isGameOver clear once the user has started") {
    Fixture f(9);
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));
    f.model.start();
    REQUIRE(f.model.phase() == GamePhase::Playing);

    for (int i = 0; i < 4; ++i) CHECK(f.nav.navigateForward());
    CHECK(f.model.game.isAtEndOfNavigation());
    CHECK(f.model.game.hasGameResult());
    // model.started guards the restore, so the user's intent to play wins.
    CHECK(f.model.phase() != GamePhase::Finished);
}

TEST_CASE("navigateToStart rewinds the record and syncs every engine") {
    Fixture f;
    FakeEngine coach;
    FakeEngine other;
    f.useCoach(coach);
    // The coach also appears in the active player list; it must not be synced
    // twice, and a non-engine player must be skipped entirely.
    LocalHumanPlayer human("Human");
    f.players = {static_cast<Player*>(&coach), static_cast<Player*>(&other), &human};
    REQUIRE(f.load("simple.sgf"));
    f.model.endGame(GameState::DOUBLE_PASS);
    f.model.board.showTerritory = true;

    CHECK(f.nav.navigateToStart());

    CHECK(f.model.game.moveCount() == 0);
    CHECK(f.model.game.getLoadedMovesCount() == 4);   // tree untouched
    CHECK(f.model.state.msg == GameState::NONE);
    CHECK(f.model.state.colorToMove == Color::BLACK);
    CHECK(f.model.phase() != GamePhase::Playing);
    CHECK(f.model.phase() != GamePhase::Finished);
    CHECK_FALSE(f.model.board.showTerritory);
    CHECK(f.observer.boardChanges == 1);
    CHECK(f.observer.lastBlackStones == 0);

    // syncEngine is used instead of per-move undo: once for the coach, once for
    // each other engine player.
    CHECK(f.syncCalls == 2);
    CHECK(f.synced[0] == static_cast<Engine*>(&coach));
    CHECK(f.synced[1] == static_cast<Engine*>(&other));
    CHECK(coach.undos == 0);
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("navigateToStart reports failure when the coach cannot be synced") {
    Fixture f;
    FakeEngine coach;
    f.useCoach(coach);
    f.syncResult = false;
    REQUIRE(f.load("simple.sgf"));

    CHECK_FALSE(f.nav.navigateToStart());
    // The record was already rewound before the sync was attempted; that is why
    // the caller has to treat a false return as "engines may be out of sync".
    CHECK(f.model.game.moveCount() == 0);
    CHECK(f.syncCalls == 1);
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("navigateToEnd replays to the end of the main line") {
    Fixture f;
    FakeEngine coach;
    FakeEngine other;
    f.useCoach(coach);
    f.players = {&other};
    REQUIRE(f.load("variations.sgf", /*startAtRoot=*/true));

    CHECK(f.nav.navigateToEnd());
    CHECK(f.model.game.moveCount() == 4);
    CHECK(f.model.game.isAtEndOfNavigation());
    REQUIRE(coach.plays.size() == 4);
    CHECK(coach.plays[2].pos == Position(3, 3));    // dp, the first variation
    CHECK(other.plays.size() == 4);
    CHECK(f.observer.boardChanges == 1);            // one notification at the end
    CHECK(f.observer.stonesPlaced == 0);
    CHECK(f.model.state.colorToMove == Color::BLACK);
    CHECK_FALSE(f.nav.isNavigating());

    // Idempotent: already at the end, nothing more is played.
    CHECK(f.nav.navigateToEnd());
    CHECK(coach.plays.size() == 4);
}

TEST_CASE("navigateToEnd of a scored game turns on the territory overlay") {
    Fixture f(9);
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));

    CHECK(f.nav.navigateToEnd());
    CHECK(f.model.game.shouldShowTerritory());
    CHECK(f.model.board.showTerritory);
    CHECK(f.model.board.showTerritoryAuto);
    CHECK(f.model.phase() == GamePhase::Finished);
}

TEST_CASE("navigateToEnd of a resigned game shows the result without territory") {
    Fixture f(9);
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("resign.sgf", /*startAtRoot=*/true));

    CHECK(f.nav.navigateToEnd());
    CHECK(f.model.game.moveCount() == 3);
    CHECK_FALSE(f.model.game.shouldShowTerritory());
    CHECK_FALSE(f.model.board.showTerritory);
    CHECK(f.model.phase() == GamePhase::Finished);
    CHECK(f.model.state.msg == GameState::BLACK_RESIGNED);   // RE[W+R]
}

TEST_CASE("navigateToEnd stops early when the coach rejects a move") {
    Fixture f;
    FakeEngine coach;
    coach.playResult = false;
    f.useCoach(coach);
    REQUIRE(f.load("simple.sgf", /*startAtRoot=*/true));

    // It still returns true ("we are at the end now") but the cursor never moved.
    CHECK(f.nav.navigateToEnd());
    CHECK(f.model.game.moveCount() == 0);
    CHECK(coach.plays.size() == 1);
    CHECK(f.model.game.getLoadedMovesCount() == 4);
}

TEST_CASE("navigateToVariation follows an existing child without branching") {
    Fixture f;
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("variations.sgf", /*startAtRoot=*/true));
    f.forward(2);
    auto variations = f.model.game.getVariations();
    REQUIRE(variations.size() == 3);

    auto result = f.nav.navigateToVariation(variations[1]);   // B[pd]
    CHECK(result.success);
    CHECK_FALSE(result.newBranch);
    CHECK(f.model.game.moveCount() == 3);
    CHECK(f.model.game.lastMove().pos == Position(15, 15));
    CHECK(f.model.game.getVariations().size() == 1);
    CHECK_FALSE(f.model.game.hasNewMoves());     // no tree modification
    REQUIRE(coach.plays.size() == 1);
    CHECK(f.observer.stonesPlaced == 1);
    CHECK(f.model.state.msg == GameState::NONE);
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("navigateToVariation creates a promoted branch in an unfinished game") {
    Fixture f;
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("simple.sgf", /*startAtRoot=*/true));
    f.forward(2);                                 // after W[pp]
    REQUIRE(f.model.game.getLoadedMovesCount() == 4);

    Move novelty(Position(9, 9), Color::BLACK);
    auto result = f.nav.navigateToVariation(novelty);
    CHECK(result.success);
    CHECK(result.newBranch);
    CHECK(f.model.game.moveCount() == 3);
    CHECK(f.model.game.lastMove().pos == Position(9, 9));
    CHECK(f.model.game.hasNewMoves());
    // The new move becomes the main line, so the old continuation is a side
    // branch and the main line is now three moves long.
    CHECK(f.model.game.getLoadedMovesCount() == 3);
    f.model.game.undo();
    auto variations = f.model.game.getVariations();
    REQUIRE(variations.size() == 2);
    CHECK(variations[0].pos == Position(9, 9));
    CHECK(variations[1].pos == Position(3, 3));   // the original dp
    // A promoted branch resumes play.
    CHECK(f.model.phase() == GamePhase::Playing);
}

TEST_CASE("navigateToVariation without promotion appends and stays paused") {
    Fixture f;
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("simple.sgf", /*startAtRoot=*/true));
    f.forward(2);

    Move novelty(Position(9, 9), Color::BLACK);
    auto result = f.nav.navigateToVariation(novelty, /*promote=*/false);
    CHECK(result.success);
    CHECK(result.newBranch);
    CHECK(f.model.game.moveCount() == 3);
    f.model.game.undo();
    auto variations = f.model.game.getVariations();
    REQUIRE(variations.size() == 2);
    CHECK(variations[0].pos == Position(3, 3));   // original dp keeps the lead
    CHECK(variations[1].pos == Position(9, 9));
    CHECK(f.model.game.getLoadedMovesCount() == 4);
    CHECK(f.model.phase() != GamePhase::Playing);          // stays in navigation mode
}

TEST_CASE("navigateToVariation on a finished game forks a fresh game record") {
    Fixture f(9);
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("resign.sgf"));
    REQUIRE(f.model.game.hasGameResult());
    REQUIRE(f.model.game.moveCount() == 3);

    Move continuation(Position(1, 1), Color::WHITE);
    auto result = f.nav.navigateToVariation(continuation);
    CHECK(result.success);
    CHECK(result.newBranch);
    CHECK(f.model.game.moveCount() == 4);
    // branchFromFinishedGame() copies the path into a new game without RE.
    CHECK_FALSE(f.model.game.hasGameResult());
    CHECK_FALSE(f.model.game.hasLoadedExternalDoc());
    CHECK(f.model.phase() == GamePhase::Playing);
    CHECK(f.model.game.getBoardSize() == 9);
}

TEST_CASE("navigateToVariation with a pass sets the pass message") {
    Fixture f(9);
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));
    f.forward(2);

    auto result = f.nav.navigateToVariation(Move(Move::PASS, Color::BLACK));
    CHECK(result.success);
    CHECK_FALSE(result.newBranch);              // B[] already exists
    CHECK(f.model.state.msg == GameState::BLACK_PASS);
    CHECK(f.model.state.passVariationLabel == "3");
}

TEST_CASE("navigateToTreePath navigates then syncs only the coach") {
    Fixture f;
    FakeEngine coach;
    FakeEngine other;
    f.useCoach(coach);
    f.players = {&other};
    REQUIRE(f.load("variations.sgf", /*startAtRoot=*/true));

    CHECK(f.nav.navigateToTreePath(4, {1}));
    CHECK(f.model.game.moveCount() == 4);
    CHECK(f.model.game.lastMove().pos == Position(3, 3));   // W[dp] of variation 2
    CHECK(f.model.state.colorToMove == Color::BLACK);
    CHECK(f.observer.boardChanges == 1);
    // Deliberate: other engines are synced lazily when they need to move.
    CHECK(f.syncCalls == 1);
    CHECK(f.synced[0] == static_cast<Engine*>(&coach));
    CHECK(coach.plays.empty());
    CHECK(other.plays.empty());
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("navigateToTreePath reports failure for an impossible path") {
    Fixture f;
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("variations.sgf"));

    CHECK_FALSE(f.nav.navigateToTreePath(99, {0}));
    CHECK(f.model.game.moveCount() == 0);       // record rewound to the root
    CHECK(f.syncCalls == 0);                    // no engine work was attempted
    CHECK(f.observer.boardChanges == 0);
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("navigateToTreePath restores the territory overlay at a finished end") {
    Fixture f(9);
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));

    CHECK(f.nav.navigateToTreePath(4, {}));
    CHECK(f.model.game.isGameFinished());
    CHECK(f.model.phase() == GamePhase::Finished);
    CHECK(f.model.board.showTerritory);
    CHECK(f.model.board.showTerritoryAuto);
}

TEST_CASE("a tsumego hint is applied after navigating back to the start") {
    Fixture f(9);
    FakeEngine coach;
    f.useCoach(coach);
    REQUIRE(f.load("tsumego.sgf", /*startAtRoot=*/true));
    f.model.tsumegoMode = true;
    f.model.tsumegoHintBlack = "Black to move";
    f.model.tsumegoHintWhite = "White to move";

    REQUIRE(f.nav.navigateForward());          // B[bh] with comment "correct"
    CHECK(f.model.state.comment == "correct");

    REQUIRE(f.nav.navigateBack());
    CHECK(f.model.game.getViewPosition() == 0);
    CHECK(f.model.state.colorToMove == Color::BLACK);
    CHECK(f.model.state.comment == "Black to move");
}

// ---------------------------------------------------------------------------
// Tree walking through the model (engine-independent)
// ---------------------------------------------------------------------------

TEST_CASE("tree walking through the model matches the SGF structure") {
    Fixture f;
    REQUIRE(f.load("variations.sgf", /*startAtRoot=*/true));

    CHECK(f.model.game.isNavigating());
    CHECK_FALSE(f.model.game.isAtEndOfNavigation());
    CHECK(f.model.game.getLoadedMovesCount() == 4);
    CHECK(f.model.game.getViewPosition() == 0);
    CHECK(f.model.game.getColorToMove() == Color::BLACK);

    f.forward(2);
    CHECK(f.model.game.getViewPosition() == 2);
    CHECK(f.model.game.getColorToMove() == Color::BLACK);

    auto variations = f.model.game.getVariations();
    REQUIRE(variations.size() == 3);
    CHECK(variations[0].pos == Position(3, 3));      // dp — main line
    CHECK(variations[1].pos == Position(15, 15));    // pd
    CHECK(variations[2].pos == Position(9, 9));      // jj

    REQUIRE(f.model.game.navigateToChild(variations[2]));
    CHECK(f.model.game.isAtEndOfNavigation());
    CHECK(f.model.game.getVariations().empty());
    CHECK(f.model.game.getViewPosition() == 3);

    // Back down to the branch point and out along the main line instead.
    f.model.game.undo();
    CHECK(f.model.game.getViewPosition() == 2);
    REQUIRE(f.model.game.navigateToChild(variations[0]));
    f.forward(1);
    CHECK(f.model.game.getViewPosition() == 4);
    CHECK(f.model.game.isAtEndOfNavigation());
}

TEST_CASE("getColorToMove follows the tree position") {
    Fixture f(9);
    REQUIRE(f.load("handicap.sgf", /*startAtRoot=*/true));
    // HA[2] with AB and no AW: white opens.
    CHECK(f.model.game.getColorToMove() == Color::WHITE);
    f.forward(1);
    CHECK(f.model.game.getColorToMove() == Color::BLACK);
    f.forward(1);
    CHECK(f.model.game.getColorToMove() == Color::WHITE);
    f.model.game.undo();
    CHECK(f.model.game.getColorToMove() == Color::BLACK);
}

TEST_CASE("shouldShowTerritory only holds at the end of a scored game") {
    Fixture f(9);

    SUBCASE("double-pass game with a point result") {
        REQUIRE(f.load("double_pass.sgf"));
        CHECK(f.model.game.isAtEndOfNavigation());
        CHECK(f.model.game.isGameFinished());
        CHECK(f.model.game.shouldShowTerritory());

        f.model.game.undo();
        CHECK_FALSE(f.model.game.shouldShowTerritory());
    }

    SUBCASE("resigned game") {
        REQUIRE(f.load("resign.sgf"));
        CHECK(f.model.game.isAtEndOfNavigation());
        CHECK(f.model.game.hasGameResult());
        CHECK_FALSE(f.model.game.shouldShowTerritory());
    }

    SUBCASE("unfinished game") {
        REQUIRE(f.load("variations.sgf"));
        CHECK(f.model.game.isAtEndOfNavigation());
        CHECK_FALSE(f.model.game.shouldShowTerritory());
    }

    SUBCASE("at the root of a finished game") {
        REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));
        CHECK_FALSE(f.model.game.isAtEndOfNavigation());
        CHECK_FALSE(f.model.game.shouldShowTerritory());
    }
}

TEST_CASE("isOnBadMovePath taints every position below a BM node") {
    Fixture f;
    REQUIRE(f.load("markup.sgf", /*startAtRoot=*/true));

    CHECK_FALSE(f.model.game.isOnBadMovePath());
    f.forward(1);                                  // B[dd]
    CHECK_FALSE(f.model.game.isBadMove());
    CHECK_FALSE(f.model.game.isOnBadMovePath());
    f.forward(1);                                  // W[pp] BM[1]
    CHECK(f.model.game.isBadMove());
    CHECK(f.model.game.isOnBadMovePath());
    f.forward(1);                                  // B[jj]
    CHECK_FALSE(f.model.game.isBadMove());
    CHECK(f.model.game.isOnBadMovePath());

    // Walking back above the bad move clears it again.
    f.model.game.undo();
    f.model.game.undo();
    CHECK_FALSE(f.model.game.isOnBadMovePath());
}

TEST_CASE("tree path navigation through the model reaches side variations") {
    Fixture f;
    REQUIRE(f.load("variations.sgf"));
    auto path = f.model.game.getTreePath();
    CHECK(path.length == 4);
    REQUIRE(path.branchChoices.size() == 1);
    CHECK(path.branchChoices[0] == 0);

    REQUIRE(f.model.game.navigateToTreePath(4, {1}));
    CHECK(f.model.game.getViewPosition() == 4);
    CHECK(f.model.game.lastMove().pos == Position(3, 3));   // W[dp] of variation 2

    CHECK_FALSE(f.model.game.navigateToTreePath(4, {2}));   // that line is shorter
    CHECK(f.model.game.getViewPosition() == 0);             // rewound to the root
}

// ---------------------------------------------------------------------------
// buildBoardFromSGF
// ---------------------------------------------------------------------------

TEST_CASE("buildBoardFromSGF rebuilds the position without an engine") {
    Fixture f(9);
    REQUIRE(f.load("capture.sgf"));
    REQUIRE(f.model.game.moveCount() == 3);

    Board board(9);
    f.nav.buildBoardFromSGF(board);
    // B[ba] W[aa] B[ab]: the lone white corner stone is captured.
    CHECK(board.stonesOnBoard(Color::BLACK) == 2);
    CHECK(board.stonesOnBoard(Color::WHITE) == 0);
    CHECK(board[Position(0, 8)].stone == Color::EMPTY);

    f.model.game.undo();
    Board earlier(9);
    f.nav.buildBoardFromSGF(earlier);
    CHECK(earlier.stonesOnBoard(Color::BLACK) == 1);
    CHECK(earlier.stonesOnBoard(Color::WHITE) == 1);

    CHECK_FALSE(f.nav.isNavigating());   // a pure read, no guard involved
}

TEST_CASE("buildBoardFromSGF includes handicap setup stones") {
    Fixture f(9);
    REQUIRE(f.load("handicap.sgf", /*startAtRoot=*/true));

    Board board(9);
    f.nav.buildBoardFromSGF(board);
    CHECK(board.stonesOnBoard(Color::BLACK) == 2);
    CHECK(board.stonesOnBoard(Color::WHITE) == 0);

    f.forward(2);
    Board later(9);
    f.nav.buildBoardFromSGF(later);
    CHECK(later.stonesOnBoard(Color::BLACK) == 3);
    CHECK(later.stonesOnBoard(Color::WHITE) == 1);
}

// ---------------------------------------------------------------------------
// Observer notification
// ---------------------------------------------------------------------------

TEST_CASE("notifyBoardChange fans out to every observer") {
    Fixture f(9);
    RecordingObserver second;
    f.observers.push_back(&second);
    REQUIRE(f.load("capture.sgf"));

    Board board(9);
    f.nav.buildBoardFromSGF(board);

    f.nav.notifyBoardChange(board);
    CHECK(f.observer.boardChanges == 1);
    CHECK(f.observer.stonesPlaced == 0);
    CHECK(f.observer.lastBlackStones == 2);
    CHECK(second.boardChanges == 1);

    Move played(Position(0, 7), Color::BLACK);
    f.nav.notifyBoardChangeWithMove(board, played);
    CHECK(f.observer.boardChanges == 2);
    CHECK(f.observer.stonesPlaced == 1);
    CHECK(f.observer.lastMove.pos == Position(0, 7));
    CHECK(f.observer.lastMove.col == Color::BLACK);
    CHECK(second.stonesPlaced == 1);
}

TEST_CASE("an empty observer list is harmless") {
    Fixture f(9);
    f.observers.clear();
    Board board(9);
    f.nav.notifyBoardChange(board);
    f.nav.notifyBoardChangeWithMove(board, Move(Position(1, 1), Color::WHITE));
    CHECK(f.observer.boardChanges == 0);
}

// ---------------------------------------------------------------------------
// Tsumego hint
// ---------------------------------------------------------------------------

TEST_CASE("applyTsumegoHint fills an empty comment at the initial position") {
    Fixture f(9);
    REQUIRE(f.load("tsumego.sgf", /*startAtRoot=*/true));
    f.model.tsumegoHintBlack = "Black to move";
    f.model.tsumegoHintWhite = "White to move";
    f.model.tsumegoMode = true;
    f.model.state.comment.clear();
    f.model.state.colorToMove = f.model.game.getColorToMove();
    REQUIRE(f.model.state.colorToMove == Color::BLACK);   // PL[B]

    f.nav.applyTsumegoHint();
    CHECK(f.model.state.comment == "Black to move");

    SUBCASE("the hint follows the colour to move") {
        f.model.state.comment.clear();
        f.model.state.colorToMove = Color::WHITE;
        f.nav.applyTsumegoHint();
        CHECK(f.model.state.comment == "White to move");
    }

    SUBCASE("an existing comment is never overwritten") {
        f.model.state.comment = "problem statement";
        f.nav.applyTsumegoHint();
        CHECK(f.model.state.comment == "problem statement");
    }

    SUBCASE("no hint outside tsumego mode") {
        f.model.tsumegoMode = false;
        f.model.state.comment.clear();
        f.nav.applyTsumegoHint();
        CHECK(f.model.state.comment.empty());
    }

    SUBCASE("no hint away from the initial position") {
        f.forward(1);
        f.model.state.comment.clear();
        f.nav.applyTsumegoHint();
        CHECK(f.model.state.comment.empty());
    }
}

// ---------------------------------------------------------------------------
// Model wiring
// ---------------------------------------------------------------------------

TEST_CASE("createNewRecord seeds the game record from the model state") {
    Fixture f(13);
    f.model.state.komi = 7.5f;
    f.model.state.black = "Alice";
    f.model.state.white = "Bob";
    f.model.createNewRecord();

    CHECK(f.model.game.isNavigating());
    CHECK(f.model.game.getBoardSize() == 13);
    CHECK(f.model.game.moveCount() == 0);
    CHECK(f.model.game.getPlayerNames().first == "Alice");
    CHECK(f.model.game.getPlayerNames().second == "Bob");
    CHECK(f.model.game.getColorToMove() == Color::BLACK);

    // With a game but at the root, navigation is still refused — and now the
    // reason is the position, not the missing record.
    CHECK_FALSE(f.nav.navigateBack());
    CHECK_FALSE(f.nav.navigateToStart());
    CHECK_FALSE(f.nav.navigateForward());
    CHECK(f.coachRequests == 0);
    CHECK_FALSE(f.nav.isNavigating());
}

TEST_CASE("createNewRecord with handicap stones records them and flips the first player") {
    Fixture f(9);
    f.model.state.komi = 0.5f;
    f.model.state.black = "Student";
    f.model.state.white = "Teacher";
    f.model.setupBlackStones = {Position(2, 2), Position(6, 6)};
    f.model.createNewRecord();

    CHECK(f.model.game.getBoardSize() == 9);
    CHECK(f.model.game.getColorToMove() == Color::WHITE);

    Board board(9);
    f.nav.buildBoardFromSGF(board);
    CHECK(board.stonesOnBoard(Color::BLACK) == 2);
    CHECK(board[Position(2, 2)].stone == Color::BLACK);
    CHECK(board[Position(6, 6)].stone == Color::BLACK);
}

TEST_CASE("model.start resumes a finished game and model.pause stops it") {
    Fixture f(9);
    REQUIRE(f.load("resign.sgf"));

    f.model.endGame(GameState::RESIGNATION);
    f.model.start();
    CHECK(f.model.phase() == GamePhase::Playing);
    CHECK(f.model.phase() != GamePhase::Finished);
    CHECK(f.model.state.reason == GameState::NO_REASON);

    f.model.pause();
    CHECK(f.model.phase() != GamePhase::Playing);
    CHECK(f.model.phase() != GamePhase::Finished);

    // pause() deliberately cannot un-finish a game — enterReview() is what
    // navigateBack() uses for that, once it has moved the position off the end.
    f.model.endGame(GameState::RESIGNATION);
    f.model.pause();
    CHECK(f.model.phase() == GamePhase::Finished);
    f.model.enterReview();
    CHECK(f.model.phase() != GamePhase::Finished);
}
