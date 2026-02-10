#include "gtpclient.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>
#include <spdlog/spdlog.h>

#ifdef _WIN32
// Windows implementation
Process::Process(const std::string& program, const std::vector<std::string>& args, const std::string& workDir) {
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

bool Process::readLine(std::string& line) {
    line.clear();
    while (true) {
        // Check buffer first
        size_t pos = stdoutBuffer_.find('\n');
        if (pos != std::string::npos) {
            line = stdoutBuffer_.substr(0, pos);
            stdoutBuffer_.erase(0, pos + 1);
            // Remove \r if present
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }

        // Read more data
        char buf[256];
        DWORD bytesRead;
        if (!ReadFile(hStdoutRead_, buf, sizeof(buf), &bytesRead, NULL) || bytesRead == 0) {
            if (!stdoutBuffer_.empty()) {
                line = std::move(stdoutBuffer_);
                stdoutBuffer_.clear();
                return true;
            }
            return false;
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

void Process::closeStdout() {
    if (hStdoutRead_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hStdoutRead_);
        hStdoutRead_ = INVALID_HANDLE_VALUE;
    }
}

void Process::closeStderr() {
    if (hStderrRead_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hStderrRead_);
        hStderrRead_ = INVALID_HANDLE_VALUE;
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

bool Process::running() const {
    if (hProcess_ == INVALID_HANDLE_VALUE) return false;
    DWORD exitCode;
    return GetExitCodeProcess(hProcess_, &exitCode) && exitCode == STILL_ACTIVE;
}

#else
// POSIX implementation
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

Process::Process(const std::string& program, const std::vector<std::string>& args, const std::string& workDir) {
    int stdinPipe[2], stdoutPipe[2], stderrPipe[2];

    if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0 || pipe(stderrPipe) < 0) {
        throw std::runtime_error("Failed to create pipes");
    }

    pid_ = fork();
    if (pid_ < 0) {
        throw std::runtime_error("Failed to fork");
    }

    if (pid_ == 0) {
        // Child process
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stderrPipe[0]);

        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);

        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        if (!workDir.empty()) {
            chdir(workDir.c_str());
        }

        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(program.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(program.c_str(), argv.data());
        _exit(127); // exec failed
    }

    // Parent process
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

bool Process::readLine(std::string& line) {
    line.clear();
    while (true) {
        size_t pos = stdoutBuffer_.find('\n');
        if (pos != std::string::npos) {
            line = stdoutBuffer_.substr(0, pos);
            stdoutBuffer_.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }

        char buf[256];
        ssize_t bytesRead = read(stdoutFd_, buf, sizeof(buf));
        if (bytesRead <= 0) {
            if (!stdoutBuffer_.empty()) {
                line = std::move(stdoutBuffer_);
                stdoutBuffer_.clear();
                return true;
            }
            return false;
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

void Process::closeStdout() {
    if (stdoutFd_ >= 0) {
        close(stdoutFd_);
        stdoutFd_ = -1;
    }
}

void Process::closeStderr() {
    if (stderrFd_ >= 0) {
        close(stderrFd_);
        stderrFd_ = -1;
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

bool Process::running() const {
    if (pid_ < 0) return false;
    int status;
    return waitpid(pid_, &status, WNOHANG) == 0;
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

    std::string program = findExecutable(exe, path);
    spdlog::info("About to run GTP engine [{}]", program);

    std::istringstream iss(cmdline);
    std::vector<std::string> params((std::istream_iterator<std::string>(iss)),
                                    std::istream_iterator<std::string>());

    spdlog::info("running child [{} {}]", program, cmdline);

    initFilters(messages);

    try {
        proc_ = std::make_unique<Process>(program, params, path);

        // Always create stderr reader to prevent pipe buffer deadlock.
        // If the child process writes to stderr and nobody reads it,
        // the pipe buffer fills up and the child blocks.
        stderrReader_ = std::make_unique<StderrReaderThread>(*proc_, [this](const std::string& line) {
            (*this)(line);  // Logs at debug level and applies any filters
        });
    } catch (const std::exception& e) {
        spdlog::error("Failed to start GTP engine: {}", e.what());
        throw;
    }
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

GtpClient::CommandOutput GtpClient::showboard() {
    return issueCommand("showboard");
}

GtpClient::CommandOutput GtpClient::name() {
    return issueCommand("name");
}

GtpClient::CommandOutput GtpClient::version() {
    return issueCommand("version");
}

GtpClient::CommandOutput GtpClient::issueCommand(const std::string& command) {
    CommandOutput ret;
    if (terminated_) return {"= "};

    spdlog::info("{1} << {0}", command, exe);

    if (!proc_->write(command + "\n")) {
        if (!terminated_) spdlog::error("Failed to write command");
        return terminated_ ? CommandOutput{"= "} : ret;
    }

    spdlog::info("getting response...");
    bool error = true;

    std::string line;
    while (proc_->readLine(line)) {
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if(ret.empty()) {
            error = !line.empty() && line[0] != '=';
            if (line.empty()) {
                error = false;
            }
        }
        if(!error) {
            spdlog::info("{1} >> {0}", line, exe);
        } else {
            spdlog::error("{} >> {} (command: {})", exe, line, command);
        }
        if (line.empty()) break;
        ret.push_back(line);
    }
    if (terminated_) return {"= "};
    return ret;
}

bool GtpClient::success(const CommandOutput& ret) {
    return !ret.empty() && ret.at(0).front() == '=';
}

GtpClient::~GtpClient() {
    spdlog::debug("~GtpClient: sending quit to {}", exe);
    spdlog::default_logger()->flush();
    proc_->write("quit\n");
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

void GtpClient::terminateProcess() {
    terminated_ = true;
    if (proc_) proc_->terminate();
}

void GtpClient::interpolate(std::string& out) {
    for(auto it = vars.begin(); it != vars.end(); ++it) {
        replaceAll(out, it.key(), it.value());
    }
    rtrim(ltrim(out));
}
