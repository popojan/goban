#ifndef GOBAN_GOBANMODEL_H
#define GOBAN_GOBANMODEL_H

#include "Metrics.h"
#include "Board.h"
#include "GamePhase.h"
#include "GameState.h"
#include "GameSnapshot.h"
#include <spdlog/spdlog.h>
#include <atomic>
#include <memory>
#include <mutex>
#include "GameObserver.h"
#include "GameRecord.h"

class GobanModel: public GameObserver {
public:
    explicit GobanModel(int boardSize = Board::DEFAULT_SIZE, int handicap = 0, float komi = 0.0f)
        : invalidated(true),
          calcCapturedBlack(0), calcCapturedWhite(0), ddc{}, metrics(), cursor({0, 0}), board(boardSize) {
        // Initialize metrics so board can render before engine initialization
        metrics.calc(boardSize);
        state.metricsReady = true;  // Metrics are valid after calc()
        // These were previously accepted and then silently dropped, so any
        // caller passing them got the GameState defaults instead.
        state.handicap = handicap;
        state.komi = komi;
    }

    ~GobanModel() override;
    void onGameMove(const Move&, const std::string& comment) override;

    void onStonePlaced(const Move &move) override;

    void onKomiChange(float) override;
    void onHandicapChange(const std::vector<Position>&) override;
    void onPlayerChange(int, const std::string&) override;

    void onBoardSized(int) override;

    void onBoardChange(const Board&) override;

	bool isPointOnBoard(const Position& coord) const;

    // --- Lifecycle transitions (ADR-0002 step 2) -----------------------------
    // These are the only ways the phase moves. Each names an intent rather than
    // a flag write, so "who ended the game?" is answerable by grepping for
    // endGame() instead of reading three files.

    /// Begin or resume active play. Clears any stale end-of-game reason, so it
    /// doubles as "this game is no longer over".
    void start() {
        state.reason = GameState::NO_REASON;
        transitionTo(GamePhase::Playing, "start");
    }

    /// Stop active play without abandoning the game. A no-op on a finished
    /// game: something has to un-finish it first, which is enterReview().
    void pause() {
        if (phase() == GamePhase::Finished) return;
        transitionTo(restingPhase(), "pause");
    }

    /// Leave a game — finished or in progress — for review. This is what
    /// navigating backwards does: unlike pause() it clears the finished state,
    /// because the position being shown is no longer the end of the game.
    void enterReview() {
        transitionTo(restingPhase(), "enterReview");
    }

    /// The game ended. `reason` is DOUBLE_PASS or RESIGNATION; NO_REASON is
    /// rejected, since a finished game always has a result to show.
    void endGame(GameState::Reason reason) {
        if (reason == GameState::NO_REASON) {
            spdlog::error("endGame(NO_REASON) refused — a game cannot end for no reason");
            return;
        }
        state.reason = reason;
        transitionTo(GamePhase::Finished, "endGame");
    }

    void createNewRecord() {
        game.initGame(board.getSize(), state.komi, setupBlackStones.size(), state.black, state.white);
        game.setHandicapStones(setupBlackStones);
        transitionTo(GamePhase::Setup, "createNewRecord");
        // Replacing the record is a change the UI must see, and it does not go
        // through onBoardChange: the new-game path notifies onBoardSized, which
        // runs *before* this. See GameSnapshot.
        publishSnapshot();
    }

    Color changeTurn() {
        state.colorToMove = Color::other(state.colorToMove);
        spdlog::debug("changeTurn = {}", state.colorToMove.toString());
        return state.colorToMove;
    }


    unsigned getBoardSize() const;

    float result(const Move& lastMove);

    void calcCaptured(Metrics& m, int capturedBlack, int capturedWhite);
    void updateReservoirs();

    /// True exactly while the game loop may call genmove.
    explicit operator bool() const { return phase() == GamePhase::Playing; }

    /// The authoritative lifecycle state (ADR-0002 step 2).
    [[nodiscard]] GamePhase phase() const { return gamePhase.load(); }

