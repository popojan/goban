// Tests for GamePhase — docs/adr/0002-explicit-game-state.md.
//
// Step 1 added the phase as a value derived from the `started` / `isGameOver`
// pair. Step 2 inverted that: `GobanModel::gamePhase` is now the authoritative
// state, the two booleans are gone, and `isStarted()` / `isGameOver()` survive
// only as compatibility accessors that are pure functions of the phase.
//
// So this file has two halves. The first pins that the accessors really are
// functions of the phase — which is what makes step 3 (deleting them) a
// mechanical substitution rather than a behaviour change — and that the old
// `started && isGameOver` combination is now unrepresentable. The second pins
// the transition table: what each named transition, and each operation that
// triggers one, does to the phase.
//
// Two step-1 findings were resolved here rather than preserved:
//
//   * a freshly constructed model is now Setup, not Finished;
//   * `started` no longer outlives the game — ending a live game leaves
//     isStarted() false, matching a loaded finished game.
#include <doctest/doctest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "GameNavigator.h"
#include "GamePhase.h"
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

/// Minimal in-process Engine, as in test_navigator.cpp: navigation only ever
/// calls play() and undo(), so no GTP subprocess is needed.
class FakeEngine : public Engine {
public:
    explicit FakeEngine(const std::string& name = "Fake") : Engine(name) {}

    Move genmove(const Color&) override { return Move(Move::PASS, Color::BLACK); }
    float final_score() override { return 0.0f; }
    void applyTerritory(Board&) override {}

    bool play(const Move&) override { return true; }
    bool undo() override { return true; }
    bool clear() override { return true; }
};

/// A GobanModel with a navigator wired to a single coach, and its daily session
/// file redirected into a scratch directory so nothing touches games/.
struct Fixture {
    std::filesystem::path dir;
    GobanModel model;
    GameNavigator::ObserverList observers;
    FakeEngine coach;
    std::vector<Player*> players;
    GameNavigator nav;

    explicit Fixture(int boardSize = 19)
        : model(boardSize),
          nav(model,
              [this]() -> Engine* { return &coach; },
              [this]() { return players; },
              observers,
              [](Engine*) { return true; })
    {
        quietLogging();
        static int counter = 0;
        const auto base = std::filesystem::temp_directory_path();
        do {
            std::ostringstream name;
            name << "goban_gamephase_test_" << counter++;
            dir = base / name.str();
        } while (std::filesystem::exists(dir));
        std::filesystem::create_directories(dir);
        model.game.setDefaultFileName((dir / (todayStamp() + ".sgf")).string());
    }

    ~Fixture() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    /// Load a record and enter review, which is what the application does:
    /// GameThread::applyLoadedGame() notifies onBoardSized(), and that calls
    /// enterReview(). Loading without it would leave the phase and the record
    /// disagreeing in a way no code path can actually produce.
    bool load(const std::string& name, bool startAtRoot = false) {
        GameRecord::SGFGameInfo info;
        const bool ok = model.game.loadFromSGF(fixture(name), info, 0, startAtRoot);
        if (ok) model.enterReview();
        return ok;
    }

    /// What GobanControl::newGameNow() does to the model: size the board (which
    /// enters review) and then replace the record.
    void newGame(int boardSize) {
        model.onBoardSized(boardSize);
        model.createNewRecord();
    }

