#ifndef BOARD_H
#define BOARD_H

#include <spdlog/spdlog.h>
#include <array>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <random>
#include <mutex>
#include <glm/glm.hpp>

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

    std::string toString() const;

private:
    volatile Value col;

};

class Position {
    int c;
    int r;

    void fromGTP(char col, char row) {
        c = col >= 'I' ? 7 + c - 'I' : c - 'A';
        r = row;
    }

public:
    float x;
    float y;
    Position(): c(0), r(0), x(0.0), y(0.0) { }

    Position(int col, int row): c(col), r(row), x(0), y(0) { }

    operator bool() const { return c >= 0 && r >= 0; }
    //bool operator ==(const Position& b) const { return c == b.c && r == b.r;}
    //bool operator !=(const Position& b) const { return !(*this == b);}

    int col() const { return c; }
    int row() const { return r; }

    friend std::istream& operator>> (std::istream&, Position&);

    friend std::ostream& operator<< (std::ostream&, const Position&);
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
        std::istringstream ss(s); Move m(Move::INVALID, col); ss >> m; return m;
    }

    friend std::istream& operator>> (std::istream& stream, Move& );

    friend std::ostream& operator<< (std::ostream& stream, const Move& );

    std::string toString() const;
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
    static const int MAXBOARD = 19;
    static const int MINBOARD = 9;
    static const int BOARDSIZE = (MAXBOARD + 2) * (MAXBOARD + 1) + 1;
    static const int DEFAULTSIZE = 19;
    typedef std::array<float, Board::MAXBOARD*Board::MAXBOARD << 2> Stones;
    typedef std::array<CoordText, Board::MAXBOARD*Board::MAXBOARD> Overlay;
    enum Change { NO_CHANGE = 0, STONE_PLACED = 1, STONE_REMOVED = 2, TERRITORY_CHANGED = 4, SIZE_CHANGED = 8};
private:
    typedef std::array<Color, BOARDSIZE> board_array;
public:
    typedef board_array::iterator iterator;
    typedef board_array::const_iterator const_iterator;

    const float* getStones() const;
    float* getStones();

    int getSize() const { return boardSize;}

    std::size_t getSizeOf() const { return 4*sizeof(float)*stones.size(); }

    void copyStateFrom(const Board&);
    void copyTerritoryFrom(const Board&);

    void setStoneRadius(float r) { rStone = r; r1 = 0.5f*(1.0f - r); dist = std::normal_distribution<float>(0.0f, 3.0f*r1); r1 *= 0.98f; }

    double fixStone(int i, int j, int i0, int j0);

    const Overlay& getOverlay() const;

    Overlay& getOverlay();

    const Color  operator[](const Position& pos) const { return board[ord(pos)]; }

    Color& operator[](const Position& pos) {
        return board[ord(pos)];
    }
    const Color operator()(const Position& pos) const {
        return territory[ord(pos)];
    }
    Color& operator()(const Position& pos) {
        return territory[ord(pos)];
    }

    inline int row(int pos) const { return (pos) / (MAXBOARD + 1u) - 1u; }
    inline int col(int pos) const { return (pos) % (MAXBOARD + 1u) - 1u; }
    inline int ord(const Position& p) const { return ord(p.col(), p.row()); }
    inline int ord(int i, int j) const { return (MAXBOARD + 2) + (i) * (MAXBOARD + 1) + (j); }

    int capturedCount(const Color& whose) const;

    void clear(int boardSize = DEFAULTSIZE);
    void clearTerritory(int boardSize = DEFAULTSIZE);

    Board(int size = DEFAULTSIZE);

    bool parseGtp(const std::vector<std::string>& lines);

    bool parseGtpInfluence(const std::vector<std::string>& lines);

    const_iterator begin() const { return board.begin(); }
    const_iterator end() const { return board.end(); }
    iterator begin() { return board.begin(); }
    iterator end() { return board.end(); }
    const_iterator tbegin() const { return territory.begin(); }
    const_iterator tend() const { return territory.end(); }
    iterator tbegin() { return territory.begin(); }
    iterator tend() { return territory.end(); }

    void invalidate();

    friend std::ostream& operator<< (std::ostream&, const Board&);

    int stoneChanged(const Position& p, const Color& col);
    int areaChanged(const Position& p, const Color& col);
    int sizeChanged(int newSize);
    int updateStones(const Board& previous, bool showTerritory);

    bool isEmpty() const;
	bool toggleTerritory();
	bool toggleTerritoryAuto(bool);

	double placeCursor(const Position& p, const Color& col);
    double placeFuzzy(const Position& p, bool nofix = false);

    bool collides(int i, int j, int i0, int j0);

private:

    const static float mBlackArea;
    const static float mWhiteArea;
    const static float mBlack;
    const static float mWhite;
    const static float mDeltaCaptured;
    const static float safedist;

    std::shared_ptr<spdlog::logger> console;

    Overlay overlay;

    std::array<Color, BOARDSIZE> board;
    std::array<Color, BOARDSIZE> territory;

    int capturedBlack;
    int capturedWhite;
    int boardSize;

    float r1, rStone;

    std::default_random_engine generator;
    std::normal_distribution<float> dist;

    bool invalidated;
public:
	const static float mEmpty;
	Stones stones;
	int order;
	volatile long positionNumber;
	bool showTerritory, showTerritoryAuto;
	int lastPlayed_i, lastPlayed_j;
	Position cursor;
    volatile long moveNumber;
    double collision;
};

#endif // BOARD_H
