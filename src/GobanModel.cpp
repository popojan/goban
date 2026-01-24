//
// Created by jan on 7.5.17.
//

#include "ElementGame.h"
#include "GobanModel.h"
#include <glm/glm.hpp>

void GobanModel::onBoardSized(int boardSize) {

    spdlog::info("{} {} {} {}", boardSize, state.komi, state.black, state.white);

	board.clear(boardSize);
    board.clearTerritory();
	board.toggleTerritoryAuto(false);
    board.positionNumber += 1;

	isGameOver    = false;
	started = false;

    auto black = state.black;
    auto white = state.white;

	state = GameState();
    state.black = black;
    state.white = white;

    state.reservoirBlack = state.reservoirWhite = (boardSize*boardSize - 1)/2 + 1;
	calcCapturedBlack = 0;
	calcCapturedWhite = 0;

    metrics.calc(boardSize);
    calcCaptured(metrics, state.capturedBlack, state.capturedWhite);
    state.metricsReady = true;
}

GobanModel::~GobanModel() = default;

float GobanModel::result(const Move& lastMove) {
    state.scoreDelta = board.score;
    if (state.reason == GameState::DOUBLE_PASS) {
        state.winner = Color(state.scoreDelta < 0.0 ? Color::WHITE : Color::BLACK);
        state.msg = state.winner == Color::WHITE ? GameState::WHITE_WON : GameState::BLACK_WON;
    }
    else if (state.reason == GameState::RESIGNATION) {
        // Winner may be set in onGameMove (live game) or derived from lastMove (SGF loading)
        if (state.winner == Color::EMPTY) {
            // SGF loading path: derive from the resignation move color
            state.winner = (lastMove.col == Color::BLACK) ? Color::WHITE : Color::BLACK;
        }
        bool blackResigned = (state.winner == Color::WHITE);
        state.msg = blackResigned ? GameState::BLACK_RESIGNED : GameState::WHITE_RESIGNED;
        state.scoreDelta = blackResigned ? -1.0 : 1.0;
    }
    return state.scoreDelta;
}

unsigned GobanModel::getBoardSize() const {
    return board.getSize();
}

bool GobanModel::isPointOnBoard(const Position& p) const {
    const int N = board.getSize();
    return p.col() >= 0 && p.col() < N && p.row() >= 0 && p.row() < N;
}

void GobanModel::calcCaptured(Metrics& m, int capturedBlack, int capturedWhite) {
    using namespace glm;
    capturedBlack = capturedWhite = Metrics::maxc;

    for (int i = 0; i < 2 * maxCaptured; ++i) {
        float factor = 1.00f;
        float cc0z = 0.0f;
        float cc0x = 0.0f;
        float ccx = cc0x;
        float ccz = cc0z;
        float rr = factor * m.innerBowlRadius;
        float ccy = 0.0f;
        int di = calcCapturedBlack;
        bool white = false;

        if (i + calcCapturedBlack >= capturedBlack) {
            float cc1z = 0.0f;
            float cc1x = 0.0f;
            ccx = cc1x;
            ccz = cc1z;
            di = calcCapturedWhite;
            white = true;
        }

        float mindy = 1e6, mindx = 0, mindz = 0;
        constexpr int ITERS = 500;
        for (int k = 0; k < ITERS; ++k) {
            const int turns = static_cast<int>(sqrt(ITERS));
            const float a = (rr - m.stoneRadius) / (2.0f * 3.1415926f * static_cast<float>(turns));
            const float phi = static_cast<float>(turns)
                * 2.0f * 3.1415926f * static_cast<float>(k) / static_cast<float>(ITERS);
            const float r = a * phi;
            float dx = cos(phi) * r;
            float dz = sin(phi) * r;
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
            }
        }
        ddc[4 * (i + di) + 0] = ccx + mindx;
        ddc[4 * (i + di) + 1] = ccy + mindy;
        ddc[4 * (i + di) + 2] = ccz + mindz;
        ddc[4 * (i + di) + 3] = static_cast<float>(board.getRandomStoneRotation());
    }
    calcCapturedBlack = capturedBlack;
    calcCapturedWhite = capturedWhite;
    //}
    int dBlack = max(capturedBlack - Metrics::maxc, 0);
    int dWhite = max(capturedWhite - Metrics::maxc, 0);
    for (int i = 0; i < Metrics::maxc; ++i) {
        m.tmpc[4 * i + 0] = ddc[4 * (i + dBlack) + 0];
        m.tmpc[4 * i + 1] = ddc[4 * (i + dBlack) + 1];
        m.tmpc[4 * i + 2] = ddc[4 * (i + dBlack) + 2];
        m.tmpc[4 * i + 3] = ddc[4 * (i + dBlack) + 3];
        m.tmpc[4 * Metrics::maxc + 4 * i + 0] = ddc[4 * maxCaptured + 4 * (i + dWhite) + 0];
        m.tmpc[4 * Metrics::maxc + 4 * i + 1] = ddc[4 * maxCaptured + 4 * (i + dWhite) + 1];
        m.tmpc[4 * Metrics::maxc + 4 * i + 2] = ddc[4 * maxCaptured + 4 * (i + dWhite) + 2];
        m.tmpc[4 * Metrics::maxc + 4 * i + 3] = ddc[4 * maxCaptured + 4 * (i + dWhite) + 3];
    }

}

