#include "player.h"

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
    // DEPRECATED: Use local board building via GameRecord::buildBoardFromMoves() instead.
    // This method relies on engine-specific GTP commands (list_stones, showboard) which
    // may not be supported by all engines (e.g., Pachi).
    spdlog::warn("DEPRECATED: GtpEngine::showboard() called - use local board building instead");

    board.territoryReady = false;

    // Try list_stones first (simple, engine-independent format: "= A1 B2 C3...")
    // Fall back to parsing showboard output if list_stones is not supported
    auto blackResult = GtpClient::issueCommand("list_stones black");

    if (GtpClient::success(blackResult)) {
        // list_stones supported - use it for both colors
        int blackCount = 0, whiteCount = 0;

        // Clear stones but preserve board size and other state
        for (int row = 0; row < board.getSize(); ++row) {
            for (int col = 0; col < board.getSize(); ++col) {
                board[Position(col, row)].stone = Color::EMPTY;
            }
        }

        // Parse black stones
        if (!blackResult.empty() && blackResult[0].length() > 2) {
            std::istringstream ss(blackResult[0].substr(2));  // Skip "= "
            Position pos;
            while (ss >> pos) {
                if (pos.col() >= 0 && pos.col() < board.getSize() &&
                    pos.row() >= 0 && pos.row() < board.getSize()) {
                    board[pos].stone = Color::BLACK;
                    blackCount++;
                }
            }
        }

        // Get and parse white stones
        auto whiteResult = GtpClient::issueCommand("list_stones white");
        if (GtpClient::success(whiteResult) && !whiteResult.empty() && whiteResult[0].length() > 2) {
            std::istringstream ss(whiteResult[0].substr(2));  // Skip "= "
            Position pos;
            while (ss >> pos) {
                if (pos.col() >= 0 && pos.col() < board.getSize() &&
                    pos.row() >= 0 && pos.row() < board.getSize()) {
                    board[pos].stone = Color::WHITE;
                    whiteCount++;
                }
            }
        }

        spdlog::debug("showboard via list_stones: {} black, {} white", blackCount, whiteCount);
    } else {
        // list_stones not supported - fall back to parsing showboard output
        spdlog::debug("list_stones not supported, falling back to showboard parsing");
        board.parseGtp(GtpClient::showboard());
    }

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
    board.territoryReady = success;
    return success;
}

const Board& GtpEngine::showterritory(bool final, Color colorToMove) {
    // DEPRECATED: Use local board building + applyTerritory() instead.
    // This method calls showboard() which relies on engine-specific GTP commands.
    spdlog::warn("DEPRECATED: GtpEngine::showterritory() called - use applyTerritory() instead");

    // Refresh board state from GTP engine first (reuses list_stones approach)
    showboard();
    estimateTerritory(final, colorToMove);
    board.score = final ? final_score() : 0.0f;
    // Set display flags so observers know to show territory
    board.showTerritory = true;
    board.showTerritoryAuto = true;
    board.invalidate();
    return board;
}

void GtpEngine::applyTerritory(Board& targetBoard) {
    // Apply territory calculation to an existing board (e.g., from local SGF replay)
    // This avoids calling showboard() which would overwrite stones with engine state

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