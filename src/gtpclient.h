#ifndef GTPCLIENT_H
#define GTPCLIENT_H

#include <string>
#include <vector>
#include <regex>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
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
    Process(const std::string& program, const std::vector<std::string>& args, const std::string& workDir);
    ~Process();

    bool write(const std::string& data) const;
    bool readLine(std::string& line);
    bool readLineStderr(std::string& line);
    void closeStdin();
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
    std::vector<OutputFilter> outputFilters;
    std::unique_ptr<StderrReaderThread> stderrReader_;

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

    CommandOutput showboard();

    CommandOutput name();

    CommandOutput version();

    CommandOutput issueCommand(const std::string &command);

    static bool success(const CommandOutput &ret);

    /// Kill the engine process immediately (unblocks any blocking readLine).
    void terminateProcess();
};

#endif // GTPCLIENT_H
