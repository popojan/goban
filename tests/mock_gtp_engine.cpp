// Deterministic mock GTP engine for hermetic tests.
//
// Why this exists: the test suite must not depend on GNU Go, Pachi or KataGo
// being installed, nor on their (nondeterministic, slow, version-dependent)
// move choices. This engine implements the GTP subset goban actually uses,
// with its own independent rules implementation — deliberately NOT reusing
// src/Board.cpp, so that a bug in Board cannot hide itself by also being
// present in the test double.
//
// Build (standalone):
//   g++ -std=gnu++17 -O2 -o mock_gtp_engine tests/mock_gtp_engine.cpp
//
// Options:
//   --name <s>          reported by the `name` command       (default MockGtp)
//   --version <s>       reported by the `version` command    (default 1.0)
//   --script D4 Q16 pass
//                       genmove returns these vertices in order, then falls
//                       back to first-legal-point scanning. Accepts the
//                       vertices as separate arguments, comma-separated, or a
//                       single space-separated string — GtpClient splits its
//                       configured parameter string on whitespace, so a
//                       multi-word value cannot survive as one argv entry.
//   --delay-ms <n>      sleep before every response (latency simulation)
//   --think-ms <n>      sleep before genmove replies only. This is what a slow
//                       engine actually looks like: fast to configure, slow to
//                       choose a move. Use this rather than --delay-ms when
//                       simulating "engine is thinking", or setup and sync will
//                       crawl too and mask what you are testing.
//   --hang-on <cmd>     never respond to <cmd> — sleeps forever. Used to test
//                       that GtpClient applies a read timeout instead of
//                       deadlocking the game thread.
//   --fail-on <cmd>     answer <cmd> with a GTP error response
//   --unknown <cmd>     answer <cmd> with "unknown command", to exercise the
//                       graceful-degradation paths (e.g. engines lacking
//                       final_status_list)
//   --log <file>        append every command received, for assertions about
//                       what the application actually sent
//   --stderr-file <p>   emit the contents of <p> on stderr before answering
//                       each genmove. Engines such as KataGo report analysis on
//                       stderr, and goban scrapes it with the regex filters
//                       configured in config/*.json; this lets those filters be
//                       tested. A file is used rather than an inline string
//                       because realistic analysis lines contain spaces and
//                       "--", neither of which survives argv splitting.
//
// genmove is deterministic in both modes: scripted, or the first legal point
// in a fixed scan order (bottom-left to top-right), passing when none exists.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr char kEmpty = '.';
constexpr char kBlack = 'X';
constexpr char kWhite = 'O';

// GTP column letters skip 'I'.
const std::string kCols = "ABCDEFGHJKLMNOPQRST";

struct Options {
    std::string name = "MockGtp";
    std::string version = "1.0";
    std::vector<std::string> script;
    int delayMs = 0;
    std::string hangOn;
    std::string failOn;
    std::string unknownCmd;
    std::string logFile;
    std::string stderrFile;
    int thinkMs = 0;
    // Analysis streaming. The values are fixed so a scenario can assert an exact
    // number; they are reported from the *side to move*'s point of view, exactly
    // as KataGo does, which is what makes the client's conversion to Black's
    // frame testable at all.
    bool analyze = true;
    int analyzeIntervalMs = 100;
    double analyzeWinrate = 0.6;
    double analyzeScoreLead = 2.5;
};

class Position {
public:
    explicit Position(int size) { reset(size); }

    void reset(int size) {
        size_ = size;
        cells_.assign(static_cast<size_t>(size) * size, kEmpty);
        capturedByBlack_ = 0;
        capturedByWhite_ = 0;
        history_.clear();
        koForbidden_.reset();
    }

    int size() const { return size_; }
    int capturedByBlack() const { return capturedByBlack_; }
    int capturedByWhite() const { return capturedByWhite_; }

    char at(int col, int row) const { return cells_[index(col, row)]; }

    bool onBoard(int col, int row) const {
        return col >= 0 && col < size_ && row >= 0 && row < size_;
    }

