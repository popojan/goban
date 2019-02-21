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

class ElementGame;

class GobanModel {
public:
    GobanModel(ElementGame& p, int boardSize = Board::DEFAULTSIZE, int handicap = 0, float komi = 0.0f)
            : parent(p), prevPass(false), over(false), invalidated(false), holdsStone(false), cursor({0,0}) {
        console = spdlog::get("console");
        console->info("Preloading sounds...");
        player.preload({"data/sound/collision.wav", "data/sound/stone.wav"});
        newGame(boardSize, handicap, komi);
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

    bool placeCursor(Board& target, const Position& last);

    //int boardChanged(Board&);
public:
    ElementGame& parent;
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

    bool holdsStone;
    Position cursor;
    AudioPlayer player;
};


#endif //GOBAN_GOBANMODEL_H
