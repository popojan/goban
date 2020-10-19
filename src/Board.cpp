#include "Board.h"
#include <sstream>
#include <algorithm>

const float Board::mBlackArea = 2.0f;
const float Board::mWhiteArea = 3.0f;
const float Board::mEmpty = 0.0f;
const float Board::mBlack = 5.0f;
const float Board::mWhite = 6.0f;
const float Board::mDeltaCaptured = 0.5f;
const float Board::safedist = 1.01f;

Board::Board(int size) : capturedBlack(0), capturedWhite(0), boardSize(size), r1(.0f), rStone(.0f),
    dist(0.0f, 0.05f), invalidated(false), showTerritory(false), showTerritoryAuto(false),
    territoryReady(false), lastPlayed_i(-1), lastPlayed_j(-1), cursor({0, 0}), moveNumber(0),
    collision(0.0)
{
    clear(size);
    positionNumber = generator();;
}

bool Board::isEmpty() const {
    return std::all_of(points.begin(), points.end(), [](const Point& p){return p.stone == Color::EMPTY;});
}

const float* Board::getStones() const { return &glstones[0]; }
float* Board::getStones() { return &glstones[0]; }

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
    if (c >= 'I') j = 7u + c - 'I'; else j = c - 'A';
    i = d - 1;
    pos = Position(j, i);
    return stream;
}

std::ostream& operator<< (std::ostream& stream, const Position& pos) {
    char c = pos.c < 8 ? 'A' + (char)pos.c : 'I' + (char)pos.c - (char)7;
    unsigned d = pos.r + 1u;
    stream << c << d;
    return stream;
}