    // Returns false if the move is illegal (occupied, suicide, or ko).
    bool play(char color, int col, int row) {
        if (!onBoard(col, row) || at(col, row) != kEmpty) return false;
        if (koForbidden_ && koForbidden_->first == col && koForbidden_->second == row) {
            return false;
        }

        const std::vector<char> before = cells_;
        const char opponent = (color == kBlack) ? kWhite : kBlack;

        cells_[index(col, row)] = color;

        int captured = 0;
        for (const auto& n : neighbours(col, row)) {
            if (at(n.first, n.second) != opponent) continue;
            if (liberties(n.first, n.second) == 0) {
                captured += removeGroup(n.first, n.second);
            }
        }

        // Suicide is illegal: roll the whole thing back.
        if (liberties(col, row) == 0) {
            cells_ = before;
            return false;
        }

        if (color == kBlack) {
            capturedByBlack_ += captured;
        } else {
            capturedByWhite_ += captured;
        }

        // Basic ko: a single-stone capture that leaves the capturing stone in
        // atari forbids immediate recapture at the vacated point.
        koForbidden_.reset();
        if (captured == 1) {
            for (const auto& n : neighbours(col, row)) {
                if (before[index(n.first, n.second)] == opponent &&
                    at(n.first, n.second) == kEmpty &&
                    groupSize(col, row) == 1 && liberties(col, row) == 1) {
                    koForbidden_ = {n.first, n.second};
                    break;
                }
            }
        }

        history_.push_back(before);
        return true;
    }

    void playPass() { history_.push_back(cells_); koForbidden_.reset(); }

    bool undo() {
        if (history_.empty()) return false;
        cells_ = history_.back();
        history_.pop_back();
        koForbidden_.reset();
        return true;
    }

    void placeStone(char color, int col, int row) {
        if (onBoard(col, row)) cells_[index(col, row)] = color;
    }

    std::vector<std::pair<int, int>> stones(char color) const {
        std::vector<std::pair<int, int>> out;
        for (int row = 0; row < size_; ++row) {
            for (int col = 0; col < size_; ++col) {
                if (at(col, row) == color) out.emplace_back(col, row);
            }
        }
        return out;
    }

    // Area score from Black's perspective, komi excluded.
    int areaScore() const {
        int black = 0;
        int white = 0;
        std::vector<bool> seen(cells_.size(), false);

        for (int row = 0; row < size_; ++row) {
            for (int col = 0; col < size_; ++col) {
                const char c = at(col, row);
                if (c == kBlack) { ++black; continue; }
                if (c == kWhite) { ++white; continue; }
                if (seen[index(col, row)]) continue;

                // Flood the empty region and see whose stones enclose it.
                std::vector<std::pair<int, int>> region;
                std::vector<std::pair<int, int>> stack{{col, row}};
                seen[index(col, row)] = true;
                bool touchesBlack = false;
                bool touchesWhite = false;

                while (!stack.empty()) {
                    const auto [c0, r0] = stack.back();
                    stack.pop_back();
                    region.emplace_back(c0, r0);
                    for (const auto& n : neighbours(c0, r0)) {
                        const char nc = at(n.first, n.second);
                        if (nc == kBlack) { touchesBlack = true; continue; }
                        if (nc == kWhite) { touchesWhite = true; continue; }
                        if (!seen[index(n.first, n.second)]) {
                            seen[index(n.first, n.second)] = true;
                            stack.push_back(n);
                        }
                    }
                }

                if (touchesBlack && !touchesWhite) black += static_cast<int>(region.size());
                if (touchesWhite && !touchesBlack) white += static_cast<int>(region.size());
            }
        }
        return black - white;
    }

private:
    size_t index(int col, int row) const {
        return static_cast<size_t>(row) * size_ + col;
    }

    std::vector<std::pair<int, int>> neighbours(int col, int row) const {
        std::vector<std::pair<int, int>> out;
        const int dc[] = {1, -1, 0, 0};
        const int dr[] = {0, 0, 1, -1};
        for (int i = 0; i < 4; ++i) {
            const int c = col + dc[i];
            const int r = row + dr[i];
            if (onBoard(c, r)) out.emplace_back(c, r);
        }
        return out;
    }

