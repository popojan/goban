// Tests for GamePhase — step 1 of docs/adr/0002-explicit-game-state.md.
//
// GobanModel::phase() is *derived* from the `started` / `isGameOver` pair, so
// none of this can change behaviour. Its whole purpose is to write the implicit
// state machine down: the first half of this file pins the truth table (which
// flag combinations map to which phase, and that `operator bool()` is exactly
// "Playing"), the second half pins the transition table (what each operation
// that writes those flags does to the phase).
//
// When step 2 inverts the relationship and makes the phase authoritative, these
// are the tests that say whether the inversion preserved behaviour. Anything
// here that looks wrong is a bug in today's code, not in the test — the two
// cases we already know about are called out inline:
//
//   * a freshly constructed model reports Finished, not Setup;
//   * pause() cannot leave Finished, so a finished game that is paused stays
//     finished until something clears isGameOver explicitly.
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

    bool load(const std::string& name, bool startAtRoot = false) {
        GameRecord::SGFGameInfo info;
        return model.game.loadFromSGF(fixture(name), info, 0, startAtRoot);
    }

    /// What GobanControl::newGameNow() does to the model: size the board (which
    /// clears the flags) and then replace the record.
    void newGame(int boardSize) {
        model.onBoardSized(boardSize);
        model.createNewRecord();
    }

    void setFlags(bool started, bool gameOver) {
        model.started = started;
        model.isGameOver = gameOver;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// The truth table: flags -> phase
// ---------------------------------------------------------------------------

TEST_CASE("the flag pair maps onto exactly four phases") {
    Fixture f(9);
    f.newGame(9);

    f.setFlags(false, false);
    CHECK(f.model.phase() == GamePhase::Setup);      // empty record

    f.setFlags(true, false);
    CHECK(f.model.phase() == GamePhase::Playing);

    f.setFlags(false, true);
    CHECK(f.model.phase() == GamePhase::Finished);

    // Reachable: a move that ends the game sets isGameOver and leaves started
    // alone. isGameOver has to win, or the loop would keep calling genmove.
    f.setFlags(true, true);
    CHECK(f.model.phase() == GamePhase::Finished);
}

TEST_CASE("with both flags clear the record decides between Setup and Paused") {
    Fixture f(9);
    f.newGame(9);
    f.setFlags(false, false);
    CHECK(f.model.phase() == GamePhase::Setup);

    // A loaded game viewed from its root: moveCount() is 0 here too, so the
    // view position alone cannot tell these apart — the root's children can.
    REQUIRE(f.load("simple.sgf", /*startAtRoot=*/true));
    REQUIRE(f.model.game.moveCount() == 0);
    f.setFlags(false, false);
    CHECK(f.model.phase() == GamePhase::Paused);

    // And at the end of that same unfinished game.
    REQUIRE(f.load("simple.sgf"));
    REQUIRE(f.model.game.moveCount() == 4);
    REQUIRE(f.model.game.isAtEndOfNavigation());
    f.setFlags(false, false);
    CHECK(f.model.phase() == GamePhase::Paused);
}

TEST_CASE("operator bool is exactly the Playing phase") {
    Fixture f(9);
    f.newGame(9);
    for (bool started : {false, true}) {
        for (bool gameOver : {false, true}) {
            f.setFlags(started, gameOver);
            CHECK(static_cast<bool>(f.model)
                  == (f.model.phase() == GamePhase::Playing));
        }
    }
}

TEST_CASE("phase names are stable") {
    CHECK(std::string(phaseName(GamePhase::Setup)) == "setup");
    CHECK(std::string(phaseName(GamePhase::Playing)) == "playing");
    CHECK(std::string(phaseName(GamePhase::Paused)) == "paused");
    CHECK(std::string(phaseName(GamePhase::Finished)) == "finished");
}

TEST_CASE("a freshly constructed model reports Finished") {
    // Wart, pinned deliberately: isGameOver defaults to true as a stand-in for
    // "not ready yet", so the very first phase is Finished rather than Setup.
    // Nothing depends on it — the game loop's other guard, !model, already
    // covers this window — but step 2 has to decide it on purpose.
    GobanModel fresh(9);
    CHECK(fresh.phase() == GamePhase::Finished);
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

    f.setFlags(true, true);
    REQUIRE(f.model.phase() == GamePhase::Finished);
    f.newGame(9);
    CHECK(f.model.phase() == GamePhase::Setup);
}

TEST_CASE("sizing the board alone does not reach Setup while a record survives") {
    // onBoardSized() clears both flags but leaves game alone, so on its own it
    // lands in Paused. Only newGameNow()'s second half — createNewRecord() —
    // completes the transition. Worth pinning: it is the one place where the
    // phase depends on something outside the two flags.
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
    f.setFlags(false, false);
    REQUIRE(f.model.phase() == GamePhase::Paused);
    f.model.start();
    CHECK(f.model.phase() == GamePhase::Playing);

    f.setFlags(false, true);
    REQUIRE(f.model.phase() == GamePhase::Finished);
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

    // pause() only clears `started`. Navigating back is what also clears
    // isGameOver; a bare pause() on a finished game is a no-op phase-wise.
    f.setFlags(true, true);
    f.model.pause();
    CHECK(f.model.phase() == GamePhase::Finished);
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
    f.setFlags(true, true);
    REQUIRE(f.model.phase() == GamePhase::Finished);

    REQUIRE(f.nav.navigateBack());
    CHECK(f.model.phase() == GamePhase::Paused);
}

TEST_CASE("navigateToStart pauses, and stays Paused at the root of a real game") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf"));
    f.setFlags(true, true);

    REQUIRE(f.nav.navigateToStart());
    REQUIRE(f.model.game.moveCount() == 0);
    // The record is at its root but is not empty, so this is Paused, not Setup.
    CHECK(f.model.phase() == GamePhase::Paused);
}

TEST_CASE("navigateForward restores Finished only at the end of a finished game") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));
    f.setFlags(false, false);
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
    // The `!model.started` guard on the restore means the user's intent to keep
    // playing wins over the record's result.
    CHECK(f.model.phase() == GamePhase::Playing);
}

TEST_CASE("navigateToEnd restores Finished at the end of a finished game") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));
    f.setFlags(false, false);

    REQUIRE(f.nav.navigateToEnd());
    CHECK(f.model.phase() == GamePhase::Finished);
}

TEST_CASE("navigateToTreePath restores Finished at the end of a finished game") {
    Fixture f(9);
    REQUIRE(f.load("double_pass.sgf", /*startAtRoot=*/true));
    f.setFlags(false, false);

    const size_t mainLine = f.model.game.getLoadedMovesCount();
    REQUIRE(f.nav.navigateToTreePath(static_cast<int>(mainLine), {}));
    CHECK(f.model.phase() == GamePhase::Finished);
}

TEST_CASE("navigateToVariation enters Playing only when it promotes the branch") {
    Fixture f(9);
    REQUIRE(f.load("simple.sgf", /*startAtRoot=*/true));
    f.setFlags(false, false);
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
