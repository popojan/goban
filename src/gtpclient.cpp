#include "gtpclient.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>
#include <spdlog/spdlog.h>

#ifndef _WIN32
#include <cerrno>
#include <poll.h>
#endif

#ifdef _WIN32
// Windows implementation

/// Serialises process creation.
///
/// Windows has no per-handle close-on-exec: inheritance is decided at
/// CreateProcess time, and bInheritHandles=TRUE passes *every* currently
/// inheritable handle in the process. The pipe ends below are created
/// inheritable and stay that way until the CloseHandle calls at the end of this
/// constructor, so a second engine spawning in that window inherits the first
/// one's stdout and stderr write handles and holds them for its whole life. The
/// parent then never sees a broken pipe when the first engine dies — it simply
/// stops answering.
///
/// Engines are started concurrently (GameThread::loadEnginesParallel), so this
/// is reachable. The POSIX side solves the same problem precisely, with
/// pipe2(O_CLOEXEC); there is no equivalent here, so the window is closed by
/// letting only one spawn run at a time. Engines still *load* in parallel — only
/// the few milliseconds of pipe-and-spawn are serialised.
///
/// The exec-status pipe the POSIX branch carries has no counterpart here and
/// needs none: CreateProcessW reports failure synchronously.
static std::mutex g_spawnMutex;

Process::Process(const std::string& program, const std::vector<std::string>& args, const std::string& workDir) {
    std::lock_guard<std::mutex> spawnLock(g_spawnMutex);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdinRead, hStdoutWrite, hStderrWrite;

    // Create pipes
    if (!CreatePipe(&hStdinRead, &hStdinWrite_, &sa, 0) ||
        !CreatePipe(&hStdoutRead_, &hStdoutWrite, &sa, 0) ||
        !CreatePipe(&hStderrRead_, &hStderrWrite, &sa, 0)) {
        throw std::runtime_error("Failed to create pipes");
    }

    // Ensure the write handle to stdin and read handles from stdout/stderr are not inherited
    SetHandleInformation(hStdinWrite_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead_, HANDLE_FLAG_INHERIT, 0);

    // Build command line as wide string for Unicode support
    std::string cmdLine = "\"" + program + "\"";
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }
    auto wCmdLine = std::filesystem::u8path(cmdLine).wstring();

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    // Build wide environment block with workDir added to PATH
    // This ensures child process can find DLLs in its working directory
    // Windows searches PATH during process creation for DLL loading
    std::wstring wEnvBlock;
    std::wstring wWorkDir;
    if (!workDir.empty()) {
        wWorkDir = std::filesystem::u8path(workDir).wstring();
        wchar_t* currentEnv = GetEnvironmentStringsW();
        if (currentEnv) {
            wchar_t* var = currentEnv;
            bool pathFound = false;
            while (*var) {
                std::wstring varStr(var);
                // Case-insensitive check for PATH (Windows env vars are case-insensitive)
                std::wstring varUpper = varStr.substr(0, 5);
                for (auto& c : varUpper) c = towupper(c);
                if (varUpper == L"PATH=") {
                    // Prepend workDir to existing PATH
                    wEnvBlock += varStr.substr(0, varStr.find(L'=') + 1) + wWorkDir + L";" + varStr.substr(varStr.find(L'=') + 1);
                    pathFound = true;
                } else {
                    wEnvBlock += varStr;
                }
                wEnvBlock += L'\0';
                var += wcslen(var) + 1;
            }
            if (!pathFound) {
                wEnvBlock += L"PATH=" + wWorkDir;
                wEnvBlock += L'\0';
            }
            wEnvBlock += L'\0'; // Double null terminator
            FreeEnvironmentStringsW(currentEnv);
        }
    }

    // Create the child process with Unicode support
    if (!CreateProcessW(
        NULL,
        const_cast<wchar_t*>(wCmdLine.c_str()),
        NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT,
        wEnvBlock.empty() ? NULL : const_cast<wchar_t*>(wEnvBlock.c_str()),
        wWorkDir.empty() ? NULL : wWorkDir.c_str(),
        &si, &pi)) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrWrite);
        throw std::runtime_error("Failed to create process: " + std::to_string(GetLastError()));
    }

    hProcess_ = pi.hProcess;
    hThread_ = pi.hThread;

    // Close handles that are now owned by the child process
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
}

