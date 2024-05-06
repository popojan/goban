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
                 reason(NO_REASON), adata(), metricsReady(false),
                 holdsStone(false)
    { }

    enum Reason { NO_REASON, DOUBLE_PASS, RESIGNATION } reason;
    struct Result {
        int black_territory;
        int white_territory;
        int black_prisoners;
        int white_prisoners;
        int black_captured;
        int white_captured;
        int black_area;
        int white_area;
        float delta;
        Reason reason;
        Result() :
                black_territory(0),
                white_territory(0),
                black_prisoners(0),
                white_prisoners(0),
                black_captured(0),
                white_captured(0),
                black_area(0),
                white_area(0),
                delta(0.0),
                reason(NO_REASON)
        { }
    } adata;
    bool metricsReady;
	bool holdsStone;
};


#endif
