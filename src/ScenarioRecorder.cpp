#include "ScenarioRecorder.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>

#include <spdlog/spdlog.h>

ScenarioRecorder& ScenarioRecorder::instance() {
    static ScenarioRecorder recorder;
    return recorder;
}

void ScenarioRecorder::recordAction(const std::string& name,
                                    const std::vector<std::string>& args,
                                    const nlohmann::json& stateBefore) {
    if (!enabled || suppressDepth > 0) return;

    std::ostringstream line;
    line << name;
    for (const auto& a : args) line << ' ' << a;

    std::lock_guard<std::mutex> lock(mutex);
    entries.push_back({line.str(), {}, stateBefore});
    while (entries.size() > MAX_ENTRIES) {
        entries.pop_front();
        truncated = true;
    }
}

void ScenarioRecorder::recordState(const nlohmann::json& state) {
    if (!enabled) return;
    std::lock_guard<std::mutex> lock(mutex);
    if (entries.empty()) return;
    entries.back().stateJson = state.dump();
}

void ScenarioRecorder::recordEngineMove(const std::string& engineName,
                                        const std::string& vertex) {
    if (!enabled) return;
    std::lock_guard<std::mutex> lock(mutex);
    // Only the sequence matters for replay; the name is logged for context.
    engineMoves.push_back(vertex);
    spdlog::debug("recorder: {} played {}", engineName, vertex);
}

void ScenarioRecorder::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    entries.clear();
    engineMoves.clear();
    truncated = false;
}

size_t ScenarioRecorder::size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return entries.size();
}


bool ScenarioRecorder::writePrologue(std::ostream& out, const nlohmann::json& state) {
    if (state.is_null() || state.empty()) {
        out << "# WARNING: the starting state was not captured, so this report is\n"
            << "# NOT directly replayable — it records what was done, not what it\n"
            << "# was done to. Reconstruct the starting position by hand.\n";
        return false;
    }

    const std::string sgf = state.value("sgf_file", std::string());
    // Three modes now, and only two of them can be *named*: a puzzle is entered
    // by opening one, so it has to be reached through the load rather than
    // through `game_mode`. Restoring it with load_sgf and a mode change would
    // reproduce neither the cursor at the root nor the auto-played setup move.
    const std::string mode = state.value("mode", std::string("match"));
    const bool tsumego = mode == "tsumego";

    out << "# --- prologue: puts a fresh instance back at the starting point ---\n";
    if (!sgf.empty()) {
        out << (tsumego ? "load_tsumego " : "load_sgf ")
            << sgf << " " << state.value("game_index", 0) << "\n";
        out << "settle\n";
        out << "# NOTE: this refers to an external SGF. If it has since changed or\n"
            << "# moved, the replay will not match.\n";
    } else {
        out << "new_game " << state.value("board_size", 19) << "\n";
        out << "settle\n";
        if (tsumego) {
            out << "# WARNING: the recording was made in tsumego mode but no file\n"
                << "# was loaded, so the problem cannot be reproduced here.\n";
        }
    }

    out << "switch_player black " << state.value("black_player", std::string("Human")) << "\n";
    out << "switch_player white " << state.value("white_player", std::string("Human")) << "\n";
    out << "settle\n";

    // Named rather than toggled: `toggle_explore_mode` asserts nothing about
    // where it started, and there are three modes for it to start in now.
    if (!tsumego && mode != "match") {
        out << "game_mode " << mode << "\n";
    }

    const size_t movesIn = state.value("move_count", 0);
    if (movesIn > 0 && sgf.empty()) {
        out << "# WARNING: the recording starts " << movesIn << " move(s) into a game\n"
            << "# that was not loaded from a file, so those moves are not reproduced\n"
            << "# here. Re-record from a new game if the position matters.\n";
    }

    out << "# --- end prologue ---\n\n";
    return true;
}

std::string ScenarioRecorder::save(const std::string& directory) {
    std::lock_guard<std::mutex> lock(mutex);

    if (entries.empty()) {
        spdlog::warn("recorder: nothing recorded yet");
        return {};
    }

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        spdlog::error("recorder: cannot create {}: {}", directory, ec.message());
        return {};
    }

    std::time_t t = std::time(nullptr);
    std::tm tm{};
    tm = *std::localtime(&t);
    std::ostringstream name;
    name << "bugreport-" << std::put_time(&tm, "%Y-%m-%dT%H-%M-%S") << ".scn";
    const std::string path = (std::filesystem::path(directory) / name.str()).string();

    std::ofstream out(path);
    if (!out) {
        spdlog::error("recorder: cannot write {}", path);
        return {};
    }

    out << "# Recorded session — " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n"
        << "#\n"
        << "# Replay:  ./tests/run_scenarios.sh " << path << "\n"
        << "#\n"
        << "# Each step is followed by `settle` (a best-effort wait for the app to\n"
        << "# go quiet) and by the state it produced. To turn something\n"
        << "# that looked wrong into a regression test, promote the relevant\n"
        << "# value to an assertion and correct it, e.g.\n"
        << "#     expect view_position 0\n"
        << "# then delete the state comments you do not need.\n";

    if (truncated) {
        out << "#\n"
            << "# NOTE: only the last " << MAX_ENTRIES << " actions were kept; earlier\n"
            << "# ones were dropped. The prologue below reconstructs the state the\n"
            << "# oldest retained action started from.\n";
    }

    if (!engineMoves.empty()) {
        out << "#\n"
            << "# Engine replies captured during the session. Point the scenario\n"
            << "# config's bot at:\n"
            << "#     mock_gtp_engine --script";
        for (const auto& m : engineMoves) out << ' ' << m;
        out << "\n"
            << "# so the run reproduces without the original engine installed.\n";
    }

    out << "\n";

    // Reconstruct the state the oldest retained action started from — unless
    // that action already establishes one, in which case a prologue would
    // describe a position the replay never visits.
    const std::string& first = entries.front().action;
    const bool selfStarting = first.rfind("new_game", 0) == 0 ||
                              first.rfind("load_sgf", 0) == 0;
    if (selfStarting) {
        out << "# The recording begins with " << first
            << ", so no prologue is needed.\n\n";
    } else {
        writePrologue(out, entries.front().stateBefore);
    }

    for (const auto& e : entries) {
        out << e.action << "\n";
        // Replay runs far faster than a human, and much of what an action
        // triggers is asynchronous (GTP on the game thread, queued navigation).
        // Waiting for quiescence after each step is what makes a recording
        // reproducible rather than racy.
        out << "settle\n";
        if (!e.stateJson.empty()) {
            out << "  # state: " << e.stateJson << "\n";
        }
    }

    spdlog::info("recorder: wrote {} ({} steps)", path, entries.size());
    return path;
}
