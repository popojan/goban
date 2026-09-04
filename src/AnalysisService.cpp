#include "AnalysisService.h"

#include "Configuration.h"
#include "GameSnapshot.h"
#include "GameThread.h"
#include "GobanModel.h"
#include "gtpclient.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace {

using namespace std::chrono_literals;

/// How often the loop looks at the world. Everything else is measured against
/// the debounce, so this only has to be smaller than that.
constexpr auto TICK = 100ms;

/// How long a position must hold still before it is worth analysing. Holding an
/// arrow key must not queue one search per key repeat, and a position the user
/// is already flicking past has no value analysed — so this is the semantics,
/// not a concession to cost.
constexpr auto DEBOUNCE = 200ms;

/// How often the engine is asked to report, in centiseconds, as the analysis
/// commands take it. Also the rate at which the stream notices that the cursor
/// has moved, since the loop is blocked in a read between reports.
constexpr int REPORT_INTERVAL_CS = 25;

/// A stream this quiet is wedged. Generous, because the first report of a
/// position can follow a network warm-up.
constexpr int STREAM_IDLE_TIMEOUT_MS = 15000;

/// Stopping is itself a command and an engine can refuse to come back from one.
/// Bounded on the `scoringTimeout()` precedent — a wedged engine must not be
/// able to hold this thread forever.
constexpr int STOP_TIMEOUT_MS = 5000;

/// Floor on how often the renderer is woken. The board is otherwise static
/// while the human thinks, and that idleness is what makes this feature
/// affordable at all (issue #52); spending it on a full ray-traced frame per
/// engine report would undo exactly the thing the design rests on.
constexpr auto MIN_PUBLISH_INTERVAL = 500ms;

/// Keys of the lz/kata-analyze report format that take exactly one value.
const std::set<std::string>& scalarKeys() {
    static const std::set<std::string> keys = {
        "move", "visits", "winrate", "scoreMean", "scoreLead", "scoreSelfplay",
        "scoreStdev", "prior", "lcb", "utility", "utilityLcb", "order", "weight",
        "edgeVisits", "edgeWeight", "isSymmetryOf", "playSelectionValue",
    };
    return keys;
}

/// Keys whose value runs to the end of the block. `pv` is the obvious one;
/// `pvVisits` and the ownership maps are lists too, and a parser that assumed
/// one token per key would read their tail as further keys.
const std::set<std::string>& listKeys() {
    static const std::set<std::string> keys = {
        "pv", "pvVisits", "pvEdgeVisits", "ownership", "ownershipStdev",
        "movesOwnership",
    };
    return keys;
}

bool isKey(const std::string& token) {
    return token == "info" || scalarKeys().count(token) || listKeys().count(token);
}

/// A bare GTP vertex — no `=` prefix, which is what `Move::parseGtp()` expects.
Move parseVertex(const std::string& vertex, const Color& col) {
    if (vertex.empty()) return {Move::INVALID, col};
    std::string upper = vertex;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper == "PASS") return {Move::PASS, col};
    if (upper == "RESIGN") return {Move::RESIGN, col};

    std::istringstream ss(vertex);
    Position pos;
    ss >> pos;
    if (!pos) return {Move::INVALID, col};
    return {pos, col};
}

}  // namespace

std::vector<Move> playableMoves(const std::vector<Move>& path) {
    std::vector<Move> out;
    out.reserve(path.size());
    for (const auto& m : path) {
        if (m == Move::NORMAL || m == Move::PASS) out.push_back(m);
    }
    return out;
}

