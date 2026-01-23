#include "Board.h"
#include <sstream>
#include <algorithm>

const float Board::mBlackArea = 2.0f;
const float Board::mWhiteArea = 3.0f;
const float Board::mEmpty = 0.0f;
const float Board::mAnnotation = 1.0f;  // Empty point with hidden grid (for overlays)
const float Board::mBlack = 5.0f;
const float Board::mWhite = 6.0f;
const float Board::mDeltaCaptured = 0.5f;
const float Board::safeDist = 1.01f;

Board::Board(int size) : capturedBlack(0), capturedWhite(0), boardSize(size), r1(.0f), rStone(.0f),
    dist(0.0f, 0.05f), invalidated(false), squareYtoXRatio(1.0), showTerritory(false), showTerritoryAuto(false),
    territoryReady(false), cursor({0, 0}),
    collision(0.0), generator(std::random_device()())
{
    clear(size);
    positionNumber = generator();
}

const float* Board::getStones() const { return &glStones[0]; }

std::ostream& operator<< (std::ostream& stream, const Color& color) {
    if (color == Color::BLACK)
        stream << "B"; //"X";
    else if (color == Color::WHITE)
        stream << "W"; //"O";
    else
        stream << "E"; //".";
    return stream;
}

std::ostream& operator<< (std::ostream& stream, const Board& board) {
    for (int i = board.boardSize; i != 0; --i) {
        for(int j = 0; j < board.boardSize; ++j)
            stream << board[Position(j, i)].stone << " ";
        stream << std::endl;
    }
    return stream;
}

std::istream& operator>> (std::istream& stream, Position& pos) {
    unsigned char c = '\x00';
    int d = -1;
    stream >> c >> d;
    int i, j;
    if(d < 0)
        return stream;
    if (c >= 'I') j = 7 + c - 'I'; else j = c - 'A';
    i = d - 1;
    pos = Position(j, i);
    return stream;
}

std::ostream& operator<< (std::ostream& stream, const Position& pos) {
    char c = pos.c < 8 ? (char)('A' + pos.c) : (char)('I' + pos.c - 7);
    unsigned d = pos.r + 1u;
    stream << c << d;
    return stream;
}

std::ostream& operator<< (std::ostream& stream, const Move& move) {
    stream << (move.col == Color::BLACK ? "B " : "W ");
    if(move.spec == Move::PASS) {
        stream << "PASS";
    }
    else if (move.spec == Move::NORMAL){
        stream << move.pos;
    }
    return stream;
}

std::string Move::toString() const {
    std::ostringstream ssout;
    ssout << *this;
    return ssout.str();
}

std::string Color::toString() const {
    std::ostringstream ssout;
    ssout << *this;
    return ssout.str();
}

std::istream& operator>> (std::istream& stream, Move& move) {
    char c = 0;
    stream >> c;
    if(c == '=') {
        std::string s;
        stream >> s;
        if (s.substr(0, 4) == "PASS" || s.substr(0, 4) == "pass") {
            move.spec = Move::PASS;
        }
        else if (s.substr(0, 6) == "resign" || s.substr(0, 6) == "RESIGN") {
            move.spec = Move::RESIGN;
        }
        else {
            std::istringstream ss(s);
            ss >> move.pos;
            move.spec = move.pos ? Move::NORMAL : Move::INVALID;
        }
    }
    else
        move.spec = Move::INVALID;
    return stream;

}

bool Board::collides(int i, int j, int i0, int j0) {
    int idx = ((boardSize  * i + j) << 2) + 2;
    int idx0 = ((boardSize  * i0 + j0) << 2) + 2;
    float x0 = glStones[idx0 - 2];
    float y0 = glStones[idx0 - 1];
    float x = glStones[idx - 2];
    float y = glStones[idx - 1];
    glm::vec2 v(x0-x,(y0-y) * squareYtoXRatio);
    return glm::length(v) <= safeDist * rStone;
}

