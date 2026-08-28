#include "player.h"

#include "ScenarioRecorder.h"

Move GtpEngine::genmove(const Color& colorToMove) {
    GtpClient::CommandOutput ret(issueCommand(colorToMove == Color::BLACK ? "genmove B" : "genmove W"));
    if(ret.empty()) {
        spdlog::warn("Invalid GTP response.");
        return {Move::INVALID, colorToMove};
    }
    spdlog::debug("Parsing move string [{}]", ret[0]);

    // Capture the reply so a recorded session can be replayed against the mock
    // engine, without the original (possibly nondeterministic) engine installed.
    if (ret[0].size() > 2 && ret[0][0] == '=') {
        std::string vertex = ret[0].substr(1);
        const size_t start = vertex.find_first_not_of(" \t");
        const size_t end = vertex.find_last_not_of(" \t\r\n");
        if (start != std::string::npos) {
            vertex = vertex.substr(start, end - start + 1);
            ScenarioRecorder::instance().recordEngineMove(getName(), vertex);
        }
    }

    return Move::parseGtp(ret[0], colorToMove);
}

bool GtpEngine::fixed_handicap(int handicap, std::vector<Position>& stones) {
    std::stringstream ssout;
    ssout << "fixed_handicap " << handicap;
    GtpClient::CommandOutput out = GtpClient::issueCommand(ssout.str());
    if (GtpClient::success(out)) {
        stones.clear();
        std::stringstream ssin(out[0].substr(2));
        Position pos;
        while ((ssin >> pos)){
            stones.push_back(pos);
        }
        return true;
    };
    return false;
}

bool GtpEngine::komi(float komi) {
    std::stringstream ssout;
    ssout << "komi " << komi;
    return GtpClient::success(GtpClient::issueCommand(ssout.str()));
}

bool GtpEngine::play(const Move& m) {
    // Only NORMAL and PASS moves are valid GTP play commands
    // RESIGN, INVALID, INTERRUPT, KIBITZED are not sendable via "play"
    if (m != Move::NORMAL && m != Move::PASS) {
        spdlog::debug("GtpEngine::play: skipping non-playable move type {}", m.toString());
        return true;  // No-op for non-playable moves
    }

    std::stringstream ssout;
    ssout << "play " << m;
    return GtpClient::success(GtpClient::issueCommand(ssout.str()));
}

bool GtpEngine::boardsize(unsigned boardSize) {
    std::stringstream ssout;
    ssout << "boardsize " << boardSize;
    bool success = GtpClient::success(GtpClient::issueCommand(ssout.str()));
    if (success) {
        board.clear(boardSize);  // Sync internal board state with new size
    }
    return success;
}

bool GtpEngine::clear() {
    board.clear(board.getSize());
    return GtpClient::success(GtpClient::issueCommand("clear_board"));
}

bool GtpEngine::undo() {
    return GtpClient::success(GtpClient::issueCommand("undo"));
}

namespace {

/// Appends the vertices in a `final_status_list` reply to `out`.
///
/// Shared by the `dead` and `seki` queries so the two cannot drift: the reply is
/// one or more lines, the first carrying the `=` GTP prefix, each holding
/// whitespace-separated vertices.
void parseVertexList(const GtpClient::CommandOutput& reply, std::vector<Position>& out) {
    for (const auto& line : reply) {
        std::istringstream ss(line);
        if (ss.peek() == '=') ss.get();
        Position pos;
        while (ss >> pos) {
            if (pos.col() >= 0 && pos.row() >= 0) out.push_back(pos);
        }
    }
}

}  // namespace

bool GtpEngine::applyTerritory(Board& targetBoard) {
    // Apply territory calculation to an existing board built locally from the
    // SGF replay, rather than asking the engine for the board state.
    //
    // Scoring is bounded more tightly than a genmove. The command timeout has to
    // tolerate a strong engine thinking, but nothing about scoring a finished
    // position justifies minutes — and when it does take minutes it is because
    // the engine is at the wrong position or wedged, which is precisely the case
    // that should fail rather than freeze the game thread.
    const ScopedTimeout boundedForScoring(*this, scoringTimeout());

    // Get dead stones from engine (requires final_status_list support)
    auto deadResult = GtpClient::issueCommand("final_status_list dead");

    if (!GtpClient::success(deadResult)) {
        // Engine doesn't support final_status_list - graceful degradation
        spdlog::warn("Engine [{}] doesn't support final_status_list, territory not shown",
                     getName());
        targetBoard.territoryReady = false;
        return false;
    }

    // Parse dead stone positions
    std::vector<Position> deadStones;
    parseVertexList(deadResult, deadStones);
    // Seki as well, and it is not optional detail: a group alive in seki has
    // eyes that belong to nobody, and the flood fill reaches them from exactly
    // the stones an ordinary eye is reached from. Asking only for `dead` — which
    // is all this did — hands those points to whoever surrounds them.
    //
    // `seki` is one of GTP 2's three standard statuses, alongside `alive` and
    // `dead`; GNU Go answers it (an invalid status comes back as
    // "? invalid status", so a refusal is distinguishable). Support is uneven in
    // practice, though, so a refusal is read as "no seki here" and costs nothing
    // — the same graceful degradation the dead list itself gets, one level down.
    std::vector<Position> sekiStones;
    const auto sekiResult = GtpClient::issueCommand("final_status_list seki");
    if (GtpClient::success(sekiResult)) {
        parseVertexList(sekiResult, sekiStones);
    } else {
        spdlog::debug("Engine [{}] did not answer final_status_list seki; "
                      "assuming no seki", getName());
    }

    spdlog::debug("applyTerritory: {} dead stones, {} seki stones from engine",
                  deadStones.size(), sekiStones.size());

    // Calculate territory using flood-fill
    targetBoard.calculateTerritoryFromDeadStones(deadStones, sekiStones);

    // The shading is valid from here on, whatever happens to the score.
    targetBoard.showTerritory = true;
    targetBoard.showTerritoryAuto = true;

    const std::optional<float> score = final_score();
    if (!score) {
        // Territory can be shown, but there is no result to state. Leaving
        // territoryReady false lets the caller try another engine or report the
        // failure honestly; writing 0.0 here claimed a drawn game instead, and
        // that invented zero is what set the scoring fallback going.
        spdlog::warn("Engine [{}] gave dead stones but no final_score", getName());
        targetBoard.territoryReady = false;
        return false;
    }

    targetBoard.score = *score;
    targetBoard.territoryReady = true;
    return true;
}

std::optional<float> GtpEngine::final_score() {
    const GtpClient::CommandOutput ret = GtpClient::issueCommand("final_score");
    if (!GtpClient::success(ret) || ret.empty() || ret[0].size() < 2) {
        return std::nullopt;
    }
    // "= B+12.5" / "= W+3" / "= 0" (jigo). A reply we cannot parse is a failure,
    // not a zero.
    std::istringstream ss(ret[0].substr(2));
    char winner = '\0';
    float score = 0.0f;
    if (!(ss >> winner)) {
        return std::nullopt;
    }
    if (winner == '0') {
        return 0.0f;  // drawn
    }
    if ((winner != 'B' && winner != 'W') || !(ss >> score)) {
        return std::nullopt;
    }
    return winner == 'B' ? score : -score;
}

