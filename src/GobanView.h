/** \file
 *  \brief Rendering, camera and sound — the GameObserver on the UI side.
 *
 * Turns board changes into repaint flags, overlays and stone sounds, and holds
 * the authoritative camera state (`cameraPan`, `cameraDistance`, `cam.rLast`),
 * which `boardCoordinate()` replicates on the C++ side for screen-to-board ray
 * casting. The board itself is ray traced in the fragment shaders that
 * GobanShader drives; the coordinate system is documented in CLAUDE.md.
 *
 * UI thread only — it holds the OpenGL context. Its observer callbacks arrive
 * from the game thread, so they may do no more than raise `updateFlag` (atomic
 * for that reason) and copy plain data. Rendering is event driven: a change
 * nobody flags is a change nobody draws, because the main loop is otherwise
 * blocked in glfwWaitEvents().
 */
#ifndef GOBAN_GOBANVIEW_H
#define GOBAN_GOBANVIEW_H

#include <string>
#include <atomic>
#include <optional>
#include "GobanOverlay.h"
#include <RmlUi/Core/Types.h>
#include "GobanShader.h"
#include "GobanModel.h"
#include "GameObserver.h"
#include "Camera.h"
#include "GameState.h"
#include "Configuration.h"
#include "AudioPlayer.hpp"
#include "AnalysisService.h"


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
    void saveView();          // Save current camera to preset (user-triggered)
    void saveCurrentView();   // Save current camera for session restore (auto on exit)
    void shadeIt(float time, const GobanShader &shader, int flags) const;

    // Smooth camera transition via quaternion slerp + pan/distance lerp
    void animateCamera(const DDG::Quaternion& targetRotation,
                       const glm::vec2& targetPan, float targetDistance,
                       float duration = 0.6f);

    void animateIntro();

    void requestRepaint(int what = UPDATE_SOME);
    bool needsRender() const { return updateFlag != UPDATE_NONE || animationRunning || cameraAnim.active; }

    // Scenario screenshots. The runner sets the path; the main loop captures
    // the back buffer right before the swap — the only moment a finished frame
    // exists — and clears the request. All on the UI thread.
    void requestScreenshot(const std::string& path) {
        pendingScreenshot = path;
        requestRepaint(UPDATE_ALL);
    }
    [[nodiscard]] bool screenshotPending() const { return !pendingScreenshot.empty(); }
    std::string takeScreenshotRequest() {
        std::string path = pendingScreenshot;
        pendingScreenshot.clear();
        return path;
    }
    void stopAudioIfInactive() { player.stopIfInactive(); }
    void playSound(const std::string& id, double volume = 1.0) { player.play(id, volume); }
    bool toggleLastMoveOverlay();
    bool toggleNextMoveOverlay();
    void setTsumegoMode(bool enabled);
    bool isTsumegoMode() const { return tsumegoMode; }

    void zoomToStones();

    void Update();
    void moveCursor(float, float);
    void updateCursor();
    void updateLastMoveOverlay();
    void updateNavigationOverlay();  // Show next move annotation during SGF navigation

    /// Draws the engine's top moves, coloured by how much win rate they give up
    /// against its best (ADR-0007 phase 2). Runs immediately after
    /// updateNavigationOverlay() so it can tint the labels that pass just wrote:
    /// where a suggestion is a move already in the record, the record's own
    /// "3a" stays and only takes on colour.
    void updateAnalysisOverlay();

    /// The evaluation as text on the wood, in the margin nearest the camera,
    /// instead of in the RmlUi panel. The panel is small, overlays a board that
    /// took some trouble to render, and is the one piece of the interface not in
    /// the scene; this is the experiment in putting it there.
    ///
    /// Bottom edge because it is the only strip independent of the shader — all
    /// four variants draw the same wood there, and it is empty in every one.
    void updateEvaluationReadout();
    bool toggleEvaluationOnBoard();
    [[nodiscard]] bool isEvaluationOnBoard() const { return showEvaluationOnBoard; }
    void setEvaluationOnBoard(bool shown);
    /// What the board readout currently says, empty when it says nothing. The
    /// glyphs themselves are unreachable from a headless run, so this is the
    /// only way a scenario can check that the text was composed correctly —
    /// which colour leads, and how the score is written.
    [[nodiscard]] const std::string& evaluationReadoutText() const { return readoutText; }

    /// Where the readout sits along the board edge. An aesthetic choice, so it
    /// is offered rather than decided: centred under the board, or tucked
    /// against the left or right grid line.
    void setEvaluationAlign(TextAlign align);
    [[nodiscard]] TextAlign evaluationAlign() const { return readoutAlign; }
    static const char* alignName(TextAlign align);
    /// Parses a name back, returning nullopt for anything else so a caller can
    /// tell a typo from a choice.
    static std::optional<TextAlign> parseAlign(const std::string& name);

    /// Off by default, and that is the point. The panel's numbers are read
    /// *after* a move, so a player can invent their own and judge it. Stars on
    /// the board are read *before*, and once the engine has pointed at a point
    /// you cannot un-see it — the freedom to choose your own is gone from the
    /// first move. Two different features, separately switchable.
    bool toggleAnalysisOverlay();
    [[nodiscard]] bool isAnalysisOverlayShown() const { return showAnalysisOverlay; }
    void setAnalysisOverlay(bool shown);

    /// Where the suggestions come from. Set once by ElementGame; the view reads
    /// the published report exactly as it reads GobanModel::snapshot(), so
    /// goban_core stays unaware of the renderer.
    void setAnalysisService(const AnalysisService* service) { analysis = service; }

    /// Suggestions this overlay drew a label for, and suggestions that landed on
    /// a label somebody else wrote and were only tinted. Counted separately for
    /// `dumpState()`, because the difference between them *is* the combine rule.
    [[nodiscard]] size_t analysisLabelCount() const { return analysisLabels.size(); }
    [[nodiscard]] size_t analysisTintCount() const { return analysisTints.size(); }

