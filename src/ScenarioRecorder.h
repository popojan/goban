#ifndef GOBAN_SCENARIORECORDER_H
#define GOBAN_SCENARIORECORDER_H

#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/// Records what the user did, so an unexpected result can be turned into a
/// regression scenario without anyone having to remember the steps.
///
/// Design notes:
///
/// * **Always on.** Recording costs a few strings per interaction and is kept
///   in a bounded ring buffer. A mode you had to arm in advance would only ever
///   capture bugs you already expected, which are not the interesting ones.
///
/// * **Engine replies are recorded too.** Each genmove answer is captured, and
///   the emitted scenario configures `mock_gtp_engine --script` with exactly
///   those moves. That is what lets a session played against KataGo replay
///   deterministically with no engine installed.
///
/// * **State is emitted as comments, not assertions.** Auto-generating an
///   `expect` for every key would be over-specified and brittle. Instead each
///   step carries the state it produced, so turning "this looked wrong" into a
///   test is a matter of promoting one comment to an `expect` line and
///   correcting the value.
///
/// This is a singleton because the call sites (the controller, and the engine
/// wrapper on the game thread) have no shared owner to thread it through.
/// Access is mutex-guarded for that reason.
class ScenarioRecorder {
public:
    static ScenarioRecorder& instance();

    /// Records a user action, e.g. ("click", {"3", "3"}).
    ///
    /// `stateBefore` is what makes a truncated buffer usable: the oldest
    /// retained entry's pre-state is exactly the starting point a replay has to
    /// reconstruct, so the emitted scenario can open with a prologue that puts
    /// the app back there.
    void recordAction(const std::string& name, const std::vector<std::string>& args,
                      const nlohmann::json& stateBefore = {});

    /// Records the state produced by the most recent action. Kept separate so
    /// the controller can sample state after the action has been applied.
    void recordState(const nlohmann::json& state);

    /// Records an engine's genmove reply, for the replay script.
    void recordEngineMove(const std::string& engineName, const std::string& vertex);

    /// Writes a runnable scenario. Returns the path written, or an empty string
    /// on failure.
    std::string save(const std::string& directory);

    /// RAII guard suppressing nested records. A command handler that calls
    /// another recorded controller method would otherwise log the same user
    /// action twice (switch_player -> switchPlayer, click -> command("clear")).
    class SuppressNested {
    public:
        SuppressNested() { ++instance().suppressDepth; }
        ~SuppressNested() { --instance().suppressDepth; }
        SuppressNested(const SuppressNested&) = delete;
        SuppressNested& operator=(const SuppressNested&) = delete;
    };

    void setEnabled(bool on) { enabled = on; }
    [[nodiscard]] bool isEnabled() const { return enabled; }

    /// Drop everything recorded so far (e.g. after saving a report).
    void clear();

    [[nodiscard]] size_t size() const;

private:
    ScenarioRecorder() = default;

    struct Entry {
        std::string action;         // already formatted, e.g. "click 3 3"
        std::string stateJson;      // state after the action, one line
        nlohmann::json stateBefore; // state the action started from
    };

    /// Emits commands that put a fresh instance into `state`. Returns false if
    /// the starting point cannot be reconstructed, in which case the report says
    /// so rather than pretending to be replayable.
    static bool writePrologue(std::ostream& out, const nlohmann::json& state);

    // Bounded so a long session cannot grow without limit. Large enough to
    // cover far more than anyone would reconstruct from memory.
    static constexpr size_t MAX_ENTRIES = 500;

    mutable std::mutex mutex;
    std::deque<Entry> entries;
    std::vector<std::string> engineMoves;
    bool enabled = true;
    bool truncated = false;   // entries were dropped from the front
    int suppressDepth = 0;    // >0 while inside an already-recorded action
};

#endif  // GOBAN_SCENARIORECORDER_H
