// GtpClient integration tests, driven by tests/mock_gtp_engine.cpp.
//
// These are the first tests that exercise real process spawning and GTP
// round-trips. They are hermetic: no GNU Go, Pachi or KataGo required.

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "gtpclient.h"
#include "mock_engine_path.h"

namespace {

// GtpClient logs every command and response at info level, which drowns the
// test report. Quieten it once, on first use.
void quietLogging() {
    static bool done = false;
    if (!done) {
        spdlog::set_level(spdlog::level::off);
        done = true;
    }
}

// findExecutable() resolves <path>/<exe> and then has Process chdir into
// <path>, so the engine must be handed over split into directory and name.
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

// Convenience: build a client on the mock with the given extra arguments.
std::unique_ptr<GtpClient> makeClient(const std::string& args = "") {
    quietLogging();
    const MockSpawn spawn = mockSpawn();
    return std::make_unique<GtpClient>(spawn.exe, args, spawn.dir, nlohmann::json::array());
}

// The payload of a successful single-line GTP response, with the "= " stripped.
std::string body(const GtpClient::CommandOutput& out) {
    if (out.empty()) return {};
    std::string first = out.front();
    if (first.rfind("= ", 0) == 0) return first.substr(2);
    if (first == "=") return {};
    return first;
}

}  // namespace

TEST_CASE("engine identifies itself") {
    auto client = makeClient("--name TestBot --version 9.9");
    CHECK(body(client->name()) == "TestBot");
    CHECK(body(client->version()) == "9.9");
}

TEST_CASE("success() distinguishes = from ? responses") {
    auto client = makeClient();
    CHECK(GtpClient::success(client->issueCommand("boardsize 9")));
    // A syntactically valid but unacceptable size must be reported as failure.
    CHECK_FALSE(GtpClient::success(client->issueCommand("boardsize 42")));
    CHECK_FALSE(GtpClient::success(client->issueCommand("no_such_command")));
}

TEST_CASE("game setup commands round-trip") {
    auto client = makeClient();
    CHECK(GtpClient::success(client->issueCommand("boardsize 13")));
    CHECK(GtpClient::success(client->issueCommand("clear_board")));
    CHECK(GtpClient::success(client->issueCommand("komi 6.5")));
}

TEST_CASE("play accepts legal moves and rejects illegal ones") {
    auto client = makeClient();
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 9")));

    SUBCASE("legal move accepted") {
        CHECK(GtpClient::success(client->issueCommand("play B E5")));
    }

    SUBCASE("occupied point rejected") {
        REQUIRE(GtpClient::success(client->issueCommand("play B E5")));
        CHECK_FALSE(GtpClient::success(client->issueCommand("play W E5")));
    }

    SUBCASE("off-board coordinate rejected") {
        CHECK_FALSE(GtpClient::success(client->issueCommand("play B Z99")));
    }

    SUBCASE("pass accepted") {
        CHECK(GtpClient::success(client->issueCommand("play B pass")));
    }
}

TEST_CASE("scripted genmove is deterministic") {
    auto client = makeClient("--script E5 E4 pass");
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 9")));
    CHECK(body(client->issueCommand("genmove B")) == "E5");
    CHECK(body(client->issueCommand("genmove W")) == "E4");
    CHECK(body(client->issueCommand("genmove B")) == "pass");
    // Script exhausted: falls back to first legal point, still deterministic.
    CHECK(body(client->issueCommand("genmove W")) == "A1");
}

TEST_CASE("undo reverts the last move") {
    auto client = makeClient();
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 9")));
    REQUIRE(GtpClient::success(client->issueCommand("play B E5")));
    CHECK(GtpClient::success(client->issueCommand("undo")));
    // The point is free again, so White may take it.
    CHECK(GtpClient::success(client->issueCommand("play W E5")));
}

TEST_CASE("undo on an empty board fails rather than corrupting state") {
    auto client = makeClient();
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 9")));
    CHECK_FALSE(GtpClient::success(client->issueCommand("undo")));
}