QualityPalette resolveQualityPalette(const nlohmann::json& global,
                                     const nlohmann::json& shader) {
    QualityPalette palette;   // the shipped fallback; every read below is optional

    // One helper for both sources, so the global block and a shader's override
    // cannot drift in what they accept. A stop that will not parse leaves that
    // stop alone rather than taking the whole array down: a typo in the third
    // colour must not silently revert the two the user got right.
    const auto readStops = [&palette](const nlohmann::json& from) {
        if (!from.is_object()) return;
        const auto it = from.find("move_quality");
        if (it == from.end()) return;
        if (!it->is_array() || it->size() != 3) {
            spdlog::warn("annotations.move_quality must be three colours "
                         "[best, middle, worst]; keeping the built-in palette");
            return;
        }
        glm::vec4* const stops[3] = {&palette.best, &palette.middle, &palette.worst};
        for (size_t i = 0; i < 3; ++i) {
            if (!(*it)[i].is_string()) continue;
            const auto text = (*it)[i].get<std::string>();
            if (text.empty()) continue;
            if (const auto parsed = parseHexColor(text)) {
                *stops[i] = *parsed;
            } else {
                spdlog::warn("annotations.move_quality[{}]: '{}' is not #rgb, "
                             "#rrggbb or #rrggbbaa", i, text);
            }
        }
    };

    readStops(global);
    readStops(shader);   // the shader's own board wins where it has an opinion

    // Thresholds from the global block only. A shader entry carrying them would
    // be saying ten points of win rate is a blunder under one board and not
    // under another, which is not a thing a shader can know.
    if (global.is_object()) {
        const auto it = global.find("move_quality_loss");
        if (it != global.end()) {
            if (it->is_array() && it->size() == 2
                && (*it)[0].is_number() && (*it)[1].is_number()) {
                const double slightly = (*it)[0].get<double>();
                const double blunder  = (*it)[1].get<double>();
                // Ordered and positive, or the ramp inverts and every move
                // reads as its opposite. Refusing is the safe failure here.
                if (slightly > 0.0 && blunder > slightly) {
                    palette.slightly = slightly;
                    palette.blunder  = blunder;
                } else {
                    spdlog::warn("annotations.move_quality_loss: needs 0 < "
                                 "concession ({}) < blunder ({}); keeping {} and {}",
                                 slightly, blunder, palette.slightly, palette.blunder);
                }
            } else {
                spdlog::warn("annotations.move_quality_loss must be two numbers "
                             "[concession, blunder]; keeping {} and {}",
                             palette.slightly, palette.blunder);
            }
        }
    }
    return palette;
}

glm::vec4 moveQualityColor(double winrateLoss, const QualityPalette& palette) {
    // Three stops, linear between them. What they are and why they sit where
    // they do is on QualityPalette; this is only the interpolation.
    const double loss = std::max(0.0, winrateLoss);
    if (loss <= palette.slightly) {
        const float t = static_cast<float>(loss / palette.slightly);
        return palette.best + (palette.middle - palette.best) * t;
    }
    if (loss >= palette.blunder) return palette.worst;
    const float t = static_cast<float>((loss - palette.slightly)
                                       / (palette.blunder - palette.slightly));
    return palette.middle + (palette.worst - palette.middle) * t;
}

std::vector<EvalLabel> evaluationLabels(const AnalysisReport& report,
                                        const std::set<std::pair<int, int>>& labelled,
                                        const std::set<std::pair<int, int>>& markup,
                                        size_t maxLabels,
                                        const QualityPalette& palette) {
    std::vector<EvalLabel> out;
    if (report.moves.empty() || maxLabels == 0) return out;

    // Ranked as the engine ranked them. `order` is authoritative where the
    // engine supplies it; visits are the fallback, exactly as the report's own
    // summary value is chosen.
    // A pass is ranked with the rest even though it can never be drawn: it has
    // no point on the board, but it is a legal candidate and KataGo does report
    // `move pass` — routinely once the endgame is settled, which is exactly when
    // this overlay is read most carefully. Dropping it here left the baseline
    // below anchored on the best *drawable* move, so with pass genuinely best
    // every board move was measured against the least bad of a bad set: the top
    // one came out at zero loss, in the best-move colour, labelled A. The
    // overlay was recommending that you fill your own territory.
    std::vector<const AnalysisMove*> ranked;
    ranked.reserve(report.moves.size());
    for (const auto& m : report.moves) {
        if (m.visits <= 0) continue;
        if (m.move == Move::NORMAL || m.move == Move::PASS) ranked.push_back(&m);
    }
    if (ranked.empty()) return out;
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const AnalysisMove* a, const AnalysisMove* b) {
                         if ((a->order < 0) != (b->order < 0)) return a->order >= 0;
                         if (a->order != b->order && a->order >= 0) return a->order < b->order;
                         return a->visits > b->visits;
                     });

    // Both win rates are in Black's frame, and the best move is by definition
    // the least bad for whoever is to move — so the absolute difference is the
    // loss whichever colour that is, and it can never come out negative.
    const double bestWinrate = ranked.front()->winrateBlack;

    char letter = 'A';
    for (const AnalysisMove* m : ranked) {
        if (out.size() >= maxLabels) break;
        // It counted towards the baseline; it cannot go on a point. It goes to
        // the margin instead, and takes no letter — the same reason a tint-only
        // move takes none: a board showing B and C with no A reads as broken.
        // The word carries it, and the colour says how good it is.
        if (m->move == Move::PASS) {
            EvalLabel label;
            label.pass = true;
            // Explicitly off the board. A default Position is (0, 0) — a real
            // point — so leaving it would hand a caller that forgot to check
            // `pass` a plausible A1 to draw on.
            label.pos = Position(-1, -1);
            label.text = "pass";
            label.color = moveQualityColor(std::abs(bestWinrate - m->winrateBlack), palette);
            out.push_back(label);
            continue;
        }
        const std::pair<int, int> key{m->move.pos.col(), m->move.pos.row()};
        if (markup.count(key)) continue;   // the user's own annotation wins

        EvalLabel label;
        label.pos = m->move.pos;
        label.color = moveQualityColor(std::abs(bestWinrate - m->winrateBlack), palette);
        if (labelled.count(key)) {
            // Tint only: the variation keeps its own "3a", and the colour still
            // says what the engine thinks of it.
            out.push_back(label);
            continue;
        }
        // Letters do not skip. If the engine's first choice was tint-only, the
        // next move it labels is 'A' — a board showing B and C with no A reads
        // as broken, and the true rank is in the colour anyway.
        label.text = std::string(1, letter++);
        out.push_back(label);
    }
    return out;
}

