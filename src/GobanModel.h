#ifndef GOBAN_GOBANMODEL_H
#define GOBAN_GOBANMODEL_H

#include "Metrics.h"
#include "Board.h"
#include "GamePhase.h"
#include "GameState.h"
#include <spdlog/spdlog.h>
#include <atomic>
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

    void start() {
        started = true;
        isGameOver = false;
        state.reason = GameState::NO_REASON;
    }
    
    void createNewRecord() {
        game.initGame(board.getSize(), state.komi, setupBlackStones.size(), state.black, state.white);
        game.setHandicapStones(setupBlackStones);
    }

    void pause() {
        started = false;
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

    explicit operator bool() const { return !isGameOver && started; }

    /// Lifecycle phase, derived from the `started` / `isGameOver` pair.
    ///
    /// ADR-0002 step 1: read-only, so it cannot change behaviour. The order of
    /// the tests below *is* the current semantics and must not be reshuffled:
    ///
    ///  * `isGameOver` wins over `started`. Ending a game mid-play sets
    ///    `isGameOver` without clearing `started`, so the pair (true, true) is
    ///    reachable and means Finished.
    ///  * `operator bool()` is exactly `phase() == GamePhase::Playing`.
    ///  * With both flags clear the flags alone cannot tell Setup from Paused;
    ///    the record does. An empty record — at the root with no continuation —
    ///    is a board being configured; anything else is a game being reviewed.
    ///
    /// Known wart, deliberately preserved: a freshly constructed model reports
    /// Finished, because `isGameOver` defaults to true as a stand-in for "not
    /// ready yet" until `onBoardSized()` runs. Step 2 should make the initial
    /// phase Setup; `!model` already covers the game loop for that case.
    [[nodiscard]] GamePhase phase() const {
        if (isGameOver) return GamePhase::Finished;
        if (started)    return GamePhase::Playing;
        return hasEmptyRecord() ? GamePhase::Setup : GamePhase::Paused;
    }

    void setCursor(const Position& p) { cursor = p;}

public:
    Board board;

    std::atomic<bool> isGameOver{true};
    std::atomic<bool> tsumegoMode{false};
    std::string tsumegoHintBlack;  // Localized "Black to move", set on UI thread
    std::string tsumegoHintWhite;  // Localized "White to move", set on UI thread
    std::atomic<bool> started{false};
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
};


#endif //GOBAN_GOBANMODEL_H
