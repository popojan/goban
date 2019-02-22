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
#include <GameObserver.h>

class ElementGame;

class GobanModel: public GameObserver {
public:
    GobanModel(ElementGame& p, int boardSize = Board::DEFAULTSIZE, int handicap = 0, float komi = 0.0f)
            : parent(p), prevPass(false), over(false), invalidated(false), cursor({0,0}) {
        console = spdlog::get("console");
        console->info("Preloading sounds...");
        newGame(boardSize, handicap, komi);
    }
    virtual void onGameMove(const Move& move) {
    }

	void newGame(int boardSize = Board::DEFAULTSIZE, int handicap = 0, float komi = 0.0f);

    bool isPointOnBoard(const Position& coord);

    void start() {
        started = true;
        over = false;
    }

    Color changeTurn() {
        state.colorToMove = Color::other(state.colorToMove);
        console->debug("changeTurn = {}", state.colorToMove.toString());
        return state.colorToMove;
    }

    void toggleAnimation();


    bool isGameOver() { return state.adata.reason != GameState::NOREASON;}

    unsigned getBoardSize() const;

    Move getPassMove();
    Move getUndoMove();

    //bool playMove(const Move& move);

    void update(const Move& move, const Board& result);
    void update(const Board& territory);
    float result(const Move& lastMove, GameState::Result& ret);

    void calcCaptured(Metrics& m, int capturedBlack, int capturedWhite);

    operator bool() { return !over && started; }

    void setCursor(const Position& p) { cursor = p;}


    //int boardChanged(Board&);
public:
    ElementGame& parent;

    //TODO multiple views in the future

    Board board;

    float komi;
    int handicap;
    bool prevPass;
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
    std::shared_ptr<spdlog::logger> console;

    Position cursor;
};


#endif //GOBAN_GOBANMODEL_H
