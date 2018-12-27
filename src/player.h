#ifndef PLAYER_H
#define PLAYER_H

#include "spdlog/spdlog.h"
#include <string>
#include "gtpclient.h"
#include "Board.h"
#include <sstream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <algorithm>
#include <iterator>
#include <memory>

class Player
{
public:
    enum Role { NONE = 0, WHITE = 1, BLACK = 2, COACH = 4, SPECTATOR = 8, STANDBY = 16};
    enum Type { LOCAL = 1, HUMAN = 2, ENGINE = 4 };

    Player(const std::string& name, int role, int type) : name(name), role(role), type(type) {
        console = spdlog::get("console");
    }
    virtual Move genmove(const Color& colorToMove) = 0;

    virtual bool play(const Move& move) { (void)move;  return true; }
    virtual bool boardsize(int) { return true; }
    virtual bool fixed_handicap(int, std::vector<Position>&) { return false; }
    virtual bool komi(float) { return true; }
    virtual bool clear() { return true; }
    virtual bool undo() { return true; }
    void setName(const std::string& name) { this->name = name; }
    virtual std::string& getName(Color whose) { return name; }
    bool hasRole(int r) { return (role & r) != 0;  }
    void setRole(int r, bool add = true) {
        if(add) role |=  r;
        else role &= ~r;
    }
    int getRole() {return role;}
    virtual void suggestMove(const Move& move) { (void)move; }
    bool isTypeOf(int t) { return (type & t) != 0;}
    virtual ~Player() {}
protected:
    std::string name;
    int role;
    int type;
    std::shared_ptr<spdlog::logger> console;
};

class LocalHumanPlayer: public Player {
public:
    LocalHumanPlayer(const std::string& name): Player(name, NONE, LOCAL | HUMAN), waitingForInput(false) {}
    virtual Move genmove(const Color& ) {
        Move ret(move);
        if(move == Move::INVALID) {
            console->debug("LOCK human genmove");
            std::unique_lock<std::mutex> lock(mut);
            waitingForInput = true;
            while(waitingForInput) {
                cond.wait(lock);
            }
            ret = move;
        }
        move = Move();
        return ret;
    }

    virtual void suggestMove(const Move& move) {
        this->move = move;
        {
            console->debug("LOCK suggest move = {}", move);
            std::lock_guard<std::mutex> lock(mut);
            waitingForInput = false;
        }
        cond.notify_one();
    }
protected:
        Move move;
        bool waitingForInput;
        std::condition_variable cond;
        std::mutex mut;
};

class Engine: public Player
{
public:
    Engine(const std::string& name) : Player(name, NONE, LOCAL | ENGINE)  {}
    virtual Move genmove(const Color& colorToMove) = 0;
    virtual const Board& showboard() = 0;
    virtual const Board& showterritory(bool final = true, Color colorToMove = Color::EMPTY) = 0;
    virtual ~Engine() {}
    //TODO GTP API
};

class GtpEngine : public Engine, protected GtpClient {
public:

    GtpEngine(const std::string& exe, const std::string& cmdline, const std::string& path = "", const std::string& nameExtra = "") :
        Engine(nameExtra), GtpClient(exe, cmdline, path), board(19)
    {
        //setEngineName(nameExtra);
    }

    virtual ~GtpEngine() {
        GtpClient::issueCommand("quit");
    }

    virtual Move genmove(const Color& colorToMove) {
        GtpClient::CommandOutput ret(issueCommand(colorToMove == Color::BLACK ? "genmove B" : "genmove W"));
        if(ret.size() < 1) {
            Player::console->warn("Invalid GTP response.");
            return Move(Move::INVALID, colorToMove);
        }
        Player::console->debug("Parsing move string [{}]", ret[0]);
        return Move::parseGtp(ret[0], colorToMove);
    }
    virtual const Board& showboard() {
        board.parseGtp(GtpClient::showboard());
        return board;
    }

