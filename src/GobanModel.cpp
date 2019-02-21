//
// Created by jan on 7.5.17.
//

#include "ElementGame.h"
#include "GobanModel.h"
#include <glm/glm.hpp>

void GobanModel::newGame(int boardSize, int handicap, float komi) {
	this->komi = komi;
	if (komi == 0.0f) {
		this->komi = handicap > 0 ? 0.5f : 6.5f;
	}
	lastHandicap = this->handicap = handicap;
	board.clear(boardSize);

	territory.clear(boardSize);
	over = false;
	started = false;
	holdsStone = false;
	auto activePlayerId = state.activePlayerId;
	state = GameState();
	state.komi = this->komi; //TODO jedno komi
	state.handicap = this->handicap; //TODO jeden handicap
	state.activePlayerId = activePlayerId;

	//state.reason = GameState::NOREASON;
	//state.adata = GameState::Result();
	calcCapturedBlack = calcCapturedWhite = 0;

	/*auto problems(sgf::parse("./problems/alphago/master04_Xie_Erhao.sgf"));
	auto variations = problems.at(0).variations();
	if (variations[0].nodehas("C")) {
		std::cerr << "SGF " << node.get("C")[0] << std::endl;
	}

	for (int i = 0; i < search()nodes.size(); ++i) {
		if (nodes[i].has("B")) {
			std::cerr << "SGF " << nodes[i].get("B")[0] << std::endl;
		}
		if (nodes[i].has("W")) {
			std::cerr << "SGF " << nodes[i].get("W")[0] << std::endl;
		}
	}*/
    if(!state.metricsReady) {
        metrics.calc(board.getSize());
        state.metricsReady = true;
    }

}

float GobanModel::result(const Move& lastMove, GameState::Result& ret) {
    if (state.reason == GameState::DOUBLE_PASS) {
        ret.black_territory = ret.white_territory = 0;
        ret.black_prisoners = ret.white_prisoners = 0;
        ret.black_captured = board.capturedCount(Color::BLACK);
        ret.white_captured = board.capturedCount(Color::WHITE);
        for (auto pos = board.begin(), iter = territory.begin(); pos != board.end(); ++pos, ++iter) {
            const Color& stone = *pos;
            const Color& area = *iter;
            if (stone == Color::EMPTY) {
                if (area == Color::WHITE) {
                    ret.white_territory++;
                    ret.white_area++;
                }
                else if (area == Color::BLACK) {
                    ret.black_territory++;
                    ret.black_area++;
                }
            }
            else if (stone != Color::EMPTY && area != stone){
                if (area == Color::WHITE) {
                    ret.black_prisoners++;
                    ret.white_territory++;
                }
                else if (area == Color::BLACK) {
                    ret.white_prisoners++;
                    ret.black_territory++;
                }
            }
            else if (stone != Color::EMPTY && area == stone) {
                if (area == Color::WHITE)
                    ret.white_area++;
                else if (area == Color::BLACK)
                    ret.black_area++;
            }
        }
        /*if (false)//chinese
            ret.delta =
            -ret.white_area
            - ret.black_area
            + komi
            + handicap;
        else*/
        ret.delta =
                +ret.white_territory
                + ret.black_captured
                + ret.black_prisoners
                - ret.black_territory
                - ret.white_captured
                - ret.white_prisoners
                + komi;
    }
    else {
        ret.delta = lastMove == Color::BLACK ? 1.0f : -1.0f;
    }
    ret.reason = state.reason;
    if (state.reason == GameState::DOUBLE_PASS) {
        bool whiteWon = ret.delta > 0.0;
        state.msg = whiteWon ? GameState::WHITE_WON : GameState::BLACK_WON;
    }
    else if(state.reason == GameState::RESIGNATION){
        bool blackResigned =  ret.delta > 0.0;
        state.msg =  blackResigned ? GameState::BLACK_RESIGNED : GameState::WHITE_RESIGNED;
    }
    return ret.delta;
}
unsigned GobanModel::getBoardSize() const {
    return board.getSize();
}

bool GobanModel::isPointOnBoard(const Position& p) {
    const int N = board.getSize();
    return p.col() >= 0 && p.col() < N && p.row() >= 0 && p.row() < N;
}

