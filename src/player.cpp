#include "player.h"
#include <sgf/sgf.hpp>


    Move SgfPlayer::genmove(const Color& ) {
        Move ret(move);
        if(move == Move::INVALID) {
            console->debug("LOCK human genmove");
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
                        console->debug("B {}", c);
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
                        console->debug("W {}", c);
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
                console->debug("PREMATURE END");
            }
        }
    }
    void SgfPlayer::suggestMove(const Move& move) {
        if(move == Move::UNDO)
            this->move = move;
        else
            this->move = Move();
        {
            console->debug("LOCK suggest move [{}]", move.toString());
            std::lock_guard<std::mutex> lock(mut);
            waitingForInput = false;
        }
        cond.notify_one();
    }
    void SgfPlayer::parseSgf(const std::string& fname) {
        console->debug("Before parsing SGF");
        auto problems(sgf::parse(fname));
        console->debug("Parsed SGF");
        for(std::size_t i = 0; i < problems.size(); ++i) {
            sgf.push_back(std::shared_ptr<sgf::GameTree>(new sgf::GameTree(problems[i])));
        }
        console->debug("sgf.size() = {}", sgf.size());
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

    std::string& SgfPlayer::getName(Color whose) {
        return whose == Color::BLACK ? names[0] : names[1];
    }
