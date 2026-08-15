/** \file
 *  \brief Continuous analysis, in a process and a thread of its own (ADR-0007).
 *
 * The overlay's engine is never the coach's process and never the kibitz
 * engine's, even when all three are the same binary: an analysis stream is a GTP
 * command that deliberately never replies until told to stop, and ADR-0001 gives
 * the game thread exclusive ownership of the playing engines' pipes. This owns
 * one pipe, one thread, and nothing else.
 *
 * It follows the *review cursor*, not the game position, which is the whole
 * reason a separate process is needed — the coach must stay where the game is.
 * The position arrives as plain data through `GobanModel::snapshot()`; results
 * go back the same way, published as an immutable `AnalysisReport` the UI thread
 * reads per frame. Neither thread reaches into the other.
 */
#ifndef GOBAN_ANALYSISSERVICE_H
#define GOBAN_ANALYSISSERVICE_H

#include "Board.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

class GtpClient;
class GobanModel;
class GameThread;

/// One continuation the engine suggests, for the board annotations that Stage 2
/// will draw. Parsed already, because the parser has the line in front of it.
struct AnalysisMove {
    Move move{Move::INVALID, Color::EMPTY};
    int visits = 0;
    double winrateBlack = 0.5;
    /// Rank the engine gave this move, or -1 when it gave none. The distinction
    /// matters: defaulting to 0 makes "the engine did not say" indistinguishable
    /// from "this is the best move", and the first block of a report that omits
    /// `order` then wins on a technicality.
    int order = -1;
    /// Per block, not per report: `scoreLead` arrives *before* `order` in
    /// KataGo's output, so a report-level field would be overwritten by the last
    /// block parsed rather than kept from the best one.
    std::optional<double> scoreLeadBlack;
};

/// What the analysis engine last reported about one position, as plain
/// immutable data.
///
/// Everything here is in **Black's** frame of reference. Engines report for the
/// side to move, so a display fed the raw numbers flips every move and reads as
/// noise; converting once, here, is the same lesson as the prisoner counts that
/// shipped swapped.
struct AnalysisReport {
    /// Which position this describes — `GameSnapshot::positionId`. The UI does
    /// not check it; it is here so a report can be recognised as belonging to a
    /// position that has since been left.
    unsigned long long positionId = 0;

    double winrateBlack = 0.5;
    /// Score is optional because not every analysis-capable engine reports one:
    /// `scoreLead` is KataGo's, and plain `lz-analyze` has no equivalent.
    std::optional<double> scoreLeadBlack;
    int visits = 0;
    std::vector<AnalysisMove> moves;
};

/// Where the service is. Reported to the UI so it can say something truthful
/// rather than nothing, and asserted on by scenarios.
enum class AnalysisState {
    Disabled,     ///< Switched off, or no engine nominated. Nothing is running.
    Starting,     ///< Spawning the process and probing what it can do.
    Unavailable,  ///< Started, but cannot analyse — or died. Warned about once.
    Yielded,      ///< Capable and enabled, but a playing engine has the device.
    Running       ///< Streaming.
};

const char* analysisStateName(AnalysisState state);

/** \brief Owns the analysis engine, its thread, and the last report.
 *
 * Lives in `goban_core`, so it knows nothing about rendering: waking the
 * renderer is a `std::function` the application layer supplies.
 */
class AnalysisService {
public:
    AnalysisService(GobanModel& model, GameThread& engine);
    ~AnalysisService();

    AnalysisService(const AnalysisService&) = delete;
    AnalysisService& operator=(const AnalysisService&) = delete;