double Board::fixStone(int i, int j, int i0, int j0, size_t rep) {
    const size_t MAX_FIX = 100;

    spdlog::debug("fixStone ({},{})/({},{})", i, j, i0, j0);
    int idx = ((boardSize  * i + j) << 2) + 2;
    int idx0 = ((boardSize  * i0 + j0) << 2) + 2;
    float mValue = glStones[idx0];
    double ret = 0.0;
    if (mValue != mEmpty &&  mValue != mBlackArea && mValue != mWhiteArea) {
        bool fixNeeded = collides(i, j, i0, j0);
        if(fixNeeded) {
            float x0 = glStones[idx0 - 2];
            float y0 = glStones[idx0 - 1];
            float x = glStones[idx - 2];
            float y = glStones[idx - 1];
            Position p(i0, j0);
            glm::vec2 v(x0-x,y0-y);
            v = glm::vec2(x, y) + glm::vec2(1.0, squareYtoXRatio) * safeDist * rStone * glm::normalize(v);
            p.x = v.x;
            p.y = v.y;
            spdlog::debug("2nd IF [{},{}]->[{},{}]", x0,y0, p.x,p.y);
            x = glStones[idx0 - 2];
            y = glStones[idx0 - 1];
            glStones[idx0 - 2] = p.x;
            glStones[idx0 - 1] = p.y;
            Point& pt = (*this)[Position(j0, i0)];
            pt.x = p.x;
            pt.y = p.y;
            (*this)[Position(j0, i0)].y = p.y;
            ret = std::sqrt(glm::distance(glm::vec2(p.x, p.y), glm::vec2(x, y))/(float)boardSize);
            if (i0 + 1 < boardSize &&  collides(i0, j0, i0 + 1, j0) && rep < MAX_FIX)
                ret = std::max(ret, fixStone(i0, j0, i0 + 1, j0, rep + 1));
            if (i0 - 1 >= 0 &&  collides(i0, j0, i0 - 1, j0) && rep < MAX_FIX)
                ret = std::max(ret, fixStone(i0, j0, i0 - 1, j0, rep + 1));
            if (j0 + 1 < boardSize &&  collides(i0, j0, i0, j0 + 1) && rep < MAX_FIX)
                ret = std::max(ret, fixStone(i0, j0, i0, j0 + 1, rep + 1));
            if (j0 - 1 >= 0 &&  collides(i0, j0, i0, j0 - 1) && rep < MAX_FIX)
                ret = std::max(ret, fixStone(i0, j0, i0, j0 - 1, rep + 1));
        }
        return ret;
    }
    return ret;
}

double Board::placeFuzzy(const Position& p, bool noFix){
    int j = p.col();
    int i = p.row();
    const float halfN = 0.5f * (float)boardSize - 0.5f;
    float x = p.x;
    float y = p.y;
    double ret = 0.0;
    if(p.x == 0 && p.y == 0) {
        x = (float)j - halfN + std::max(-3.0f * r1, std::min(3.0f * r1, dist(generator)));
        y = (float)i - halfN + std::max(-3.0f * r1, std::min(3.0f * r1, dist(generator)));
    }
    else {
        x = x - halfN;
        y = y - halfN;
    }

    unsigned idx = ((boardSize  * i + j) << 2u) + 2u;

    glStones[idx - 2] = x;
    glStones[idx - 1] = y;

    (*this)[p].x = x;
    (*this)[p].y = y;

    //random rotation for the first time
    glStones[idx + 1] = (float)randomStoneRotation;
    if(noFix == false) {
        if (i + 1 < boardSize)
            ret = std::max(ret, fixStone(i, j, i + 1, j));
        if(i -  1 >= 0)
            ret = std::max(ret, fixStone(i, j, i -  1, j));
        if (j + 1 < boardSize)
            ret = std::max(ret, fixStone(i, j, i, j + 1));
        if(j -  1 >= 0)
            ret = std::max(ret, fixStone(i, j, i, j -  1));
    }
    return ret;
}

void Board::removeOverlay(const Position& p) {
    auto& overlay = (*this)[p].overlay;
    overlay.text = std::string("");
    overlay.layer = -1u;
}

void Board::setOverlay(const Position& p, const std::string& text, const Color& c) {
    auto& overlay = (*this)[p].overlay;
    overlay.text = text;
    overlay.layer = c == Color::BLACK ? 1 : 2;
}