    /// Drive the model into a phase the way the application would.
    void enter(GamePhase phase) {
        switch (phase) {
            case GamePhase::Setup:
            case GamePhase::Paused:   model.enterReview(); break;
            case GamePhase::Playing:  model.start(); break;
            case GamePhase::Finished: model.endGame(GameState::DOUBLE_PASS); break;
        }
        REQUIRE(model.phase() == phase);
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// The phase is authoritative; the old flags are functions of it
// ---------------------------------------------------------------------------

TEST_CASE("the compatibility accessors are pure functions of the phase") {
    Fixture f(9);
    f.newGame(9);

    for (GamePhase phase : {GamePhase::Setup, GamePhase::Playing,
                            GamePhase::Finished}) {
        f.enter(phase);
        CHECK(f.model.isStarted()   == (phase == GamePhase::Playing));
        CHECK(f.model.isGameOver()  == (phase == GamePhase::Finished));
        CHECK(static_cast<bool>(f.model) == (phase == GamePhase::Playing));
    }

    // Paused needs a non-empty record to be reachable at all.
    REQUIRE(f.load("simple.sgf"));
    REQUIRE(f.model.phase() == GamePhase::Paused);
    CHECK_FALSE(f.model.isStarted());
    CHECK_FALSE(f.model.isGameOver());
    CHECK_FALSE(static_cast<bool>(f.model));
}

TEST_CASE("started and game-over can no longer both be true") {
    // The old model let a live game end without clearing `started`, so the pair
    // (true, true) was reachable and every reader had to know isGameOver won.
    // Now Finished is one state and isStarted() is false in it.
    Fixture f(9);
    f.newGame(9);
    f.model.state.black = "Black";
    f.model.state.white = "White";
    f.model.start();
    REQUIRE(f.model.isStarted());

    f.model.onGameMove(Move(Move::PASS, Color::BLACK), "");
    f.model.onGameMove(Move(Move::PASS, Color::WHITE), "");

    CHECK(f.model.phase() == GamePhase::Finished);
    CHECK(f.model.isGameOver());
    CHECK_FALSE(f.model.isStarted());
}

TEST_CASE("komi is editable in Setup and Paused, and nowhere else") {
    // The old guard was `!started`, which let komi through on a finished game
    // that had been *loaded* but not on one that had just ended — the same
    // position, two answers. Both are refused now: RE was scored with the komi
    // that was in force, so changing it after the fact corrupts the record.
    Fixture f(9);
    f.newGame(9);
    f.model.state.komi = 6.5f;

    f.model.onKomiChange(0.5f);                      // Setup
    CHECK(f.model.state.komi == doctest::Approx(0.5f));

    REQUIRE(f.load("simple.sgf"));
    REQUIRE(f.model.phase() == GamePhase::Paused);
    f.model.onKomiChange(7.5f);
    CHECK(f.model.state.komi == doctest::Approx(7.5f));

    f.model.start();
    f.model.onKomiChange(1.5f);
    CHECK(f.model.state.komi == doctest::Approx(7.5f));   // refused while playing

    f.model.endGame(GameState::DOUBLE_PASS);
    f.model.onKomiChange(2.5f);
    CHECK(f.model.state.komi == doctest::Approx(7.5f));   // and once finished
}

TEST_CASE("the resting phase follows the record, not the caller") {
    Fixture f(9);
    f.newGame(9);
    f.model.enterReview();
    CHECK(f.model.phase() == GamePhase::Setup);      // empty record

    // A loaded game viewed from its root: moveCount() is 0 here too, so the
    // view position alone cannot tell these apart — the root's children can.
    REQUIRE(f.load("simple.sgf", /*startAtRoot=*/true));
    REQUIRE(f.model.game.moveCount() == 0);
    CHECK(f.model.phase() == GamePhase::Paused);

    // And at the end of that same unfinished game.
    REQUIRE(f.load("simple.sgf"));
    REQUIRE(f.model.game.moveCount() == 4);
    REQUIRE(f.model.game.isAtEndOfNavigation());
    CHECK(f.model.phase() == GamePhase::Paused);
}

TEST_CASE("endGame refuses to finish a game for no reason") {
    Fixture f(9);
    f.newGame(9);
    f.model.start();

    f.model.endGame(GameState::NO_REASON);
    CHECK(f.model.phase() == GamePhase::Playing);    // refused, not applied
    CHECK(f.model.state.reason == GameState::NO_REASON);

    f.model.endGame(GameState::RESIGNATION);
    CHECK(f.model.phase() == GamePhase::Finished);
    CHECK(f.model.state.reason == GameState::RESIGNATION);
}

TEST_CASE("phase names are stable") {
    CHECK(std::string(phaseName(GamePhase::Setup)) == "setup");
    CHECK(std::string(phaseName(GamePhase::Playing)) == "playing");
    CHECK(std::string(phaseName(GamePhase::Paused)) == "paused");
    CHECK(std::string(phaseName(GamePhase::Finished)) == "finished");
}

TEST_CASE("a freshly constructed model starts in Setup") {
    // Step 1 found this reported Finished, because isGameOver defaulted to true
    // as a stand-in for "not ready yet". The game loop's other guard, !model,
    // is what actually holds it off, and that still holds here.
    GobanModel fresh(9);
    CHECK(fresh.phase() == GamePhase::Setup);
    CHECK_FALSE(static_cast<bool>(fresh));
}

// ---------------------------------------------------------------------------
// The transition table: operation -> phase
// ---------------------------------------------------------------------------

TEST_CASE("a new game returns to Setup from any phase") {
    Fixture f(9);

    f.newGame(9);
    CHECK(f.model.phase() == GamePhase::Setup);

    f.model.start();
    REQUIRE(f.model.phase() == GamePhase::Playing);
    f.newGame(9);
    CHECK(f.model.phase() == GamePhase::Setup);

    f.enter(GamePhase::Finished);
    f.newGame(9);
    CHECK(f.model.phase() == GamePhase::Setup);
}

TEST_CASE("sizing the board alone does not reach Setup while a record survives") {
    // onBoardSized() calls enterReview(), which resolves against the record —
    // and the old record is still attached at that point, so on its own it lands
    // in Paused. Only newGameNow()'s second half, createNewRecord(), completes
    // the transition to Setup.
    Fixture f(9);
    REQUIRE(f.load("simple.sgf"));
    f.model.onBoardSized(9);
    CHECK(f.model.phase() == GamePhase::Paused);

    f.model.createNewRecord();
    CHECK(f.model.phase() == GamePhase::Setup);
}

TEST_CASE("start enters Playing from Setup, Paused and Finished alike") {
    Fixture f(9);

    f.newGame(9);
    f.model.start();
    CHECK(f.model.phase() == GamePhase::Playing);

    REQUIRE(f.load("simple.sgf"));
    REQUIRE(f.model.phase() == GamePhase::Paused);
    f.model.start();
    CHECK(f.model.phase() == GamePhase::Playing);

    f.enter(GamePhase::Finished);
    f.model.start();
    CHECK(f.model.phase() == GamePhase::Playing);
    CHECK(f.model.state.reason == GameState::NO_REASON);
}

TEST_CASE("pause leaves Playing but cannot leave Finished") {
    Fixture f(9);
    REQUIRE(f.load("simple.sgf"));

    f.model.start();
    REQUIRE(f.model.phase() == GamePhase::Playing);
    f.model.pause();
    CHECK(f.model.phase() == GamePhase::Paused);

    // pause() means "stop playing", not "un-finish". Leaving Finished is
    // enterReview()'s job, and navigateBack() is its only caller that matters.
    f.enter(GamePhase::Finished);
    f.model.pause();
    CHECK(f.model.phase() == GamePhase::Finished);
    f.model.enterReview();
    CHECK(f.model.phase() == GamePhase::Paused);
}

TEST_CASE("pausing a game with nothing recorded falls back to Setup") {
    Fixture f(9);
    f.newGame(9);
    f.model.start();
    f.model.pause();
    CHECK(f.model.phase() == GamePhase::Setup);
}

TEST_CASE("a double pass finishes the game") {
    Fixture f(9);
    f.newGame(9);
    f.model.state.black = "Black";
    f.model.state.white = "White";
    f.model.start();
    REQUIRE(f.model.phase() == GamePhase::Playing);

    f.model.onGameMove(Move(Move::PASS, Color::BLACK), "");
    CHECK(f.model.phase() == GamePhase::Playing);   // one pass is not the end

    f.model.onGameMove(Move(Move::PASS, Color::WHITE), "");
    CHECK(f.model.phase() == GamePhase::Finished);
    CHECK(f.model.state.reason == GameState::DOUBLE_PASS);
}

TEST_CASE("a resignation finishes the game") {
    Fixture f(9);
    f.newGame(9);
    f.model.state.black = "Black";
    f.model.state.white = "White";
    f.model.start();

    f.model.onGameMove(Move(Position(3, 3), Color::BLACK), "");
    REQUIRE(f.model.phase() == GamePhase::Playing);

    f.model.onGameMove(Move(Move::RESIGN, Color::WHITE), "");
    CHECK(f.model.phase() == GamePhase::Finished);
    CHECK(f.model.state.reason == GameState::RESIGNATION);
}

TEST_CASE("navigateBack leaves a finished game for Paused") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf"));
    f.enter(GamePhase::Finished);

    REQUIRE(f.nav.navigateBack());
    CHECK(f.model.phase() == GamePhase::Paused);
}

TEST_CASE("navigateToStart pauses, and stays Paused at the root of a real game") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf"));
    f.enter(GamePhase::Finished);