TEST_CASE("fixed_handicap returns the placed vertices") {
    auto client = makeClient();
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 19")));
    const auto out = client->issueCommand("fixed_handicap 4");
    REQUIRE(GtpClient::success(out));
    const std::string vertices = body(out);
    CHECK(vertices.find("D4") != std::string::npos);
    CHECK(vertices.find("Q16") != std::string::npos);
    CHECK(vertices.find("D16") != std::string::npos);
    CHECK(vertices.find("Q4") != std::string::npos);

    SUBCASE("an unsupported handicap count is rejected") {
        auto fresh = makeClient();
        REQUIRE(GtpClient::success(fresh->issueCommand("boardsize 19")));
        CHECK_FALSE(GtpClient::success(fresh->issueCommand("fixed_handicap 1")));
    }
}

TEST_CASE("final_score reflects komi") {
    auto client = makeClient();
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 9")));
    REQUIRE(GtpClient::success(client->issueCommand("komi 6.5")));
    // Empty board, area scoring: nobody owns anything, so komi decides.
    CHECK(body(client->issueCommand("final_score")) == "W+6.5");
}

TEST_CASE("an engine without final_status_list degrades gracefully") {
    // Mirrors the real graceful-degradation path in GtpEngine::applyTerritory,
    // which warns and shows no territory rather than failing the game.
    auto client = makeClient("--unknown final_status_list");
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 9")));
    CHECK_FALSE(GtpClient::success(client->issueCommand("final_status_list dead")));
    // The engine must still be usable for everything else afterwards.
    CHECK(GtpClient::success(client->issueCommand("play B E5")));
}

TEST_CASE("a wedged engine times out instead of hanging forever") {
    // Before the timeout existed, issueCommand() looped on a blocking read with
    // no way out, so an engine that never answered hung the game thread — the
    // issue #45 failure mode, and fatal for unattended runs.
    auto client = makeClient("--hang-on genmove");
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 9")));

    client->setCommandTimeout(700);

    const auto start = std::chrono::steady_clock::now();
    const auto out = client->issueCommand("genmove B");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK_FALSE(GtpClient::success(out));
    // Allow a little slack: steady_clock and poll() round in opposite
    // directions, so an exact >= 700 is off by a millisecond in practice.
    CHECK(elapsed >= 650);
    CHECK(elapsed < 5000);   // returned promptly, not on some far longer fallback

    // The engine is killed on timeout: a late reply would otherwise be read as
    // the answer to the next command, which is worse than a dead engine.
    CHECK_FALSE(GtpClient::success(client->issueCommand("name")));
}

TEST_CASE("a slow but responsive engine is not cut off early") {
    // The timeout must not punish an engine that is merely slow.
    auto client = makeClient("--delay-ms 300 --script E5");
    client->setCommandTimeout(5000);
    REQUIRE(GtpClient::success(client->issueCommand("boardsize 9")));
    CHECK(body(client->issueCommand("genmove B")) == "E5");
}

TEST_CASE("stderr output is captured and passed through the filters") {
    // KataGo reports analysis on stderr; goban scrapes it with the regex
    // filters from config/*.json. This checks the plumbing end to end using
    // the same filter shape the shipped configuration uses.
    quietLogging();
    const MockSpawn spawn = mockSpawn();
    const nlohmann::json messages = nlohmann::json::array({
        {{"regex", "^:\\s+T.*--\\s*([A-Z0-9]+)"}, {"output", "$1"}, {"var", "$primaryMove"}},
    });

    // Absolute path: Process chdirs into the engine's directory.
    const auto stderrFile = std::filesystem::temp_directory_path() / "goban_mock_stderr.txt";
    {
        std::ofstream out(stderrFile);
        REQUIRE(out.good());
        out << ": T 1234 --  E5\n";
    }

    GtpClient client(spawn.exe, "--script E5 --stderr-file " + stderrFile.string(),
                     spawn.dir, messages);
    REQUIRE(GtpClient::success(client.issueCommand("boardsize 9")));
    CHECK(body(client.issueCommand("genmove B")) == "E5");
    // The stderr reader runs on its own thread; give it a moment to drain
    // before the client is torn down.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
