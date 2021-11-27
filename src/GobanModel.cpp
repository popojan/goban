//
// Created by jan on 7.5.17.
//

#include "ElementGame.h"
#include "GobanModel.h"
#include <glm/glm.hpp>

void GobanModel::onBoardSized(int boardSize) {

    spdlog::info("{} {} {} {}", boardSize, state.komi, state.black, state.white);

	board.clear(boardSize);
    board.clearTerritory(boardSize);
	board.toggleTerritoryAuto(false);
    board.positionNumber += 1;

	over    = false;
	started = false;

    auto black = state.black;
    auto white = state.white;

	state = GameState();
    state.black = black;
    state.white = white;

    state.reservoirBlack = state.reservoirWhite = (boardSize*boardSize - 1)/2 + 1;
	calcCapturedBlack = 0;
	calcCapturedWhite = 0;

    if(!state.metricsReady) {
        metrics.calc(boardSize);
        calcCaptured(metrics, state.capturedBlack, state.capturedWhite);
        state.metricsReady = true;
    }

}
GobanModel::~GobanModel() {
    //parent->RemoveReference();
}

float GobanModel::result(const Move& lastMove, GameState::Result& ret) {
    if (state.reason == GameState::DOUBLE_PASS) {
        ret.black_territory = ret.white_territory = 0;
        ret.black_prisoners = ret.white_prisoners = 0;
        ret.black_captured = board.capturedCount(Color::BLACK);
        ret.white_captured = board.capturedCount(Color::WHITE);
        auto& points = board.get();
        for (auto pit = points.begin(); pit != points.end(); ++pit) {
            const Color& stone = pit->stone;
            const Color& area = pit->influence;
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
            else if (area != stone){
                if (area == Color::WHITE) {
                    ret.black_prisoners++;
                    ret.white_territory++;
                }
                else if (area == Color::BLACK) {
                    ret.white_prisoners++;
                    ret.black_territory++;
                }
            }
            else {
                if (area == Color::WHITE)
                    ret.white_area++;
                else if (area == Color::BLACK)
                    ret.black_area++;
            }
        }
        ret.delta =
                +ret.white_territory
                + ret.black_captured
                + ret.black_prisoners
                - ret.black_territory
                - ret.white_captured
                - ret.white_prisoners
                + state.komi;
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
    capturedBlack = capturedWhite = m.maxc;
    //if (capturedBlack != calcCapturedBlack || capturedWhite != calcCapturedWhite) {
    float cc0x = 0.0f;//m.bowlsCenters[0];
    float cc0z = 0.0f;//m.bowlsCenters[2];
    float cc1x = 0.0f;//m.bowlsCenters[3];
    float cc1z = 0.0f;//m.bowlsCenters[5];
    float magic = 0.0f;
    float factor = 1.00f;
    float ddcy = 0.5f * m.h - factor * m.innerBowlRadius - magic;

    vec3 s0a(cc0x, -magic, cc0z);
    vec3 s1a(cc0x, ddcy, cc0z);
    vec3 s0b(cc1x, -magic, cc1z);
    vec3 s1b(cc1x, ddcy, cc1z);
    vec3 sy(0.0f, m.stoneSphereRadius - 0.5 * m.h, 0.0f);


    for (int i = 0; i < 2 * maxCaptured; ++i) {
        float ccx = cc0x;
        float ccz = cc0z;
        float rr = factor * m.innerBowlRadius;
        float ccy = 0.0f;//0.1f - rr;
        vec3 s1 = s1a;
        vec3 s0 = s0a;
        int di = calcCapturedBlack;
        bool white = false;

        /*if (i + calcCapturedWhite >= maxCaptured + capturedWhite
            || (i + calcCapturedBlack >= capturedBlack && i <maxCaptured)) {
            continue;
        }*/
        if (i + calcCapturedBlack >= capturedBlack) {
            ccx = cc1x;
            ccz = cc1z;
            s1 = s1b;
            s0 = s0b;
            di = calcCapturedWhite;
            white = true;
        }

        float randx = (float(rand()) / RAND_MAX - 0.5f);
        float randz = (float(rand()) / RAND_MAX - 0.5f);
        float mindy = 1e6, mindx = 0, mindz = 0;
        const int ITERS = 500;
        for (int k = 0; k < ITERS; ++k) {
            const int turns = int(sqrt(ITERS));
            const float a = (rr - m.stoneRadius) / (2.0f * 3.1415926f * turns);
            const float phi = turns * 2.0f * 3.1415926f * k / float(ITERS);
            const float r = a * phi;
            const float alpha = (rand() < (RAND_MAX >> 10)) ? 0.9 : 0.0;
            float nrandx = cos(phi) * r; //;alpha * randx + (1.0 - alpha) * (float(rand()) / RAND_MAX - 0.5f);
            float nrandz = sin(phi) * r;//;alpha * randz + (1.0 - alpha) * (float(rand()) / RAND_MAX - 0.5f);
            float dx = nrandx;
            float dz = nrandz;
            float d = rr * rr - dx * dx - dz * dz;
            if (d < 0) continue;
            float dy = rr - sqrt(d) + 0.5f * m.h;
            vec3 dir(dx, rr - dy - m.stoneSphereRadius + 0.5f * m.h, dz);
            float mult = rr / (m.stoneSphereRadius + sqrt(dot(dir, dir)));
            if (mult < 1.0f) {
                dir *= mult ;
                dx = dir.x;
                dy = rr - m.stoneSphereRadius + 0.5f * m.h - dir.y;
                dz = dir.z;
            }
            for (int j = white ? maxCaptured : 0; j < i + di; ++j) {
                float ax = (dx + ccx - ddc[4 * j + 0]);
                float az = (dz + ccz - ddc[4 * j + 2]);
                if (az * az + ax * ax < 4.0f * m.stoneSphereRadius * m.stoneSphereRadius) {
                    dy = max(dy, ddc[4 * j + 1] +
                                 sqrt(4.0f * m.stoneSphereRadius * m.stoneSphereRadius - az * az - ax * ax) -
                                 2.0f * m.stoneSphereRadius + m.h - ccy);
                }
            }
            if (dy < mindy) {
                mindy = dy;
                mindx = dx;
                mindz = dz;
                randx = nrandx;
                randz = nrandz;
            }
        }
        ddc[4 * (i + di) + 0] = ccx + mindx;
        ddc[4 * (i + di) + 1] = ccy + mindy;
        ddc[4 * (i + di) + 2] = ccz + mindz;
        ddc[4 * (i + di) + 3] = board.getRandomStoneRotation();
    }
    calcCapturedBlack = capturedBlack;
    calcCapturedWhite = capturedWhite;
    //}
    int dBlack = max(capturedBlack - m.maxc, 0);
    int dWhite = max(capturedWhite - m.maxc, 0);
    for (int i = 0; i < m.maxc; ++i) {
        m.tmpc[4 * i + 0] = ddc[4 * (i + dBlack) + 0];
        m.tmpc[4 * i + 1] = ddc[4 * (i + dBlack) + 1];
        m.tmpc[4 * i + 2] = ddc[4 * (i + dBlack) + 2];
        m.tmpc[4 * i + 3] = ddc[4 * (i + dBlack) + 3];
        m.tmpc[4 * m.maxc + 4 * i + 0] = ddc[4 * maxCaptured + 4 * (i + dWhite) + 0];
        m.tmpc[4 * m.maxc + 4 * i + 1] = ddc[4 * maxCaptured + 4 * (i + dWhite) + 1];
        m.tmpc[4 * m.maxc + 4 * i + 2] = ddc[4 * maxCaptured + 4 * (i + dWhite) + 2];
        m.tmpc[4 * m.maxc + 4 * i + 3] = ddc[4 * maxCaptured + 4 * (i + dWhite) + 3];
    }

}

Move GobanModel::getPassMove() {
    return Move(Move::PASS, state.colorToMove);
}
Move GobanModel::getUndoMove() {
    return Move(Move::UNDO, state.colorToMove);
}

void GobanModel::onHandicapChange(const std::vector<Position>& stones) {
    handicapStones = stones;
}

void GobanModel::onGameMove(const Move& move, const std::string& comment) {
    spdlog::debug("LOCK model");
    std::lock_guard<std::mutex> lock(mutex);

    if ((move == Move::PASS && prevPass) || move == Move::RESIGN) {
        state.reason = move == Move::RESIGN ? GameState::RESIGNATION : GameState::DOUBLE_PASS;
        board.toggleTerritoryAuto(true);
        over = true;
        spdlog::debug("Main Over! Reason {}", state.reason);
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
        if(state.holdsStone == false) {
            if (state.colorToMove == Color::BLACK)
                state.reservoirBlack -= 1;
            else
                state.reservoirWhite -= 1;
        }
    }
    if(!(move == Move::UNDO)) {
        game.move(move);
        if(!comment.empty()) {
            game.annotate(comment);
        }
    }
    state.holdsStone = false;
    changeTurn();
}

void GobanModel::onBoardChange(const Board& result) {
    spdlog::debug("LOCK board");
    std::lock_guard<std::mutex> lock(mutex);

    board.copyStateFrom(result);
    board.positionNumber += 1;

    state.capturedBlack = board.capturedCount(Color::BLACK);
    state.capturedWhite = board.capturedCount(Color::WHITE);

    spdlog::debug("over {} ready {}", over, result.territoryReady);

    if(over && result.territoryReady) {
        this->result(game.lastMove(), state.adata);
        game.finalizeGame(state.adata);
        game.saveAs("");
    }
}

void GobanModel::onKomiChange(float newKomi) {
    if (!started) {
        spdlog::debug("setting komi {}", newKomi);
        state.komi = newKomi;
    }
}

void GobanModel::onPlayerChange(int role, const std::string& name) {
    std::ostringstream val;
    val << GameRecord::eventNames[GameRecord::PLAYER_SWITCHED];
    if(role & Player::BLACK) {
        state.black = name;
        val << "black=" << name;

    }
    if(role & Player::WHITE) {
        state.white = name;
        val << "white=" << name;
    }
    val << " ";
    game.annotate(val.str());
}
