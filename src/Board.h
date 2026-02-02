#ifndef BOARD_H
#define BOARD_H

#include <spdlog/spdlog.h>
#include <array>
#include <sstream>
#include <vector>
#include <iostream>
#include <random>
#include <atomic>
#include <glm/glm.hpp>
#include "Metrics.h"

class Color {
public:

    enum Value { EMPTY = 0, WHITE = 1, BLACK = 2 };

    Color(): col(EMPTY) {}

    Color(Value col): col(col) {}

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

    bool operator== (const Color::Value b) const {
        return col == b;
    }

    bool operator!= (const Color::Value b) const {
        return col != b;
    }

    friend std::ostream& operator<< (std::ostream& stream, const Color& color);

    [[nodiscard]] std::string toString() const;

private:
    Value col;  // Value type - no volatile needed (copied by value)

};

class Position {
    int c;
    int r;

public:
    float x;
    float y;

    Position(): c(0), r(0), x(0.0), y(0.0) { }

    Position(int col, int row): c(col), r(row), x(0), y(0) { }

    explicit operator bool() const { return c >= 0 && r >= 0; }
    operator int() const = delete;
    bool operator ==(const Position& b) const { return c == b.c && r == b.r;}
    bool operator !=(const Position& b) const { return !(*this == b);}

    [[nodiscard]] int col() const { return c; }
    [[nodiscard]] int row() const { return r; }

    [[nodiscard]] std::string toSgf(int boardSize) const {
        std::ostringstream ss;
        ss << static_cast<char>('a' + c) << static_cast<char>('a' + boardSize - r - 1);
        return ss.str();
    }

    static Position fromSgf(const std::string& sgfPoint, int boardSize) {
        if (sgfPoint.length() != 2) {
            return Position(-1, -1);
        }
        int col = sgfPoint[0] - 'a';
        int row = boardSize - (sgfPoint[1] - 'a') - 1;
        return Position(col, row);
    }

    friend std::istream& operator>> (std::istream&, Position&);

    friend std::ostream& operator<< (std::ostream&, const Position&);
};

class Move {

public:

    enum Special { INVALID, INTERRUPT, NORMAL, PASS, RESIGN, KIBITZED};

    Move(): spec(INVALID), col(Color::EMPTY) {}

    Move(Special spec, const Color& color): spec(spec), col(color) {}

    Move(const Position& pos, const Color& col)
        : spec(NORMAL), pos(pos), col(col)
    { }

    explicit operator bool() const { return spec != INVALID && spec != INTERRUPT && spec != KIBITZED; }
    explicit operator Position() const { return pos; }
    explicit operator Color() const { return col; }

    bool operator== (Special b) const { return spec == b; }
    bool operator!= (Special b) const { return spec != b; }

    bool operator== (const Color::Value & b) const { return col == b; }

    bool operator== (const Position& p) const { return pos == p; }

    static Move parseGtp(const std::string& s, const Color& col) {
        std::istringstream ss(s); Move m(Move::INVALID, col); ss >> m; return m;
    }

    friend std::istream& operator>> (std::istream& stream, Move& );

    friend std::ostream& operator<< (std::ostream& stream, const Move& );

    [[nodiscard]] std::string toString() const;

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
    static constexpr int MAX_BOARD = 19;
    static constexpr int MIN_BOARD = 9;
    static constexpr int BOARD_SIZE = MAX_BOARD * MAX_BOARD;
    static constexpr int DEFAULT_SIZE = 19;

    enum Change { NO_CHANGE = 0, STONE_PLACED = 1, STONE_REMOVED = 2, TERRITORY_CHANGED = 4, SIZE_CHANGED = 8};

public:
    [[nodiscard]] const float* getStones() const;

    [[nodiscard]] int getSize() const { return boardSize;}

    long getSizeOf() const { return 4 * sizeof(float) * points.size(); }

    void copyStateFrom(const Board&);

    void setStoneRadius(float r) {
        rStone = r;
        r1 = 0.5f * (1.0f - r);
        dist = std::normal_distribution<float>(0.0f, 3.0f * r1);
        r1 *= 0.98f;
    }

    double fixStone(int i, int j, int i0, int j0, size_t rep = 0);

    [[nodiscard]] const std::array<Point, BOARD_SIZE>& get() const {
        return points;
    }
    std::array<Point, BOARD_SIZE>& get() {
        return points;
    }

    Point operator[](const Position& pos) const {
        return points[ord(pos)];
    }

    Point& operator[](const Position& pos) {
        return points[ord(pos)];
    }

