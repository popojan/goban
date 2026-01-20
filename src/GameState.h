#ifndef GOBAN_VIEWSTATE_H
#define GOBAN_VIEWSTATE_H

#include "Board.h"
#include <vector>
#include <string>

// SGF markup types for board annotations
enum class MarkupType {
    LABEL,      // LB - text label
    TRIANGLE,   // TR - triangle marker
    SQUARE,     // SQ - square marker
    CIRCLE,     // CR - circle marker
    MARK        // MA - X marker
};

// Single markup annotation on the board
struct BoardMarkup {
    Position pos;
    MarkupType type;
    std::string label;  // Only used for LABEL type

    BoardMarkup(Position p, MarkupType t, const std::string& l = "")
        : pos(p), type(t), label(l) {}
};

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
    std::string comment;  // SGF comment for current move (C property)
    std::vector<BoardMarkup> markup;  // SGF markup annotations (LB/TR/SQ/CR/MA)
    
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
