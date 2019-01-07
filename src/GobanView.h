//
// Created by jan on 7.5.17.
//

#include <utility>
#include <string>
#include <Rocket/Core/Types.h>
#include <Rocket/Core/Texture.h>
#include "GobanShader.h"
#include "GobanModel.h"
#include "Metrics.h"
#include "Camera.h"
#include "GobanOverlay.h"
#include "GameState.h"
#include "Shell.h"

#ifndef GOBAN_GOBANVIEW_H
#define GOBAN_GOBANVIEW_H

class GobanView {
public:
    enum {UPDATE_NONE = 0, UPDATE_BOARD = 1, UPDATE_STONES = 2, UPDATE_GUI = 4, UPDATE_OVERLAY = 8, UPDATE_SOME = 16, UPDATE_SHADER = 32, UPDATE_ALL = -1};

	GobanView(GobanModel& m)
		: gobanShader(*this), gobanOverlay(*this), model(m), MAX_FPS(false), VIEWPORT_WIDTH(0), VIEWPORT_HEIGHT(0),
		translate(0.0, 0.0, 0.0), newTranslate(0.0, 0.0, 0.0), resolution(1024.0, 768.0), lastTime(0.0f),
		startTime(0.0f), animationRunning(false), isPanning(false), isZooming(false), isRotating(false),
		needsUpdate(0), cam(1.0, 0.0, 0.0, 0.0), startX(0), startY(0), lastX(.0f), lastY(.0f), updateFlag(0),
		currentProgram(-1),	showOverlay(true),  cursor(-1,-1), lastMoveNumber(0)
	{

	    console = spdlog::get("console");

	    gobanOverlay.init();
        gobanShader.init();
		initCam();
		updateTranslation();
		translate[0] = newTranslate[0];
        translate[1] = newTranslate[1];
        translate[2] = newTranslate[2];
        updateFlag |= GobanView::UPDATE_SHADER;
        gobanShader.setReady();
        gobanOverlay.setReady();
    }

    ~GobanView() {
        gobanShader.destroy();
    }
    void Render(int, int);

    void updateTranslation();
    //bool invalidate(int inc = 1);
    void reshape(int, int);

    void initCam();
    void initPan(float x, float y);
    void endPan();
    void initRotation(float x, float y);
    void endRotation();
    void initZoom(float x, float y);
    void endZoom();
    void mouseMoved(float x, float y);
    void zoomRelative(float);

    void setGamma(float gamma) {
        gobanShader.setGamma(gamma);
        updateFlag |= UPDATE_SHADER;
    };

    void setContrast(float contrast) {
		gobanShader.setContrast(contrast);
        updateFlag |= UPDATE_SHADER;
    }

    float getGamma() {
		return gobanShader.getGamma();
    }

    void resetAdjustments() {
        gobanShader.setGamma(1.0);
        gobanShader.setContrast(0.0);
        updateFlag |= UPDATE_SHADER;
    }

    float getContrast() {
		return gobanShader.getContrast();
    }

    void toggleFpsLimit() {MAX_FPS = !MAX_FPS; }

    void cycleShaders()  {
        updateFlag |= GobanView::UPDATE_ALL;
		gobanShader.cycleShaders();
        state.metricsReady = false;
    }

    void toggleAnimation(time_t);

    Position getBoardCoordinate(float x, float y)const ;
    glm::vec2 boardCoordinate(float x, float y) const;

    void resetView();
	void shadeit(float, GobanShader&);

    bool needsRepaint();
    void requestRepaint(int what = UPDATE_SOME);
	void toggleOverlay();
	void Update();
	void moveCursor(float, float);

private:
    std::shared_ptr<spdlog::logger> console;
public:
    GobanShader gobanShader;
    GobanOverlay gobanOverlay;
    GobanModel& model;

    bool MAX_FPS;
    int WINDOW_WIDTH = 0, WINDOW_HEIGHT = 0;
    float VIEWPORT_WIDTH, VIEWPORT_HEIGHT;
    glm::vec3 translate, newTranslate;
    glm::vec2 resolution;
    float lastTime, startTime;
    volatile bool animationRunning;
    volatile bool calculatingScore = false;
    bool isPanning, isZooming, isRotating;
    int needsUpdate;
    DDG::Camera cam;
    float startX, startY, lastX, lastY;

    Board board;
    GameState state;

    int updateFlag;
    int currentProgram;
	bool showOverlay;
    Position cursor, lastCursor;
    long lastMoveNumber;
};


#endif //GOBAN_GOBANVIEW_H