    virtual bool fixed_handicap(int handicap, std::vector<Position>& stones) {
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

    virtual bool komi(float komi) {
        std::stringstream ssout;
        ssout << "komi " << komi;
        return GtpClient::success(GtpClient::issueCommand(ssout.str()));
    }

    virtual bool play(const Move& m) {
        std::stringstream ssout;
        ssout << "play " << m;
        return GtpClient::success(GtpClient::issueCommand(ssout.str()));
    }
    virtual bool boardsize(int boardSize) {
        std::stringstream ssout;
        ssout << "boardsize " << boardSize;
        return GtpClient::success(GtpClient::issueCommand(ssout.str()));
    }
    virtual bool clear() {
        return GtpClient::success(GtpClient::issueCommand("clear"));
    }
    virtual bool undo() {
        return GtpClient::success(GtpClient::issueCommand("undo"));
    }
    virtual bool estimateTerritory(bool final, const Color& colorToMove) {
        //GtpClient::CommandOutput ret = GtpClient::issueCommand("known_command final_status_list");
        //if(GtpClient::success(ret) && ret.front().substr(2).compare("true") == 0) {
        if (final) {
            Player::console->debug("initial territory");
            territory.clear(board.getSize());
            bool success = true;
            Player::console->debug("dead");
            success |= setTerritory(GtpClient::issueCommand("final_status_list dead"), territory, Color::EMPTY);
            Player::console->debug("white");
            success |= setTerritory(GtpClient::issueCommand("final_status_list white_territory"), territory, Color::WHITE);
            Player::console->debug("black");
            success |= setTerritory(GtpClient::issueCommand("final_status_list black_territory"), territory, Color::BLACK);
            return success;
        }
        else {
            std::stringstream ss;
            ss << "initial_influence " << colorToMove << " influence_regions";
            GtpClient::CommandOutput ret = GtpClient::issueCommand(ss.str());
            territory.clear(board.getSize());
            territory.parseGtpInfluence(ret);
            ret = GtpClient::issueCommand("dragon_status");
            for (size_t i = 0; i < ret.size(); ++i) {
                Player::console->debug(ret[i]);
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
                            territory[p] = Color::other(board[p]);
                        }
                    }
                }
            }
            return true;
        }
    }
    virtual const Board& showterritory(bool final = true, Color colorToMove = Color::EMPTY) {
        estimateTerritory(final, colorToMove);
        territory.invalidate();
        return territory;
    }


protected:
    Board board;
    Board territory;

    bool setTerritory(const GtpClient::CommandOutput& ret, Board& b, const Color& color) {
        if(GtpClient::success(ret) && ret.at(0).length() > 2) {
            std::stringstream ss;
            ss << ret.front().substr(2) << "\n";
            std::copy(++ret.begin(), ret.end(), std::ostream_iterator<std::string>(ss, "\n"));
            std::string s;
            Player::console->debug(ss.str());
            return true;
        }
        return false;
    }

    void setEngineName(const std::string& nameExtra) {
        GtpClient::CommandOutput retName = GtpClient::name();
        Player::console->debug("name");
        GtpClient::CommandOutput retVersion = GtpClient::version();
        Player::console->debug("version");
        std::ostringstream ss;
        try {
            ss << retName.at(0).substr(2) << " " << retVersion.at(0).substr(2);
            Player::console->debug("parsed");
        }
        catch( std::out_of_range&) { }
        if (!nameExtra.empty())
            ss << " " << nameExtra;
        Player::console->debug(ss.str());
        Engine::setName(ss.str());
    }
};

namespace sgf {
class GameTree;
class Node;
}

class SgfPlayer: public Player {
public:
    SgfPlayer(const std::string& name, const std::string& fname): Player(name, NONE, LOCAL | HUMAN), waitingForInput(false), fname(fname), sgf(0), ni(0), variation(0), node(0) {
        parseSgf(fname);
    }
    virtual Move genmove(const Color& );
    virtual void suggestMove(const Move& move);
    virtual bool undo();
    virtual std::string& getName(Color whose);
    virtual bool clear();
private:
    void parseSgf(const std::string& fname);
    void nextMove();
protected:
        Move move;
        bool waitingForInput;
        std::condition_variable cond;
        std::mutex mut;
        std::string fname;
        std::vector<std::shared_ptr<sgf::GameTree> > sgf;
        std::size_t ni;
        sgf::GameTree* variation;
        sgf::Node* node;
        std::vector<sgf::Node* > history;
        std::vector<sgf::GameTree* > vhistory;
        std::vector<int> nhistory;
        std::vector<Color > chistory;
        Color colorToMove;
        std::string names[2];
};

#endif // PLAYER_H
