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
#include <utility>

class Player
{
public:
    enum Role { NONE = 0, WHITE = 1, BLACK = 2, COACH = 4, SPECTATOR = 8, KIBITZ = 16};
    enum Type { LOCAL = 1, HUMAN = 2, ENGINE = 4 };

    Player(std::string  name, int role, int type) : name(std::move(name)), role(role), type(type) {

    }
    virtual Move genmove(const Color& colorToMove) = 0;

    virtual bool play(const Move& move) { (void)move;  return true; }
    virtual bool boardsize(unsigned int) { return true; }
    virtual bool fixed_handicap(int, std::vector<Position>&) { return false; }
    virtual bool komi(float) { return true; }
    virtual bool clear() { return true; }
    virtual bool undo() { return true; }
    virtual std::string getName() { return name; }
    [[nodiscard]] bool hasRole(int r) const { return (role & r) != 0;  }
    void setRole(int r, bool add = true) {
        if(add) role |=  r;
        else role &= ~r;
    }
    [[nodiscard]] int getRole() const {return role;}
    virtual void suggestMove(const Move& move) { (void)move; }
    [[nodiscard]] bool isTypeOf(int t) const { return (type & t) != 0;}
    virtual ~Player() = default;
protected:
    std::string name;
    int role;
    int type;
};

class LocalHumanPlayer: public Player {
public:
    explicit LocalHumanPlayer(const std::string& name): Player(name, NONE, LOCAL | HUMAN) {}
    Move genmove(const Color& ) override {
        Move ret(move);
        if(move == Move::INVALID) {
            spdlog::debug("LOCK human genmove");
            std::unique_lock<std::mutex> lock(mut);
            cond.wait(lock);
            ret = move;
        }
        move = Move();
        return ret;
    }

    void suggestMove(const Move& m) override {
        this->move = m;
        {
            spdlog::debug("LOCK suggest move = {}", m.toString());
            std::lock_guard<std::mutex> lock(mut);
        }
        cond.notify_one();
    }
protected:
        Move move;
        std::condition_variable cond;
        std::mutex mut;
};

class Engine: public Player
{
public:
    explicit Engine(const std::string& name) : Player(name, NONE, LOCAL | ENGINE), board(19)  {}
    Move genmove(const Color& colorToMove) override = 0;
    virtual const Board& showboard() = 0;
    virtual const Board& showterritory(bool final, Color colorToMove) = 0;
    virtual float final_score() = 0;
    ~Engine() override = default;
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
    }

    ~GtpEngine() override = default;

    Move genmove(const Color& colorToMove) override;
    const Board& showboard() override;
    bool fixed_handicap(int handicap, std::vector<Position>& stones) override;
    bool komi(float komi) override;
    bool play(const Move& m) override;
    bool boardsize(unsigned boardSize) override;
    bool clear() override;
    bool undo() override;
    virtual bool estimateTerritory(bool final, const Color& colorToMove);
    const Board& showterritory(bool final, Color colorToMove) override;
    float final_score() override;

protected:

    static bool setTerritory(const GtpClient::CommandOutput& ret, Board& b, const Color& color);
};

#endif // PLAYER_H