AnalysisSyncPlan planIncrementalSync(const std::vector<Move>& sent,
                                     const std::vector<Move>& target) {
    size_t prefix = 0;
    while (prefix < sent.size() && prefix < target.size() && sent[prefix] == target[prefix]) {
        ++prefix;
    }
    return {sent.size() - prefix, prefix};
}

/// The position the analysis engine should be at: everything needed to put an
/// engine there, and the id that says which position it is.
struct AnalysisTarget {
    unsigned long long positionId = 0;
    std::vector<Move> path;
    int boardSize = 0;
    float komi = 0.0f;
    std::vector<Position> setupBlack;
    std::vector<Position> setupWhite;
    Color colorToMove = Color::BLACK;
};

const char* analysisStateName(AnalysisState state) {
    switch (state) {
        case AnalysisState::Disabled:    return "disabled";
        case AnalysisState::Starting:    return "starting";
        case AnalysisState::Unavailable: return "unavailable";
        case AnalysisState::Yielded:     return "yielded";
        case AnalysisState::Running:     return "running";
    }
    return "disabled";
}

bool parseAnalysisLine(const std::string& line, const Color& colorToMove,
                       AnalysisReport& out) {
    if (line.rfind("info", 0) != 0) return false;

    std::istringstream ss(line);
    std::vector<std::string> tokens;
    for (std::string token; ss >> token;) tokens.push_back(token);

    const bool blackToMove = colorToMove == Color::BLACK;
    std::vector<AnalysisMove> moves;
    AnalysisMove current;
    bool inBlock = false;

    auto flush = [&]() {
        if (inBlock && current.move != Move::INVALID) moves.push_back(current);
        current = AnalysisMove{};
    };

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& key = tokens[i];
        if (key == "info") {
            flush();
            inBlock = true;
            continue;
        }
        if (!inBlock) continue;

        if (listKeys().count(key)) {
            // Consume to the next key. Vertices and numbers in a principal
            // variation are not keys, so this stops in the right place.
            while (i + 1 < tokens.size() && !isKey(tokens[i + 1])) ++i;
            continue;
        }
        if (!scalarKeys().count(key)) continue;   // an extension we do not read
        if (i + 1 >= tokens.size()) break;
        const std::string& value = tokens[++i];

        try {
            if (key == "move") {
                current.move = parseVertex(value, colorToMove);
            } else if (key == "visits") {
                current.visits = std::stoi(value);
            } else if (key == "order") {
                current.order = std::stoi(value);
            } else if (key == "winrate") {
                const double w = std::stod(value);
                current.winrateBlack = blackToMove ? w : 1.0 - w;
            } else if (key == "scoreLead") {
                const double s = std::stod(value);
                current.scoreLeadBlack = blackToMove ? s : -s;
            }
        } catch (const std::exception&) {
            // A malformed number is one bad field, not a bad engine. Skip it;
            // a block that never gets a move is dropped by flush().
            continue;
        }
    }
    flush();

    if (moves.empty()) return false;

    // The position's value is the best move's. `order 0` says which that is —
    // and it is not always the first block. An engine that omits `order`
    // entirely leaves every block at -1, and then the most-visited one wins;
    // that fallback is why `order` defaults to -1 rather than 0.
    const AnalysisMove* best = &moves.front();
    for (const auto& m : moves) {
        if (m.order == 0) { best = &m; break; }
        if (m.visits > best->visits) best = &m;
    }

    out.winrateBlack = best->winrateBlack;
    out.scoreLeadBlack = best->scoreLeadBlack;
    out.visits = 0;
    for (const auto& m : moves) out.visits += m.visits;
    out.moves = std::move(moves);
    return true;
}