    void collectGroup(int col, int row, std::set<std::pair<int, int>>& group) const {
        const char color = at(col, row);
        if (color == kEmpty) return;
        std::vector<std::pair<int, int>> stack{{col, row}};
        group.insert({col, row});
        while (!stack.empty()) {
            const auto [c0, r0] = stack.back();
            stack.pop_back();
            for (const auto& n : neighbours(c0, r0)) {
                if (at(n.first, n.second) == color && !group.count(n)) {
                    group.insert(n);
                    stack.push_back(n);
                }
            }
        }
    }

    int liberties(int col, int row) const {
        std::set<std::pair<int, int>> group;
        collectGroup(col, row, group);
        std::set<std::pair<int, int>> libs;
        for (const auto& s : group) {
            for (const auto& n : neighbours(s.first, s.second)) {
                if (at(n.first, n.second) == kEmpty) libs.insert(n);
            }
        }
        return static_cast<int>(libs.size());
    }

    int groupSize(int col, int row) const {
        std::set<std::pair<int, int>> group;
        collectGroup(col, row, group);
        return static_cast<int>(group.size());
    }

    int removeGroup(int col, int row) {
        std::set<std::pair<int, int>> group;
        collectGroup(col, row, group);
        for (const auto& s : group) cells_[index(s.first, s.second)] = kEmpty;
        return static_cast<int>(group.size());
    }

    int size_ = 19;
    std::vector<char> cells_;
    int capturedByBlack_ = 0;
    int capturedByWhite_ = 0;
    std::vector<std::vector<char>> history_;
    std::optional<std::pair<int, int>> koForbidden_;
};

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string formatVertex(int col, int row) {
    return std::string(1, kCols[static_cast<size_t>(col)]) + std::to_string(row + 1);
}

// Returns false for an unparseable vertex. "pass" and "resign" are handled by
// the caller before this point.
bool parseVertex(const std::string& text, int size, int& col, int& row) {
    if (text.empty()) return false;
    const std::string up = toUpper(text);
    const size_t c = kCols.find(up[0]);
    if (c == std::string::npos) return false;
    int number = 0;
    std::istringstream ss(up.substr(1));
    if (!(ss >> number)) return false;
    if (number < 1 || number > size) return false;
    col = static_cast<int>(c);
    row = number - 1;
    if (col >= size) return false;
    return true;
}

const std::vector<std::vector<std::string>>& handicapTable(int size) {
    // Standard fixed handicap points, in GTP's documented placement order.
    static const std::vector<std::vector<std::string>> k19 = {
        {}, {}, {"D4", "Q16"}, {"D4", "Q16", "D16"},
        {"D4", "Q16", "D16", "Q4"}, {"D4", "Q16", "D16", "Q4", "K10"},
        {"D4", "Q16", "D16", "Q4", "D10", "Q10"},
        {"D4", "Q16", "D16", "Q4", "D10", "Q10", "K10"},
        {"D4", "Q16", "D16", "Q4", "D10", "Q10", "K4", "K16"},
        {"D4", "Q16", "D16", "Q4", "D10", "Q10", "K4", "K16", "K10"}};
    static const std::vector<std::vector<std::string>> k13 = {
        {}, {}, {"D4", "K10"}, {"D4", "K10", "D10"},
        {"D4", "K10", "D10", "K4"}, {"D4", "K10", "D10", "K4", "G7"},
        {"D4", "K10", "D10", "K4", "D7", "K7"},
        {"D4", "K10", "D10", "K4", "D7", "K7", "G7"},
        {"D4", "K10", "D10", "K4", "D7", "K7", "G4", "G10"},
        {"D4", "K10", "D10", "K4", "D7", "K7", "G4", "G10", "G7"}};
    static const std::vector<std::vector<std::string>> k9 = {
        {}, {}, {"C3", "G7"}, {"C3", "G7", "C7"},
        {"C3", "G7", "C7", "G3"}, {"C3", "G7", "C7", "G3", "E5"},
        {"C3", "G7", "C7", "G3", "C5", "G5"},
        {"C3", "G7", "C7", "G3", "C5", "G5", "E5"},
        {"C3", "G7", "C7", "G3", "C5", "G5", "E3", "E7"},
        {"C3", "G7", "C7", "G3", "C5", "G5", "E3", "E7", "E5"}};
    static const std::vector<std::vector<std::string>> kNone = {};
    if (size == 19) return k19;
    if (size == 13) return k13;
    if (size == 9) return k9;
    return kNone;
}

