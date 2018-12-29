//
// Created by jan on 7.5.17.
//

#ifndef GOBAN_GOBANMODEL_H
#define GOBAN_GOBANMODEL_H

#include "Metrics.h"
#include "Board.h"
#include "GameState.h"


class ElementGame;

class GobanModel {
public:
    GobanModel(ElementGame& p, int boardSize = Board::DEFAULTSIZE, int handicap = 0, float komi = 0.0f)
            : parent(p), over(false), invalidated(false) {
        newGame(boardSize, handicap, komi);
    }

	void newGame(int boardSize = Board::DEFAULTSIZE, int handicap = 0, float komi = 0.0f);

    bool isPointOnBoard(const Position& coord);

    void start() {
        started = true;
        over = false;
    }

    void toggleAnimation();


    float result(GameState::Result& ret, const Move& lastMove);


    bool isGameOver() { return state.adata.reason != GameState::NOREASON;}

    unsigned getBoardSize() const;

    Move getPassMove();
    Move getUndoMove();

    //bool playMove(const Move& move);

    void Update();

    void calcCaptured(Metrics& m, int capturedBlack, int capturedWhite);

    operator bool() { return !over && started; }

    //int boardChanged(Board&);
public:
    ElementGame& parent;
    Board board, territory;

    float komi;
    int handicap;
    volatile bool over;
    volatile bool started;
    GameState state;
    std::vector<Move> history;
    bool invalidated;

    static const int maxCaptured = 191;
    int calcCapturedBlack, calcCapturedWhite;
    float ddc[6 * maxCaptured];

    Metrics metrics;
    int lastHandicap;
	std::mutex mutex;
};


#endif //GOBAN_GOBANMODEL_H