public:
    GobanShader gobanShader;
    GobanOverlay gobanOverlay;
    GobanModel& model;

    bool MAX_FPS;
    int WINDOW_WIDTH = 0, WINDOW_HEIGHT = 0;
    float VIEWPORT_WIDTH, VIEWPORT_HEIGHT;
    static constexpr float FOCAL_LENGTH = 3.0f; // must match vertex shaders
    glm::vec2 cameraPan{0.0f, 0.0f};          // board-plane look-at point (x, z)
    glm::vec2 baseCameraPan{0.0f, 0.0f};      // committed pan (drag baseline)
    float cameraDistance = 3.5f;               // distance from camera to board
    float baseCameraDistance = 3.5f;            // committed distance (drag baseline)
    glm::vec2 resolution;
    float lastTime, startTime;
    bool animationRunning;
    bool isPanning, isZooming, isRotating;
    DDG::Camera cam;
    float startX, startY, lastX, lastY;

    Board board;

    struct CameraAnimation {
        DDG::Quaternion startRotation;
        DDG::Quaternion targetRotation;
        glm::vec2 startCameraPan{};
        glm::vec2 targetCameraPan{};
        float startCameraDistance = 0;
        float targetCameraDistance = 0;
        float startTime = 0;
        float duration = 0.6f;
        bool active = false;
    } cameraAnim;

    GameState state;

    std::atomic<int> updateFlag;  // Thread-safe: accessed from main thread and GameThread
    int currentProgram;
    bool showLastMoveOverlay;
    bool showNextMoveOverlay;
    bool tsumegoMode = false;
    Position lastMove;
    std::vector<Position> navOverlays; // Positions of navigation overlays (next move previews, supports branches)
    std::vector<Position> markupOverlays; // Positions of SGF markup annotations (LB/TR/SQ/CR/MA)
    bool showAnalysisOverlay = false;
    bool showEvaluationOnBoard = false;
    TextAlign readoutAlign = TextAlign::Center;
    std::string readoutText;
    const AnalysisService* analysis = nullptr;
    /// Kept apart because they are undone differently: a label this overlay
    /// added is removed, whereas a point it merely tinted belongs to somebody
    /// else and only has its colour reset.
    std::vector<Position> analysisLabels;
    std::vector<Position> analysisTints;
    AudioPlayer player;
    std::string pendingScreenshot;  // see requestScreenshot()

    void clearView();
};


#endif //GOBAN_GOBANVIEW_H