std::ostream& operator<< (std::ostream& stream, const Move& move) {
    stream << (move.col == Color::BLACK ? "B " : "W ");
    if(move.spec == Move::PASS) {
        stream << "PASS";
    }
    else if(move.spec == Move::UNDO) {
        stream << "UNDO";
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
        if (s.substr(0, 4).compare("PASS") == 0 || s.substr(0, 4).compare("pass") == 0) {
            move.spec = Move::PASS;
        }
        else if (s.substr(0, 6).compare("resign") == 0 || s.substr(0, 6).compare("RESIGN") == 0) {
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
    float x0 = glstones[idx0 - 2];
    float y0 = glstones[idx0 - 1];
    float x = glstones[idx - 2];
    float y = glstones[idx - 1];
    glm::vec2 v(x0-x,y0-y);
    return glm::length(v) <= safedist*rStone;
}

double Board::fixStone(int i, int j, int i0, int j0, size_t rep) {
    const double MAX_FIX = 100;

    spdlog::debug("fixStone ({},{})/({},{})", i, j, i0, j0);
    int idx = ((boardSize  * i + j) << 2) + 2;
    int idx0 = ((boardSize  * i0 + j0) << 2) + 2;
    float mValue = glstones[idx0];
    double ret = 0.0;
    if (mValue != mEmpty &&  mValue != mBlackArea && mValue != mWhiteArea) {
        spdlog::debug("1st IF");
        bool fixNeeded = collides(i, j, i0, j0);
        if(fixNeeded) {
            float x0 = glstones[idx0 - 2];
            float y0 = glstones[idx0 - 1];
            float x = glstones[idx - 2];
            float y = glstones[idx - 1];
            Position p(i0, j0);
            glm::vec2 v(x0-x,y0-y);
            v = glm::vec2(x, y) + 1.01f*safedist * rStone * glm::normalize(v);
            p.x = v.x;
            p.y = v.y;
            spdlog::debug("2nd IF [{},{}]->[{},{}]", x0,y0, p.x,p.y);
            unsigned idx = ((boardSize  * i0 + j0) << 2u) + 2u;
	    x = glstones[idx - 2];
	    y = glstones[idx - 1];
            glstones[idx0 - 2] = p.x;
            glstones[idx0 - 1] = p.y;
            Point& pt = (*this)[Position(j0, i0)];
            pt.x = p.x;
            pt.y = p.y;
            (*this)[Position(j0, i0)].y = p.y;
            ret = std::sqrt(glm::distance(glm::vec2(p.x, p.y), glm::vec2(x, y))/boardSize);
            if (i0 + 1 < boardSize &&  collides(i0, j0, i0 + 1, j0) && rep < MAX_FIX)
                ret = std::max(ret, fixStone(i0, j0, i0 + 1, j0, rep + 1));
            if (i0 - 1 >= 0 &&  collides(i0, j0, i0 - 1, j0) && rep < MAX_FIX)
                ret = std::max(ret, fixStone(i0, j0, i0 - 1, j0, rep + 1));
            if (j0 + 1 < boardSize &&  collides(i0, j0, i0, j0 + 1) && rep < MAX_FIX)
                ret = std::max(ret, fixStone(i0, j0, i0, j0 + 1, rep + 1));
            if (j0 - 1 >= 0 &&  collides(i0, j0, i0, j0 - 1) && rep < MAX_FIX)
                ret = std::max(ret, fixStone(i0, j0, i0, j0 - 1, rep + 1));
        } else if (fixNeeded) {
            //TODO dump stone positions
            spdlog::error("Cannot fix overlapping fuzzy-placed stones.");
        }
        return ret;
    }
    return ret;
}

double Board::placeFuzzy(const Position& p, bool nofix){
    int j = p.col();
    int i = p.row();
    const float halfN = 0.5f * boardSize - 0.5f;
    float x = p.x;
    float y = p.y;
    double ret = 0.0;
    if(p.x == 0 && p.y == 0) {
        x = j - halfN + std::max(-3.0f * r1, std::min(3.0f * r1, dist(generator)));
        y = i - halfN + std::max(-3.0f * r1, std::min(3.0f * r1, dist(generator)));
    }
    else {
        x = x - halfN;
        y = y - halfN;
    }

    unsigned idx = ((boardSize  * i + j) << 2u) + 2u;

    glstones[idx - 2] = x;
    glstones[idx - 1] = y;

    (*this)[p].x = x;
    (*this)[p].y = y;

    //random rotation for the first time
    glstones[idx + 1] = randomStoneRotation;
    if(nofix == false) {
        if (i + 1 < boardSize)
            ret = std::max(ret, fixStone(i, j, i + 1u, j));
        if(i -  1 >= 0)
            ret = std::max(ret, fixStone(i, j, i -  1u, j));
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

int Board::stoneChanged(const Position& p, const Color& c) {
    float mValue = mEmpty;
    int ret = NO_CHANGE;
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
    glstones[idx + 0] = mValue;
    (*this)[p].stone = c;
    return ret;
}


int Board::areaChanged(const Position& p, const Color& area) {
    Color stone = (*this)[p].stone;
    int i = p.row();
    int j = p.col();
    unsigned idx = ((boardSize  * i + j) << 2u) + 2u;
	float mValue = mEmpty;
    if(stone != Color::EMPTY) {
        mValue = stone == Color::BLACK ? mBlack : mWhite;
        if (area != stone && area != Color::EMPTY && showTerritory) {
            mValue += mDeltaCaptured;
        }
    }
    else if(showTerritory && area != Color::EMPTY) {
        mValue = area == Color::BLACK ? mBlackArea : mWhiteArea;
        placeFuzzy(p, true);
    }
    else if(area == Color::EMPTY || !showTerritory) {
        mValue = mEmpty;
    }
    glstones[idx + 0] = mValue;
    (*this)[p].influence = area;
    return TERRITORY_CHANGED;
}

int Board::sizeChanged(int newSize) {
    boardSize = newSize;
    return SIZE_CHANGED;
}

int Board::updateStones(const Board& board, bool showTerritory) {
    int newSize = board.getSize();
    int changed = 0;

    this->showTerritory = board.showTerritory;
	this->showTerritoryAuto = board.showTerritoryAuto;

    if(newSize != boardSize) {
        sizeChanged(newSize);
        this->clearTerritory(newSize);
        this->clear(newSize);
        changed = 1;
        return changed;
    }
    for(int col = 0; col < MAXBOARD; ++col) {
        for(int row = 0; row < MAXBOARD; ++row) {
            Position pos(row, col);

            const Point& np = board[pos];
            Color newStone(np.stone);
            Color newArea(np.influence);

            if(!this->showTerritory) {
                // || !board.territoryReady) ugly flashing
                newArea = Color::EMPTY;
            }

            Point& p = (*this)[pos];
            if(newStone != p.stone) {
                spdlog::trace("stone changed [{},{}]", p.x, p.y);
                changed |= stoneChanged(pos, newStone);
            }
            if(newArea != p.influence) {
                spdlog::trace("area changed [{},{}]", p.x, p.y);
                changed |= areaChanged(pos, newArea);
            }
        }
    }
    return changed;
}
    /*
    float halfN = 0.5f * boardSize - 0.5f;
    auto bbegin = board.begin(), bend = board.end();
	this->showTerritory = board.showTerritory;
	this->showTerritoryAuto = board.showTerritoryAuto;
	int lpi = lastPlayed_i;
	int lpj = lastPlayed_j;
	bool placedSomeStone = false;
    for (auto cit = bbegin, pit = board.tbegin(); cit != bend; ++cit, ++pit) {
        int pos = static_cast<unsigned>(cit - bbegin);
        int j = board.row(pos);
        int i = board.col(pos);
        if(i < 0 || j < 0 || i >= boardSize || j >= boardSize) continue;
        const Color& stone = *cit;
        const Color& area = *pit;
        float mValue = mEmpty;
		int oidx = boardSize  * i + j;
		if(stone != Color::EMPTY) {
            if (area == Color::EMPTY || area == stone || !showTerritory)
                mValue = stone == Color::BLACK ? mBlack : mWhite;
            else
                mValue = (area == Color::BLACK ? mWhite : mBlack) + mDeltaCaptured;
        }
        else if(showTerritory && area != Color::EMPTY) {
            mValue = area == Color::BLACK ? mBlackArea : mWhiteArea;
			float x = j - halfN;
			float y = i - halfN;
			overlay[oidx].x = x;
			overlay[oidx].y = y;
        }
        unsigned idx = ((boardSize  * i + j) << 2u) + 2u;
        bool placedOnCursor = cursor.row() == j && cursor.col() == i;
		if (stones[idx] != mValue) {
		    int change = 0;
			bool territoryToggle = std::abs(stones[idx] - mValue) == mDeltaCaptured;
			bool placeStone = !territoryToggle && mValue != mEmpty
			        &&  mValue != mBlackArea && mValue != mWhiteArea;
			if (placeStone) {
				placedSomeStone = true;
                change = 2;
                placeFuzzy(Position(j, i));
				//order += 1;
				overlay[oidx].x = stones[idx - 2];
				overlay[oidx].y = stones[idx - 1];
				lastPlayed_i = i;
				lastPlayed_j = j;
			}
			stones[idx + 0] = mValue;
			changed = std::max(1, change);
			bool isCaptured = mValue == mWhite + mDeltaCaptured || mValue == mBlack + mDeltaCaptured;
			bool isArea = mValue == mBlackArea || mValue == mWhiteArea;
			//bool isEmpty = mValue == mEmpty;
			bool isStone = mValue == mWhite || mValue == mBlack || isCaptured;
			if (isArea) {
				if (mValue == mBlackArea) {
					//overlay[oidx].text = std::string("B");
					//overlay[oidx].layer = 0;
				}
				else if (mValue == mWhiteArea) {
					//overlay[oidx].text = std::string("W");
					//overlay[oidx].layer = 0;
				}
			}
			else if (isCaptured && (lastPlayed_i != i || lastPlayed_j != j)) {
				//overlay[oidx].text = "D";
				//overlay[oidx].layer = mValue == mBlack + mDeltaCaptured ? 1 : 2;
			}
			else if (isStone && placeStone && lastPlayed_i == i && lastPlayed_j == j) {
				std::stringstream ss;
                order = board.order;
				ss << order;
				overlay[oidx].text = ss.str();
				overlay[oidx].layer = mValue == mBlack || mValue == mBlack + mDeltaCaptured ? 1 : 2;
			}
			else {
				overlay[oidx].text = std::string("");
				overlay[oidx].layer = -1u;
			}
		}

	}
	if (placedSomeStone && lpi > -1 && lpj > -1)  {
		int oidx = boardSize  * lpi + lpj;
		overlay[oidx].text = std::string("");
		overlay[oidx].layer = -1u;
	}
    return changed;*/
//}

int Board::capturedCount(const Color& whose) const {
    return whose == Color::WHITE ? capturedWhite : capturedBlack;
}

void Board::clear(int boardsize) {

    this->boardSize = boardsize;

    glstones.fill(mEmpty);
    points.fill({
        Color::EMPTY,
        Color::EMPTY,
        {"", -1u},
        0, 0
    });

    capturedBlack = 0;
    capturedWhite = 0;
    positionNumber += 1;
}

void Board::copyStateFrom(const Board& b) {
    glstones = b.glstones;
    points = b.points;
    boardSize = b.boardSize;
    capturedBlack = b.capturedBlack;
    capturedWhite = b.capturedWhite;
    territoryReady = b.territoryReady;
    moveNumber = b.moveNumber;
}

bool Board::parseGtp(const std::vector<std::string>& lines) {
    bool success = false;
    try {
        if(lines.front().at(0) == '=') {
            std::istringstream ssin(lines.at(2));

            int size = 0;
            ssin >> size;
            spdlog::debug("Size: {}", size);
            if(size >= MINBOARD && size <= MAXBOARD) {
                unsigned bcaptured = 0u, wcaptured = 0u;
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
                        unsigned count = 0;
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

double Board::placeCursor(const Position& coord, const Color& col) {

    glm::vec2 v(
        coord.x - coord.col() - 0.5f,
        coord.y - coord.row() - 0.5f
    );

    glm::vec2 add(6.0f * r1 * v);

    Position pos(coord);
    pos.x = coord.col() + add.x;
    pos.y = coord.row() + add.y;

    return stoneChanged(pos, col);
}
