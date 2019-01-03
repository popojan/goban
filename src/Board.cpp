#include "Board.h"
#include <sstream>
#include <algorithm>

const float Board::mBlackArea = 2.0;
const float Board::mWhiteArea = 3.0;
const float Board::mEmpty = 0.0;
const float Board::mBlack = 5.0;
const float Board::mWhite = 6.0;
const float Board::mDeltaCaptured = 0.5;

Board::Board(int size) : capturedBlack(0), capturedWhite(0), boardSize(size), r1(.0f), rStone(.0f),
    dist(0.0f, 0.05f), invalidated(false), order(0), moveNumber(0)
{
    console = spdlog::get("console");
    clear(size);
    positionNumber = generator();;
}

bool Board::isEmpty() const {
    return std::all_of(board.begin(), board.end(), [](Color c){return c == Color::EMPTY;});
}

const float* Board::getStones() const { return &stones[0]; }
float* Board::getStones() { return &stones[0]; }

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
            stream << board[Position(j, i)] << " ";
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

double Board::fixStone(int i, int j, int i0, int j0) {
    console->debug("fixStone ({},{})/({},{})", i, j, i0, j0);
    int idx = ((boardSize  * i + j) << 2) + 2;
    int idx0 = ((boardSize  * i0 + j0) << 2) + 2;
    float mValue = stones[idx0];
    double ret = 0.0;
    if (mValue != mEmpty &&  mValue != mBlackArea && mValue != mWhiteArea) {
        console->debug("1st IF");
        float x0 = stones[idx0 - 2];
        float y0 = stones[idx0 - 1];
        float x = stones[idx - 2];
        float y = stones[idx - 1];
        Position p(i0, j0);
        glm::vec2 v(x0-x,y0-y);
        if(glm::length(v) <= 1.05f*rStone) {
            v = glm::vec2(x, y) + 1.05f * rStone * glm::normalize(v);
            p.x = v.x;
            p.y = v.y;
            console->debug("2nd IF [{},{}]->[{},{}]", x0,y0, p.x,p.y);
            unsigned idx = ((boardSize  * i0 + j0) << 2u) + 2u;
            stones[idx - 2] = p.x;
            stones[idx - 1] = p.y;
            ret = glm::distance(glm::vec2(p.x, p.y), glm::vec2(x, y))/0.71;
        }
        /*float dd = 2.0f;
        if (i == i0) dd = std::abs(x0 - x);
        if (j == j0) dd = std::abs(y0 - y);
        if (dd < rStone) {
            if (i == i0) {
                float delta = 0.0f;
                for (int k = 0; k < 100 && std::abs(x0 - x) < rStone; ++k)
                    x0 = delta + j0 - halfN + std::max(-r1, std::min(r1, dist(generator)));
                stones[idx0 - 2] = x0;
            }
            if (j == j0) {
                float delta = 0.0f;
                for (int k = 0; k < 100 && std::abs(y0 - y) < rStone; ++k)
                    y0 = delta + i0 - halfN + std::max(-r1, std::min(r1, dist(generator)));
                stones[idx0 - 1] = y0;
            }
			std::size_t oidx = boardSize  * i0 + j0;
			overlay[oidx].x = stones[idx0 - 2];
			overlay[oidx].y = stones[idx0 - 1];
            return false;
        }*/
        return ret;
    }
    return ret;
}

double Board::placeFuzzy(const Position& p){
    int j = p.col();
    int i = p.row();
    const float halfN = 0.5f * boardSize - 0.5f;
    float x = p.x;
    float y = p.y;
    double vol = 0.0, ret = 0.0;
    if(x == 0 && y == 0) {
        x = j - halfN + std::max(-3.0f * r1, std::min(3.0f * r1, dist(generator)));
        y = i - halfN + std::max(-3.0f * r1, std::min(3.0f * r1, dist(generator)));
    }
    else {
        glm::vec2 v(x - j, y - i);
        glm::vec2 add(3.0f * r1 * glm::normalize(v)* glm::length(v)/0.71f);
        x = j - halfN + add.x;
        y = i - halfN + add.y;
    }
    unsigned idx = ((boardSize  * i + j) << 2u) + 2u;
    stones[idx - 2] = x;
    stones[idx - 1] = y;
    stones[idx + 1] = 3.14f*dist(generator);
    for (int ii = i + 1u, i0 = i; ii < boardSize; i0 = ii, ++ii) {
        ret = std::max(vol, ret);
        if ((vol = fixStone(i0, j, ii, j)) == 0.0) break;
    }
    for (int ii = i - 1u, i0 = i; ii >= 0; i0 = ii, --ii) {
        ret = std::max(vol, ret);
        if ((vol = fixStone(i0, j, ii, j)) == 0.0) break;
    }
    for (int jj = j + 1u, j0 = j; jj < boardSize; j0 = jj, ++jj) {
        ret = std::max(vol, ret);
        if ((vol = fixStone(i, j0, i, jj)) == 0.0) break;
    }
    for (int jj = j - 1u, j0 = j; jj >= 0; j0 = jj, --jj) {
        ret = std::max(vol, ret);
        if ((vol = fixStone(i, j0, i, jj)) == 0.0) break;
    }
    return ret;
}

