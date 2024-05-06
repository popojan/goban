//
// Created by jan on 7.5.17.
//

#ifndef GOBAN_GOBANMODEL_H
#define GOBAN_GOBANMODEL_H

#include "Metrics.h"
#include "Board.h"
#include "GameState.h"
#include <spdlog/spdlog.h>
#include "AudioPlayer.hpp"
#include "GameObserver.h"
#include "GameRecord.h"

class ElementGame;

class GobanModel: public GameObserver {
public:
    GobanModel(ElementGame *p, int boardSize = Board::DEFAULT_SIZE, int handicap = 0, float komi = 0.0f)
            : parent(p), prevPass(false), over(false), invalidated(false),
            calcCapturedBlack(0), calcCapturedWhite(0), cursor({0,0}) {
        spdlog::info("Preloading sounds...");
        //newGame(boardSize, handicap, komi);
    }
    ~GobanModel();
    virtual void onGameMove(const Move&, const std::string& comment);
    virtual void onKomiChange(float);
    virtual void onHandicapChange(const std::vector<Position>&);
    virtual void onPlayerChange(int, const std::string&);

    virtual void onBoardSized(int);

    virtual void onBoardChange(const Board&);

	//void newGame(int boardSize = Board::DEFAULT_SIZE, int handicap = 0, float komi = 0.0f);

    bool isPointOnBoard(const Position& coord) const;

    void start() {
        started = true;
        over = false;
        game.initGame(board.getSize(), state.komi, handicapStones.size(), state.black, state.white);
        game.setHandicapStones(handicapStones);
    }

    Color changeTurn() {
        state.colorToMove = Color::other(state.colorToMove);
        spdlog::debug("changeTurn = {}", state.colorToMove.toString());
        return state.colorToMove;
    }

    bool isGameOver() { return state.adata.reason != GameState::NO_REASON;}

    unsigned getBoardSize() const;

    Move getUndoMove() const;

    float result(const Move& lastMove, GameState::Result& ret);

    void calcCaptured(Metrics& m, int capturedBlack, int capturedWhite);

    operator bool() { return !over && started; }

    void setCursor(const Position& p) { cursor = p;}

public:
    ElementGame* parent;

    //TODO multiple views in the future

    Board board;

    bool prevPass;
    bool over;
    bool started;
    GameState state;

    GameRecord game; //TODO find usage and create API including SGF generation

    bool invalidated;

    static const int maxCaptured = 191;
    int calcCapturedBlack, calcCapturedWhite;
    float ddc[8 * maxCaptured];

    Metrics metrics;

	std::mutex mutex;

    Position cursor;
    std::vector<Position> handicapStones;
};


#endif //GOBAN_GOBANMODEL_H
