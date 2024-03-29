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
        return b.col == EMPTY ?  b.col : b.col == BLACK ? WHITE : BLACK;
    }

    explicit operator int() const {
        return col == EMPTY ? -1 : (col == BLACK ? 0 : 1);
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
    operator int() const = delete;
    bool operator ==(const Position& b) const { return c == b.c && r == b.r;}
    bool operator !=(const Position& b) const { return !(*this == b);}

    int col() const { return c; }
    int row() const { return r; }

    std::string toSgf(int boardSize) const {
        std::ostringstream ss;
        ss << static_cast<char>('a' + c) << static_cast<char>('a' + boardSize - r - 1);
        return ss.str();
    }

    friend std::istream& operator>> (std::istream&, Position&);

    friend std::ostream& operator<< (std::ostream&, const Position&);
};

class Move {

public:

    enum Special { INVALID, INTERRUPT, NORMAL, PASS, UNDO, RESIGN, KIBITZED};

    Move(): spec(INVALID), col(Color::EMPTY) {}

    Move(Special spec, const Color& color): spec(spec), col(color) {}

    Move(const Position& pos, const Color& col)
        : spec(NORMAL), pos(pos), col(col)
    { }

    operator bool() const { return spec != INVALID && spec != INTERRUPT && spec != KIBITZED; }
    operator Position() const { return pos; }
    operator Color() const { return col; }

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
public:
    Position pos;
    Color col;

};


struct Overlay {
    std::string text;
	unsigned layer;
};

//stonePlace
class Point {
public:
    Color stone;
    Color influence;
    Overlay overlay;
    float x;
    float y;
};

class Board
{
public:
    static const int MAXBOARD = 19;
    static const int MINBOARD = 9;
    static const int BOARDSIZE = MAXBOARD * MAXBOARD;
    static const int DEFAULTSIZE = 19;

    enum Change { NO_CHANGE = 0, STONE_PLACED = 1, STONE_REMOVED = 2, TERRITORY_CHANGED = 4, SIZE_CHANGED = 8};

public:
    const float* getStones() const;
    float* getStones();

    int getSize() const { return boardSize;}

    std::size_t getSizeOf() const { return 4*sizeof(float)*points.size(); }

    void copyStateFrom(const Board&);
    void copyTerritoryFrom(const Board&);

    void setStoneRadius(float r) {
        rStone = r;
        r1 = 0.5f * (1.0f - r);
        dist = std::normal_distribution<float>(0.0f, 3.0f * r1);
        r1 *= 0.98f;
    }

    double fixStone(int i, int j, int i0, int j0, size_t rep = 0);

    const
    std::array<Point, BOARDSIZE>& get() const {
        return points;
    }
    std::array<Point, BOARDSIZE>& get() {
        return points;
    }

    const
    Point operator[](const Position& pos) const {
        return points[ord(pos)];
    }

    Point& operator[](const Position& pos) {
        return points[ord(pos)];
    }

    int capturedCount(const Color& whose) const;

    void clear(int boardSize = DEFAULTSIZE);
    void clearTerritory(int boardSize = DEFAULTSIZE) {
        for(auto pit = points.begin(); pit != points.end(); ++pit) {
            pit->influence = Color::EMPTY;
        }
    };

    Board(int size = DEFAULTSIZE);

    bool parseGtp(const std::vector<std::string>& lines);

    bool parseGtpInfluence(const std::vector<std::string>& lines);

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
    void removeOverlay(const Position& p);
    void setOverlay(const Position& p, const std::string& text, const Color& c);
    void setRandomStoneRotation() {
    	randomStoneRotation = 3.1415f * udist(generator);
    }

    double getRandomStoneRotation() {
        return 3.1415f * udist(generator);
    }
private:

    inline int ord(const Position& p) const {
        return p.col() * MAXBOARD + p.row();
    }

private:
    const static float mBlackArea;
    const static float mWhiteArea;
    const static float mBlack;
    const static float mWhite;
    const static float mDeltaCaptured;
    const static float safedist;

    std::array<float, 4*BOARDSIZE> glstones;
    std::array<Point, BOARDSIZE> points;

    int capturedBlack;
    int capturedWhite;
    int boardSize;

    float r1, rStone;

    std::default_random_engine generator;
    std::normal_distribution<float> dist;
    std::uniform_real_distribution<float> udist;

    bool invalidated;

    const static float mEmpty;
    double randomStoneRotation;

public:
    volatile long positionNumber;
    bool showTerritory;
    bool showTerritoryAuto;
    bool territoryReady;
private:
    int lastPlayed_i, lastPlayed_j;
    Position cursor, lastCursor;
    volatile long moveNumber;
public:
    double collision;
};

#endif // BOARD_H
