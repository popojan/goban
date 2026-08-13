#include "ScenarioRunner.h"

// GobanControl pulls in glad, which must be seen before anything that includes a
// system OpenGL header — AppState.h includes GLFW. Keep this order.
#include "GobanControl.h"
#include "AppState.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

#include <spdlog/spdlog.h>

namespace {

constexpr double DEFAULT_WAIT_SECONDS = 10.0;

/// Maps a key name to an RmlUi KeyIdentifier, so a scenario can exercise the
/// handful of behaviours that live in GobanControl::keyPress() rather than in
/// the command registry — Space falling through to kibitz at the end of an
/// unfinished branch being the one the prose test plan has always described and
/// no automated test could reach.
///
/// The numeric values are RmlUi's, documented in docs/keyboard-shortcuts.md;
/// a bare number is accepted too, for keys with no name here.
bool keyCode(std::string name, int& out) {
    for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    static const std::map<std::string, int> named = {
        {"space", 1},  {"backspace", 69}, {"enter", 72}, {"escape", 81},
        {"end", 88},   {"home", 89},      {"left", 90},  {"up", 91},
        {"right", 92}, {"down", 93},
    };
    const auto it = named.find(name);
    if (it != named.end()) {
        out = it->second;
        return true;
    }
    // KI_A is 12 and the alphabet runs contiguously from there.
    if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z') {
        out = 12 + (name[0] - 'a');
        return true;
    }
    try {
        size_t consumed = 0;
        const int value = std::stoi(name, &consumed);
        if (consumed == name.size()) {
            out = value;
            return true;
        }
    } catch (const std::exception&) {
    }
    return false;
}

std::vector<std::string> tokenize(const std::string& line) {
    std::istringstream ss(line);
    std::vector<std::string> out;
    std::string word;
    while (ss >> word) out.push_back(word);
    return out;
}

}  // namespace

bool ScenarioRunner::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        spdlog::error("scenario: cannot open '{}'", path);
        return false;
    }

    std::string line;
    size_t lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Strip comments. '#' only starts a comment at the beginning of a token,
        // so SGF-ish or coordinate arguments containing '#' survive.
        const size_t hash = line.find('#');
        if (hash == 0) continue;
        if (hash != std::string::npos && std::isspace(static_cast<unsigned char>(line[hash - 1]))) {
            line = line.substr(0, hash);
        }

        auto words = tokenize(line);
        if (words.empty()) continue;

        Step step;
        step.line = lineNo;
        step.directive = line;
        step.name = words.front();
        step.args.assign(words.begin() + 1, words.end());
        steps.push_back(std::move(step));
    }

    spdlog::info("scenario: loaded {} steps from {}", steps.size(), path);
    return true;
}

void ScenarioRunner::pump(GobanControl& control, bool enginesLoaded) {
    if (done) return;

    // Nothing may run until engines are up: before that every game command is
    // swallowed by the syncingUI guard, which would look like a silent failure.
    if (!enginesLoaded) return;

    // Execute as many steps as complete immediately. A step that returns false
    // is still waiting, so yield to the main loop and retry next frame.
    while (cursor < steps.size()) {
        const Step& step = steps[cursor];
        if (!execute(step, control)) return;
        ++cursor;
        ++executed;
        if (done) return;
    }
    done = true;
}

