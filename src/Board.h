#ifndef BOARD_H
#define BOARD_H

#include <array>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <random>
#include <mutex>

class BoardGL;

class Color {
public:

    enum Value { EMPTY = 0, WHITE = 1, BLACK = 2 };

    Color(Value col = EMPTY): col(col) {}

    static Color other(const Color& b) {
        return b == EMPTY ?  b : b == BLACK ? WHITE : BLACK;
    }

    bool operator== (const Color& b) const {
        return col == b.col;
    }

    bool operator!= (const Color& b) const {
        return col != b.col;
    }

    friend std::ostream& operator<< (std::ostream& stream, const Color& color);

private:
    volatile Value col;

};

class Position {
    unsigned c;
    unsigned r;

    Position(unsigned char col, unsigned char row):
        c(col >= 'I' ? 7 + c - 'I' : c - 'A'),
        r(row) { }

public:

    Position(): c(0), r(0) {}

    Position(unsigned col, unsigned row):
        c(col),
        r(row) { }

    operator bool() const { return c >= 0 && r >= 0; }

    unsigned col() const { return c; }
    unsigned row() const { return r; }

    friend std::istream& operator>> (std::istream& stream, Position& color);

    friend std::ostream& operator<< (std::ostream& stream, const Position& color);
};

class Move {

public:

    enum Special { INVALID, INTERRUPT, NORMAL, PASS, UNDO, RESIGN };

    Move(): spec(INVALID), col(Color::EMPTY) {}

    Move(Special spec, const Color& color): spec(spec), col(color) {}

    Move(const Position& pos, const Color& col)
        : spec(NORMAL), pos(pos), col(col)
    { }

    operator bool() const { return spec != INVALID; }

    bool operator== (Special b) const { return spec == b; }

    bool operator== (const Color::Value & b) const { return col == b; }

    bool operator== (const Position& p) const { return pos == p; }

    static Move parseGtp(const std::string& s, const Color& col) {
        std::cerr<< "DEBUG = " << s << std::endl;
        std::istringstream ss(s); Move m(Move::INVALID, col); ss >> m; return m;
    }

    friend std::istream& operator>> (std::istream& stream, Move& );

    friend std::ostream& operator<< (std::ostream& stream, const Move& );

private:
    Special spec;
    Position pos;
    Color col;

};


struct CoordText {
    std::string text;
	unsigned layer;
    float x;
    float y;
};

//stonePlace

class Board
{
public:
    static const unsigned MAXBOARD = 19;
    static const unsigned MINBOARD = 9;
    static const unsigned BOARDSIZE = (MAXBOARD + 2) * (MAXBOARD + 1) + 1;
    static const unsigned DEFAULTSIZE = 19;
    typedef std::array<float, Board::MAXBOARD*Board::MAXBOARD << 2> Stones;
    typedef std::array<CoordText, Board::MAXBOARD*Board::MAXBOARD> Overlay;
private:
    typedef std::array<Color, BOARDSIZE> board_array;
public:
    typedef board_array::iterator iterator;
    typedef board_array::const_iterator const_iterator;

    const float* getStones() const;

    unsigned getSize() const { return boardSize;}

    std::size_t getSizeOf() const { return 4*sizeof(float)*stones.size(); }

    void copyStateFrom(const Board&);

    void setStoneRadius(float r) { rStone = r; r1 = 0.5f*(1.0f - r); dist = std::normal_distribution<float>(0.0f, 3.0f*r1); r1 *= 0.98f; }

    bool fixStone(unsigned i, unsigned j, unsigned i0, unsigned j0);

    const Overlay& getOverlay() const;

    const Color  operator[](const Position& pos) const { return board[ord(pos)]; }

    Color& operator[](const Position& pos) {
        return board[ord(pos)];
    }

    inline int row(unsigned pos) const { return (pos) / (MAXBOARD + 1u) - 1u; }
    inline int col(unsigned pos) const { return (pos) % (MAXBOARD + 1u) - 1u; }
    inline int ord(const Position& p) const { return ord(p.col(), p.row()); }
    inline int ord(unsigned i, unsigned j) const { return (MAXBOARD + 2) + (i) * (MAXBOARD + 1) + (j); }

    unsigned capturedCount(const Color& whose) const;

    void clear(unsigned boardsize = DEFAULTSIZE);

    Board(unsigned size = DEFAULTSIZE);

    bool parseGtp(const std::vector<std::string>& lines);

    bool parseGtpInfluence(const std::vector<std::string>& lines);

    const_iterator begin() const { return board.begin(); }
    const_iterator end() const { return board.end(); }
    iterator begin() { return board.begin(); }
    iterator end() { return board.end(); }

    void invalidate();

    friend std::ostream& operator<< (std::ostream&, const Board&);

    int updateStones(const Board& previous, const Board& current, bool showTerritory);

    bool isEmpty() const;
	bool toggleTerritory();
	bool toggleTerritoryAuto(bool);

private:
    const static float mBlackArea;
    const static float mWhiteArea;
    const static float mBlack;
    const static float mWhite;
    const static float mDeltaCaptured;

    std::array<Color, BOARDSIZE> board;

    Overlay overlay;

    unsigned capturedBlack;
    unsigned capturedWhite;
    unsigned boardSize;

    float r1, rStone;

    std::default_random_engine generator;
    std::normal_distribution<float> dist;

    bool invalidated;
public:
	const static float mEmpty;
	Stones stones;
	int order;
	volatile unsigned long positionNumber;
	bool showTerritory, showTerritoryAuto;
	int lastPlayed_i, lastPlayed_j;
};

#endif // BOARD_H
