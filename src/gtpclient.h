#ifndef GTPCLIENT_H
#define GTPCLIENT_H

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

private:
    int commandTimeoutMs_ = DEFAULT_COMMAND_TIMEOUT_MS;
};

#endif // GTPCLIENT_H