    [[nodiscard]] int capturedCount(const Color::Value& whose) const;
    [[nodiscard]] int stonesOnBoard(const Color::Value& whose) const;

    // Returns bounding rect of all stones (grid coords), with margin clamped to board.
    // Returns false if no stones on board.
    bool stoneBounds(Position& minPos, Position& maxPos, int margin = 1) const;

    void clear(int boardSize = DEFAULT_SIZE);
    void clearTerritory() {
        for(auto & point : points) {
            point.influence = Color::EMPTY;
        }
    };

    // Calculate territory using flood-fill from dead stones
    // This is GTP-standard compliant (only needs 'dead' status, not gnugo extensions)
    void calculateTerritoryFromDeadStones(const std::vector<Position>& deadStones);

    // Go rule implementation - apply move with capture processing
    // Returns number of opponent stones captured (0 if none)
    // Updates internal koPosition if exactly 1 stone captured
    // Does NOT validate the move - caller should check isValidMove() beforehand
    int applyMoveWithCaptures(const Move& move);

    // Check if a move would be valid (not ko, not suicide unless captures)
    // Uses internal koPosition for ko rule checking
    bool isValidMove(const Position& pos, const Color& color) const;

    // Build board state by replaying moves (stops on illegal move, returns count applied)
    int replayMoves(const std::vector<Move>& moves);

    // Sync glStones visual array from points logical state (call after replayMoves)
    void syncVisualState();

    // Get current ko position (for display or debugging)
    [[nodiscard]] Position getKoPosition() const { return koPosition; }

    explicit Board(int size = DEFAULT_SIZE);

private:
    // Find all stones connected to the given position (same color flood-fill)
    std::vector<Position> findGroup(const Position& pos) const;

    // Count liberties (unique empty adjacent positions) for a group
    int countLiberties(const std::vector<Position>& group) const;

    // Remove a group of stones from the board, returns count removed
    int removeGroup(const std::vector<Position>& group);

public:

    bool parseGtp(const std::vector<std::string>& lines);

    bool parseGtpInfluence(const std::vector<std::string>& lines);

    void invalidate();

    friend std::ostream& operator<< (std::ostream&, const Board&);

    int updateStone(const Position& p, const Color& col);
    int updateArea(const Position& p, const Color& col);
    int sizeChanged(int newSize);
    int updateStones(const Board& previous);

    bool toggleTerritory();
    bool toggleTerritoryAuto(bool);

    int placeCursor(const Position& p, const Color& col);
    double placeFuzzy(const Position& p, bool noFix = false);

    bool collides(int i, int j, int i0, int j0) const;
    void removeOverlay(const Position& p);
    void setOverlay(const Position& p, const std::string& text, const Color& c);
    void setBoardOverlay(const Position& p, const std::string& text);  // Board-level overlay (layer 0)
    void removeBoardOverlay(const Position& p);
    void setRandomStoneRotation() {
    	randomStoneRotation = 3.1415f * uDist(generator);
    }

    double getRandomStoneRotation() {
        return 3.1415f * uDist(generator);
    }

    void updateMetrics(const Metrics &m) {
        squareYtoXRatio = m.squareSizeY / m.squareSizeX;
        setStoneRadius(2.0f * m.stoneRadius / m.squareSizeX);
    }

private:

    [[nodiscard]] static inline int ord(const Position& p) {
        return p.col() * MAX_BOARD + p.row();
    }

private:
    const static float mBlackArea;
    const static float mWhiteArea;
    const static float mBlack;
    const static float mWhite;
    const static float mDeltaCaptured;
    const static float safeDist;

    std::array<float, 4 * BOARD_SIZE> glStones{};
    std::array<Point, BOARD_SIZE> points;

    int capturedBlack;
    int capturedWhite;
    int boardSize;
    Position koPosition{-1, -1};  // Ko position (invalid if no ko)

    float r1, rStone;

    std::default_random_engine generator;
    std::normal_distribution<float> dist;
    std::uniform_real_distribution<float> uDist;

    bool invalidated;

    const static float mEmpty;
    const static float mAnnotation;  // Empty point with hidden grid (for overlays)
    double randomStoneRotation{};

    double squareYtoXRatio;

public:
    std::atomic<unsigned long> positionNumber{0};
    bool showTerritory;
    bool showTerritoryAuto;
    bool territoryReady;
private:
    Position cursor;
    std::atomic<long> moveNumber{0};
public:
    double collision;
    float score;
};

#endif // BOARD_H
