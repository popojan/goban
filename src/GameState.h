//
// Created by jan on 30.6.17.
//

#ifndef GOBAN_VIEWSTATE_H
#define GOBAN_VIEWSTATE_H

#include "Board.h"

class GameState {
public:
    Color colorToMove;
    std::string black;
    std::string white;
    int capturedBlack, capturedWhite;
    float komi;
    int handicap;
    float result;
    std::string cmd;

    enum Message {
        NONE, WHITE_PASS, BLACK_PASS, WHITE_RESIGNS, BLACK_RESIGNS,
        BLACK_RESIGNED, WHITE_RESIGNED, WHITE_WON, BLACK_WON, PAUSED,
        CALCULATING_SCORE
    };

    Message msg;

    std::array<int, 2> activePlayerId;

    GameState(): colorToMove(Color::BLACK), black(""), white(""), capturedBlack(-1), capturedWhite(-1),
            komi(-1.0f), handicap(-1), result(0.0f), cmd("xxx"), msg(PAUSED), reason(NOREASON), adata(),
            metricsReady(false), showTerritory(false), showOverlay(false), holdsStone(false), dirty(true)
    {
        activePlayerId[0] = activePlayerId[1] = -1;
    }

    enum Reason { NOREASON, DOUBLE_PASS, RESIGNATION, INVALID_MOVE } reason;
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
                reason(NOREASON)
        { }
    } adata;
    bool metricsReady;
	bool showTerritory, showOverlay, holdsStone;
	volatile bool dirty;

};


#endif //GOBAN_VIEWSTATE_H
