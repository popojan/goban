#ifndef GOBAN_VIEWSTATE_H
#define GOBAN_VIEWSTATE_H

#include "Board.h"

class GameState {
public:
    Color colorToMove;
    std::string black;
    std::string white;
    int capturedBlack, capturedWhite;
    int reservoirBlack, reservoirWhite;
    float komi;
    int handicap;
    float result;
    std::string cmd;
    std::string err;
    
    enum Message {
        NONE, WHITE_PASS, BLACK_PASS, WHITE_RESIGNS, BLACK_RESIGNS,
        BLACK_RESIGNED, WHITE_RESIGNED, WHITE_WON, BLACK_WON, PAUSED,
        CALCULATING_SCORE
    };

    Message msg;

    GameState(): colorToMove(Color::BLACK), black(), white(), capturedBlack(0), capturedWhite(0),
                 reservoirBlack(32), reservoirWhite(32),
                 komi(0.5f), handicap(0), result(0.0f), cmd("xxx"), msg(PAUSED),
                 reason(NO_REASON), scoreDelta(0.0f), winner(Color::EMPTY), metricsReady(false),
                 holdsStone(false)
    { }

    enum Reason { NO_REASON, DOUBLE_PASS, RESIGNATION } reason;
    
    // Simplified game result - only contains essential scoring information
    float scoreDelta;  // Score difference (positive = black leads, negative = white leads)
    Color winner;      // Winner of the game (EMPTY = no winner/ongoing game)
    bool metricsReady;
	bool holdsStone;
};


#endif