class Engine {
public:
    explicit Engine(Options opts) : opts_(std::move(opts)), pos_(19) {}

    int run() {
        std::string line;
        while (std::getline(std::cin, line)) {
            // Strip comments and trailing CR (Windows-style input).
            const size_t hash = line.find('#');
            if (hash != std::string::npos) line = line.substr(0, hash);
            if (!line.empty() && line.back() == '\r') line.pop_back();

            std::istringstream ss(line);
            std::string first;
            if (!(ss >> first)) continue;

            // An optional leading integer is a command id to echo back.
            std::string id;
            std::string command = first;
            if (!first.empty() && std::all_of(first.begin(), first.end(), ::isdigit)) {
                id = first;
                if (!(ss >> command)) continue;
            }

            std::vector<std::string> args;
            std::string arg;
            while (ss >> arg) args.push_back(arg);

            // Any input line terminates an analysis stream. That is the whole
            // protocol — there is no `stop` command — so it happens here, before
            // the line is looked at, and the closing blank line goes out before
            // whatever response this command produces.
            stopAnalysis();

            logCommand(line);

            if (!opts_.hangOn.empty() && command == opts_.hangOn) {
                // Deliberately never answer. The client must time out.
                for (;;) std::this_thread::sleep_for(std::chrono::hours(1));
            }
            if (opts_.delayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(opts_.delayMs));
            }
            if (!opts_.failOn.empty() && command == opts_.failOn) {
                respond(id, false, "forced failure (--fail-on)");
                continue;
            }
            if (!opts_.unknownCmd.empty() && command == opts_.unknownCmd) {
                respond(id, false, "unknown command");
                continue;
            }

            if (command == "quit") {
                respond(id, true, "");
                return 0;
            }
            dispatch(id, command, args);
        }
        return 0;
    }