    /// Turn the overlay on or off. Enabling is what spawns the process — a
    /// second set of network weights is not loaded during startup, which is the
    /// one window where the renderer and an engine reliably contend (issue #45).
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return enabled.load(); }

    /// Whether an engine nominated itself for the role at all. False means the
    /// toggle should not be offered; it is not a failure.
    [[nodiscard]] bool isConfigured() const { return configured; }

    [[nodiscard]] AnalysisState state() const { return serviceState.load(); }

    /// The engine being started, for the status line. Empty unless `Starting`.
    [[nodiscard]] std::string startingEngineName() const;

    /// The last report, or nullptr when there is nothing truthful to show.
    /// Never a placeholder: a bar sitting at 50% because nothing has been
    /// computed cannot be told from a genuine 50%.
    [[nodiscard]] std::shared_ptr<const AnalysisReport> report() const;

    /// Called after a report is published, on the analysis thread. The
    /// application wires this to a repaint request. It fires only when a
    /// *displayed* value actually changed — see the publish gate in the .cpp.
    void setOnUpdate(std::function<void()> callback) { onUpdate = std::move(callback); }

    /// Starts the thread. Separate from the constructor because the engines have
    /// to have finished loading first, and because a scenario run may never call
    /// it at all.
    void start();

    /// Stops the thread and kills the process. Idempotent; the destructor calls
    /// it. Must run before the model or the game thread goes away.
    void stop();

private:
    void loop();
    /// Spawns and probes. Sets `analyzeCommand` on success.
    bool startEngine();
    void stopEngine();
    /// Brings the engine to `target`, incrementally where possible. Returns
    /// false if it refused something, which takes the whole service down to
    /// Unavailable — a desynchronised analysis engine reports on a position
    /// nobody is looking at.
    bool syncTo(const struct AnalysisTarget& target);
    /// Runs one stream until the position changes, the game claims the device,
    /// or the service is switched off.
    void streamUntilStale(const struct AnalysisTarget& target);
    void publish(const AnalysisReport& next);
    void setState(AnalysisState next);

    GobanModel& model;
    GameThread& engine;

    std::unique_ptr<GtpClient> client;
    /// `kata-analyze` or `lz-analyze`, whichever `list_commands` reported.
    std::string analyzeCommand;
    std::string engineName;
    nlohmann::json botConfig;
    bool configured = false;

    std::thread thread;
    std::atomic<bool> enabled{false};
    std::atomic<bool> running{false};
    std::atomic<AnalysisState> serviceState{AnalysisState::Disabled};
    std::mutex wakeMutex;
    std::condition_variable wake;

    /// The position the engine currently holds, for the incremental diff.
    std::vector<Move> sentPath;
    int sentBoardSize = 0;
    float sentKomi = 0.0f;
    std::vector<Position> sentSetupBlack;
    std::vector<Position> sentSetupWhite;
    bool engineHasPosition = false;

    mutable std::mutex reportMutex;
    std::shared_ptr<const AnalysisReport> lastReport;
    /// What the UI is currently showing, rounded as it is displayed. The publish
    /// gate compares against these, not against the raw values.
    int shownWinratePercent = -1;
    int shownScoreTenths = 0;
    bool shownHasScore = false;
    std::chrono::steady_clock::time_point lastPublished{};

    std::function<void()> onUpdate;
};

/// Parses one `info …` line of the lz/kata-analyze report format into moves in
/// Black's frame of reference. Free and header-declared so it unit-tests without
/// a process. `colorToMove` is whose turn it is at the analysed position, which
/// is what the engine's numbers are relative to.
bool parseAnalysisLine(const std::string& line, const Color& colorToMove,
                       AnalysisReport& out);

/// How to get an engine from the path it holds to the path it should hold.
struct AnalysisSyncPlan {
    size_t undos = 0;      ///< `undo` commands to send first.
    size_t playFrom = 0;   ///< Index into the target path to `play` from.
};

/// The incremental diff, as a pure function of the two paths: their common
/// prefix is already on the engine's board, so an arrow key costs one command
/// rather than one per move played so far. This is the reason a private pipe is
/// worth having, and the reason the ADR's "re-sending the whole position on
/// every arrow key" cost was an artefact of reusing `syncEngineToPosition()`.
AnalysisSyncPlan planIncrementalSync(const std::vector<Move>& sent,
                                     const std::vector<Move>& target);

/// The moves that can actually be sent as `play`. Filtering happens once,
/// before the diff, because the `undo` count is counted against what was sent —
/// skipping an unplayable move at the point of sending would leave the two out
/// of step by one for the rest of the game.
std::vector<Move> playableMoves(const std::vector<Move>& path);

#endif // GOBAN_ANALYSISSERVICE_H
