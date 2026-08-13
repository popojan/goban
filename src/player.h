/** \file
 *  \brief The player abstraction: human, SGF replay, and GTP engine.
 *
 * One interface — `genmove()` plus the board-manipulating GTP verbs — behind
 * which the game loop cannot tell a human from an engine. LocalHumanPlayer
 * blocks in `genmove()` on a condition variable until the UI hands it a move;
 * GtpEngine blocks on a pipe read. That symmetry is the design, and also its
 * sharpest edge: a `genmove()` in flight cannot be aborted, so only the game
 * thread may ever call one (ADR-0001).
 *
 * Constructed on the loader threads, used from the game thread, owned by
 * PlayerManager. `Engine` adds what only a real engine can answer — final score
 * and territory.
 */
#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include "gtpclient.h"
#include "Board.h"
#include <condition_variable>
#include <mutex>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

class Player
{
public:
    enum Type { LOCAL = 1, HUMAN = 2, ENGINE = 4, SGF_PLAYER = 8 };

    Player(std::string  name, int type) : name(std::move(name)), type(type) {

    }
    virtual Move genmove(const Color& colorToMove) = 0;

    virtual bool play(const Move& move) { (void)move;  return true; }
    virtual bool boardsize(unsigned int) { return true; }
    virtual bool fixed_handicap(int, std::vector<Position>&) { return false; }
    virtual bool komi(float) { return true; }
    virtual bool clear() { return true; }
    virtual bool undo() { return true; }
    virtual std::string getName() { return name; }
    virtual void suggestMove(const Move& move) { (void)move; }
    [[nodiscard]] bool isTypeOf(int t) const { return (type & t) != 0; }
    void addType(int t) { type |= t; }
    virtual ~Player() = default;
protected:
    std::string name;
    int type;
};

class LocalHumanPlayer: public Player {
public:
    explicit LocalHumanPlayer(const std::string& name): Player(name, LOCAL | HUMAN) {}
    Move genmove(const Color& ) override {
        std::unique_lock<std::mutex> lock(mut);
        cond.wait(lock, [this]() { return move != Move::INVALID; });
        Move ret(move);
        move = Move();
        return ret;
    }

    /// Hand the waiting genmove() a move.
    ///
    /// An INVALID move is ignored rather than stored. "Suggest nothing" is not a
    /// thing anyone means, and treating it as one silently discarded real moves:
    /// the game loop reads `queuedMove` (usually INVALID), sets `playerToMove`,
    /// and only then calls suggestMove. A click landing in that gap reaches
    /// GameThread::playLocalMove(), which sees `playerToMove` already set and
    /// delivers the move here — and the loop's own suggestMove(INVALID),
    /// arriving microseconds later, overwrote it. genmove() then waited for a
    /// move that had already been made.
    ///
    /// Rare, because the window is a few instructions wide, and invisible in
    /// interactive use: a stone fails to appear and the player clicks again. In
    /// a scripted run it is a scenario that hangs until its wait times out,
    /// which is how it was found.
    void suggestMove(const Move& m) override {
        if (m == Move::INVALID) return;
        {
            std::lock_guard<std::mutex> lock(mut);
            this->move = m;
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
    explicit Engine(const std::string& name) : Player(name, LOCAL | ENGINE), board(19)  {}
    Move genmove(const Color& colorToMove) override = 0;

    /// The score from the engine's point of view, positive for Black, or
    /// nullopt when it could not produce one.
    ///
    /// The distinction is not pedantry: 0.0 is a legitimate result (jigo), so a
    /// float alone cannot say "failed". Reading a failure as a score of zero is
    /// what sent scoring off to interrogate a second engine, and from there into
    /// a multi-minute stall — see GameThread::processScoring().
    virtual std::optional<float> final_score() = 0;

    /// Territory shading from the engine's dead-stone list, applied to a board
    /// built locally from the SGF. Returns whether a *score* was also obtained;
    /// the shading may be valid when the score is not, in which case
    /// `targetBoard.showTerritory` is set but `territoryReady` is not.
    virtual bool applyTerritory(Board& targetBoard) = 0;
    ~Engine() override = default;
protected:
    Board board;
};

class SGFPlayer : public Player {
public:
    explicit SGFPlayer(const std::string& name = "SGF Player") : Player(name, LOCAL | SGF_PLAYER), currentMoveIndex(0) {}
    
    void setMoves(const std::vector<Move>& moves) {
        sgfMoves = moves;
        currentMoveIndex = 0;
    }
    
    Move genmove(const Color& colorToMove) override {
        if (currentMoveIndex >= sgfMoves.size()) {
            return Move(Move::INVALID, colorToMove);
        }
        
        Move move = sgfMoves[currentMoveIndex];
        if (move.col == colorToMove) {
            currentMoveIndex++;
            return move;
        }
        
        return Move(Move::INVALID, colorToMove);
    }
    
    bool hasMoreMoves() const {
        return currentMoveIndex < sgfMoves.size();
    }
    
    void reset() {
        currentMoveIndex = 0;
    }
    
    size_t getCurrentMoveIndex() const {
        return currentMoveIndex;
    }
    
private:
    std::vector<Move> sgfMoves;
    size_t currentMoveIndex;
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
    bool fixed_handicap(int handicap, std::vector<Position>& stones) override;
    bool komi(float komi) override;
    bool play(const Move& m) override;
    bool boardsize(unsigned boardSize) override;
    bool clear() override;
    bool undo() override;
    std::optional<float> final_score() override;
    bool applyTerritory(Board& targetBoard) override;

    // KataGo-specific scoring via kata-analyze (returns 0.0 if not supported)
    float kataAnalyzeScore(const Color& colorToMove);
    bool supportsKataAnalyze();
};

#endif // PLAYER_H