Process::~Process() {
    closeStdin();
    if (hStdoutRead_ != INVALID_HANDLE_VALUE) CloseHandle(hStdoutRead_);
    if (hStderrRead_ != INVALID_HANDLE_VALUE) CloseHandle(hStderrRead_);
    if (hThread_ != INVALID_HANDLE_VALUE) CloseHandle(hThread_);
    if (hProcess_ != INVALID_HANDLE_VALUE) CloseHandle(hProcess_);
}

bool Process::write(const std::string& data) const {
    if (hStdinWrite_ == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    return WriteFile(hStdinWrite_, data.c_str(), static_cast<DWORD>(data.size()), &written, NULL) && written == data.size();
}

Process::ReadStatus Process::readLine(std::string& line, int timeoutMs) {
    line.clear();
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs < 0 ? 0 : timeoutMs);

    while (true) {
        // Check buffer first
        size_t pos = stdoutBuffer_.find('\n');
        if (pos != std::string::npos) {
            line = stdoutBuffer_.substr(0, pos);
            stdoutBuffer_.erase(0, pos + 1);
            // Remove \r if present
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return ReadStatus::Ok;
        }

        // ReadFile on an anonymous pipe blocks, so poll with PeekNamedPipe to
        // keep the timeout honest.
        if (timeoutMs >= 0) {
            DWORD available = 0;
            while (true) {
                if (!PeekNamedPipe(hStdoutRead_, NULL, 0, NULL, &available, NULL)) {
                    available = 0;
                    break;  // pipe closed; fall through to ReadFile for EOF handling
                }
                if (available > 0) break;
                if (std::chrono::steady_clock::now() >= deadline) {
                    return ReadStatus::Timeout;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        // Read more data
        char buf[256];
        DWORD bytesRead;
        if (!ReadFile(hStdoutRead_, buf, sizeof(buf), &bytesRead, NULL) || bytesRead == 0) {
            if (!stdoutBuffer_.empty()) {
                line = std::move(stdoutBuffer_);
                stdoutBuffer_.clear();
                return ReadStatus::Ok;
            }
            return ReadStatus::Eof;
        }
        stdoutBuffer_.append(buf, bytesRead);
    }
}

bool Process::readLineStderr(std::string& line) {
    line.clear();
    while (true) {
        size_t pos = stderrBuffer_.find('\n');
        if (pos != std::string::npos) {
            line = stderrBuffer_.substr(0, pos);
            stderrBuffer_.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }

        char buf[256];
        DWORD bytesRead;
        if (!ReadFile(hStderrRead_, buf, sizeof(buf), &bytesRead, NULL) || bytesRead == 0) {
            if (!stderrBuffer_.empty()) {
                line = std::move(stderrBuffer_);
                stderrBuffer_.clear();
                return true;
            }
            return false;
        }
        stderrBuffer_.append(buf, bytesRead);
    }
}

void Process::closeStdin() {
    if (hStdinWrite_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hStdinWrite_);
        hStdinWrite_ = INVALID_HANDLE_VALUE;
    }
}

int Process::wait() const {
    if (hProcess_ == INVALID_HANDLE_VALUE) return -1;
    WaitForSingleObject(hProcess_, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(hProcess_, &exitCode);
    return static_cast<int>(exitCode);
}

bool Process::waitFor(int timeoutMs) const {
    if (hProcess_ == INVALID_HANDLE_VALUE) return true;
    return WaitForSingleObject(hProcess_, timeoutMs) == WAIT_OBJECT_0;
}

void Process::terminate() {
    if (hProcess_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(hProcess_, 1);
        WaitForSingleObject(hProcess_, 2000);
    }
    // Don't close pipe handles here — the StderrReaderThread may still
    // have a pending ReadFile. Handles are closed by ~Process after the
    // reader thread is joined.
}

#else
// POSIX implementation
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

/// Longest the parent waits to hear whether exec succeeded.
static constexpr int EXEC_STATUS_TIMEOUT_MS = 2000;

/// Creates a pipe whose ends are both close-on-exec.
///
/// pipe2() sets the flag atomically, which is the point: pipe() followed by
/// fcntl() leaves a window in which a fork() on another thread inherits the
/// descriptor without it. That is not hypothetical here — engines start
/// concurrently, and it hung startup in roughly one suite run in five.
///
/// The fallback is for platforms without pipe2 (macOS). The window remains
/// there, which is why the exec-status read is bounded rather than trusted.
static int createPipeCloexec(int fds[2]) {
#if defined(__linux__) || defined(__FreeBSD__)
    return pipe2(fds, O_CLOEXEC);
#else
    if (pipe(fds) < 0) return -1;
    fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC);
    return 0;
#endif
}

Process::Process(const std::string& program, const std::vector<std::string>& args, const std::string& workDir) {
    int stdinPipe[2], stdoutPipe[2], stderrPipe[2];

    // Close-on-exec, like the exec-status pipe below and for the same reason:
    // engines are spawned concurrently (GameThread::loadEnginesParallel), and a
    // plain pipe() leaks into every child forked in the meantime. A leaked write
    // end of *another* engine's stdout means the parent never sees EOF when that
    // engine dies — it just stops answering.
    //
    // The child's own descriptors survive because dup2() clears FD_CLOEXEC on
    // the descriptor it creates, so 0/1/2 are inherited across its exec while
    // every other engine's pipes are not.
    if (createPipeCloexec(stdinPipe) < 0 ||
        createPipeCloexec(stdoutPipe) < 0 ||
        createPipeCloexec(stderrPipe) < 0) {
        throw std::runtime_error("Failed to create pipes");
    }

    // A pipe that closes on a successful exec and carries errno otherwise. Without
    // it a child that cannot start is invisible: fork() succeeds, the parent
    // reports the engine ready, and the first symptom is "Failed to write
    // command" against a process that died before it ever ran. See issue #55.
    //
    // Created close-on-exec *atomically*. pipe() followed by fcntl() is not the
    // same thing: engines load concurrently (GameThread::loadEnginesParallel),
    // and a fork() in another thread landing between the two inherits this write
    // end without the flag. That engine then holds it open for its whole life,
    // the read below never sees EOF, and startup hangs — which is exactly what
    // the first version of this code did, intermittently, in about one suite run
    // in five.
    int execPipe[2];
    if (createPipeCloexec(execPipe) < 0) {
        throw std::runtime_error("Failed to create pipes");
    }

    pid_ = fork();
    if (pid_ < 0) {
        throw std::runtime_error("Failed to fork");
    }

    if (pid_ == 0) {
        // Child process
        close(execPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stderrPipe[0]);

        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);

        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        // Only async-signal-safe calls from here on, so failures are reported by
        // writing errno up the pipe rather than logged.
        if (!workDir.empty() && chdir(workDir.c_str()) != 0) {
            const int err = errno;
            (void) !::write(execPipe[1], &err, sizeof(err));
            _exit(126);
        }

        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(program.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(program.c_str(), argv.data());
        const int err = errno;
        (void) !::write(execPipe[1], &err, sizeof(err));
        _exit(127); // exec failed
    }

    // Parent process
    close(execPipe[1]);
    int childErrno = 0;
    ssize_t got = 0;
    {
        // Bounded, so a leaked write end can never hang startup again. A
        // successful exec closes the pipe immediately, so this returns at once
        // in every normal case; the timeout only fires if something is holding
        // the descriptor open, and then the right answer is to assume the engine
        // started and let the usual read timeouts report it.
        struct pollfd pfd{execPipe[0], POLLIN, 0};
        const int ready = poll(&pfd, 1, EXEC_STATUS_TIMEOUT_MS);
        if (ready > 0) {
            got = ::read(execPipe[0], &childErrno, sizeof(childErrno));
        } else if (ready == 0) {
            spdlog::warn("Timed out reading exec status for '{}'; assuming it started",
                         program);
        }
    }
    close(execPipe[0]);
    if (got == sizeof(childErrno)) {
        // The child never reached exec. Reap it, then say what actually went
        // wrong — which folder, which program, and the system's own reason.
        int status = 0;
        waitpid(pid_, &status, 0);
        pid_ = -1;
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        throw std::runtime_error(
            "cannot start '" + program + "' in '" + (workDir.empty() ? "." : workDir)
            + "': " + std::strerror(childErrno));
    }

    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    stdinFd_ = stdinPipe[1];
    stdoutFd_ = stdoutPipe[0];
    stderrFd_ = stderrPipe[0];
}

Process::~Process() {
    closeStdin();
    if (stdoutFd_ >= 0) close(stdoutFd_);
    if (stderrFd_ >= 0) close(stderrFd_);
}

bool Process::write(const std::string& data) const {
    if (stdinFd_ < 0) return false;
    ssize_t written = ::write(stdinFd_, data.c_str(), data.size());
    return written == static_cast<ssize_t>(data.size());
}

Process::ReadStatus Process::readLine(std::string& line, int timeoutMs) {
    line.clear();
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs < 0 ? 0 : timeoutMs);

    while (true) {
        size_t pos = stdoutBuffer_.find('\n');
        if (pos != std::string::npos) {
            line = stdoutBuffer_.substr(0, pos);
            stdoutBuffer_.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return ReadStatus::Ok;
        }

        if (timeoutMs >= 0) {
            if (stdoutFd_ < 0) return ReadStatus::Eof;
            for (;;) {
                const auto now = std::chrono::steady_clock::now();
                if (now >= deadline) return ReadStatus::Timeout;
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - now).count();

                struct pollfd pfd{};
                pfd.fd = stdoutFd_;
                pfd.events = POLLIN;
                const int ready = poll(&pfd, 1, static_cast<int>(remaining));
                if (ready > 0) break;                 // data (or hangup) available
                if (ready == 0) return ReadStatus::Timeout;
                if (errno == EINTR) continue;         // signal, not a real failure
                return ReadStatus::Eof;
            }
        }

        char buf[256];
        ssize_t bytesRead = read(stdoutFd_, buf, sizeof(buf));
        if (bytesRead <= 0) {
            if (!stdoutBuffer_.empty()) {
                line = std::move(stdoutBuffer_);
                stdoutBuffer_.clear();
                return ReadStatus::Ok;
            }
            return ReadStatus::Eof;
        }
        stdoutBuffer_.append(buf, bytesRead);
    }
}