bool ScenarioRunner::execute(const Step& step, GobanControl& control) {
    const std::string& name = step.name;

    if (name == "wait") {
        const double seconds = step.args.empty() ? 0.0 : std::stod(step.args[0]) / 1000.0;
        if (!waiting) {
            waiting = true;
            waitDeadline = AppState::GetElapsedTime() + seconds;
            return false;
        }
        if (AppState::GetElapsedTime() < waitDeadline) return false;
        waiting = false;
        return true;
    }

    if (name == "wait_idle" || name == "settle") {
        // `settle` is the best-effort form: it waits for quiescence but never
        // fails. Recorded bug reports use it because a bot-vs-bot match is
        // never idle — an engine is always to move — so a strict wait after
        // `start` could not possibly succeed.
        const bool strict = (name == "wait_idle");
        const double timeout = step.args.empty()
            ? (strict ? DEFAULT_WAIT_SECONDS : 2.0)
            : std::stod(step.args[0]) / 1000.0;
        if (!waiting) {
            waiting = true;
            waitDeadline = AppState::GetElapsedTime() + timeout;
        }
        if (control.isIdle()) {
            waiting = false;
            return true;
        }
        if (AppState::GetElapsedTime() >= waitDeadline) {
            waiting = false;
            if (strict) {
                recordFailure(step, "timed out waiting for idle", control.dumpState());
            }
            return true;
        }
        return false;
    }

    if (name == "wait_until") {
        const Condition cond = parseCondition(step.args);
        if (!cond.valid) {
            recordFailure(step, "wait_until needs <key> [op] <value>", control.dumpState());
            return true;
        }
        if (!waiting) {
            waiting = true;
            waitDeadline = AppState::GetElapsedTime() + DEFAULT_WAIT_SECONDS;
        }
        const auto state = control.dumpState();
        bool missing = false;
        if (evaluate(cond, state, missing)) {
            waiting = false;
            return true;
        }
        if (missing) {
            waiting = false;
            recordFailure(step, "unknown state key '" + cond.key + "'", state);
            return true;
        }
        if (AppState::GetElapsedTime() >= waitDeadline) {
            waiting = false;
            recordFailure(step,
                "timed out waiting for " + cond.key + " " + cond.op + " " + cond.value, state);
            return true;
        }
        return false;
    }

    if (name == "expect")      { checkExpect(step, control, false); return true; }
    if (name == "expect_not")  { checkExpect(step, control, true);  return true; }

    if (name == "dump_state") {
        const std::string label = step.args.empty() ? "" : step.args[0];
        spdlog::info("scenario: state {} = {}", label, control.dumpState().dump(2));
        return true;
    }

    if (name == "fail_fast") {
        failFast = step.args.empty() || step.args[0] != "off";
        return true;
    }

    if (name == "screenshot") {
        if (step.args.empty()) {
            recordFailure(step, "screenshot needs a file path", control.dumpState());
            return true;
        }
        if (!screenshotRequested) {
            control.requestScreenshot(step.args[0]);
            screenshotRequested = true;
            return false;
        }
        if (control.screenshotPending()) return false;
        screenshotRequested = false;
        return true;
    }

    if (name == "key") {
        if (step.args.empty()) {
            recordFailure(step, "key needs a key name", control.dumpState());
            return true;
        }
        int code = 0;
        if (!keyCode(step.args[0], code)) {
            recordFailure(step, "unknown key '" + step.args[0] + "'", control.dumpState());
            return true;
        }
        // Down then up, as a real press arrives. The adjustment commands act on
        // the down edge to allow key repeat; everything else acts on the up.
        control.keyPress(code, 0, 0, true);
        control.keyPress(code, 0, 0, false);
        return true;
    }

    if (name == "quit_when_done") {
        done = true;
        return true;
    }

    // Anything else is a normal application command. Hand over the whole
    // line: legacy command names contain spaces ("reset camera", "cycle
    // shaders"), and the single-string overload resolves those before
    // falling back to tokenisation.
    spdlog::info("scenario: line {}: {}", step.line, step.directive);
    control.command(step.directive);
    return true;
}

bool ScenarioRunner::checkExpect(const Step& step, GobanControl& control, bool negate) {
    const Condition cond = parseCondition(step.args);
    if (!cond.valid) {
        recordFailure(step, "expect needs <key> [op] <value>", control.dumpState());
        return false;
    }

    const auto state = control.dumpState();
    bool missing = false;
    const bool matched = evaluate(cond, state, missing);

    if (missing) {
        recordFailure(step, "unknown state key '" + cond.key + "'", state);
        return false;
    }
    if (matched == !negate) return true;

    std::ostringstream detail;
    detail << cond.key << " is " << valueToString(state[cond.key])
           << ", expected " << (negate ? "not " : "") << cond.op << " " << cond.value;
    recordFailure(step, detail.str(), state);
    return false;
}

void ScenarioRunner::recordFailure(const Step& step, const std::string& detail,
                                   const nlohmann::json& state) {
    failed.push_back({step.line, step.directive, detail});
    spdlog::error("scenario: FAILED at line {}: {}", step.line, step.directive);
    spdlog::error("scenario:   {}", detail);
    // The full state is the diagnostic — which assertion tripped is rarely
    // enough to tell what the application actually did. Kept on one line so it
    // survives grepping for "scenario:" in the run log.
    spdlog::error("scenario:   state = {}", state.dump());

    if (failFast) {
        spdlog::error("scenario: stopping (fail_fast is on)");
        done = true;
    }
}

