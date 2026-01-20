#include "player.h"
#include "SGF.h"

Move GtpEngine::genmove(const Color& colorToMove) {
    GtpClient::CommandOutput ret(issueCommand(colorToMove == Color::BLACK ? "genmove B" : "genmove W"));
    if(ret.empty()) {
        spdlog::warn("Invalid GTP response.");
        return {Move::INVALID, colorToMove};
    }
    spdlog::debug("Parsing move string [{}]", ret[0]);
    return Move::parseGtp(ret[0], colorToMove);
}

const Board& GtpEngine::showboard() {
    board.territoryReady = false;
    board.parseGtp(GtpClient::showboard());
    return board;
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
    std::stringstream ssout;
    ssout << "play " << m;
    return GtpClient::success(GtpClient::issueCommand(ssout.str()));
}

bool GtpEngine::boardsize(unsigned boardSize) {
    std::stringstream ssout;
    ssout << "boardsize " << boardSize;
    return GtpClient::success(GtpClient::issueCommand(ssout.str()));
}

bool GtpEngine::clear() {
    board.clear(board.getSize());
    return GtpClient::success(GtpClient::issueCommand("clear_board"));
}

bool GtpEngine::undo() {
    return GtpClient::success(GtpClient::issueCommand("undo"));
}

bool GtpEngine::estimateTerritory(bool finalize, const Color& colorToMove) {
    bool success = true;
    if (finalize) {
        spdlog::debug("Estimating final territory (GTP-standard method)");

        // Get dead stones using standard GTP command
        auto deadResult = GtpClient::issueCommand("final_status_list dead");
        spdlog::debug("final_status_list dead: {} response(s)", deadResult.size());

        // Parse dead stone positions
        std::vector<Position> deadStones;
        if (GtpClient::success(deadResult) && !deadResult.empty()) {
            // Parse positions from response (format: "= A1 B2 C3 ..." or multi-line)
            for (const auto& line : deadResult) {
                std::istringstream ss(line);
                // Skip leading '=' if present
                char c = ss.peek();
                if (c == '=') {
                    ss.get();
                }
                Position pos;
                while (ss >> pos) {
                    if (pos.col() >= 0 && pos.row() >= 0) {
                        deadStones.push_back(pos);
                        spdlog::debug("Dead stone at: col={} row={}", pos.col(), pos.row());
                    }
                }
            }
        }

        // Calculate territory using flood-fill from dead stones
        board.calculateTerritoryFromDeadStones(deadStones);
        success = true;

        spdlog::debug("Territory estimation completed with {} dead stones", deadStones.size());
    }
    else {
        /*
        std::stringstream ss;
        ss << "initial_influence " << colorToMove << " influence_regions";
        GtpClient::CommandOutput ret = GtpClient::issueCommand(ss.str());
        board.clearTerritory();
        board.parseGtpInfluence(ret);
        ret = GtpClient::issueCommand("dragon_status");
        for (size_t i = 0; i < ret.size(); ++i) {
            spdlog::debug(ret[i]);
            std::stringstream ssi(ret[i]);
            char c;
            if (i == 0) {
                ssi >> c;
            }
            Position pos;
            ssi >> pos;
            ssi >> c >> c;
            if (c == 'd') {
                {
                    std::ostringstream ss;
                    ss << "dragon_stones " << pos;
                    ret = GtpClient::issueCommand(ss.str());
                }
                if (GtpClient::success(ret)) {
                    std::istringstream ss(ret[0].substr(2));
                    Position p;
                    while ((ss >> p)){
                        board[p].influence = Color::other(board[p].stone);
                    }
                }
            }
        }
        */
    }
    board.territoryReady = success;
    return success;
}

const Board& GtpEngine::showterritory(bool final, Color colorToMove) {
    // Refresh board state from GTP engine first (play/undo don't update internal board)
    board.parseGtp(GtpClient::showboard());
    estimateTerritory(final, colorToMove);
    board.score = final ? final_score() : 0.0f;
    board.invalidate();
    return board;
}

bool GtpEngine::setTerritory(const GtpClient::CommandOutput& ret, Board& b, const Color& color) {
    if(GtpClient::success(ret) && ret.at(0).length() > 2) {
        std::stringstream ss;
        ss << ret.front().substr(2) << "\n";
        std::copy(++ret.begin(), ret.end(), std::ostream_iterator<std::string>(ss, "\n"));
        std::string s;
        spdlog::debug(ss.str());
        Position p;
        while((ss >> p)) {
            if(color == Color::EMPTY)
                b[p].influence = Color::other(b[p].stone);
            else
                b[p].influence = color;
        }
        return true;
    }
    return false;
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