AnalysisService::AnalysisService(GobanModel& model, GameThread& engine)
    : model(model), engine(engine) {
}

AnalysisService::~AnalysisService() {
    stop();
}

std::string AnalysisService::startingEngineName() const {
    return serviceState.load() == AnalysisState::Starting ? engineName : std::string();
}

std::shared_ptr<const AnalysisReport> AnalysisService::report() const {
    std::lock_guard<std::mutex> lock(reportMutex);
    return lastReport;
}

void AnalysisService::setState(AnalysisState next) {
    const AnalysisState prev = serviceState.exchange(next);
    if (prev != next) {
        spdlog::debug("analysis: {} -> {}", analysisStateName(prev), analysisStateName(next));
    }
}

void AnalysisService::start() {
    if (running.exchange(true)) return;

    // Resolved once, here rather than in the constructor: the bots load on their
    // own threads and nothing has claimed the role until they have finished.
    if (auto cfg = engine.analysisConfig()) {
        botConfig = *cfg;
        configured = !botConfig.value("command", std::string()).empty();
        engineName = botConfig.value("name", botConfig.value("command", std::string()));
    }
    if (!configured) {
        // Nobody asked for this, so it is not a fault: state the outcome, name
        // the reason, and stop offering it (see isConfigured()).
        spdlog::info("live evaluation unavailable: no engine is designated for analysis "
                     "(\"analysis\": 1), and none carrying \"kibitz\" supports "
                     "kata-analyze or lz-analyze");
        setState(AnalysisState::Unavailable);
        running = false;
        return;
    }
    thread = std::thread([this] { loop(); });
}

void AnalysisService::stop() {
    if (!running.exchange(false)) {
        return;
    }
    wake.notify_all();
    if (thread.joinable()) thread.join();
}

void AnalysisService::setEnabled(bool value) {
    if (enabled.exchange(value) == value) return;
    if (value) {
        // A previous failure is not a life sentence: the user asking again is
        // the one signal worth retrying on.
        if (serviceState.load() == AnalysisState::Unavailable) {
            setState(AnalysisState::Disabled);
        }
    }
    wake.notify_all();
}

void AnalysisService::loop() {
    AnalysisTarget pending;
    bool havePending = false;
    auto pendingSince = std::chrono::steady_clock::now();

    while (running.load()) {
        {
            std::unique_lock<std::mutex> lock(wakeMutex);
            wake.wait_for(lock, TICK, [this] { return !running.load(); });
        }
        if (!running.load()) break;

        if (!enabled.load()) {
            if (client) stopEngine();
            setState(AnalysisState::Disabled);
            havePending = false;
            continue;
        }
        // A tsumego is a puzzle, and the engine's first suggestion *is* the
        // answer. availableActions() already refuses both toggles here
        // (UiActions.cpp), but a rule the commands honour and the renderer does
        // not is the disabled-button-that-guards-nothing shape all over again:
        // whoever switched the overlay on before loading the problem kept it,
        // and could not switch it off, because the menu item was greyed.
        //
        // Suppressed, not stopped. Dropping the report silences all three
        // surfaces at once — they each key off report() — while leaving the
        // process alive, because respawning KataGo costs a weights load every
        // time a problem is opened or closed.
        if (model.tsumegoMode.load()) {
            // Unconditionally, and before the report check below: leaving the
            // state at Running while nothing streams would be a lie the scenario
            // suite reads as success. setState is idempotent.
            setState(AnalysisState::Disabled);
            bool had = false;
            {
                std::lock_guard<std::mutex> lock(reportMutex);
                if (lastReport) { lastReport.reset(); had = true; }
            }
            if (had) {
                shownWinratePercent = -1;
                shownHasScore = false;
                if (onUpdate) onUpdate();
            }
            havePending = false;
            continue;
        }
        // Latched until the user switches the feature off and on again. Without
        // it a failed probe would respawn the process ten times a second.
        if (serviceState.load() == AnalysisState::Unavailable) continue;

        if (!client && !startEngine()) {
            setState(AnalysisState::Unavailable);
            continue;
        }

        const auto snap = model.snapshot();
        AnalysisTarget target;
        target.positionId = snap->positionId;
        target.path       = playableMoves(snap->pathMoves);
        target.boardSize  = snap->boardSize;
        target.komi       = snap->komi;
        target.setupBlack = snap->setupBlack;
        target.setupWhite = snap->setupWhite;
        target.colorToMove = snap->colorToMove;

        if (!havePending || target.positionId != pending.positionId) {
            pending = std::move(target);
            havePending = true;
            pendingSince = std::chrono::steady_clock::now();
            continue;   // a new position restarts the debounce, never queues
        }
        if (std::chrono::steady_clock::now() - pendingSince < DEBOUNCE) continue;

        if (!engine.analysisMayRun()) {
            setState(AnalysisState::Yielded);
            continue;
        }
        if (pending.boardSize <= 0) continue;   // nothing has been set up yet

        if (!syncTo(pending)) {
            // A desynchronised analysis engine reports confidently on a position
            // nobody is looking at, which is worse than reporting nothing.
            spdlog::error("analysis: [{}] could not be brought to the current "
                          "position; the evaluation overlay is off", engineName);
            stopEngine();
            setState(AnalysisState::Unavailable);
            continue;
        }
        setState(AnalysisState::Running);
        streamUntilStale(pending);
    }
    stopEngine();
}

