#include "player.h"
#include <sgf/sgf.hpp>


    Move SgfPlayer::genmove(const Color& ) {
        Move ret(move);
        if(move == Move::INVALID) {
            spdlog::debug("LOCK human genmove");
            std::unique_lock<std::mutex> lock(mut);
            waitingForInput = true;
            while(waitingForInput) {
                cond.wait(lock);
            }
            if(move == Move::UNDO)
                undo();
            else if(node) {
                //if (node->has("C")) {
                //    s = node->get("C")[0];
                //}
                bool haswb = false;
                while(haswb == false) {
                    if(node->has("B")) {
                        auto a = node->get("B");
                        std::string c;
                        if(a.size() <= 0|| a[0].size() < 2) {
                            c = std::string("pd");
                        } else {
                            c = a[0];
                        }
                        unsigned col = static_cast<unsigned>(c.at(0)- 'a');
                        unsigned row = 18 - static_cast<unsigned>(c.at(1) - 'a');
                        spdlog::debug("B {}", c);
                        move = Move(Position(col, row), Color::BLACK);
                        colorToMove = Color::WHITE;
                        haswb = true;
                    }
                    if (node->has("W")) {
                        auto a = node->get("W");
                        std::string c;
                        if(a.size() <= 0 || a[0].size() < 2) {
                            c = std::string("pd");
                        } else {
                            c = a[0];
                        }
                        unsigned col = static_cast<unsigned>(c.at(0) - 'a');
                        unsigned row = 18 - static_cast<unsigned>(c.at(1) - 'a');
                        spdlog::debug("W {}", c);
                        move = Move(Position(col, row), Color::WHITE);
                        colorToMove = Color::BLACK;
                        haswb = true;
                    }
                    if(haswb) {
                        history.push_back(node);
                        vhistory.push_back(variation);
                        nhistory.push_back(ni);
                        chistory.push_back(colorToMove);
                    }
                    nextMove();
                }
            } else {
                move = Move(Move::PASS, colorToMove);
                colorToMove = Color::other(colorToMove);
            }
            ret = move;
        }
        move = Move();
        return ret;
    }

    bool SgfPlayer::undo() {
        node = history.back();
        variation = vhistory.back();
        ni = nhistory.back();
        vhistory.pop_back();
        history.pop_back();
        nhistory.pop_back();
        colorToMove = chistory.back();
        chistory.pop_back();
        return true;
    }

    void SgfPlayer::nextMove() {
        auto& nodes = variation->nodes();
        ni += 1;
        if(ni < nodes.size()) {
            node = &nodes[ni];
        }
        else {
            int varc = variation->variations().size();
            ni = 0;
            if(varc > 0) {
                variation = &variation->variations()[0];
                auto& nodes = variation->nodes();
                node = &nodes[ni];
            } else {
                node = 0;
                spdlog::debug("PREMATURE END");
            }
        }
    }
    void SgfPlayer::suggestMove(const Move& move) {
        if(move == Move::UNDO)
            this->move = move;
        else
            this->move = Move();
        {
            spdlog::debug("LOCK suggest move [{}]", move.toString());
            std::lock_guard<std::mutex> lock(mut);
            waitingForInput = false;
        }
        cond.notify_one();
    }
    void SgfPlayer::parseSgf(const std::string& fname) {
        spdlog::debug("Before parsing SGF");
        auto problems(sgf::parse(fname));
        spdlog::debug("Parsed SGF");
        for(std::size_t i = 0; i < problems.size(); ++i) {
            sgf.push_back(std::shared_ptr<sgf::GameTree>(new sgf::GameTree(problems[i])));
        }
        spdlog::debug("sgf.size() = {}", sgf.size());
        variation = sgf.at(0).get();
        auto& nodes = variation->nodes();
        if(nodes.size() > 0) {
            node = &nodes[ni];
        }
        if(node->has("PW")){
            std::stringstream ss;
            ss << "sgf: " << node->get("PW")[0];
            names[1] = ss.str();
        }
        if(node->has("PB")){
            std::stringstream ss;
            ss << "sgf: " << node->get("PB")[0];
            names[0] = ss.str();
        }
    }
    bool SgfPlayer::clear() {
        if(history.size() > 0) {
            node = history.front();
            variation = vhistory.front();
            ni = nhistory.front();
            vhistory.clear();
            history.clear();
            nhistory.clear();
            colorToMove = chistory.front();
            chistory.clear();
        }
        return true;
    }

    std::string& SgfPlayer::getName(const Color& whose) {
        return whose == Color::BLACK ? names[0] : names[1];
    }


Move GtpEngine::genmove(const Color& colorToMove) {
    GtpClient::CommandOutput ret(issueCommand(colorToMove == Color::BLACK ? "genmove B" : "genmove W"));
    if(ret.size() < 1) {
        spdlog::warn("Invalid GTP response.");
        return Move(Move::INVALID, colorToMove);
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

bool GtpEngine::boardsize(int boardSize) {
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
        spdlog::debug("initial territory");
        board.clearTerritory(board.getSize());
        spdlog::debug("dead");
        success |= setTerritory(GtpClient::issueCommand("final_status_list dead"), board, Color::EMPTY);
        spdlog::debug("white");
        success |= setTerritory(GtpClient::issueCommand("final_status_list white_territory"), board, Color::WHITE);
        spdlog::debug("black");
        success |= setTerritory(GtpClient::issueCommand("final_status_list black_territory"), board, Color::BLACK);
    }
    else {
        std::stringstream ss;
        ss << "initial_influence " << colorToMove << " influence_regions";
        GtpClient::CommandOutput ret = GtpClient::issueCommand(ss.str());
        board.clearTerritory(board.getSize());
        board.parseGtpInfluence(ret);
        ret = GtpClient::issueCommand("dragon_status");
        for (size_t i = 0; i < ret.size(); ++i) {
            spdlog::debug(ret[i]);
            std::stringstream ss(ret[i]);
            char c;
            if (i == 0) {
                ss >> c;
            }
            Position pos;
            ss >> pos;
            ss >> c >> c;
            if (c == 'd') {
                std::ostringstream ss;
                ss << "dragon_stones " << pos;
                GtpClient::CommandOutput ret = GtpClient::issueCommand(ss.str());
                if (GtpClient::success(ret)) {
                    std::istringstream ss(ret[0].substr(2));
                    Position p;
                    while ((ss >> p)){
                        board[p].influence = Color::other(board[p].stone);
                    }
                }
            }
        }
    }
    board.territoryReady = success;
    return success;
}

const Board& GtpEngine::showterritory(bool final, Color colorToMove) {
    estimateTerritory(final, colorToMove);
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

void GtpEngine::setEngineName(const std::string& nameExtra) {
    GtpClient::CommandOutput retName = GtpClient::name();
    spdlog::debug("name");
    GtpClient::CommandOutput retVersion = GtpClient::version();
    spdlog::debug("version");
    std::ostringstream ss;
    try {
        ss << retName.at(0).substr(2) << " " << retVersion.at(0).substr(2);
        spdlog::debug("parsed");
    }
    catch( std::out_of_range&) { }
    if (!nameExtra.empty())
        ss << " " << nameExtra;
    spdlog::debug(ss.str());
    Engine::setName(ss.str());
}