void Board::setBoardOverlay(const Position& p, const std::string& text) {
    // Board-level overlay for empty points (e.g., ko capture position)
    // Uses layer 0 and sets material to mAnnotation for grid hiding
    auto& overlay = (*this)[p].overlay;
    overlay.text = text;
    overlay.layer = 0;  // Board level (rendered before stones)

    // Set material to annotation to render board patch that hides grid
    int i = p.row();
    int j = p.col();
    unsigned idx = ((boardSize * i + j) << 2u) + 2u;

    // Only set annotation material if point is empty (no stone)
    if ((*this)[p].stone == Color::EMPTY) {
        glStones[idx + 0] = mAnnotation;
        // Center the position at the grid intersection for the patch
        glStones[idx - 2] = static_cast<float>(j) - 0.5f * static_cast<float>(boardSize) + 0.5f;
        glStones[idx - 1] = static_cast<float>(i) - 0.5f * static_cast<float>(boardSize) + 0.5f;
    }
}

void Board::removeBoardOverlay(const Position& p) {
    auto& overlay = (*this)[p].overlay;

    // Only clear if this is a board-level overlay (layer 0)
    // Don't clear stone-level overlays (layers 1-2)
    if (overlay.layer != 0) {
        return;
    }

    overlay.text = std::string("");
    overlay.layer = -1u;

    // Reset material to empty if it was annotation
    int i = p.row();
    int j = p.col();
    unsigned idx = ((boardSize * i + j) << 2u) + 2u;

    if (glStones[idx + 0] == mAnnotation) {
        glStones[idx + 0] = mEmpty;
    }
}

int Board::updateStone(const Position& p, const Color& c) {
    float mValue = mEmpty;
    int ret;
    int i = p.row();
    int j = p.col();
    unsigned idx = ((boardSize  * i + j) << 2u) + 2u;
    if(c == Color::WHITE)
        mValue = mWhite;
    else if(c == Color::BLACK)
        mValue = mBlack;
    if(c == Color::WHITE || c == Color::BLACK) {
        double collides = placeFuzzy(p);
        collision = std::max(collision, collides);
        ret = STONE_PLACED;
    } else {
        ret = STONE_REMOVED;
        removeOverlay(p);
    }
    //TODO refactor arrays and indexing to simplify
    glStones[idx + 0] = mValue;
    (*this)[p].stone = c;
    return ret;
}


int Board::updateArea(const Position& p, const Color& col) {
    Color stone = (*this)[p].stone;
    int i = p.row();
    int j = p.col();
    unsigned idx = ((boardSize  * i + j) << 2u) + 2u;
	float mValue = mEmpty;
    if(stone != Color::EMPTY) {
        mValue = stone == Color::BLACK ? mBlack : mWhite;
        if (col != stone && col != Color::EMPTY && showTerritory) {
            mValue += mDeltaCaptured;
        }
    }
    else if(showTerritory && col != Color::EMPTY) {
        mValue = col == Color::BLACK ? mBlackArea : mWhiteArea;
        placeFuzzy(p, true);
    }
    else if(col == Color::EMPTY || !showTerritory) {
        mValue = mEmpty;
    }
    if ((*this)[p].influence != col) {
        glStones[idx + 0] = mValue;
        (*this)[p].influence = col;
        return TERRITORY_CHANGED;
    }
    return NO_CHANGE;
}

int Board::sizeChanged(int newSize) {
    boardSize = newSize;
    return SIZE_CHANGED;
}

int Board::updateStones(const Board& board) {
    int newSize = board.getSize();
    int changed = 0;

    this->showTerritory = board.showTerritory;
	this->showTerritoryAuto = board.showTerritoryAuto;

    if(newSize != boardSize) {
        sizeChanged(newSize);
        this->clearTerritory();
        this->clear(newSize);
        changed = 1;
        return changed;
    }
    for(int col = 0; col < MAX_BOARD; ++col) {
        for(int row = 0; row < MAX_BOARD; ++row) {
            Position pos(row, col);

            const Point& np = board[pos];
            Color newStone(np.stone);
            Color newArea(np.influence);

            if(!this->showTerritory) {
                newArea = Color();
            }

            Point& p = (*this)[pos];
            if(newStone != p.stone) {
                spdlog::trace("stone changed [{},{}]", p.x, p.y);
                changed |= updateStone(pos, newStone);
            }
            if(newArea != p.influence) {
                spdlog::trace("area changed [{},{}]", p.x, p.y);
                changed |= updateArea(pos, newArea);
            }
        }
    }
    return changed;
}
//}