private:
    void logCommand(const std::string& line) {
        if (opts_.logFile.empty()) return;
        std::ofstream out(opts_.logFile, std::ios::app);
        if (out) out << line << "\n";
    }

    void emitStderr() {
        if (opts_.stderrFile.empty()) return;
        std::ifstream in(opts_.stderrFile);
        if (!in) return;
        std::string line;
        while (std::getline(in, line)) std::cerr << line << "\n";
        std::cerr << std::flush;
    }

    void respond(const std::string& id, bool ok, const std::string& body) {
        std::lock_guard<std::mutex> lock(outMutex_);
        std::cout << (ok ? "=" : "?") << id;
        if (!body.empty()) std::cout << " " << body;
        // GTP responses terminate with a blank line.
        std::cout << "\n\n" << std::flush;
    }

    /// Starts an lz/kata-analyze stream: the ordinary success header with **no**
    /// terminating blank line, then a report every interval until something
    /// arrives on stdin. The blank line that closes the response is written by
    /// stopAnalysis(), which is the shape a real analysis engine has and the
    /// reason GtpClient needs a streaming reader at all.
    void startAnalysis(const std::string& id, const std::string& cmd,
                       const std::vector<std::string>& args) {
        // The colour is parsed only to reject a malformed request. The reported
        // values are deliberately fixed and always from the *side to move*'s
        // point of view, exactly as a real engine reports them — which is what
        // makes the client's conversion to Black's frame testable.
        if (!args.empty() && colorFrom(args[0]) == 0) {
            return respond(id, false, "syntax error");
        }
        int intervalMs = opts_.analyzeIntervalMs;
        // Second argument is the report interval in centiseconds, as both
        // kata-analyze and lz-analyze define it.
        if (args.size() > 1) {
            int centiseconds = 0;
            if (std::istringstream(args[1]) >> centiseconds) {
                if (centiseconds > 0) intervalMs = centiseconds * 10;
            }
        }
        const bool kata = cmd == "kata-analyze";

        {
            std::lock_guard<std::mutex> lock(outMutex_);
            std::cout << "=" << id << "\n" << std::flush;
        }

        analyzing_ = true;
        analysisThread_ = std::thread([this, intervalMs, kata] {
            // The first two empty points in scan order, so the report is
            // deterministic and follows the position the client synced us to.
            std::vector<std::string> candidates;
            for (int row = 0; row < pos_.size() && candidates.size() < 2; ++row) {
                for (int col = 0; col < pos_.size() && candidates.size() < 2; ++col) {
                    if (pos_.at(col, row) == kEmpty) candidates.push_back(formatVertex(col, row));
                }
            }
            if (candidates.empty()) candidates.push_back("pass");

            while (analyzing_.load()) {
                std::ostringstream report;
                for (size_t i = 0; i < candidates.size(); ++i) {
                    if (i) report << " ";
                    report << "info move " << candidates[i]
                           << " visits " << (100 - 40 * static_cast<int>(i))
                           << " utility 0.1"
                           << " winrate " << (opts_.analyzeWinrate - 0.05 * static_cast<double>(i));
                    if (kata) {
                        report << " scoreMean " << opts_.analyzeScoreLead
                               << " scoreLead " << opts_.analyzeScoreLead;
                    }
                    report << " order " << i
                           << " pv " << candidates[i];
                    if (candidates.size() > 1) report << " " << candidates[1 - i];
                }
                {
                    std::lock_guard<std::mutex> lock(outMutex_);
                    if (!analyzing_.load()) break;
                    std::cout << report.str() << "\n" << std::flush;
                }
                for (int slept = 0; slept < intervalMs && analyzing_.load(); slept += 10) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
    }

    void stopAnalysis() {
        if (!analyzing_.exchange(false)) return;
        if (analysisThread_.joinable()) analysisThread_.join();
        std::lock_guard<std::mutex> lock(outMutex_);
        std::cout << "\n" << std::flush;   // closes the analyze response
    }

    void dispatch(const std::string& id, const std::string& cmd,
                  const std::vector<std::string>& args) {
        if (cmd == "protocol_version") return respond(id, true, "2");
        if (cmd == "name") return respond(id, true, opts_.name);
        if (cmd == "version") return respond(id, true, opts_.version);

        if (cmd == "list_commands") {
            std::string commands =
                "protocol_version\nname\nversion\nknown_command\nlist_commands\n"
                "boardsize\nclear_board\nkomi\nplay\ngenmove\nundo\n"
                "fixed_handicap\nfinal_score\nfinal_status_list\nshowboard\nquit";
            // Capability is advertised here and nowhere else — a client that
            // decides what an engine can do from its *name* is the thing this
            // exists to test against.
            if (opts_.analyze) commands += "\nlz-analyze\nkata-analyze";
            return respond(id, true, commands);
        }

        if (cmd == "known_command") {
            if (args.empty()) return respond(id, false, "syntax error");
            static const std::set<std::string> known = {
                "protocol_version", "name", "version", "known_command", "list_commands",
                "boardsize", "clear_board", "komi", "play", "genmove", "undo",
                "fixed_handicap", "final_score", "final_status_list", "showboard", "quit"};
            const bool analyzeCmd = args[0] == "lz-analyze" || args[0] == "kata-analyze";
            const bool isKnown = (known.count(args[0]) > 0 || (analyzeCmd && opts_.analyze))
                                 && args[0] != opts_.unknownCmd;
            return respond(id, true, isKnown ? "true" : "false");
        }

        if (cmd == "lz-analyze" || cmd == "kata-analyze") {
            if (!opts_.analyze) return respond(id, false, "unknown command");
            return startAnalysis(id, cmd, args);
        }

        if (cmd == "boardsize") {
            int size = 0;
            if (args.empty() || !(std::istringstream(args[0]) >> size)) {
                return respond(id, false, "syntax error");
            }
            if (size < 2 || size > 19) return respond(id, false, "unacceptable size");
            pos_.reset(size);
            scriptIndex_ = 0;
            return respond(id, true, "");
        }

        if (cmd == "clear_board") {
            pos_.reset(pos_.size());
            scriptIndex_ = 0;
            return respond(id, true, "");
        }

        if (cmd == "komi") {
            if (args.empty()) return respond(id, false, "syntax error");
            std::istringstream ss(args[0]);
            float value = 0.0f;
            if (!(ss >> value)) return respond(id, false, "syntax error");
            komi_ = value;
            return respond(id, true, "");
        }

        if (cmd == "play") {
            if (args.size() < 2) return respond(id, false, "syntax error");
            const char color = colorFrom(args[0]);
            if (color == 0) return respond(id, false, "syntax error");
            const std::string vertex = toUpper(args[1]);
            if (vertex == "PASS") {
                pos_.playPass();
                return respond(id, true, "");
            }
            if (vertex == "RESIGN") return respond(id, true, "");
            int col = 0;
            int row = 0;
            if (!parseVertex(vertex, pos_.size(), col, row)) {
                return respond(id, false, "invalid coordinate");
            }
            if (!pos_.play(color, col, row)) return respond(id, false, "illegal move");
            return respond(id, true, "");
        }

        if (cmd == "genmove" || cmd == "reg_genmove") {
            if (args.empty()) return respond(id, false, "syntax error");
            const char color = colorFrom(args[0]);
            if (color == 0) return respond(id, false, "syntax error");
            emitStderr();
            if (opts_.thinkMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(opts_.thinkMs));
            }
            return respond(id, true, generateMove(color, cmd == "reg_genmove"));
        }

        if (cmd == "undo") {
            if (!pos_.undo()) return respond(id, false, "cannot undo");
            return respond(id, true, "");
        }

        if (cmd == "fixed_handicap") {
            int count = 0;
            if (args.empty() || !(std::istringstream(args[0]) >> count)) {
                return respond(id, false, "syntax error");
            }
            const auto& table = handicapTable(pos_.size());
            if (count < 2 || count >= static_cast<int>(table.size())) {
                return respond(id, false, "invalid number of handicap stones");
            }
            std::string out;
            for (const auto& vertex : table[static_cast<size_t>(count)]) {
                int col = 0;
                int row = 0;
                if (parseVertex(vertex, pos_.size(), col, row)) {
                    pos_.placeStone(kBlack, col, row);
                }
                if (!out.empty()) out += " ";
                out += vertex;
            }
            return respond(id, true, out);
        }

        if (cmd == "final_score") {
            const float score = static_cast<float>(pos_.areaScore()) - komi_;
            if (score > 0.0f) return respond(id, true, "B+" + trimFloat(score));
            if (score < 0.0f) return respond(id, true, "W+" + trimFloat(-score));
            return respond(id, true, "0");
        }

        if (cmd == "final_status_list") {
            if (args.empty()) return respond(id, false, "syntax error");
            const std::string what = args[0];
            if (what == "dead" || what == "seki") {
                // Deterministic and conservative: nothing is dead.
                return respond(id, true, "");
            }
            if (what == "alive") {
                std::string out;
                for (char color : {kBlack, kWhite}) {
                    for (const auto& s : pos_.stones(color)) {
                        if (!out.empty()) out += " ";
                        out += formatVertex(s.first, s.second);
                    }
                }
                return respond(id, true, out);
            }
            return respond(id, false, "invalid status");
        }

        if (cmd == "showboard") {
            return respond(id, true, "\n" + renderBoard());
        }

        respond(id, false, "unknown command");
    }

    static char colorFrom(const std::string& text) {
        const std::string up = toUpper(text);
        if (up == "B" || up == "BLACK") return kBlack;
        if (up == "W" || up == "WHITE") return kWhite;
        return 0;
    }

    static std::string trimFloat(float value) {
        std::ostringstream ss;
        ss.setf(std::ios::fixed);
        ss.precision(1);
        ss << value;
        return ss.str();
    }

    std::string generateMove(char color, bool regression) {
        // Scripted moves first, so tests can drive an exact game.
        while (scriptIndex_ < opts_.script.size()) {
            const std::string vertex = toUpper(opts_.script[scriptIndex_++]);
            if (vertex == "PASS") {
                if (!regression) pos_.playPass();
                return "pass";
            }
            if (vertex == "RESIGN") return "resign";
            int col = 0;
            int row = 0;
            if (!parseVertex(vertex, pos_.size(), col, row)) continue;
            if (regression) return formatVertex(col, row);
            if (pos_.play(color, col, row)) return formatVertex(col, row);
            // Scripted move was illegal in this position; fall through to scan.
            break;
        }

        for (int row = 0; row < pos_.size(); ++row) {
            for (int col = 0; col < pos_.size(); ++col) {
                if (pos_.at(col, row) != kEmpty) continue;
                if (regression) {
                    return formatVertex(col, row);
                }
                if (pos_.play(color, col, row)) return formatVertex(col, row);
            }
        }
        if (!regression) pos_.playPass();
        return "pass";
    }

    // GNU Go's showboard layout. Nothing in goban parses this any more (the
    // board is reconstructed locally from the move list), but a GTP engine is
    // expected to implement showboard, and it is handy when debugging by hand.
    std::string renderBoard() const {
        const int size = pos_.size();
        std::ostringstream out;

        std::string header = "  ";
        for (int col = 0; col < size; ++col) {
            header += " ";
            header += kCols[static_cast<size_t>(col)];
        }
        out << header << "\n";

        for (int row = size - 1; row >= 0; --row) {
            std::ostringstream rowText;
            rowText << (row + 1 < 10 ? " " : "") << (row + 1);
            for (int col = 0; col < size; ++col) {
                rowText << " " << pos_.at(col, row);
            }
            rowText << " " << (row + 1);
            std::string text = rowText.str();
            // Capture counts ride on the top two rows, as GNU Go does; the
            // parser keys on the trailing "stones".
            if (row == size - 1) {
                text += "     WHITE (O) has captured " +
                        std::to_string(pos_.capturedByWhite()) + " stones";
            } else if (row == size - 2) {
                text += "     BLACK (X) has captured " +
                        std::to_string(pos_.capturedByBlack()) + " stones";
            }
            out << text << "\n";
        }
        out << header;
        return out.str();
    }

    Options opts_;
    Position pos_;
    float komi_ = 0.0f;
    size_t scriptIndex_ = 0;

    // Analysis streaming. The reporting thread is the only thing that writes to
    // stdout while the main thread is blocked in getline, and stopAnalysis()
    // joins it before any response goes out, so outMutex_ guards a window that
    // is narrow but real: the two do overlap while a stop is being processed.
    std::atomic<bool> analyzing_{false};
    std::thread analysisThread_;
    std::mutex outMutex_;
};

// Splits on whitespace and commas, so a vertex list can arrive as one quoted
// string, as "A1,B2", or as separate argv entries.
std::vector<std::string> splitVertices(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    for (const char c : text) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
            if (!current.empty()) { out.push_back(current); current.clear(); }
        } else {
            current += c;
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        const auto next = [&]() -> std::string {
            return (i + 1 < argc) ? argv[++i] : std::string();
        };
        if (flag == "--name") opts.name = next();
        else if (flag == "--version") opts.version = next();
        else if (flag == "--script") {
            // Consume every following argument that is not itself a flag, so
            // that "--script E5 E4 pass" works after argv splitting.
            while (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
                for (const auto& v : splitVertices(argv[++i])) opts.script.push_back(v);
            }
        }
        else if (flag == "--delay-ms") opts.delayMs = std::atoi(next().c_str());
        else if (flag == "--think-ms") opts.thinkMs = std::atoi(next().c_str());
        else if (flag == "--hang-on") opts.hangOn = next();
        else if (flag == "--fail-on") opts.failOn = next();
        else if (flag == "--unknown") opts.unknownCmd = next();
        else if (flag == "--log") opts.logFile = next();
        else if (flag == "--stderr-file") opts.stderrFile = next();
        else if (flag == "--no-analyze") opts.analyze = false;
        else if (flag == "--analyze-interval-ms") opts.analyzeIntervalMs = std::atoi(next().c_str());
        else if (flag == "--analyze-winrate") opts.analyzeWinrate = std::atof(next().c_str());
        else if (flag == "--analyze-score") opts.analyzeScoreLead = std::atof(next().c_str());
        else {
            std::cerr << "mock_gtp_engine: unknown option " << flag << "\n";
            return 2;
        }
    }

    // GTP is line-oriented; make sure we are not tripped up by buffering.
    std::ios::sync_with_stdio(true);
    return Engine(std::move(opts)).run();
}