void GobanModel::calcCaptured(Metrics& m, int capturedBlack, int capturedWhite) {
    using namespace glm;
    if (capturedBlack != calcCapturedBlack || capturedWhite != calcCapturedWhite) {

        float cc0x = m.bowlsCenters[0];
        float cc0z = m.bowlsCenters[2];
        float cc1x = m.bowlsCenters[3];
        float cc1z = m.bowlsCenters[5];
        float ddcy = 0.5f*m.h - m.innerBowlRadius - 0.3f;

        vec3 s0a(cc0x, -0.3, cc0z);
        vec3 s1a(cc0x, ddcy, cc0z);
        vec3 s0b(cc1x, -0.3, cc1z);
        vec3 s1b(cc1x, ddcy, cc1z);
        vec3 sy(0.0f, m.stoneSphereRadius - 0.5*m.h, 0.0f);


        for (int i = 0; i < 2 * maxCaptured; ++i) {
            float ccx = cc0x;
            float ccz = cc0z;
            float rr = m.innerBowlRadius;
            float ccy = 0.1f - rr;
            vec3 s1 = s1a;
            vec3 s0 = s0a;
            int di = calcCapturedBlack;
            bool white = false;

            if (i + calcCapturedWhite >= maxCaptured + capturedWhite || (i + calcCapturedBlack >= capturedBlack && i <maxCaptured)) {
                continue;
            }
            if (i + calcCapturedBlack >= capturedBlack) {
                ccx = cc1x;
                ccz = cc1z;
                s1 = s1b;
                s0 = s0b;
                di = calcCapturedWhite;
                white = true;
            }

            float mindy = 1e6, mindx = 0, mindz = 0;
            for (int k = 0; k < 5000; ++k) {
                float dx = 2.0f*rr * (float(rand()) / RAND_MAX - 0.5f);
                float dz = 2.0f*rr * (float(rand()) / RAND_MAX - 0.5f);
                float d = rr*rr - dx*dx - dz*dz;
                if (d < 0) continue;
                float dy = rr - sqrt(d) + 0.5f*m.h;
                vec3 dir(dx, rr - dy - m.stoneSphereRadius + 0.5f*m.h, dz);
                float mult = m.innerBowlRadius / (m.stoneSphereRadius + sqrt(dot(dir, dir)));
                if (mult < 1.0f) {
                    dir *= mult;
                    dx = dir.x;
                    dy = rr - m.stoneSphereRadius + 0.5f*m.h - dir.y;
                    dz = dir.z;
                }
                for (int j = white ? maxCaptured:0; j < i + di; ++j) {
                    float ax = (dx + ccx - ddc[3 * j + 0]);
                    float az = (dz + ccz - ddc[3 * j + 2]);
                    if (az*az + ax * ax < 4.0f*m.stoneSphereRadius*m.stoneSphereRadius) {
                        dy = max(dy, ddc[3 * j + 1] + sqrt(4.0f*m.stoneSphereRadius*m.stoneSphereRadius - az*az - ax*ax) - 2.0f*m.stoneSphereRadius + m.h - ccy);//ddc[3 * j + 1] + h - ccy);
                    }
                }
                if (dy < mindy) {
                    mindy = dy;
                    mindx = dx;
                    mindz = dz;
                }
            }
            ddc[3 * (i+di) + 0] = ccx + mindx;
            ddc[3 * (i+di) + 1] = ccy + mindy;
            ddc[3 * (i+di) + 2] = ccz + mindz;
        }
        calcCapturedBlack = capturedBlack;
        calcCapturedWhite = capturedWhite;
    }
    int dBlack = max(capturedBlack - m.maxc, 0);
    int dWhite = max(capturedWhite - m.maxc, 0);
    for (int i = 0; i < m.maxc; ++i){
        m.tmpc[3 * i + 0] = ddc[3 * (i + dBlack) + 0];
        m.tmpc[3 * i + 1] = ddc[3 * (i + dBlack) + 1];
        m.tmpc[3 * i + 2] = ddc[3 * (i + dBlack) + 2];
        m.tmpc[3 * m.maxc + 3 * i + 0] = ddc[3 * maxCaptured + 3 * (i + dWhite) + 0];
        m.tmpc[3 * m.maxc + 3 * i + 1] = ddc[3 * maxCaptured + 3 * (i + dWhite) + 1];
        m.tmpc[3 * m.maxc + 3 * i + 2] = ddc[3 * maxCaptured + 3 * (i + dWhite) + 2];

    }
}

//bool GobanModel::playMove(const Move& move) {}

Move GobanModel::getPassMove() {
    return Move(Move::PASS, state.colorToMove);
}
Move GobanModel::getUndoMove() {
    return Move(Move::UNDO, state.colorToMove);
}

void GobanModel::update(const Move& move, const Board& result) {

    console->debug("LOCK board");
    std::lock_guard<std::mutex> lock(mutex);

    board.copyStateFrom(result);
    board.positionNumber += 1;
    board.order += 1;

    state.capturedBlack = board.capturedCount(Color::BLACK);
    state.capturedWhite = board.capturedCount(Color::WHITE);
    calcCaptured(metrics, state.capturedBlack, state.capturedWhite);

    history.push_back(move);

    if ((move == Move::PASS && prevPass) || move == Move::RESIGN) {
        state.reason = move == Move::RESIGN ? GameState::RESIGNATION : GameState::DOUBLE_PASS;
        over = true;
        console->debug("Main Over! Reason {}", state.reason);
        this->result(move, state.adata);
        if (state.reason == GameState::DOUBLE_PASS)
            board.toggleTerritoryAuto(true);
    }
    else if (move == Move::PASS) {
        prevPass = true;
        state.msg = state.colorToMove == Color::BLACK
            ? GameState::BLACK_PASS
            : GameState::WHITE_PASS;
    }
    else {
        prevPass = false;
        state.msg = GameState::NONE;
        if(move != Move::PASS) {
            player.play("data/sound/stone.wav", 1.0);
        }
    }
    changeTurn();
}

void GobanModel::update(const Board& territory) {
    console->debug("LOCK territory");
    std::lock_guard<std::mutex> lock(mutex);
    this->territory.copyStateFrom(territory);
    board.positionNumber += 1;
}

bool GobanModel::placeCursor(Board& viewBoard, const Position& cursor){
    if(holdsStone && isPointOnBoard(cursor) && board[cursor] == Color::EMPTY && viewBoard[cursor] == Color::EMPTY){
        double vol = viewBoard.placeCursor(cursor, state.colorToMove);
        if(vol > 0.0 && player.playbackCount() < 5){
	    console->info("collision volume = {}", vol);
            player.play("data/sound/collision.wav", vol);
	}
        return vol > 0.0;
    }
    return false;
}