bool AnalysisService::startEngine() {
    setState(AnalysisState::Starting);

    const auto path       = botConfig.value("path", std::string());
    const auto command    = botConfig.value("command", std::string());
    const auto parameters = botConfig.value("parameters", std::string());

    try {
        // No stderr filters: those exist to turn an engine's chatter into SGF
        // comments for a move it played, and this process never plays one.
        client = std::make_unique<GtpClient>(command, parameters, path,
                                             nlohmann::json::array());
    } catch (const std::exception& e) {
        spdlog::error("analysis: failed to start [{}]: {}", engineName, e.what());
        client.reset();
        return false;
    }

    // Every command this client sends is logged at debug rather than info. The
    // GTP trace in last_run.log is what a bug report is read from, and a
    // continuous stream plus a replay per navigation jump would bury it.
    client->setQuiet(true);
    if (botConfig.contains("timeout_ms")) {
        client->setCommandTimeout(botConfig.value("timeout_ms",
                                                  GtpClient::DEFAULT_COMMAND_TIMEOUT_MS));
    }

    const auto commands = client->issueCommand("list_commands");
    if (!GtpClient::success(commands)) {
        spdlog::warn("analysis: [{}] did not answer list_commands", engineName);
        client.reset();
        return false;
    }

    // Capability comes from the engine, not from its name. Preferring
    // kata-analyze is not arbitrary: it is a superset, and the score estimate
    // lives only there.
    bool hasKata = false, hasLz = false;
    for (std::string entry : commands) {
        if (!entry.empty() && entry[0] == '=') entry = entry.substr(1);
        const size_t first = entry.find_first_not_of(" \t");
        const size_t last  = entry.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        entry = entry.substr(first, last - first + 1);
        if (entry == "kata-analyze") hasKata = true;
        if (entry == "lz-analyze")   hasLz   = true;
    }
    if (!hasKata && !hasLz) {
        // Reaching here means the configuration asked for this engine — either
        // "analysis": 1 or an analysis_command override — so it not working is
        // worth a warning. The merely-inherited case never spawns; see
        // PlayerManager::analysisConfig().
        spdlog::warn("live evaluation unavailable: [{}] is configured for analysis but "
                     "supports neither kata-analyze nor lz-analyze", engineName);
        client.reset();
        return false;
    }
    analyzeCommand = hasKata ? "kata-analyze" : "lz-analyze";
    engineHasPosition = false;
    spdlog::info("analysis: [{}] started, using {}", engineName, analyzeCommand);
    return true;
}

void AnalysisService::stopEngine() {
    client.reset();          // ~GtpClient sends quit and reaps the process
    engineHasPosition = false;
    sentPath.clear();
    analyzeCommand.clear();
    {
        std::lock_guard<std::mutex> lock(reportMutex);
        lastReport.reset();
    }
    shownWinratePercent = -1;
    shownHasScore = false;
    if (onUpdate) onUpdate();
}

