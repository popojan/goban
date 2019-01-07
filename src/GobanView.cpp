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

/*bool GobanView::invalidate(int inc) {
    bool cleared = false;
    if (inc != 0) {
        needsUpdate++;
    }
    else {
        cleared = needsUpdate > 0 || MAX_FPS || animationRunning;
        needsUpdate = 0;
    }
    return cleared;
}*/

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
		//gobanShader.setGamma(1.0f);
		//gobanShader.setContrast(0.0f);
		console->debug("setMetrics");
		gobanShader.setMetrics(model.metrics);
	}

	//glDisable(GL_BLEND);


	gobanShader.draw(model, cam, updateFlag, time);
	gobanShader.unuse();
}

void GobanView::Render(int w, int h)
{

	if(!gobanShader.isReady())
        return;
    glDisable(GL_BLEND);
	glClear(GL_DEPTH_BUFFER_BIT);
	//glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);


	float time = Shell::GetElapsedTime();

	if (WINDOW_HEIGHT != h || WINDOW_WIDTH != w) {
		reshape(w, h);
		startTime = time;
		lastTime = 0.0;
		animationRunning = true;
		updateFlag = UPDATE_ALL;
	}

	if (updateFlag & UPDATE_STONES) {
	    board.updateStones(model.board, model.territory, model.board.showTerritory);
        if(model.placeCursor(board, cursor))
            updateFlag |= UPDATE_OVERLAY;
		board.positionNumber = model.board.positionNumber;
		board.moveNumber = model.board.moveNumber;
	}

	shadeit(time, gobanShader);

   	glEnable(GL_BLEND);
	//shadeit(time, gobanShaderStones);

	glEnable(GL_DEPTH_TEST);
	if (updateFlag & UPDATE_OVERLAY){
		gobanOverlay.Update(board.getOverlay(), model);
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
	//glDisable(GL_DEPTH_TEST);

	if (time - startTime >= gobanShader.animT) {
		if (showOverlay) {
			gobanOverlay.use();
			gobanOverlay.draw(model, cam, updateFlag & UPDATE_OVERLAY, time, 1);
			gobanOverlay.unuse();
		}
		animationRunning = false;
	}

	glEnable(GL_BLEND);
	//glDisable(GL_DEPTH_TEST);
	updateFlag = UPDATE_NONE;
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

void GobanView::toggleAnimation(time_t currentTime){
    if (!animationRunning) {
        startTime = currentTime;
    }
    else {
        lastTime += currentTime - startTime;
        gobanShader.setTime(lastTime);
		gobanShader.setTime(lastTime);
    }
    animationRunning = !animationRunning;
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
    //overlay cursor
    /*
    if(cursor > 0) {
        auto& overlay = view.board.getOverlay();
        overlay[cursor].text = "";
        overlay[cursor].layer = -1;
        view.requestRepaint(GobanView::UPDATE_OVERLAY);
        cursor = 0;
    }
    auto coord = view.getBoardCoordinate(x, y);
    if(model.isPointOnBoard(coord)) {
        unsigned int boardSize = model.getBoardSize();
        float halfN = 0.5f * boardSize - 0.5f;
        unsigned int oidx = boardSize*coord.first + coord.second;
        console->debug("oidx = {}", oidx);
        auto& overlay = view.board.getOverlay();
        overlay[oidx].text = "X";
        overlay[oidx].x = coord.first - halfN;
        overlay[oidx].y = coord.second - halfN;
        overlay[oidx].layer = 0;
        console->debug("overlay coord = [{},{}]", overlay[oidx].x, overlay[oidx].y);
        view.requestRepaint(GobanView::UPDATE_OVERLAY);
        cursor = oidx;
    }
    */
    /*if(cursor.first > -1) {
        float* stones = model.board.getStones();
        unsigned int boardSize = model.getBoardSize();
        unsigned int oidx = ((boardSize  * cursor.first + cursor.second) << 2u) + 2u;
        stones[oidx + 0] = Board::mEmpty;
        model.board[cursor] = Color::EMPTY;
        requestRepaint(GobanView::UPDATE_STONES);
        cursor = {-1, -1};
    }*/
    Position coord = getBoardCoordinate(x, y);
    cursor = coord;
    requestRepaint(GobanView::UPDATE_STONES);
}