void GobanModel::onHandicapChange(const std::vector<Position>& stones) {
    handicapStones = stones;
}

void GobanModel::onGameMove(const Move& move, const std::string& comment) {
    spdlog::debug("LOCK model");
    std::lock_guard<std::mutex> lock(mutex);

    // Check for consecutive passes by looking at game history
    // Since coach has already processed this move, we can check the last two moves
    bool isDoublePass = false;
    if (move == Move::PASS && game.moveCount() >= 1) {
        const Move& lastMove = game.lastMove();
        isDoublePass = (lastMove == Move::PASS);
    }
    
    if (isDoublePass || move == Move::RESIGN) {
        state.reason = move == Move::RESIGN ? GameState::RESIGNATION : GameState::DOUBLE_PASS;
        if (move == Move::RESIGN) {
            // Set winner immediately - opposite of who resigned
            state.winner = (move.col == Color::BLACK) ? Color::WHITE : Color::BLACK;
        }
        board.toggleTerritoryAuto(true);
        isGameOver = true;
        spdlog::debug("Main Over! Reason {}", static_cast<int>(state.reason));
    }
    else if (move == Move::PASS) {
        state.msg = state.colorToMove == Color::BLACK
            ? GameState::BLACK_PASS
            : GameState::WHITE_PASS;
    }
    else {
        state.msg = GameState::NONE;
        if(state.holdsStone == false) {
            if (state.colorToMove == Color::BLACK)
                state.reservoirBlack -= 1;
            else
                state.reservoirWhite -= 1;
        }
    }
    // On first move, update PB/PW to capture actual players after setup
    if (game.moveCount() == 0) {
        game.updatePlayers(state.black, state.white);
    }
    game.move(move);
    if(!comment.empty()) {
        game.annotate(comment);
    }
    state.holdsStone = false;
    changeTurn();
}

void GobanModel::onStonePlaced(const Move& move) {
    state.holdsStone = false;
}

void GobanModel::onBoardChange(const Board& result) {
    spdlog::debug("LOCK board");
    std::lock_guard<std::mutex> lock(mutex);

    board.copyStateFrom(result);
    board.positionNumber += 1;

    state.capturedBlack = board.capturedCount(Color::BLACK);
    state.capturedWhite = board.capturedCount(Color::WHITE);

    spdlog::debug("over {} ready {}", isGameOver, result.territoryReady);

    if (result.territoryReady && game.shouldShowTerritory()) {
        // At end of scored game - compute result message
        state.reason = GameState::DOUBLE_PASS;
        this->result(game.lastMove());
        // Finalize and save only once (live game ending, result not yet written)
        if (isGameOver && !game.hasGameResult()) {
            game.finalizeGame(state.scoreDelta);
            game.saveAs("");
        }
    } else if (game.isAtEndOfNavigation() && game.isResignationResult()) {
        // At end of resigned game - show resignation message
        state.msg = game.getResultMessage();
    }
}

void GobanModel::onKomiChange(float newKomi) {
    if (!started) {
        spdlog::debug("setting komi {}", newKomi);
        state.komi = newKomi;
    }
}

void GobanModel::onPlayerChange(int which, const std::string& name) {
    if (which == 0) {
        state.black = name;
    } else {
        state.white = name;
    }

    // Only annotate player switches after first move, and not on finished games
    if (started && game.moveCount() > 0 && !game.hasGameResult()) {
        std::ostringstream val;
        val << GameRecord::eventNames[GameRecord::PLAYER_SWITCHED];
        if (which == 0) {
            val << "black=" << name;
        } else {
            val << "white=" << name;
        }
        val << " ";
        game.annotate(val.str());
    }
}
