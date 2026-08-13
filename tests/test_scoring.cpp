// Scoring, and the ways it is allowed to fail.
//
// Every case here comes from one session (2026-08-13) in which a loaded game
// sat on "Calculating score…" indefinitely. The chain was four defects deep, so
// the tests are grouped by the one they pin.

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "Board.h"
#include "gtpclient.h"
#include "player.h"
#include "mock_engine_path.h"

namespace {

void quietLogging() {
    static bool done = false;
    if (!done) {
        spdlog::set_level(spdlog::level::off);
        done = true;
    }
}

struct MockSpawn {
    std::string exe;
    std::string dir;
};

MockSpawn mockSpawn() {
    const std::filesystem::path full(goban_test::mockEnginePath());
    REQUIRE_MESSAGE(!full.empty(),
                    "mock engine path is unset; run via tests/compile_one.sh or cmake");
    REQUIRE_MESSAGE(std::filesystem::exists(full),
                    "mock engine binary not found: " << full.string());
    return {full.filename().string(), full.parent_path().string()};
}

std::unique_ptr<GtpEngine> makeEngine(const std::string& args = "") {
    quietLogging();
    const MockSpawn spawn = mockSpawn();
    return std::make_unique<GtpEngine>(spawn.exe, args, spawn.dir, "Mock",
                                       nlohmann::json::array());
}

}  // namespace

// --- final_score: a failure is not a score of zero ---------------------------

TEST_CASE("final_score reports a real result") {
    auto engine = makeEngine();
    REQUIRE(engine->boardsize(9));
    REQUIRE(engine->komi(6.5f));

    const std::optional<float> score = engine->final_score();
    REQUIRE(score.has_value());
    // Empty board, area scoring: komi decides, and White leading is negative.
    CHECK(*score == doctest::Approx(-6.5f));
}

TEST_CASE("a drawn game scores zero rather than reporting failure") {
    // The reason final_score() returns an optional at all. Jigo is a legitimate
    // result that a bare float cannot tell apart from "the engine could not
    // answer" — and the old code returned 0.0f for both.
    auto engine = makeEngine();
    REQUIRE(engine->boardsize(9));
    REQUIRE(engine->komi(0.0f));

    const std::optional<float> score = engine->final_score();
    REQUIRE_MESSAGE(score.has_value(), "jigo must be a value, not an absent score");
    CHECK(*score == doctest::Approx(0.0f));
}

TEST_CASE("an engine that cannot score reports no score at all") {
    auto engine = makeEngine("--fail-on final_score");
    REQUIRE(engine->boardsize(9));

    CHECK_FALSE(engine->final_score().has_value());
}

// --- applyTerritory: shading and score succeed or fail independently ---------

TEST_CASE("applyTerritory scores a position it can score") {
    auto engine = makeEngine();
    REQUIRE(engine->boardsize(9));
    REQUIRE(engine->komi(6.5f));

    Board board(9);
    CHECK(engine->applyTerritory(board));
    CHECK(board.territoryReady);
    CHECK(board.showTerritory);
    CHECK(board.score == doctest::Approx(-6.5f));
}

TEST_CASE("a scoreless engine does not claim a drawn game") {
    // Pachi answering "? unclear groups" to final_score is what started the
    // whole failure: applyTerritory set territoryReady anyway, leaving a score
    // of 0.0 that read as a legitimate draw and sent scoring looking for a
    // second opinion.
    auto engine = makeEngine("--fail-on final_score");
    REQUIRE(engine->boardsize(9));

    Board board(9);
    CHECK_FALSE(engine->applyTerritory(board));
    CHECK_FALSE(board.territoryReady);
    // The dead-stone list did arrive, so the shading is still valid; only the
    // number is missing. The caller uses exactly this to decide whether asking
    // another engine for a score is worth it.
    CHECK(board.showTerritory);
}

TEST_CASE("an engine without final_status_list shows no territory") {
    auto engine = makeEngine("--unknown final_status_list");
    REQUIRE(engine->boardsize(9));

    Board board(9);
    CHECK_FALSE(engine->applyTerritory(board));
    CHECK_FALSE(board.territoryReady);
    CHECK_FALSE(board.showTerritory);
}

// --- the retry latch ---------------------------------------------------------

TEST_CASE("territoryFailed travels with the position and clears itself") {
    // GameThread::processScoring() gives up on a position by setting this, or it
    // would retry ten times a second forever. It must survive the copy into the
    // model's board, and a fresh board must not inherit it — that is what gives
    // scoring another chance at the next position.
    Board scored(9);
    CHECK_FALSE(scored.territoryFailed);
    scored.territoryFailed = true;

    Board model(9);
    model.updateStones(scored);
    CHECK(model.territoryFailed);

    Board nextPosition(9);
    CHECK_FALSE(nextPosition.territoryFailed);
    model.updateStones(nextPosition);
    CHECK_FALSE(model.territoryFailed);
}

// --- the scoring timeout -----------------------------------------------------

TEST_CASE("the scoring timeout bounds scoring without raising other limits") {
    auto engine = makeEngine();

    SUBCASE("it caps a generous general timeout") {
        engine->setCommandTimeout(GtpClient::DEFAULT_COMMAND_TIMEOUT_MS);
        CHECK(engine->scoringTimeout() == GtpClient::DEFAULT_SCORING_TIMEOUT_MS);
    }

    SUBCASE("it never raises a stricter one") {
        engine->setCommandTimeout(2000);
        CHECK(engine->scoringTimeout() == 2000);
    }

    SUBCASE("it overrides waiting forever, which is the point of having it") {
        engine->setCommandTimeout(-1);
        CHECK(engine->scoringTimeout() == GtpClient::DEFAULT_SCORING_TIMEOUT_MS);
    }

    SUBCASE("a per-engine override is honoured") {
        engine->setScoringTimeout(1500);
        engine->setCommandTimeout(GtpClient::DEFAULT_COMMAND_TIMEOUT_MS);
        CHECK(engine->scoringTimeout() == 1500);
    }
}

TEST_CASE("ScopedTimeout restores the previous limit") {
    auto engine = makeEngine();
    engine->setCommandTimeout(4321);
    {
        const GtpClient::ScopedTimeout bounded(*engine, 100);
        CHECK(engine->getCommandTimeout() == 100);
    }
    CHECK(engine->getCommandTimeout() == 4321);
}

// --- the engine folder -------------------------------------------------------

#ifndef _WIN32
TEST_CASE("a missing engine folder is not fatal when the command resolves anyway") {
    // The stock configuration ships "path": "./engine/gnugo" with "command":
    // "gnugo", and on a machine where GNU Go comes from the distribution that
    // folder does not exist. It used to work — the child's chdir() failed
    // silently and execvp() found the engine on PATH — until the #55 fix made a
    // missing folder throw. The engine then failed to load, an arbitrary engine
    // was promoted to coach, and scoring hung.
    quietLogging();
    const MockSpawn spawn = mockSpawn();

    const char* previous = std::getenv("PATH");
    const std::string oldPath = previous ? previous : "";
    setenv("PATH", (spawn.dir + ":" + oldPath).c_str(), 1);

    std::unique_ptr<GtpClient> client;
    REQUIRE_NOTHROW(client = std::make_unique<GtpClient>(
        spawn.exe, "--name OnPath", "./no-such-engine-folder", nlohmann::json::array()));

    const GtpClient::CommandOutput out = client->name();
    REQUIRE(GtpClient::success(out));
    CHECK(out.front().find("OnPath") != std::string::npos);

    setenv("PATH", oldPath.c_str(), 1);
}
#endif
