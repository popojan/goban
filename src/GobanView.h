//
// Created by jan on 7.5.17.
//

#include <utility>
#include <string>
#include "GobanOverlay.h"
#include <Rocket/Core/Types.h>
#include <Rocket/Core/Texture.h>
#include <Rocket/Controls/ElementFormControlSelect.h>
#include <Rocket/Controls/SelectOption.h>
#include "GobanShader.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "Metrics.h"
#include "Camera.h"
#include "GameState.h"
#include "Shell.h"

#ifndef GOBAN_GOBANVIEW_H
#define GOBAN_GOBANVIEW_H

extern std::shared_ptr<Configuration> config;

class GobanView: public GameObserver {
public:
    enum {
        UPDATE_NONE = 0,
        UPDATE_BOARD = 1,
        UPDATE_STONES = 2,
        UPDATE_GUI [[maybe_unused]] = 4,
        UPDATE_OVERLAY = 8,
        UPDATE_SOME = 16,
        UPDATE_SHADER = 32,
        UPDATE_SOUND_STONE = 64,
        UPDATE_ALL = (1|2|3|4|8|16|32)
    };

    explicit GobanView(GobanModel& m);

    void onGameMove(const Move& move, const std::string& comment) override;

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

    void setEof(float eof) {
        gobanShader.setEof(eof);
        updateFlag |= UPDATE_SHADER;
    };

    void setDof(float dof) {
        gobanShader.setDof(dof);
        updateFlag |= UPDATE_SHADER;
    }
    float getEof() {
        return gobanShader.getEof();
    };

    float getDof() {
        return gobanShader.getDof();
    };


    float getGamma() const {
		return gobanShader.getGamma();
    }

    void resetAdjustments() {
        gobanShader.setGamma(1.0);
        gobanShader.setContrast(0.0);
        updateFlag |= UPDATE_SHADER;
    }

    float getContrast() const {
		return gobanShader.getContrast();
    }

    bool toggleFpsLimit() {MAX_FPS = !MAX_FPS; return MAX_FPS;}

    void switchShader(int idx)  {
        updateFlag |= GobanView::UPDATE_ALL;
        gobanShader.choose(idx);
        state.metricsReady = false;
        gobanShader.setReady();
    }

    [[nodiscard]] Position getBoardCoordinate(float x, float y)const ;
    [[nodiscard]] glm::vec2 boardCoordinate(float x, float y) const;

    void resetView();
    void saveView();
    void shadeIt(float time, GobanShader &shader) const;

    void animateIntro();

    void requestRepaint(int what = UPDATE_SOME);
    bool toggleOverlay();

    void Update();
    void moveCursor(float, float);
    void updateCursor();

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
    bool animationRunning;
    bool isPanning, isZooming, isRotating;
    DDG::Camera cam;
    float startX, startY, lastX, lastY;

    Board board;

    GameState state;

    int updateFlag;
    int currentProgram;
    bool showOverlay;
    Position lastMove;
    AudioPlayer player;

    void clearView();
};


#endif //GOBAN_GOBANVIEW_H