bool AnalysisService::syncTo(const AnalysisTarget& target) {
    const bool needReset = !engineHasPosition
        || sentBoardSize != target.boardSize
        || sentKomi != target.komi
        || sentSetupBlack != target.setupBlack
        || sentSetupWhite != target.setupWhite;

    auto ok = [](const GtpClient::CommandOutput& out) { return GtpClient::success(out); };
    auto play = [&](const Move& move) {
        std::ostringstream ss;
        ss << "play " << move;
        return ok(client->issueCommand(ss.str()));
    };

    if (needReset) {
        if (!ok(client->issueCommand("boardsize " + std::to_string(target.boardSize)))) return false;
        if (!ok(client->issueCommand("clear_board"))) return false;
        std::ostringstream komiCmd;
        komiCmd << "komi " << target.komi;
        client->issueCommand(komiCmd.str());   // advisory; engines may refuse
        for (const auto& stone : target.setupBlack) {
            if (!play(Move(stone, Color::BLACK))) return false;
        }
        for (const auto& stone : target.setupWhite) {
            if (!play(Move(stone, Color::WHITE))) return false;
        }
        sentPath.clear();
        sentBoardSize  = target.boardSize;
        sentKomi       = target.komi;
        sentSetupBlack = target.setupBlack;
        sentSetupWhite = target.setupWhite;
        engineHasPosition = true;
    }

    // The whole point of a private pipe: the common prefix is already on the
    // engine's board, so an arrow key costs one command instead of one per move
    // played so far. Replaying rather than setting up stones also keeps
    // KataGo's move-history planes meaningful.
    const AnalysisSyncPlan plan = planIncrementalSync(sentPath, target.path);
    const size_t prefix = plan.playFrom;
    for (size_t i = sentPath.size(); i > prefix; --i) {
        if (!ok(client->issueCommand("undo"))) {
            engineHasPosition = false;
            return false;
        }
        sentPath.pop_back();
    }
    for (size_t i = prefix; i < target.path.size(); ++i) {
        if (!play(target.path[i])) {
            engineHasPosition = false;
            return false;
        }
        sentPath.push_back(target.path[i]);
    }
    return true;
}

void AnalysisService::streamUntilStale(const AnalysisTarget& target) {
    // A new position is always worth one immediate repaint, whatever the last
    // one displayed.
    shownWinratePercent = -1;
    shownHasScore = false;

    std::ostringstream cmd;
    cmd << analyzeCommand << (target.colorToMove == Color::BLACK ? " B " : " W ")
        << REPORT_INTERVAL_CS;

    const bool ok = client->streamCommand(cmd.str(), [&](const std::string& line) {
        AnalysisReport next;
        next.positionId = target.positionId;
        if (parseAnalysisLine(line, target.colorToMove, next)) {
            publish(next);
        }
        if (!running.load() || !enabled.load()) return false;
        if (!engine.analysisMayRun()) return false;
        return model.snapshot()->positionId == target.positionId;
    }, STREAM_IDLE_TIMEOUT_MS);

    if (!client->stopStreaming(STOP_TIMEOUT_MS)) {
        spdlog::error("analysis: [{}] did not answer after the stream was "
                      "stopped; terminating it", engineName);
        client->terminateProcess();
        stopEngine();
        setState(AnalysisState::Unavailable);
        return;
    }
    if (!ok) {
        spdlog::warn("analysis: [{}] stream failed", engineName);
        stopEngine();
        setState(AnalysisState::Unavailable);
    }
}

void AnalysisService::publish(const AnalysisReport& next) {
    {
        std::lock_guard<std::mutex> lock(reportMutex);
        lastReport = std::make_shared<const AnalysisReport>(next);
    }

    // Wake the renderer only when what is *drawn* would differ. The rate limit
    // below never loses a change: the shown values are updated only when the
    // callback actually fires, so a suppressed change is still pending at the
    // next report and fires then.
    const int winratePercent = static_cast<int>(std::lround(next.winrateBlack * 100.0));
    const bool hasScore = next.scoreLeadBlack.has_value();
    const int scoreTenths = hasScore
        ? static_cast<int>(std::lround(*next.scoreLeadBlack * 10.0)) : 0;

    if (winratePercent == shownWinratePercent && hasScore == shownHasScore
        && scoreTenths == shownScoreTenths) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - lastPublished < MIN_PUBLISH_INTERVAL) return;

    shownWinratePercent = winratePercent;
    shownHasScore = hasScore;
    shownScoreTenths = scoreTenths;
    lastPublished = now;
    if (onUpdate) onUpdate();
}