    /// True when replacing the game on screen would discard something the user
    /// would miss, and so should be confirmed first. A finished game has
    /// nothing left to lose, an empty board has nothing to discard, and a
    /// tsumego is a puzzle rather than a game in progress.
    ///
    /// Note the second half of the record test: moveCount() is the *view*
    /// position, so it reads 0 while reviewing from the root of a real game.
    [[nodiscard]] bool hasGameWorthKeeping() const {
        if (tsumegoMode) return false;
        if (phase() == GamePhase::Finished) return false;
        return game.moveCount() > 0 || game.getLoadedMovesCount() > 0;
    }

    void setCursor(const Position& p) { cursor = p;}

public:
    Board board;

    std::atomic<bool> tsumegoMode{false};

    /// Recompute the published snapshot from the record. Must be called by
    /// whoever currently owns the record exclusively — the game thread during
    /// play and navigation, or the UI thread while the game loop is stopped.
    /// onBoardChange() is the funnel every position change already passes
    /// through, so that is where it happens; anything that changes what the UI
    /// displays without going through it must publish for itself.
    void publishSnapshot();

    /// The UI thread's view of the record. Never reads the SGF tree — see
    /// GameSnapshot and ADR-0006. Returns a value that stays valid however the
    /// record changes afterwards, so a caller can read several fields without
    /// them shifting underneath it.
    [[nodiscard]] std::shared_ptr<const GameSnapshot> snapshot() const;

    std::string tsumegoHintBlack;  // Localized "Black to move", set on UI thread
    std::string tsumegoHintWhite;  // Localized "White to move", set on UI thread
    GameState state;

    GameRecord game;

    bool invalidated;

    static constexpr int maxCaptured = 191;
    int calcCapturedBlack, calcCapturedWhite;
    float ddc[8 * maxCaptured];

    Metrics metrics;

	std::mutex mutex;

    Position cursor;
    std::vector<Position> setupBlackStones;
    std::vector<Position> setupWhiteStones;

private:
    /// True when there is no game to resume: the cursor is at the root and the
    /// root has no continuation. Note that `moveCount()` is the *view* position,
    /// so it is 0 at the root of a loaded game too — hence the second half.
    [[nodiscard]] bool hasEmptyRecord() const {
        return game.moveCount() == 0 && !game.hasNextMove();
    }

    /// Where "not playing, not finished" lands: an empty record is a board being
    /// configured, anything else is a game being reviewed.
    [[nodiscard]] GamePhase restingPhase() const {
        return hasEmptyRecord() ? GamePhase::Setup : GamePhase::Paused;
    }

    /// The single writer. Every phase change in the program goes through here,
    /// which is what makes "who ended the game?" a one-line grep and gives the
    /// lifecycle one log stream instead of three files' worth of flag writes.
    ///
    /// There is deliberately no rejection table: every ordered pair of phases
    /// turns out to be reachable through some supported user action (see the
    /// ADR-0002 implementation log). What the type buys is that the states are
    /// now mutually exclusive — the old `started && isGameOver` combination is
    /// unrepresentable — not that some pairs are forbidden.
    void transitionTo(GamePhase next, const char* via) {
        const GamePhase prev = gamePhase.exchange(next);
        if (prev != next) {
            spdlog::debug("phase: {} -> {} ({})", phaseName(prev), phaseName(next), via);
        }
    }

    /// Guards only the snapshot pointer swap — never held across anything that
    /// can block, which is the whole reason the snapshot exists rather than a
    /// lock over GameRecord itself.
    mutable std::mutex snapshotMutex;
    std::shared_ptr<const GameSnapshot> gameSnapshot{std::make_shared<const GameSnapshot>()};

    /// Authoritative lifecycle state. Atomic because the game thread ends games
    /// while the UI thread reads the phase every frame.
    std::atomic<GamePhase> gamePhase{GamePhase::Setup};
};


#endif //GOBAN_GOBANMODEL_H