int Board::updateStones(const Board& board, const Board& territory, bool showTerritory) {
    int newSize = board.getSize();
    int changed = boardSize != newSize ? 1 : 0;
    boardSize = newSize;
    float halfN = 0.5f * boardSize - 0.5f;
    auto bbegin = board.begin(), bend = board.end();
	this->showTerritory = board.showTerritory;
	this->showTerritoryAuto = board.showTerritoryAuto;
	int lpi = lastPlayed_i;
	int lpj = lastPlayed_j;
	bool placedSomeStone = false;
    for (auto cit = bbegin, pit = territory.begin(); cit != bend; ++cit, ++pit) {
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

		if (stones[idx] != mValue) {
		    int change = 0;
			bool territoryToggle = std::abs(stones[idx] - mValue) == mDeltaCaptured;
			bool placeStone = !territoryToggle && /*dif != mDeltaLastMove &&*/ mValue != mEmpty
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
	if (placedSomeStone)  {
		int oidx = boardSize  * lpi + lpj;
		overlay[oidx].text = std::string("");
		overlay[oidx].layer = -1u;
	}
    return changed;
}

int Board::capturedCount(const Color& whose) const {
    return whose == Color::WHITE ? capturedWhite : capturedBlack;
}

void Board::clear(int boardsize) {
	this->boardSize = boardsize;
    stones.fill(mEmpty);
    overlay.fill({ "", -1u, 0, 0});
    order = 0;
    board.fill(Color::EMPTY);
    capturedBlack = 0;
    capturedWhite = 0;
    positionNumber += 1;
}

void Board::copyStateFrom(const Board& b) {
    stones = b.stones;
    board = b.board;
    boardSize = b.boardSize;
    capturedBlack = b.capturedBlack;
    capturedWhite = b.capturedWhite;
    moveNumber = b.moveNumber;
}

bool Board::parseGtp(const std::vector<std::string>& lines) {
    bool success = false;
    try {
        if(lines.front().at(0) == '=') {
            std::istringstream ssin(lines.at(2));

            int size = 0;
            ssin >> size;
            console->debug("Size: {}", size);
            if(size >= MINBOARD && size <= MAXBOARD) {
                unsigned bcaptured = 0u, wcaptured = 0u;
                console->debug(lines.at(1));
			    bool white = true;
                for(int i = 2; i < 2 + size; ++i) {
                    console->debug(lines[i]);
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
                        board[ord(p)] = color;
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
                console->debug("Captured: {}, {}", capturedBlack, capturedWhite);
                console->debug(lines[2 + size]);
                moveNumber += 1;
            }
        }
    }
    catch (const std::out_of_range& oor) {
        console->error("Gtp parse error: {}", oor.what());
    }
    return success;

}

bool Board::parseGtpInfluence(const std::vector<std::string>& lines) {
    bool success = false;
    try {
        for (int i = 0; i < boardSize; ++i) {
            console->debug("row = {}/{}", i, boardSize);
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
                board[ord(Position(j, boardSize - i - 1))] = c;
            }
        }
        success = true;
    }
    catch (const std::out_of_range& oor) {
        console->error("Gtp parse error: {}", oor.what());
    }
    return success;

}

const Board::Overlay& Board::getOverlay() const {
	return overlay;
}

Board::Overlay& Board::getOverlay() {
    return overlay;
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
    //float halfN = 0.5f * boardSize - 0.5f;
    int oidx = ((boardSize  * coord.row() + coord.col()) << 2u) + 2u;
    console->debug("oidx = [{},{}] = {} ... {} {}", coord.col(), coord.row(), oidx, board[coord].toString(), col.toString());
    double altered = placeFuzzy(coord);
    //stones[oidx - 2] = coord.col() - halfN;
    //stones[oidx - 1] = coord.row() - halfN;
    stones[oidx + 0] = (col == Color::WHITE) ? mWhite : mBlack;
    //stones[oidx + 1] = 0;
    console->debug("overlay coord = [{},{}] = {}", stones[oidx - 2], stones[oidx - 1], stones[oidx + 0]);
    return altered;
}