int Board::capturedCount(const Color::Value& whose) const {
    return whose == Color::WHITE ? capturedWhite : capturedBlack;
}

void Board::clear(int boardsize) {

    this->boardSize = boardsize;

    glStones.fill(mEmpty);
    points.fill({
        Color(),
        Color(),
        {"", -1u},
        0, 0
    });

    capturedBlack = 0;
    capturedWhite = 0;
    positionNumber += 1;
}

void Board::copyStateFrom(const Board& b) {
    glStones = b.glStones;
    points = b.points;
    boardSize = b.boardSize;
    capturedBlack = b.capturedBlack;
    capturedWhite = b.capturedWhite;
    territoryReady = b.territoryReady;
    moveNumber.store(b.moveNumber.load());
    score = b.score;
}

bool Board::parseGtp(const std::vector<std::string>& lines) {
    bool success = false;
    try {
        if(lines.front().at(0) == '=') {
            std::istringstream ssin(lines.at(2));

            int size = 0;
            ssin >> size;
            spdlog::debug("Size: {}", size);
            if(size >= MIN_BOARD && size <= MAX_BOARD) {
                int bcaptured = 0, wcaptured = 0;
                spdlog::debug(lines.at(1));
			    bool white = true;
                for(int i = 2; i < 2 + size; ++i) {
                    spdlog::debug(lines[i]);
                    for(int j = 3; j < 3 + (size << 1); j += 2) {
                        char c = lines[i][j];
                        Position p((j - 3) >> 1, size - i + 1);
                        Color color;
                        if(c == 'X') {
                            color = Color::BLACK;
                        }
                        else if(c == 'O') {
                            color = Color::WHITE;
                        }
                        (*this)[p].stone = color;
                    }
                    if(lines[i].back() == 's') {
                        std::string s;
                        std::istringstream ssline(lines[i]);
                        while((ssline >> s) && s != "captured") {
                          ssline >> s;
                        }
                        int count = 0;
                        ssline >> count;
						if (white) {
							bcaptured = count;
							white = false;
						}
						else {
							wcaptured = count;
						}
                    }
                }
                boardSize = size;
                success = true;
                capturedBlack = bcaptured;
                capturedWhite = wcaptured;
                spdlog::debug("Captured: {}, {}", capturedBlack, capturedWhite);
                spdlog::debug(lines[2 + size]);
                moveNumber += 1;
            }
        }
    }
    catch (const std::out_of_range& oor) {
        spdlog::error("Gtp parse error: {}", oor.what());
    }
    return success;

}

bool Board::parseGtpInfluence(const std::vector<std::string>& lines) {
    bool success = false;
    try {
        for (int i = 0; i < boardSize; ++i) {
            spdlog::debug("row = {}/{}", i, boardSize);
            std::istringstream ssin(lines.at(i));
            if (i == 0) {
                char c;
                ssin >> c;
                success = (c == '=');
                if (!success) return false;
            }
            for (int j = 0; j < boardSize; ++j) {
                float val = 0.0;
                ssin >> val;
                Color c;
                if (val > 2)
                    c = Color::WHITE;
                else if (val < -2)
                    c = Color::BLACK;
                (*this)[Position(j, boardSize - i - 1)].influence = c;
            }
        }
        success = true;
    }
    catch (const std::out_of_range& oor) {
        spdlog::error("Gtp parse error: {}", oor.what());
    }
    return success;

}

void Board::invalidate() {
    //++positionNumber;
    std::unique_lock<std::mutex> mutex;
    invalidated = true;
}

bool Board::toggleTerritoryAuto(bool jak) {
	if (jak == false && showTerritoryAuto && showTerritory) {
		showTerritory = false;
		showTerritoryAuto = false;
		positionNumber += 1;
	}
	else if (jak == true && !showTerritory){
		showTerritory = true;
		showTerritoryAuto = true;
		positionNumber += 1;
	}
	return showTerritory;
}

