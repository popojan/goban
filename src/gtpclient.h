/** \file
 *  \brief Talking to an engine process: pipes, GTP commands, output filters.
 *
 * `Process` is the cross-platform half — fork/exec or CreateProcess, three
 * pipes, timed line reads. `GtpClient` is the protocol half: one command, one
 * response, plus the configurable regex filters that turn an engine's chatter
 * into SGF comments and template variables. One StderrReaderThread per process
 * drains stderr, so a talkative engine cannot deadlock on a full pipe.
 *
 * A command in flight owns the pipes until it replies and standard GTP has no
 * abort, so `issueCommand()` may only be called from the game thread while a
 * game is running. `setCommandTimeout()` is the backstop for an engine that
 * stops answering; note that a *killed* engine keeps reporting failure
 * (`failed_`) rather than pretending success, because move replay and scoring
 * must not proceed against nothing.
 */
#ifndef GTPCLIENT_H
#define GTPCLIENT_H

#include <algorithm>
#include <string>
#include <vector>
#include <regex>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

struct OutputFilter {
    std::string regex;
    std::string output;
    std::string var;
    std::regex compiled;
};

void replaceAll(std::string& out, const std::string& what, const std::string& by);

// Cross-platform process with stdin/stdout/stderr pipes
class Process {
public:
    enum class ReadStatus { Ok, Eof, Timeout };

    Process(const std::string& program, const std::vector<std::string>& args, const std::string& workDir);
    ~Process();

    bool write(const std::string& data) const;

    /// Reads one line, waiting at most timeoutMs. A negative timeout blocks
    /// indefinitely, which is the historical behaviour.
    ReadStatus readLine(std::string& line, int timeoutMs);

    bool readLine(std::string& line) { return readLine(line, -1) == ReadStatus::Ok; }
    bool readLineStderr(std::string& line);
    void closeStdin();
    void closeStdout();
    void closeStderr();
    int wait() const;
    bool waitFor(int timeoutMs) const;
    void terminate();
    bool running() const;

private:
#ifdef _WIN32
    HANDLE hProcess_ = INVALID_HANDLE_VALUE;
    HANDLE hThread_ = INVALID_HANDLE_VALUE;
    HANDLE hStdinWrite_ = INVALID_HANDLE_VALUE;
    HANDLE hStdoutRead_ = INVALID_HANDLE_VALUE;
    HANDLE hStderrRead_ = INVALID_HANDLE_VALUE;
#else
    pid_t pid_ = -1;
    int stdinFd_ = -1;
    int stdoutFd_ = -1;
    int stderrFd_ = -1;
#endif
    std::string stdoutBuffer_;
    std::string stderrBuffer_;
};

// Async stderr reader thread
class StderrReaderThread {
public:
    StderrReaderThread(Process& proc, std::function<void(const std::string&)> callback);
    ~StderrReaderThread();
    void stop();

private:
    void readLoop() const;
    Process& proc_;
    std::function<void(const std::string&)> callback_;
    std::thread thread_;
    std::atomic<bool> running_{true};
};

class GtpClient {
private:
    std::unique_ptr<Process> proc_;
    std::string exe;
    std::string lastLine;
    mutable std::mutex lastLineMutex_;
    std::vector<OutputFilter> outputFilters;
    std::unique_ptr<StderrReaderThread> stderrReader_;
    // Killed deliberately during shutdown: further commands answer "= " so
    // teardown stays silent.
    std::atomic<bool> terminated_{false};
    // Killed because it stopped responding. Distinct from terminated_ because
    // this one must keep reporting failure — pretending a dead engine answered
    // OK would let move replay and scoring proceed against nothing.
    std::atomic<bool> failed_{false};

    nlohmann::json vars;

public:
    typedef std::vector<std::string> CommandOutput;

    GtpClient(const std::string &exe, const std::string &cmdline,
              const std::string &path, const nlohmann::json &messages);

    virtual ~GtpClient();

    void interpolate(std::string &out);

    void operator()(const std::string &line);

    void initFilters(const nlohmann::json &messages);

    void compileFilters();

    void addOutputFilter(const std::string &msg, const std::string &format, const std::string &var);

    std::string lastError();

    CommandOutput name();

    CommandOutput version();

    CommandOutput issueCommand(const std::string &command);

    static bool success(const CommandOutput &ret);

    /// Kill the engine process immediately (unblocks any blocking readLine).
    void terminateProcess();

    /// Maximum time to wait for a response to a single GTP command.
    /// A negative value waits forever.
    ///
    /// This exists because a wedged engine used to hang the game thread with no
    /// way out (issue #45's failure mode, and a hazard for automated runs).
    /// The default is deliberately generous: a strong engine thinking at long
    /// time settings, or KataGo loading network weights, can legitimately take
    /// tens of seconds. Per-engine overrides come from "timeout_ms" in the bot
    /// configuration.
    void setCommandTimeout(int ms) { commandTimeoutMs_ = ms; }
    [[nodiscard]] int getCommandTimeout() const { return commandTimeoutMs_; }

    static constexpr int DEFAULT_COMMAND_TIMEOUT_MS = 300000;  // 5 minutes

    /// Scoring gets a tighter bound than the general timeout.
    ///
    /// The general one has to tolerate a strong engine thinking at long time
    /// settings, so it is measured in minutes. Nothing about scoring a finished
    /// position needs that, and when scoring *does* take minutes it is because
    /// the engine is wedged or sitting at the wrong position — the case that
    /// should fail rather than freeze the game thread for five minutes with
    /// "Calculating score…" on screen. Per-engine override: "scoring_timeout_ms".
    static constexpr int DEFAULT_SCORING_TIMEOUT_MS = 30000;  // 30 seconds

    void setScoringTimeout(int ms) { scoringTimeoutMs_ = ms; }

    /// Never *raises* the limit: an engine configured to answer within 5 s must
    /// not get 30 s just because it was asked to score. A negative general
    /// timeout means "wait forever", which the scoring bound overrides — that is
    /// the whole point of having one.
    [[nodiscard]] int scoringTimeout() const {
        if (commandTimeoutMs_ < 0) return scoringTimeoutMs_;
        return std::min(commandTimeoutMs_, scoringTimeoutMs_);
    }

    /// Applies a different command timeout for the duration of a scope, and
    /// restores the previous one however the scope is left.
    class ScopedTimeout {
    public:
        ScopedTimeout(GtpClient& client, int ms)
            : client_(client), previous_(client.getCommandTimeout()) {
            client_.setCommandTimeout(ms);
        }
        ~ScopedTimeout() { client_.setCommandTimeout(previous_); }
        ScopedTimeout(const ScopedTimeout&) = delete;
        ScopedTimeout& operator=(const ScopedTimeout&) = delete;
    private:
        GtpClient& client_;
        int previous_;
    };

private:
    int commandTimeoutMs_ = DEFAULT_COMMAND_TIMEOUT_MS;
    int scoringTimeoutMs_ = DEFAULT_SCORING_TIMEOUT_MS;
};

#endif // GTPCLIENT_H
