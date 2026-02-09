#ifndef GOBAN_GOBANMODEL_H
#define GOBAN_GOBANMODEL_H

#include "Metrics.h"
#include "Board.h"
#include "GameState.h"
#include <spdlog/spdlog.h>
#include <atomic>
#include "GameObserver.h"
#include "GameRecord.h"

class ElementGame;

class GobanModel: public GameObserver {
public:
    GobanModel(ElementGame *p, int boardSize = Board::DEFAULT_SIZE, int handicap = 0, float komi = 0.0f)
        : parent(p), invalidated(true),
          calcCapturedBlack(0), calcCapturedWhite(0), ddc{}, metrics(), cursor({0, 0}), board(boardSize) {
        // Initialize metrics so board can render before engine initialization
        metrics.calc(boardSize);
        state.metricsReady = true;  // Metrics are valid after calc()
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

    void setCursor(const Position& p) { cursor = p;}

public:
    ElementGame* parent;

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
};


#endif //GOBAN_GOBANMODEL_H