bool Board::toggleTerritory() {
	showTerritory = !showTerritory;
	showTerritoryAuto = false;
	positionNumber += 1;
	return showTerritory;
}

int Board::placeCursor(const Position& coord, const Color& col) {

    glm::vec2 v(
        coord.x - (float)coord.col() - 0.5f,
        coord.y - (float)coord.row() - 0.5f
    );

    glm::vec2 add(6.0f * r1 * v);

    Position pos(coord);
    pos.x = (float)coord.col() + add.x;
    pos.y = (float)coord.row() + add.y;

    return updateStone(pos, col);
}

void Board::calculateTerritoryFromDeadStones(const std::vector<Position>& deadStones) {
    clearTerritory();

    // Create a working copy of stone colors, treating dead stones as empty
    std::array<Color::Value, BOARD_SIZE> stoneState;
    for (int col = 0; col < boardSize; ++col) {
        for (int row = 0; row < boardSize; ++row) {
            Position pos(col, row);
            stoneState[ord(pos)] = points[ord(pos)].stone == Color::EMPTY
                ? Color::EMPTY
                : points[ord(pos)].stone == Color::BLACK ? Color::BLACK : Color::WHITE;
        }
    }

    // Mark dead stones as empty (they'll become territory)
    for (const auto& pos : deadStones) {
        if (pos.col() >= 0 && pos.col() < boardSize &&
            pos.row() >= 0 && pos.row() < boardSize) {
            // Mark dead stone's position as territory for opponent
            Color::Value deadColor = stoneState[ord(pos)];
            if (deadColor != Color::EMPTY) {
                points[ord(pos)].influence = Color::other(Color(deadColor));
            }
            stoneState[ord(pos)] = Color::EMPTY;
        }
    }

    // Track which empty points have been visited
    std::array<bool, BOARD_SIZE> visited;
    visited.fill(false);

    // Flood-fill to find territory regions
    for (int col = 0; col < boardSize; ++col) {
        for (int row = 0; row < boardSize; ++row) {
            Position startPos(col, row);
            int startIdx = ord(startPos);

            // Skip if not empty or already visited
            if (stoneState[startIdx] != Color::EMPTY || visited[startIdx]) {
                continue;
            }

            // Flood-fill this empty region
            std::vector<Position> region;
            std::vector<Position> stack;
            bool touchesBlack = false;
            bool touchesWhite = false;

            stack.push_back(startPos);

            while (!stack.empty()) {
                Position current = stack.back();
                stack.pop_back();

                int idx = ord(current);
                if (visited[idx]) continue;
                visited[idx] = true;

                if (stoneState[idx] == Color::EMPTY) {
                    region.push_back(current);

                    // Check all 4 neighbors
                    const int dc[] = {-1, 1, 0, 0};
                    const int dr[] = {0, 0, -1, 1};

                    for (int d = 0; d < 4; ++d) {
                        int nc = current.col() + dc[d];
                        int nr = current.row() + dr[d];

                        if (nc >= 0 && nc < boardSize && nr >= 0 && nr < boardSize) {
                            Position neighbor(nc, nr);
                            int nIdx = ord(neighbor);

                            if (stoneState[nIdx] == Color::EMPTY && !visited[nIdx]) {
                                stack.push_back(neighbor);
                            } else if (stoneState[nIdx] == Color::BLACK) {
                                touchesBlack = true;
                            } else if (stoneState[nIdx] == Color::WHITE) {
                                touchesWhite = true;
                            }
                        }
                    }
                }
            }

            // Determine territory ownership
            Color territoryColor;
            if (touchesBlack && !touchesWhite) {
                territoryColor = Color::BLACK;
            } else if (touchesWhite && !touchesBlack) {
                territoryColor = Color::WHITE;
            } else {
                // Dame (neutral) or touches both - no territory
                territoryColor = Color::EMPTY;
            }

            // Mark all points in region as territory
            for (const auto& pos : region) {
                points[ord(pos)].influence = territoryColor;
            }
        }
    }

    spdlog::debug("Territory calculated from {} dead stones", deadStones.size());
}
