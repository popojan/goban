#ifndef PLAYER_H
#define PLAYER_H

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
    enum Role { NONE = 0, WHITE = 1, BLACK = 2, COACH = 4, SPECTATOR = 8, KIBITZ = 16, STANDBY = 32};
    enum Type { LOCAL = 1, HUMAN = 2, ENGINE = 4 };

    Player(const std::string& name, int role, int type) : name(name), role(role), type(type) {

    }
    virtual Move genmove(const Color& colorToMove) = 0;

    virtual bool play(const Move& move) { (void)move;  return true; }
    virtual bool boardsize(int) { return true; }
    virtual bool fixed_handicap(int, std::vector<Position>&) { return false; }
    virtual bool komi(float) { return true; }
    virtual bool clear() { return true; }
    virtual bool undo() { return true; }
    void setName(const std::string& name) { this->name = name; }
    virtual std::string& getName(const Color& whose = Color::EMPTY) { return name; }
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
};

class LocalHumanPlayer: public Player {
public:
    LocalHumanPlayer(const std::string& name): Player(name, NONE, LOCAL | HUMAN), waitingForInput(false) {}
    virtual Move genmove(const Color& ) {
        Move ret(move);
        if(move == Move::INVALID) {
            spdlog::debug("LOCK human genmove");
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
            spdlog::debug("LOCK suggest move = {}", move);
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
    Engine(const std::string& name) : Player(name, NONE, LOCAL | ENGINE), board(19)  {}
    virtual Move genmove(const Color& colorToMove) = 0;
    virtual const Board& showboard() = 0;
    virtual const Board& showterritory(bool final = true, Color colorToMove = Color::EMPTY) = 0;
    virtual ~Engine() {}
    virtual const Board& peek() const { return board; }
protected:
    Board board;
    //TODO GTP API
};

class GtpEngine : public Engine, public GtpClient {
public:

    GtpEngine(const std::string& exe, const std::string& cmdline, const std::string& path = "",
        const std::string& nameExtra = "", const nlohmann::json& messages = {})
    : Engine(nameExtra), GtpClient(exe, cmdline, path, messages)
    {
        //setEngineName(nameExtra);
    }

    virtual ~GtpEngine() {
        GtpClient::issueCommand("quit");
    }

    virtual Move genmove(const Color& colorToMove);
    virtual const Board& showboard();
    virtual bool fixed_handicap(int handicap, std::vector<Position>& stones);
    virtual bool komi(float komi);
    virtual bool play(const Move& m);
    virtual bool boardsize(int boardSize);
    virtual bool clear();
    virtual bool undo();
    virtual bool estimateTerritory(bool final, const Color& colorToMove);
    virtual const Board& showterritory(bool final = true, Color colorToMove = Color::EMPTY);

protected:

    bool setTerritory(const GtpClient::CommandOutput& ret, Board& b, const Color& color);
    void setEngineName(const std::string& nameExtra);
};

namespace sgf {
class GameTree;
class Node;
}

class SgfPlayer: public Player {
public:
    SgfPlayer(const std::string& name, const std::string& fname): Player(name, NONE, LOCAL | HUMAN),
        waitingForInput(false), fname(fname), sgf(0), ni(0), variation(0), node(0)
    {
        parseSgf(fname);
    }
    virtual Move genmove(const Color& );
    virtual void suggestMove(const Move& move);
    virtual bool undo();
    virtual std::string& getName(const Color& whose = Color::EMPTY);
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