bool Process::readLineStderr(std::string& line) {
    line.clear();
    while (true) {
        size_t pos = stderrBuffer_.find('\n');
        if (pos != std::string::npos) {
            line = stderrBuffer_.substr(0, pos);
            stderrBuffer_.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }

        char buf[256];
        ssize_t bytesRead = read(stderrFd_, buf, sizeof(buf));
        if (bytesRead <= 0) {
            if (!stderrBuffer_.empty()) {
                line = std::move(stderrBuffer_);
                stderrBuffer_.clear();
                return true;
            }
            return false;
        }
        stderrBuffer_.append(buf, bytesRead);
    }
}

void Process::closeStdin() {
    if (stdinFd_ >= 0) {
        close(stdinFd_);
        stdinFd_ = -1;
    }
}

int Process::wait() const {
    if (pid_ < 0) return -1;
    int status;
    waitpid(pid_, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool Process::waitFor(int timeoutMs) const {
    if (pid_ < 0) return true;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        int status;
        if (waitpid(pid_, &status, WNOHANG) != 0) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

void Process::terminate() {
    // SIGKILL immediately tears down the process and closes all its FDs,
    // so any pending read() on stdout/stderr returns EOF.
    if (pid_ > 0) kill(pid_, SIGKILL);
}

#endif

// StderrReaderThread implementation
StderrReaderThread::StderrReaderThread(Process& proc, std::function<void(const std::string&)> callback)
    : proc_(proc), callback_(std::move(callback)) {
    thread_ = std::thread([this]() { readLoop(); });
}

StderrReaderThread::~StderrReaderThread() {
    stop();
}

void StderrReaderThread::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void StderrReaderThread::readLoop() const {
    std::string line;
    while (running_ && proc_.readLineStderr(line)) {
        callback_(line);
    }
}

/// Resolves a relative path argument that the user wrote against the wrong base.
///
/// The engine runs with its working directory set to its configured `path`, so
/// every relative path in `parameters` is resolved by the engine against *that*
/// folder — while `path` itself is written relative to the goban root. Two bases
/// in one config block, and the natural reading is the wrong one. With the stock
/// layout the difference is invisible, because the model sits inside the engine
/// folder: `path=./engine/katago` with `-model ./models/x.bin.gz` means
/// `./engine/katago/models/x.bin.gz` either way.
///
/// It stops being invisible the moment the engine lives somewhere else. Issue
/// #55: `path=../user_folder/katago` with `-model ../user_folder/weights/x` was
/// read as `../user_folder/katago/../user_folder/weights/x` — the folder name
/// doubled — and KataGo failed to find its weights.
///
/// So: an argument that does not resolve against the engine's folder, but does
/// resolve against the goban root, is rewritten to an absolute path. This can
/// only ever change a case that is broken today — an argument that resolves in
/// the engine folder is left exactly as the user wrote it, and one that resolves
/// in neither place is left alone too, since it may be an output file the engine
/// has yet to create.
static std::string resolveAgainstAppRoot(const std::string& arg, const std::string& workDir) {
    // Only things that look like paths, and only relative ones.
    if (workDir.empty() || arg.empty() || arg[0] == '-') return arg;
    if (arg.find('/') == std::string::npos && arg.find('\\') == std::string::npos) return arg;

    std::error_code ec;
    std::filesystem::path relative(std::filesystem::u8path(arg));
    if (relative.is_absolute()) return arg;

    const std::filesystem::path inEngineDir = std::filesystem::u8path(workDir) / relative;
    if (std::filesystem::exists(inEngineDir, ec)) return arg;   // as the user meant it

    if (!std::filesystem::exists(relative, ec)) return arg;      // broken either way, or not a file yet

    const std::filesystem::path absolute = std::filesystem::absolute(relative, ec);
    if (ec) return arg;
    spdlog::info("Engine argument '{}' does not exist in '{}' but does relative to the "
                 "application folder; using '{}'", arg, workDir, absolute.string());
    return absolute.string();
}

// GtpClient implementation
static std::string findExecutable(const std::string& exe, const std::string& path) {
    // Check if executable exists in provided path
    std::string fullPath = path + "/" + exe;

    // Check if exe has a file extension (dot after last slash/backslash)
    size_t lastSlash = exe.find_last_of("/\\");
    size_t lastDot = exe.find_last_of('.');
    bool hasExtension = (lastDot != std::string::npos &&
                        (lastSlash == std::string::npos || lastDot > lastSlash));

#ifdef _WIN32
    // On Windows, CreateProcessW searches for exe in parent's current directory,
    // not in lpCurrentDirectory. So we must return the full path.
    if (!hasExtension) {
        std::string withExe = fullPath + ".exe";
        if (std::filesystem::exists(std::filesystem::u8path(withExe))) {
            return withExe;  // Return full path including .exe
        }
    }
    if (std::filesystem::exists(std::filesystem::u8path(fullPath))) {
        return fullPath;  // Return full path
    }
#else
    if (access(fullPath.c_str(), X_OK) == 0) {
        return "./" + exe;  // Return "./<exe>", Process will chdir to path first
    }
#endif
    // Return as-is, let the OS search PATH
    return exe;
}

GtpClient::GtpClient(const std::string& exe, const std::string& cmdline,
                     const std::string& path, const nlohmann::json& messages)
 : exe(exe), vars({})
{
    spdlog::info("Starting GTP client [{}/{}]", path, exe);

    // A configured folder that does not exist is *not* fatal on its own. `path`
    // only ever supplied the working directory; `command` may name a binary the
    // system resolves from PATH, and the stock configuration relies on exactly
    // that — "path": "./engine/gnugo" with "command": "gnugo" is how a
    // distribution-installed GNU Go has always started.
    //
    // Refusing here regressed that. Before, the child's chdir() failed silently
    // and it carried on in the application folder, where execvp() found the
    // engine on PATH; making the folder an error turned a working setup into
    // "Failed to load engine 'GNU Go 3.8'". Warn and fall back to the
    // application folder instead — if the program really is missing, the child's
    // exec-status pipe still reports it with the system's own reason.
    std::string workDir = path;
    if (!workDir.empty()) {
        std::error_code ec;
        if (!std::filesystem::is_directory(std::filesystem::u8path(workDir), ec)) {
            spdlog::warn("Engine [{}] folder '{}' does not exist; running from the "
                         "application folder and letting the system resolve '{}' "
                         "(paths are relative to the application folder)",
                         exe, workDir, exe);
            workDir.clear();
        }
    }

    std::string program = findExecutable(exe, workDir);
    spdlog::info("About to run GTP engine [{}]", program);

    std::istringstream iss(cmdline);
    std::vector<std::string> params((std::istream_iterator<std::string>(iss)),
                                    std::istream_iterator<std::string>());
    for (auto& param : params) {
        param = resolveAgainstAppRoot(param, workDir);
    }

    std::ostringstream resolved;
    for (const auto& param : params) resolved << ' ' << param;
    // The resolved arguments, not the configured string: when one has been
    // rewritten, what the engine actually received is the useful thing to see.
    spdlog::info("running child [{}{}] in [{}]", program, resolved.str(),
                 workDir.empty() ? std::string(".") : workDir);

    initFilters(messages);

    // Kept so the process can be started again after a timeout kill — see
    // revive(). Resolution (findExecutable, resolveAgainstAppRoot) happens once,
    // here, so a respawn cannot silently pick a different binary.
    program_ = program;
    params_ = params;
    workDir_ = workDir;

    try {
        spawn();
    } catch (const std::exception& e) {
        spdlog::error("Failed to start GTP engine: {}", e.what());
        throw;
    }
}

void GtpClient::spawn() {
    proc_ = std::make_unique<Process>(program_, params_, workDir_);

    // Always create stderr reader to prevent pipe buffer deadlock.
    // If the child process writes to stderr and nobody reads it,
    // the pipe buffer fills up and the child blocks.
    stderrReader_ = std::make_unique<StderrReaderThread>(*proc_, [this](const std::string& line) {
        (*this)(line);  // Logs at debug level and applies any filters
    });
}

bool GtpClient::revive() {
    if (!failed_.load()) return false;   // nothing to put back
    if (terminated_.load()) return false; // deliberately shut down; leave it dead

    if (reviveCount_ >= MAX_REVIVES) {
        // Once, not on every loop iteration: failed_ stays set, so this branch
        // is reachable ten times a second otherwise.
        if (reviveCount_ == MAX_REVIVES) {
            ++reviveCount_;
            spdlog::error("{}: has stopped responding {} times; leaving it down for "
                          "the rest of the session", exe, MAX_REVIVES);
        }
        return false;
    }
    ++reviveCount_;

    // The reader before the process: it holds a reference to the Process and is
    // sitting in a read on its stderr pipe. The engine is already dead, so that
    // read has returned EOF and the join is immediate.
    stderrReader_.reset();
    proc_.reset();
    {
        std::lock_guard<std::mutex> lock(lastLineMutex_);
        lastLine.clear();   // belonged to the process that just died
    }

    try {
        spawn();
    } catch (const std::exception& e) {
        spdlog::error("{}: could not be restarted: {}", exe, e.what());
        return false;
    }

    // Only now: a command arriving between the spawn and this would be written
    // into a pipe nobody is reading yet.
    failed_ = false;
    spdlog::warn("{}: restarted after it stopped responding (attempt {} of {}). It "
                 "knows nothing of the game yet and must be resynchronised.",
                 exe, reviveCount_, MAX_REVIVES);
    return true;
}

void GtpClient::initFilters(const nlohmann::json& messages) {
    for(auto &&msg: messages) {
        addOutputFilter(
            msg.value("regex", ""),
            msg.value("output", ""),
            msg.value("var", ""));
    }
}

void replaceAll(std::string& out, const std::string& what, const std::string& by) {
    size_t index(0);
    while (index != std::string::npos) {
        spdlog::trace("replace [{}] by [{}] in [{}]", what, by, out);
        index = out.find(what, index);
        if (index != std::string::npos) {
            out.replace(index, what.size(), by);
            index += by.size();
        }
    }
}

static std::string& ltrim(std::string & str) {
    auto it2 = std::find_if(str.begin(), str.end(),
        [](char ch){return !std::isspace<char>(ch , std::locale::classic());});
    str.erase(str.begin(), it2);
    return str;
}

static std::string& rtrim(std::string & str) {
    auto it1 = std::find_if(str.rbegin(), str.rend(),
        [](char ch){ return !std::isspace<char>(ch , std::locale::classic());});
    str.erase(it1.base(), str.end());
    return str;
}

void GtpClient::operator()(const std::string& line) {
    spdlog::debug("gtp err = {}", line);
    for (auto &re: outputFilters) {
        std::smatch m;
        if(std::regex_search(line, m,  re.compiled)) {
            std::string output(re.output);
            for(size_t i = 0; i < m.size(); ++i) {
                std::ostringstream ss;
                ss << "$" << i;
                replaceAll(output, ss.str(), m[i].str());
            }
            spdlog::trace("output template matched [{}]...", output);
            interpolate(output);
            if(!re.var.empty()) {
                vars[re.var] = output;
                compileFilters();
            } else {
                std::lock_guard<std::mutex> lock(lastLineMutex_);
                lastLine = output;
            }
        }
    }
}

void GtpClient::compileFilters() {
    for (auto &re: outputFilters) {
        std::string regex(re.regex);
        interpolate(regex);
        spdlog::trace("dynamic regex template [{}] compiled as [{}]...", re.regex, regex);
        try {
            re.compiled = std::regex(regex);
        }
        catch (const std::regex_error& e) {
            spdlog::error("malformed regular expression [{}]: {}", regex, e.what());
        }
    }
}

void GtpClient::addOutputFilter(const std::string& msg, const std::string& format, const std::string& var) {
    outputFilters.push_back({msg, format, var});
    compileFilters();
}

std::string GtpClient::lastError() {
    std::lock_guard<std::mutex> lock(lastLineMutex_);
    return lastLine;
}

GtpClient::CommandOutput GtpClient::name() {
    return issueCommand("name");
}

GtpClient::CommandOutput GtpClient::version() {
    return issueCommand("version");
}

/// Where a client's ordinary command traffic is logged. See setQuiet().
static spdlog::level::level_enum trafficLevel(bool quiet) {
    return quiet ? spdlog::level::debug : spdlog::level::info;
}

GtpClient::CommandOutput GtpClient::issueCommand(const std::string& command) {
    CommandOutput ret;
    // Order matters: a timed-out engine must keep failing, whereas one shut
    // down on purpose reports success so teardown produces no error spam.
    if (failed_) return {};
    if (terminated_) return {"= "};

    spdlog::log(trafficLevel(quiet_), "{1} << {0}", command, exe);

    if (!proc_->write(command + "\n")) {
        if (!terminated_) spdlog::error("Failed to write command");
        return terminated_ ? CommandOutput{"= "} : ret;
    }

    spdlog::log(trafficLevel(quiet_), "getting response...");
    bool error = true;

    std::string line;
    while (true) {
        const Process::ReadStatus status = proc_->readLine(line, commandTimeoutMs_);

        if (status == Process::ReadStatus::Timeout) {
            if (terminated_) return {"= "};
            spdlog::error("{} >> TIMEOUT after {} ms (command: {})",
                          exe, commandTimeoutMs_, command);
            // The engine may still answer later, and that stray reply would be
            // read as the response to the *next* command — silently wrong
            // results are worse than a dead engine. Kill it so every subsequent
            // command fails fast and visibly instead.
            //
            // killProcess(), not terminateProcess(): the latter also raises
            // `terminated_`, which means "shut down on purpose, stop reporting
            // errors" — and an engine flagged that way is deliberately *not*
            // revivable. Setting it here left the failure indistinguishable from
            // a clean teardown, so nothing could put the engine back.
            spdlog::error("{}: terminating unresponsive engine to avoid a "
                          "desynchronised GTP stream", exe);
            failed_ = true;
            killProcess();
            return {};   // success() == false
        }

        if (status == Process::ReadStatus::Eof) break;

        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if(ret.empty()) {
            error = !line.empty() && line[0] != '=';
            if (line.empty()) {
                error = false;
            }
        }
        // The blank line terminating every GTP response is not content, and
        // logging it before the break made each refusal two entries — the
        // second one an error reading `gnugo >>  (command: ...)`, with nothing
        // in it. The message log counts entries, so an empty one costs the user
        // a badge as much as a real one does.
        if (line.empty()) break;
        if(!error) {
            spdlog::log(trafficLevel(quiet_), "{1} >> {0}", line, exe);
        } else {
            spdlog::error("{} >> {} (command: {})", exe, line, command);
        }
        ret.push_back(line);
    }
    if (terminated_) return {"= "};
    return ret;
}

bool GtpClient::streamCommand(const std::string& command,
                              const std::function<bool(const std::string&)>& onLine,
                              int idleTimeoutMs) {
    if (failed_ || terminated_) return false;

    spdlog::log(trafficLevel(quiet_), "{1} << {0} (stream)", command, exe);
    if (!proc_->write(command + "\n")) {
        if (!terminated_) spdlog::error("{}: failed to write stream command", exe);
        return false;
    }

    // The acknowledgement. An analysis stream opens with the ordinary GTP
    // success header and only then starts reporting, so a '?' here means the
    // engine does not know the command and there is nothing to drain.
    std::string line;
    if (proc_->readLine(line, idleTimeoutMs) != Process::ReadStatus::Ok) {
        spdlog::warn("{}: no acknowledgement for '{}'", exe, command);
        return false;
    }
    line.erase(line.find_last_not_of(" \n\r\t") + 1);
    if (line.empty() || line[0] != '=') {
        spdlog::warn("{} >> {} (stream command refused: {})", exe, line, command);
        return false;
    }

    while (true) {
        const Process::ReadStatus status = proc_->readLine(line, idleTimeoutMs);
        if (status == Process::ReadStatus::Timeout) {
            if (terminated_) return false;
            spdlog::error("{}: analysis stream silent for {} ms", exe, idleTimeoutMs);
            return false;
        }
        if (status == Process::ReadStatus::Eof) return false;

        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        // The blank line closes the response, which only happens once something
        // has stopped the stream from the other side.
        if (line.empty()) return true;

        spdlog::trace("{1} >> {0}", line, exe);
        if (!onLine(line)) return true;
    }
}

bool GtpClient::stopStreaming(int timeoutMs) {
    if (failed_ || terminated_) return true;

    if (!proc_->write("name\n")) {
        if (!terminated_) spdlog::error("{}: failed to write stream stop", exe);
        return false;
    }

    // Anything still in flight from the stream, then its closing blank line,
    // then `name`'s own response and *its* blank line. Only the second blank
    // means the pipe is quiescent, which is why a response has to be seen first.
    bool sawResponse = false;
    std::string line;
    while (true) {
        const Process::ReadStatus status = proc_->readLine(line, timeoutMs);
        if (status != Process::ReadStatus::Ok) {
            if (terminated_) return true;
            spdlog::error("{}: did not answer within {} ms after stopping the "
                          "analysis stream", exe, timeoutMs);
            return false;
        }
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if (!line.empty() && (line[0] == '=' || line[0] == '?')) {
            sawResponse = true;
        } else if (line.empty() && sawResponse) {
            return true;
        }
    }
}

bool GtpClient::success(const CommandOutput& ret) {
    return !ret.empty() && ret.at(0).front() == '=';
}

GtpClient::~GtpClient() {
    // Only an engine that is still alive gets asked to leave politely. Writing
    // into the stdin of a process we killed ourselves raises SIGPIPE, and the
    // only reason that has never shown up is that main.cpp ignores the signal
    // process-wide — a disposition goban_core has no business depending on, as
    // goban_tests proved by dying on it the moment the kill became synchronous.
    if (!failed_.load() && !terminated_.load()) {
        spdlog::debug("~GtpClient: sending quit to {}", exe);
        spdlog::default_logger()->flush();
        proc_->write("quit\n");
    }
    proc_->closeStdin();
    if (!proc_->waitFor(2000)) {
        spdlog::warn("~GtpClient: {} did not exit gracefully, force-terminating", exe);
        spdlog::default_logger()->flush();
        proc_->terminate();
    }
    (void) proc_->wait();
    spdlog::debug("~GtpClient: {} process dead, stopping stderr reader", exe);
    spdlog::default_logger()->flush();
    // Engine is dead — stderr pipe is broken, reader thread can exit
    if (stderrReader_) {
        stderrReader_->stop();
    }
    spdlog::debug("~GtpClient: {} cleanup complete", exe);
}

void GtpClient::killProcess() {
    if (!proc_) return;
    proc_->terminate();
    // Reap it. terminate() is a SIGKILL, so this returns at once — and without
    // it every killed engine left a zombie for the rest of the session, which
    // revive() would then produce one of per restart.
    (void) proc_->wait();
}

void GtpClient::terminateProcess() {
    terminated_ = true;
    killProcess();
}

void GtpClient::interpolate(std::string& out) {
    for(auto it = vars.begin(); it != vars.end(); ++it) {
        replaceAll(out, it.key(), it.value());
    }
    rtrim(ltrim(out));
}
