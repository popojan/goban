#ifndef GOBAN_GOBANVIEW_H
#define GOBAN_GOBANVIEW_H

#include <string>
#include <atomic>
#include "GobanOverlay.h"
#include <RmlUi/Core/Types.h>
#include "GobanShader.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "Camera.h"
#include "GameState.h"
#include "Configuration.h"
#include "AudioPlayer.hpp"


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
        UPDATE_ALL = (1|2|4|8|16|32)
    };

    explicit GobanView(GobanModel& m);

    void onStonePlaced(const Move& move) override;
    void onGameMove(const Move& move, const std::string& comment) override;
    void onBoardChange(const Board& board) override;
    void onBoardSized(int newBoardSize) override;

    ~GobanView() override {
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
    float getEof() const {
        return gobanShader.getEof();
    };

    float getDof() const {
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
    bool isFpsLimitEnabled() const { return MAX_FPS; }

    void switchShader(int idx);

    [[nodiscard]] Position getBoardCoordinate(float x, float y)const ;
    [[nodiscard]] glm::vec2 boardCoordinate(float x, float y) const;

    void resetView();
    void saveView();
    void shadeIt(float time, const GobanShader &shader) const;

    void animateIntro();

    void requestRepaint(int what = UPDATE_SOME);
    bool needsRender() const { return updateFlag != UPDATE_NONE || animationRunning; }
    void stopAudioIfInactive() { player.stopIfInactive(); }
    void playSound(const std::string& id, double volume = 1.0) { player.play(id, volume); }
    bool toggleLastMoveOverlay();
    bool toggleNextMoveOverlay();
    void setTsumegoMode(bool enabled);
    bool isTsumegoMode() const { return tsumegoMode; }

    void Update();
    void moveCursor(float, float);
    void updateCursor();
    void updateLastMoveOverlay();
    void updateNavigationOverlay();  // Show next move annotation during SGF navigation

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

    std::atomic<int> updateFlag;  // Thread-safe: accessed from main thread and GameThread
    int currentProgram;
    bool showLastMoveOverlay;
    bool showNextMoveOverlay;
    bool tsumegoMode = false;
    Position lastMove;
    std::vector<Position> navOverlays; // Positions of navigation overlays (next move previews, supports branches)
    std::vector<Position> markupOverlays; // Positions of SGF markup annotations (LB/TR/SQ/CR/MA)
    AudioPlayer player;

    void clearView();
};


#endif //GOBAN_GOBANVIEW_H