    REQUIRE(f.nav.navigateToStart());
    REQUIRE(f.model.game.moveCount() == 0);
    // The record is at its root but is not empty, so this is Paused, not Setup.
    CHECK(f.model.phase() == GamePhase::Paused);
}

TEST_CASE("navigateForward restores Finished only at the end of a finished game") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));
    REQUIRE(f.model.phase() == GamePhase::Paused);

    for (int i = 0; i < 3; ++i) {
        REQUIRE(f.nav.navigateForward());
        CHECK(f.model.phase() == GamePhase::Paused);
    }
    REQUIRE(f.nav.navigateForward());               // onto the closing pass
    REQUIRE(f.model.game.isAtEndOfNavigation());
    REQUIRE(f.model.game.hasGameResult());
    CHECK(f.model.phase() == GamePhase::Finished);
}

TEST_CASE("navigateForward keeps Playing once the user has started") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));
    f.model.start();

    for (int i = 0; i < 4; ++i) REQUIRE(f.nav.navigateForward());
    REQUIRE(f.model.game.isAtEndOfNavigation());
    // The `!isStarted()` guard on the restore means the user's intent to keep
    // playing wins over the record's result.
    CHECK(f.model.phase() == GamePhase::Playing);
}

TEST_CASE("navigateToEnd restores Finished at the end of a finished game") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));

    REQUIRE(f.nav.navigateToEnd());
    CHECK(f.model.phase() == GamePhase::Finished);
}

TEST_CASE("navigateToTreePath restores Finished at the end of a finished game") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));

    const size_t mainLine = f.model.game.getLoadedMovesCount();
    REQUIRE(f.nav.navigateToTreePath(static_cast<int>(mainLine), {}));
    CHECK(f.model.phase() == GamePhase::Finished);
}

TEST_CASE("navigateToVariation enters Playing only when it promotes the branch") {
    Fixture f(9);
    REQUIRE(f.load("simple.sgf", /*startAtRoot=*/true));
    REQUIRE(f.model.phase() == GamePhase::Paused);

    // Non-promoted branch (tsumego exploration): stays in navigation mode.
    auto explored = f.nav.navigateToVariation(Move(Position(7, 7), Color::BLACK), false);
    REQUIRE(explored.success);
    CHECK(f.model.phase() == GamePhase::Paused);

    // Promoted branch: the user is playing this line for real.
    auto promoted = f.nav.navigateToVariation(Move(Position(2, 2), Color::WHITE), true);
    REQUIRE(promoted.success);
    CHECK(f.model.phase() == GamePhase::Playing);
}
