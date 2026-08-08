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

void GtpEngine::applyTerritory(Board& targetBoard) {
    // Apply territory calculation to an existing board built locally from the
    // SGF replay, rather than asking the engine for the board state.

    // Get dead stones from engine (requires final_status_list support)
    auto deadResult = GtpClient::issueCommand("final_status_list dead");

    if (!GtpClient::success(deadResult)) {
        // Engine doesn't support final_status_list - graceful degradation
        spdlog::warn("Engine doesn't support final_status_list, territory not shown");
        targetBoard.territoryReady = false;
        return;
    }

    // Parse dead stone positions
    std::vector<Position> deadStones;
    if (!deadResult.empty()) {
        for (const auto& line : deadResult) {
            std::istringstream ss(line);
            char c = ss.peek();
            if (c == '=') ss.get();
            Position pos;
            while (ss >> pos) {
                if (pos.col() >= 0 && pos.row() >= 0) {
                    deadStones.push_back(pos);
                }
            }
        }
    }
    spdlog::debug("applyTerritory: {} dead stones from engine", deadStones.size());

    // Calculate territory using flood-fill
    targetBoard.calculateTerritoryFromDeadStones(deadStones);

    // Get score from engine
    targetBoard.score = final_score();

    // Set display flags
    targetBoard.showTerritory = true;
    targetBoard.showTerritoryAuto = true;
    targetBoard.territoryReady = true;
}

float GtpEngine::final_score() {
    if(const GtpClient::CommandOutput ret = GtpClient::issueCommand("final_score"); GtpClient::success(ret)) {
        std::istringstream ss(ret[0].substr(2));
        char winner;
        float score = 0.0;
        ss >> winner >> score;
        return winner == 'B' ? score : -score;
    }
    return 0.0f;
}

bool GtpEngine::supportsKataAnalyze() {
    // Check if engine supports kata-analyze by looking at name or trying the command
    std::string engineName = getName();
    std::transform(engineName.begin(), engineName.end(), engineName.begin(), ::tolower);
    return engineName.find("kata") != std::string::npos;
}

float GtpEngine::kataAnalyzeScore(const Color& colorToMove) {
    // Try kata-genmove_analyze for single-shot analysis (not streaming like kata-analyze)
    // Format: kata-genmove_analyze [color] [maxvisits]
    // This returns analysis AND makes a move, but we only care about the score
    // Alternative: use lz-analyze which also streams but we can stop it

    // Actually, use kata-raw-nn for instant neural network evaluation (no search)
    // Format: kata-raw-nn [symmetry]
    // Returns: whiteWin X blackWin Y ... (probabilities)

    // Simplest approach: use final_score first, this is a fallback
    // For kata-analyze streaming, we'd need async handling

    // Try the simpler approach: issue kata-analyze then immediately stop it
    std::string cmd = "kata-analyze " + std::string(colorToMove == Color::BLACK ? "B" : "W") + " 1";

    // Send the command but don't wait for normal termination
    // kata-analyze streams continuously, so we read what we can and stop it
    if (!GtpClient::issueCommand(cmd).empty()) {
        // Immediately send another command to stop the analysis stream
        // This causes kata-analyze to terminate
        auto stopResult = GtpClient::issueCommand("name");

        // The stop command response includes any pending kata-analyze output
        // Parse the accumulated output for score
        for (const auto& line : stopResult) {
            size_t pos = line.find("scoreLead ");
            if (pos != std::string::npos) {
                float score = 0.0f;
                std::istringstream ss(line.substr(pos + 10));
                ss >> score;
                spdlog::info("kata-analyze score: {:.1f}", score);
                return score;
            }
            pos = line.find("scoreMean ");
            if (pos != std::string::npos) {
                float score = 0.0f;
                std::istringstream ss(line.substr(pos + 10));
                ss >> score;
                spdlog::info("kata-analyze scoreMean: {:.1f}", score);
                return score;
            }
        }
    }

    spdlog::debug("kata-analyze response did not contain score");
    return 0.0f;
}