std::string ScenarioRunner::valueToString(const nlohmann::json& v) {
    if (v.is_string()) return v.get<std::string>();
    return v.dump();
}

ScenarioRunner::Condition ScenarioRunner::parseCondition(const std::vector<std::string>& args) {
    auto isOperator = [](const std::string& t) {
        return t == "==" || t == "!=" || t == ">=" || t == "<=" || t == ">" || t == "<";
    };
    auto join = [&args](size_t from) {
        std::string out;
        for (size_t i = from; i < args.size(); ++i) {
            if (i > from) out += ' ';
            out += args[i];
        }
        return out;
    };

    Condition c;
    if (args.size() < 2) return c;

    c.key = args[0];
    // Everything after the key — and after the operator, when one is given — is
    // the value, spaces included. SGF comments are the reason: `expect comment
    // markup node` used to parse "markup" as an operator and fail as malformed,
    // so a multi-word comment could not be asserted at all. A value that happens
    // to start with an operator token is still read as one, which is why the
    // operator is only recognised in second position.
    if (isOperator(args[1])) {
        if (args.size() < 3) return c;
        c.op = args[1];
        c.value = join(2);
    } else {
        c.value = join(1);
    }
    // `""` is the empty string, since a value is required and an absent one
    // cannot be told from a malformed line. Comments and filenames are routinely
    // empty, and `expect comment ""` says so where a bare `expect comment` would
    // just look like a typo.
    if (c.value == "\"\"") c.value.clear();
    c.valid = true;
    return c;
}

bool ScenarioRunner::evaluate(const Condition& cond, const nlohmann::json& state,
                              bool& keyMissing) {
    keyMissing = false;
    if (!state.contains(cond.key)) {
        keyMissing = true;
        return false;
    }
    const nlohmann::json& actual = state[cond.key];

    // "$other" compares against another state key, so a scenario can say
    // `expect view_position $main_line_moves` without hard-coding a length.
    nlohmann::json expected;
    if (!cond.value.empty() && cond.value[0] == '$') {
        const std::string other = cond.value.substr(1);
        if (!state.contains(other)) {
            keyMissing = true;
            return false;
        }
        expected = state[other];
    } else {
        expected = cond.value;   // string; coerced per actual's type below
    }

    // Booleans only support equality.
    if (actual.is_boolean()) {
        bool want;
        if (expected.is_boolean()) {
            want = expected.get<bool>();
        } else {
            const std::string t = expected.get<std::string>();
            want = (t == "true" || t == "1" || t == "yes");
        }
        const bool eq = (actual.get<bool>() == want);
        return (cond.op == "!=") ? !eq : eq;
    }

    if (actual.is_number()) {
        double a = actual.get<double>();
        double b = 0.0;
        if (expected.is_number()) {
            b = expected.get<double>();
        } else {
            try {
                b = std::stod(expected.get<std::string>());
            } catch (const std::exception&) {
                return false;
            }
        }
        if (cond.op == "==") return std::abs(a - b) < 1e-6;
        if (cond.op == "!=") return std::abs(a - b) >= 1e-6;
        if (cond.op == ">=") return a >= b;
        if (cond.op == "<=") return a <= b;
        if (cond.op == ">")  return a > b;
        if (cond.op == "<")  return a < b;
        return false;
    }

    const std::string a = valueToString(actual);
    const std::string b = expected.is_string() ? expected.get<std::string>()
                                               : valueToString(expected);
    if (cond.op == "!=") return a != b;
    return a == b;
}

int ScenarioRunner::report(const std::string& path) const {
    if (failed.empty()) {
        spdlog::info("scenario: {} — PASSED ({} steps)", path, executed);
        return 0;
    }
    spdlog::error("scenario: {} — FAILED ({} of {} steps run, {} failure(s))",
                  path, executed, steps.size(), failed.size());
    for (const auto& f : failed) {
        spdlog::error("scenario:   line {}: {} — {}", f.line, f.directive, f.detail);
    }
    return 1;
}
