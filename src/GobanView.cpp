#include "GobanView.h"
#include "Shell.h"
#include <Rocket/Core/StyleSheet.h>
#include "AudioPlayer.hpp"

#include <iostream>

void GobanView::initRotation(float x, float y) {
	if (!isRotating) {
		isRotating = true;
		startX = x;
		startY = y;
		cam.mouse(1, x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
	}
}

void GobanView::endRotation() {
    cam.mouse(0, lastX, lastY, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    translate[0] = newTranslate[0];
    translate[1] = newTranslate[1];
    translate[2] = newTranslate[2];
    updateTranslation();
    isRotating = false;
}

void GobanView::initPan(float x, float y) {
	if (!isPanning) {
		isPanning = true;
		startX = x;
		startY = y;
	}
}

void GobanView::endPan() {
    translate[0] = newTranslate[0];
    translate[1] = newTranslate[1];
    translate[2] = newTranslate[2];
    isPanning = false;
}

void GobanView::initZoom(float x, float y) {
	if (!isZooming) {
		isZooming = true;
		startX = x;
		startY = y;
	}
}

void GobanView::endZoom() {
    isZooming = false;
    translate[0] = newTranslate[0];
    translate[1] = newTranslate[1];
    translate[2] = newTranslate[2];
}

int GobanView::cycleShaders(){
    updateFlag |= GobanView::UPDATE_ALL;
    int currentProgram = gobanShader.getCurrentProgram();
    currentProgram = gobanShader.choose(currentProgram + 1);
    state.metricsReady = false;
    return currentProgram;
}
void GobanView::resetView() {
    initCam();
    lastTime = 0.0;
    startTime = Shell::GetElapsedTime();
	animationRunning = true;
}

void GobanView::zoomRelative(float percentage) {
    glm::vec4 ro(0.0f, 0.0f, -3.0f, 0.0f);
    glm::vec4 tt(newTranslate[0], newTranslate[1], newTranslate[2], 0.0f);
    glm::mat4 m0 = cam.getView(true);
    glm::vec4 dir = glm::normalize(m0*ro);
    glm::vec4 nBoard = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    float t = glm::dot(-tt-ro, nBoard) / glm::dot(dir, nBoard);
    glm::vec4 a(0.0f, 0.0f, percentage*(-3.0+t) * 0.2f - 0.01f, 1.0);
    a = cam.getView(true) * a;
    newTranslate[0] = translate[0] + a.x;
    newTranslate[1] = translate[1] + a.y;
    newTranslate[2] = translate[2] + a.z;
    translate[0] = newTranslate[0];
    translate[1] = newTranslate[1];
    translate[2] = newTranslate[2];
    requestRepaint();
}

void GobanView::mouseMoved(float x, float y) {
    lastX = x;
    lastY = y;
    if (isRotating){
        cam.motion(x - translate.x, y - translate.y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        updateTranslation();
        requestRepaint();
    }
    else if (isZooming) {
        glm::vec4 a(0.0f, 0.0f, -6.0f * (y - startY)/WINDOW_HEIGHT, 1.0);
        a = cam.getView(true) * a;
        newTranslate[0] = translate[0] + a.x;
        newTranslate[1] = translate[1] + a.y;
        newTranslate[2] = translate[2] + a.z;
        requestRepaint();
    }
    else if (isPanning) {
        glm::vec4 ro(0.0f, 0.0f, -3.0f, 0.0f);
        glm::vec4 tt(newTranslate[0], newTranslate[1], newTranslate[2], 0.0f);
        glm::mat4 m0 = cam.getView(true);
        glm::vec4 dir = glm::normalize(m0*ro);
        glm::vec4 nBoard = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        float t = glm::dot(-tt - ro, nBoard) / glm::dot(dir, nBoard);//<float, glm::precision::defaultp, glm::tvec4>
        float scale = 2.0f*(3.0f - t) / 3.0f;
        glm::vec4 a0(translate[0], translate[1], translate[2], 1.0);
        glm::vec4 a(-scale * (x - startX) / WINDOW_HEIGHT, scale * (y - startY) / WINDOW_HEIGHT, 0.0, 1.0);
        a = cam.getView(true) * a;
        newTranslate[0] = translate[0] + a.x;
        newTranslate[1] = translate[1] + a.y;
        newTranslate[2] = translate[2] + a.z;
        requestRepaint();
    }
}

void GobanView::initCam() {
	cam.rLast[0]=-1.0;
    cam.rLast[1]=1.0;
    cam.rLast[2]=0.0;
    cam.rLast[3]=0.0;
	cam.rLast.normalize();
    translate.x = translate.z = newTranslate.x = newTranslate.z = 0.0;
    translate.y = newTranslate.y = 0.5;
    updateTranslation();
    translate = newTranslate;
}

void GobanView::reshape(int width, int height) {
	WINDOW_WIDTH = width;
	WINDOW_HEIGHT = height;
	resolution = glm::vec2(static_cast<float>(width), static_cast<float>(height));

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	VIEWPORT_WIDTH = viewport[2];
	VIEWPORT_HEIGHT = viewport[3];

	gobanShader.setResolution(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
}

bool GobanView::needsRepaint() {
    return updateFlag != UPDATE_NONE || MAX_FPS;
}

void GobanView::requestRepaint(int what) {
    updateFlag |= what;
}

void GobanView::shadeit(float time, GobanShader& gobanShader) {
	gobanShader.use();

	gobanShader.setTime(lastTime);
	gobanShader.setRotation(cam.setView());
	gobanShader.setPan(newTranslate);

	if (updateFlag & UPDATE_SHADER) {
		spdlog::debug("setMetrics");
		gobanShader.setMetrics(model.metrics);
	}

	gobanShader.draw(model, cam, updateFlag, time);
	gobanShader.unuse();
}

void GobanView::Render(int w, int h)
{

	if(!gobanShader.isReady())
        return;
    glDisable(GL_BLEND);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);


	float time = Shell::GetElapsedTime();

	if (WINDOW_HEIGHT != h || WINDOW_WIDTH != w) {
		reshape(w, h);
		startTime = time;
		lastTime = 0.0;
		animationRunning = true;
		updateFlag = UPDATE_ALL;
	}
    if(updateFlag & UPDATE_SOUND_STONE) {
        updateFlag &= ~UPDATE_SOUND_STONE;
        player.play("move", 1.0);
    }

	if (updateFlag & UPDATE_STONES) {
	    board.updateStones(model.board, model.board.showTerritory);
        updateCursor(model.cursor);
        lastCursor = model.cursor;
        double vol = board.collision;
        if(vol > 0) {
            updateFlag |= UPDATE_OVERLAY;
            if(player.playbackCount() < 5){
	            player.play("clash", vol);
            }
            board.collision = false;
	    }
	}

	shadeit(time, gobanShader);

   	glEnable(GL_BLEND);

	glEnable(GL_DEPTH_TEST);
	if (updateFlag & UPDATE_OVERLAY){
        gobanOverlay.Update(board, model);
	}

	if (time - startTime >= gobanShader.animT) {
		if (showOverlay) {
			gobanOverlay.use();
			gobanOverlay.draw(model, cam, updateFlag & UPDATE_OVERLAY, time, 0);
			gobanOverlay.unuse();
		}
		animationRunning = false;
    }

    glUseProgram(0);

	if (time - startTime >= gobanShader.animT) {
		if (showOverlay) {
			gobanOverlay.use();
			gobanOverlay.draw(model, cam, updateFlag & UPDATE_OVERLAY, time, 1);
			gobanOverlay.unuse();
		}
		animationRunning = false;
	}

	glEnable(GL_BLEND);

	updateFlag = UPDATE_NONE | (UPDATE_SOUND_STONE & updateFlag);
}

void GobanView::toggleOverlay() {
	showOverlay = !showOverlay;
	if (showOverlay) {
		updateFlag |= UPDATE_OVERLAY;
	}
}
void GobanView::updateTranslation() {
    glm::vec4 ro(0.0f,0.0f,-3.0f,0.0f);
    glm::vec4 tt(translate, 0.0f);
    glm::mat4 m0 = cam.getView(true);
    glm::mat4 m = cam.setView();
    glm::vec4 dir = glm::normalize(m0*ro);
    glm::vec4 nBoard = glm::vec4(0.0f,1.0f,0.0f,0.0f);
	float denom = glm::dot(dir, nBoard);
	float t = glm::dot(-tt, nBoard)/denom;//<float, glm::defaultp, glm::tvec4>
    if(denom < 1e-3) {
        t = 0.0;
    }
	glm::vec4 ip = tt +t*dir;
	dir = glm::normalize(m* ro);
    tt = ip - t*dir;
	newTranslate = glm::vec3(tt);
}

Position GobanView::getBoardCoordinate(float x, float y) const {
    glm::vec2 p = boardCoordinate(x, y);
    float xx = p.x/model.metrics.squareSize + 0.5f*model.metrics.fNDIM;
    float yy = p.y/model.metrics.squareSize + 0.5f*model.metrics.fNDIM;
    int px = static_cast<int>(floorf(xx));
    int py = static_cast<int>(floorf(yy));
    Position ret(px, py);
    ret.x = xx;
    ret.y = yy;
    return ret;
}

glm::vec2 GobanView::boardCoordinate(float x, float y) const {
    using namespace glm;
    vec4 ta = vec4(.0f, .0f, .0f, .0f);
    vec4 ro = vec4(.0f, .0f, -3.0f, 0.0f);
    vec4 up = vec4(.0f, .1f, .0f, 0.0f);
    mat4x4 m = cam.setView();
    vec4 tt = vec4(newTranslate, 0.0f);
    vec4 roo = m*ro + tt;
    ta += tt;
    up = normalize(m*up);
    vec3 cw = vec3(normalize(ta - roo));
    vec3 cu = normalize(cross(vec3(up), cw));
    vec3 cv = cross(cw, cu);
    float ratio = resolution.x/resolution.y;
    vec2 q0 = vec2(ratio*2.0f*(x/resolution.x-0.5f), 2.0f*(0.5f - y/resolution.y));
    vec3 rdb = q0.x * cu + q0.y * cv +  3.0f * cw;
    vec3 nBoard = vec3(.0f,1.0f,.0f);
    auto t = dot(-vec3(roo), nBoard)/dot(rdb, nBoard);
    vec3 ip = vec3(roo) + vec3(rdb)*t;
    return vec2(ip.x, ip.z);
}

void GobanView::animateIntro() {
    lastTime = 0.0;
    startTime = Shell::GetElapsedTime();
    animationRunning = true;
    requestRepaint(UPDATE_BOARD|UPDATE_STONES|UPDATE_OVERLAY);
}

void GobanView::Update() {
	int newProgram = gobanShader.getCurrentProgram();
	if (currentProgram != newProgram) {
		updateFlag |= UPDATE_SHADER;
		currentProgram = newProgram;
	}
	if (board.getSize() != model.board.getSize()) {
		updateFlag |= UPDATE_BOARD | UPDATE_STONES;
	}
	if (board.positionNumber != model.board.positionNumber) {
		updateFlag |= UPDATE_STONES | UPDATE_OVERLAY;
		board.positionNumber = model.board.positionNumber;
	}
}

void GobanView::moveCursor(float x, float y) {
    Position coord = getBoardCoordinate(x, y);
    model.setCursor(coord);
    if(model.state.holdsStone) {
        updateFlag |= UPDATE_STONES | UPDATE_OVERLAY;
    }
}

int GobanView::updateCursor(const Position& lastCursor){
    Position cursor = model.cursor;
    int ret = 0;
    auto& np = model.board[cursor];

    if(model.state.holdsStone && model.isPointOnBoard(cursor) && np.stone == Color::EMPTY){
        board.placeCursor(cursor, state.colorToMove);
    }
    state.holdsStone = model.state.holdsStone;
    return ret;
}


void GobanView::onGameMove(const Move& move) {
    if(move == Move::NORMAL) {
        updateFlag |= UPDATE_SOUND_STONE;
        std::ostringstream ss;
        ss << model.history.size();
        if(lastMove) {
            board.removeOverlay(lastMove);
        }
        lastMove = move.pos;
        board.setOverlay(move.pos, ss.str(), move.col);
    }
}
