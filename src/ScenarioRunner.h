#ifndef GOBAN_SCENARIORUNNER_H
#define GOBAN_SCENARIORUNNER_H

#include <cstddef>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class GobanControl;

/// Drives the application from a script so that behaviour can be regression
/// tested without a human at the keyboard.
///
/// Why a step machine rather than a loop: everything interesting happens on
/// other threads (GTP on the game thread, rendering and RmlUi events on the
/// main thread). Blocking the main thread to wait for an engine would stall
/// event processing and deadlock the very interactions under test. So `pump()`
/// executes as many steps as it can without waiting, then returns to the main
/// loop and is called again next frame.
///
/// Script syntax, one directive per line, `#` starts a comment:
///
///   <command> [args...]   any command from the GobanControl registry
///   expect <key> <value>  assert a key from GobanControl::dumpState()
///   expect_not <key> <v>  the negation
///   wait <ms>             unconditional delay
///   wait_idle [ms]        wait until no engine is thinking and no UI sync is
///                         pending (default timeout 10000 ms)
///   wait_until <k> <v>    wait until a state key reaches a value (10 s cap)
///   dump_state [label]    log the whole state, for authoring new scenarios
///   fail_fast on|off      stop at the first failed expectation (default on)
///   screenshot <path>     render one frame and write it as binary PPM (P6);
///                         blocks until the capture happened, so later steps
///                         cannot mutate the frame being shot
///   quit_when_done        end the run (implied at end of file)
///
/// A failed expectation prints the full state, because the useful question is
/// never "which assertion failed" but "what did the application actually do".
class ScenarioRunner {
public:
    struct Failure {
        size_t line;
        std::string directive;
        std::string detail;
    };

    /// Parses the script. Returns false if the file cannot be read.
    bool load(const std::string& path);

    /// Executes steps until the script blocks on a wait or finishes.
    /// `enginesLoaded` gates the very first step: issuing game commands before
    /// async engine loading completes hits the syncingUI guard and does nothing.
    void pump(GobanControl& control, bool enginesLoaded);

    [[nodiscard]] bool finished() const { return done; }
    [[nodiscard]] const std::vector<Failure>& failures() const { return failed; }
    [[nodiscard]] size_t stepsRun() const { return executed; }

    /// Logs a summary. Returns the process exit code: 0 when every expectation
    /// held, 1 otherwise.
    int report(const std::string& path) const;

private:
    struct Step {
        size_t line = 0;
        std::string directive;              // original text, for messages
        std::string name;
        std::vector<std::string> args;
    };

    // Returns true when the step is complete and the cursor may advance.
    bool execute(const Step& step, GobanControl& control);

    bool checkExpect(const Step& step, GobanControl& control, bool negate);
    void recordFailure(const Step& step, const std::string& detail,
                       const nlohmann::json& state);

    // A condition is "<key> [op] <value>", where op defaults to == and value
    // may be "$otherKey" to compare two pieces of state.
    struct Condition {
        std::string key;
        std::string op = "==";
        std::string value;
        bool valid = false;
    };
    static Condition parseCondition(const std::vector<std::string>& args);
    static bool evaluate(const Condition& cond, const nlohmann::json& state, bool& keyMissing);

    static std::string valueToString(const nlohmann::json& v);

    std::vector<Step> steps;
    size_t cursor = 0;
    size_t executed = 0;
    bool done = false;
    bool failFast = true;
    std::vector<Failure> failed;

    // Wait bookkeeping. Deadlines are absolute times from glfwGetTime().
    bool waiting = false;
    double waitDeadline = 0.0;

    // Screenshot bookkeeping: the request was handed to the view and the step
    // stays current until the main loop reports the capture done.
    bool screenshotRequested = false;
};

#endif  // GOBAN_SCENARIORUNNER_H
