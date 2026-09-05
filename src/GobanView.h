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

#include <chrono>
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
#include "StereoComposite.h"


extern std::shared_ptr<Configuration> config;

/// When the board draws its own pointer instead of the window system's.
///
/// `Auto` is where the native pointer is genuinely *wrong* — under a stereo
/// shader, where it is composited at the screen plane and so cannot be at the
/// depth of the point it indicates. `Always` extends it to mono, where the
/// native pointer is merely ordinary and a mark on the wood is nicer.
enum class PointerMode { Auto, Always, Never };

const char* pointerModeName(PointerMode mode);
std::optional<PointerMode> parsePointerMode(const std::string& name);

/// When the prisoner counts are drawn in the margin. Same three states as
/// PointerMode, and for the same reason: `Auto` means "where the real thing is
/// missing", which here is a shader that draws no bowls — four of the six
/// shipped ones, on which the captures were invisible.
enum class PrisonerMode { Auto, Always, Never };

const char* prisonerModeName(PrisonerMode mode);
std::optional<PrisonerMode> parsePrisonerMode(const std::string& name);

// WaitKind, waitKindName() and the blink curve; see WaitIndicator.h for why the
// two game waits are drawn on the board rather than written in #lblStatus.
#include "WaitIndicator.h"

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
        saveShaderSettings();
    }

    float getContrast() const {
		return gobanShader.getContrast();
    }

    bool toggleFpsLimit() {MAX_FPS = !MAX_FPS; return MAX_FPS;}
    bool isFpsLimitEnabled() const { return MAX_FPS; }

    void switchShader(int idx);

    /// Take delivery of a shader linked in the background, if one is ready.
    /// Returns true on the frame it became current.
    ///
    /// Called from ElementGame::OnUpdate() **before** its readiness gate, since
    /// this is the thing that opens that gate — it used to sit at the end of
    /// Update(), which OnUpdate only reaches once the view is already ready, so
    /// the build could never be adopted at all.
    bool takeShaderBuild();

    [[nodiscard]] Position getBoardCoordinate(float x, float y)const ;
    [[nodiscard]] glm::vec2 boardCoordinate(float x, float y) const;

    /// The camera model the vertex shaders build — origin plus the right, up and
    /// forward axes — in one place. It was written out longhand in three
    /// functions here, each claiming in a comment to match the other two, which
    /// is the arrangement that eventually disagrees.
    struct CameraBasis {
        glm::vec3 roo;   ///< Camera position (the centre camera, not either eye)
        glm::vec3 cu;    ///< Right
        glm::vec3 cv;    ///< Up
        glm::vec3 cw;    ///< Forward, toward the board
    };
    [[nodiscard]] CameraBasis cameraBasis() const;

    /// Depth range of the board box alone, along the view axis. Unlike
    /// stereoNearPoint() the table is not consulted: this exists to make
    /// stereoConvergence() readable, and "the screen plane is at the back of the
    /// board" is a claim about exactly these two numbers.
    void stereoBoardDepth(float& nearZ, float& farZ) const;

    /// Depth of the zero-parallax plane — where the scene meets the glass.
    /// Everything nearer is in front of the screen, everything further behind.
    /// This is what `dof` decides; see Stereo::convergence(). Reported by
    /// dumpState() because there was no way to ask it, and so no way to check a
    /// suspicion about it except by looking at a screenshot through glasses.
    [[nodiscard]] float stereoConvergence() const;

    /// Depth of the nearest thing in frame, along the view axis. The board is a
    /// fixed-size box and the table runs under it toward the viewer, so both are
    /// asked: the board binds at ordinary zoom, the table's bottom edge binds
    /// once the camera pulls back — "the blades of grass at the bottom edge" the
    /// stereo literature warns about (see Stereo.h).
    [[nodiscard]] float stereoNearPoint() const;

    /// The deviation the current camera actually produces — the near-to-far
    /// separation, as a fraction of image width. This is the number the stereo
    /// literature bounds at 1/30 (Stereo.h), and it is in dumpState() so a
    /// scenario can sweep the zoom range and assert it stays there: the old
    /// rule passed at one zoom and failed at another, which is precisely what
    /// an eyeball check does not catch.
    [[nodiscard]] float stereoDeviation() const;

    /// Half the stereo base, in world units, for the current camera. The one
    /// implementation: uploaded to the vertex shader as `eof` and used by
    /// GobanOverlay to place the same two eyes. Splitting it would let the text
    /// drift off the wood it is supposed to be lying on.
    [[nodiscard]] float stereoHalfBase() const;

    /// The horizontal image shift — the stereoscopic window — for the current
    /// camera and aspect ratio. Derived, exactly as the base above is: it rests
    /// on the near point unless the user has pushed it off with `dof`. The one
    /// implementation again, and for the same reason — the board and the
    /// overlay's text must sit in the same window or the labels leave the wood.
    [[nodiscard]] float stereoWindow() const;

    /// Persist the four appearance settings that are *not* camera state: the
    /// stereo base and window, gamma and contrast. Called by the commands that
    /// change them, so they behave like every other display setting here
    /// (`anaglyph`, `pointer`, the evaluation toggles) — sticky the moment they
    /// change. They used to be written only by `save camera` and re-read by
    /// `reset camera`, so tuning an anaglyph and then reframing the board
    /// silently undid the tuning.
    void saveShaderSettings();

    void resetView();
    void saveView();          // Save current camera to preset (user-triggered)
    void saveCurrentView();   // Save current camera for session restore (auto on exit)
    void shadeIt(float time, const GobanShader &shader, int flags, int eye = 0) const;

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
    /// Sounds actually mixed to their end, for dumpState() and scenarios.
    [[nodiscard]] unsigned long long soundsPlayed() const { return player.completedPlaybacks(); }
    /// Text items the glyph pass put on screen in the last frame that ran it.
    /// Every other overlay key — `coordinates_shown`, `eval_labels`,
    /// `markup_count` — describes what was *built*, and all of them stayed true
    /// through a bug that drew none of it. Same distinction as soundsPlayed().
    [[nodiscard]] unsigned overlayGlyphs() const { return overlayGlyphsDrawn.load(); }
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

    /// The evaluation as text on the wood, in the margin nearest the camera.
    ///
    /// This began as an experiment beside an RmlUi panel and is now the only
    /// place the evaluation is shown. The panel was small, overlaid a board that
    /// took some trouble to render, and was the one piece of the interface not
    /// in the scene — so once the board version worked, keeping a choice between
    /// them meant shipping the worse one as the default. There is no placement
    /// setting any more: the evaluation is on the board, or it is off.
    ///
    /// Bottom edge because it is the only strip independent of the shader — all
    /// four variants draw the same wood there, and it is empty in every one.
    /// Every label placed by board coordinate rather than by board point: the
    /// evaluation readout, the wait indicator and, when shown, the coordinate
    /// labels. One function because setFloatingLabels() replaces the whole list.
    void promoteHintLabel();
    void updateFloatingLabels();

    /// The numbers on the wood — win rate and score estimate. Separate from
    /// `toggle_evaluation`, which decides whether the analysis engine runs at
    /// all: with one switch for both, the only way to see the suggested moves
    /// without the numbers was to turn the engine off, which also took the
    /// moves. This is a *visibility* setting and not the placement setting
    /// ADR-0012 removed — there is still exactly one place the evaluation is
    /// drawn; this says whether that part of it is drawn.
    bool toggleEvaluationReadout();
    [[nodiscard]] bool isEvaluationReadoutShown() const { return showReadout; }
    void setEvaluationReadout(bool shown);

    /// The elapsed-seconds clock in the margin. On by default and deliberately
    /// so: it exists because a genmove used to be completely silent, and a user
    /// who hides it is choosing to give that back. Sticky, like its siblings.
    bool toggleWaitClock();
    [[nodiscard]] bool isWaitClockShown() const { return showWaitClock; }
    void setWaitClock(bool shown);
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

    /// How the two eyes are combined into the anaglyph. Same two-level
    /// arrangement as the readout ink: `config/base.json` ships the default,
    /// `user.json` carries a choice the user made, and the `anaglyph` command
    /// sets it live — which is the point, since the only way to pick between
    /// these is to look at the board through the glasses.
    ///
    /// Not per shader. Ink is, because a shader could redefine the board dark
    /// (ADR-0011); this cannot, because every stereo shader here is red/cyan and
    /// the choice is about the *glasses and the eyes behind them*, which do not
    /// change when the board does.
    void setAnaglyph(Stereo::Anaglyph mode);
    [[nodiscard]] Stereo::Anaglyph anaglyph() const { return anaglyphMode; }

    /// How much of each eye's own colour survives against its plain brightness.
    /// The cheap half of coping with imperfect glasses: ghosting and rivalry
    /// both scale with how different the two eyes' images are.
    void setAnaglyphStrength(float strength);
    [[nodiscard]] float anaglyphStrength() const { return anaglyphColorStrength; }

    /// Per-channel crosstalk cancellation, tuned for the glasses actually in
    /// use. See Stereo::Crosstalk for why it is subtracted where it is.
    void setAnaglyphLeak(const Stereo::Crosstalk& leak);
    [[nodiscard]] const Stereo::Crosstalk& anaglyphLeak() const { return anaglyphCrosstalk; }

    /// Per-eye gain. The one adjustment that helps red/blue glasses, where the
    /// colour modes cannot: a blue filter is much darker than a red one, and the
    /// eyes do not average that mismatch away.
    void setAnaglyphBalance(const Stereo::EyeBalance& balance);
    [[nodiscard]] const Stereo::EyeBalance& anaglyphBalance() const { return anaglyphEyeBalance; }

    /// Which channels reach which eye. The single most consequential setting
    /// here: get it wrong and one eye receives the other's image, which is a
    /// second picture rather than a wrong colour.
    void setGlasses(Stereo::Glasses g);
    [[nodiscard]] Stereo::Glasses glasses() const { return glassesType; }

    /// Whether the pointer is over the board surface rather than over a widget,
    /// which is what decides both halves of the diegetic pointer: the mark is
    /// drawn, and the native pointer is hidden.
    ///
    /// Positional on purpose. Over the RmlUi interface the native pointer is
    /// *correct* — that interface is itself flat at the screen plane, so a
    /// zero-parallax pointer agrees with it. The mismatch exists only over the
    /// ray-traced board, so that is the only place it is taken away.
    /// The half of the answer only RmlUi knows: a widget is under the pointer,
    /// so it is not on the wood however the ray falls. The other half — whether
    /// the ray lands on an intersection — is settled in moveCursor(), which is
    /// what keeps the gate in step with the mouse rather than a frame behind it.
    void setPointerOnWidget(bool onWidget);

    /// Strength of the mark the shader draws on the wood; 0 draws nothing.
    /// Stereo only — in mono the native pointer already sits where it points,
    /// and a second indicator would be clutter rather than help.
    /// Whether the board is drawing the pointer at all, and so whether the
    /// native one is taken away. Both the mark *and* the stone in hand count:
    /// either way something on the wood is showing where the click will land, so
    /// this is the one predicate the hiding decision reads.
    [[nodiscard]] bool diegeticPointer() const {
        if (pointerMode == PointerMode::Never || pointerMarkStrength <= 0.0f) return false;
        if (!pointerOverBoard) return false;
        // Auto means "where the native pointer is actually wrong", which is a
        // stereo shader. It is the default rather than the only option: the
        // drawn pointer turns out to be worth having in mono too, so `always`
        // exists — but switching it on there also takes the native pointer away,
        // and that is a change nobody asked for by default.
        return pointerMode == PointerMode::Always || gobanShader.isStereo();
    }

    /// Strength of the mark the shader draws on the wood; 0 draws nothing.
    ///
    /// Two indicators for one point is one too many, so it stands down wherever
    /// the stone in hand is already showing where the click will land. It does
    /// *not* stand down merely because a stone is held: on a point the rules
    /// refuse there is no ghost stone, and with the native pointer hidden that
    /// would leave no pointer at all — so its presence there says "not here",
    /// which is the thing worth knowing.
    [[nodiscard]] float pointerMark() const {
        if (!diegeticPointer()) return 0.0f;
        return ghostStoneVisible() ? 0.0f : pointerMarkStrength;
    }

    /// Whether the stone in hand is being drawn at the cursor. One predicate,
    /// asked by updateCursor() when it places the ghost and by pointerMark()
    /// when it decides to stay out of the way — a second copy would let the two
    /// disagree, and then both would be drawn.
    [[nodiscard]] bool ghostStoneVisible() const;

    void setPointerMode(PointerMode mode);
    [[nodiscard]] PointerMode pointerMode_() const { return pointerMode; }

    void setPrisonerMode(PrisonerMode mode);
    [[nodiscard]] PrisonerMode prisonerMode_() const { return prisonerMode; }
    /// Whether the counts belong in the margin at all. `Auto` asks the shader:
    /// Red Carpet already shows the prisoners as a pile in its bowls, so a
    /// number beside it would be the same fact twice, while the Minimal scenes
    /// show nothing at all.
    [[nodiscard]] bool showPrisonerCounts() const {
        if (prisonerMode == PrisonerMode::Never)  return false;
        if (prisonerMode == PrisonerMode::Always) return true;
        return !gobanShader.drawsBowls();
    }
    /// What the glyph pass last drew, for dumpState(). Both set whenever the
    /// counts are shown at all, a zero included — empty only means the mode or
    /// the shader is keeping them off. See updateFloatingLabels().
    [[nodiscard]] const std::string& prisonerTextBlack() const { return prisonerTextB; }
    [[nodiscard]] const std::string& prisonerTextWhite() const { return prisonerTextW; }

    /// How much of the green channel the colour modes use. The dial for lenses
    /// that pass green through *both* filters, where no assignment of green to an
    /// eye is clean and the only remaining lever is how much of it there is.
    void setAnaglyphGreen(float green);
    [[nodiscard]] float anaglyphGreen() const { return anaglyphGreenLevel; }

    /// The readout's ink. Alpha included, because half-opacity dark is what makes
    /// text read as *part of* a wooden board rather than printed on top of it —
    /// and that is the whole claim the diegetic version is making.
    ///
    /// Shipped default in `config/base.json` (`overlay.readout_color`), which the
    /// application never writes; a user's own choice lives in `user.json`. Same
    /// two-level arrangement as the camera, and for the same reason.
    void setReadoutColor(const glm::vec4& color);
    void setReadoutStaleColor(const glm::vec4& color);
    /// The coordinate labels' ink, same two-level arrangement as the readout's:
    /// `annotations.coordinate_color` ships it, `coordinate_color` overrides it.
    void setCoordinateColor(const glm::vec4& color);
    [[nodiscard]] const glm::vec4& coordinateColor() const { return coordinateInk; }

    /// How far the coordinate labels sit out into the margin, in grid spacings
    /// from the outermost line. The wood extends 0.85, so 0.425 centres them in
    /// it; past about 0.7 the glyphs start to hang off the edge, and a full 1.0
    /// — where the next grid line would fall — needs a wider margin than the
    /// board has (`Metrics::calc()`'s 0.85).
    void setCoordinateOffset(float spacings);
    [[nodiscard]] float coordinateOffset() const { return coordOffset; }
    static constexpr float MAX_SAFE_COORD_OFFSET = 0.7f;
    [[nodiscard]] const glm::vec4& readoutColor() const { return readoutInk; }

    /// What the board is waiting for, told to it once per frame by
    /// ElementGame — which is where the two conditions are already computed, so
    /// nothing here has to interrogate the game thread.
    ///
    /// Idempotent: passing the same kind again leaves the elapsed clock running.
    /// Changing kind, or arriving from None, restarts it. While a wait is
    /// current this asks for an overlay repaint whenever the displayed count
    /// changes — `getIdleTimeout()` already wakes the main loop at 20 Hz for
    /// exactly these two states, but a wake draws nothing unless the view is
    /// dirty, and identical frames are not worth drawing.
    void setWaitIndicator(WaitKind kind);
    [[nodiscard]] WaitKind waitIndicator() const { return waitKind; }

    /// What the indicator currently reads, empty when it is not shown. The
    /// glyphs themselves are unreachable from a headless run, so — exactly as
    /// with the evaluation readout — this is the only way a scenario can assert
    /// that the thing was composed and placed at all.
    [[nodiscard]] const std::string& waitIndicatorText() const { return waitText; }

    /// Column letters along the top margin and row numbers down the left, in
    /// the same 0.85-spacing strip of wood the readout uses at the bottom.
    /// Fixed to those two edges whether or not the readout is on, so nothing
    /// ever moves and they can never collide.
    /// The two move markers. Neither is persisted, so both start on in a fresh
    /// process — which is what makes a bare toggle deterministic in a script.
    /// Exposed so a scenario can say so rather than assume it.
    [[nodiscard]] bool isLastMoveOverlayShown() const { return showLastMoveOverlay; }
    [[nodiscard]] bool isNextMoveOverlayShown() const { return showNextMoveOverlay; }

    bool toggleCoordinates();
    [[nodiscard]] bool areCoordinatesShown() const { return showCoordinates; }
    void setCoordinates(bool shown);

    /// When the engine's suggestions are drawn (ADR-0014).
    ///
    /// `Always` is what the old boolean `true` meant, and it is read *before* a
    /// decision — once the engine has pointed at a point you cannot un-see it.
    /// `OnDemand` withholds them until the player has aimed at a point long
    /// enough to have chosen it, which turns the same display from a substitute
    /// for thinking into a check on it.
    /// The order matches the three `<option>`s of #selectEvaluationMoves, which
    /// is what the menu sync casts through.
    enum class HintMode { Off, OnDemand, Always };

    bool toggleAnalysisOverlay();
    // hintModeName / parseHintMode are free functions below the class.
    /// Whether the suggestions are on screen *now* — the mode, plus whether an
    /// on-demand reveal is currently open. The renderer asks only this.
    [[nodiscard]] bool isAnalysisOverlayShown() const { return showAnalysisOverlay; }
    void setAnalysisOverlay(bool shown);
    [[nodiscard]] HintMode analysisOverlayMode() const { return hintMode; }
    void setAnalysisOverlayMode(HintMode mode);

    /// True while a stone is held over a point and the dwell has not yet
    /// elapsed — the loop must keep waking, or no frame is drawn and the reveal
    /// never happens.
    [[nodiscard]] bool hintDwellPending() const;

    /// Where the suggestions come from. Set once by ElementGame; the view reads
    /// the published report exactly as it reads GobanModel::snapshot(), so
    /// goban_core stays unaware of the renderer.
    void setAnalysisService(const AnalysisService* service) { analysis = service; }

    /// Suggestions this overlay drew a label for, and suggestions that landed on
    /// a label somebody else wrote and were only tinted. Counted separately for
    /// `dumpState()`, because the difference between them *is* the combine rule.
    [[nodiscard]] size_t analysisLabelCount() const { return analysisLabels.size(); }
    [[nodiscard]] size_t analysisTintCount() const { return analysisTints.size(); }
    /// The margin's "pass", or empty when the engine is not recommending one.
    [[nodiscard]] const std::string& analysisPassText() const { return passSuggestion; }

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
    /// Written by Render() on the UI thread, read by dumpState() — which the
    /// scenario runner may call from anywhere between frames, hence atomic.
    std::atomic<unsigned> overlayGlyphsDrawn{0};
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

    /// What this view last handed the shader — the uniforms deciding how many
    /// stones are drawn in the bowls. **Not the prisoner count**: that is model
    /// data and lives in GameSnapshot. This is the per-view record of what has
    /// been drawn, so a second view of the same model keeps its own and
    /// ElementGame::OnUpdate() can still do nothing when nothing has changed.
    ///
    /// The two roles used to share one field. GameState::capturedBlack was
    /// serving as both the model's count and this shadow, and the model half was
    /// never assigned anywhere — so the bowls stayed empty and the prisoner
    /// labels read 0 for the whole of every game, while Board had the right
    /// number all along.
    int capturedBlackShown = 0;
    int capturedWhiteShown = 0;

    std::atomic<int> updateFlag;  // Thread-safe: accessed from main thread and GameThread

    /// A board size handed over by onBoardSized(), waiting for the UI thread to
    /// act on it. -1 means nothing pending.
    ///
    /// The callback arrives on the game thread — and during startup on the
    /// engine-loader thread, while the board is already being drawn. It used to
    /// do the work itself: `board.clear()`, which assigns a std::string into
    /// every one of 361 points, and `.clear()` on four std::vectors that
    /// `updateNavigationOverlay()` walks once per repaint. That is the file
    /// comment's own rule broken ("may do no more than raise `updateFlag` and
    /// copy plain data"), and on a vector it is an invalidated iterator rather
    /// than a stale value.
    std::atomic<int> pendingBoardSize{-1};

    /// Applies a pending onBoardSized(). UI thread only, and idempotent.
    ///
    /// Called from Update() — and explicitly from
    /// GobanControl::finishGameReplacement(), which rebuilds the overlays
    /// *before* the next Update() would run. Doing it only in Update() would
    /// clear the overlays that had just been rebuilt.
    void applyPendingResize();
    int currentProgram;
    bool showLastMoveOverlay;
    bool showNextMoveOverlay;
    bool tsumegoMode = false;
    Position lastMove;
    std::vector<Position> navOverlays; // Positions of navigation overlays (next move previews, supports branches)
    std::vector<Position> markupOverlays; // Positions of SGF markup annotations (LB/TR/SQ/CR/MA)
    /// The effective "draw them now" flag, derived from hintMode and
    /// hintRevealed. Kept as the single thing the renderer reads.
    bool showAnalysisOverlay = false;
    HintMode hintMode = HintMode::Off;
    /// An on-demand reveal is open. Set when the dwell elapses; cleared when the
    /// stone is placed or put back, so it survives looking around the board.
    bool hintRevealed = false;
    Position hintDwellPoint{-1, -1};
    std::chrono::steady_clock::time_point hintDwellSince;
    void applyOverlayVisibility();
    void updateHintDwell();

    bool showCoordinates = false;
    glm::vec4 coordinateInk{0.0f, 0.0f, 0.0f, 1.0f};
    float coordOffset = 0.425f;
    Stereo::Anaglyph anaglyphMode = Stereo::Anaglyph::Gray;
    float anaglyphColorStrength = Stereo::DEFAULT_STRENGTH;
    Stereo::Crosstalk anaglyphCrosstalk{};
    Stereo::EyeBalance anaglyphEyeBalance{};
    Stereo::Glasses glassesType = Stereo::Glasses::RedCyan;
    float anaglyphGreenLevel = Stereo::DEFAULT_GREEN;
    float pointerMarkStrength = 1.0f;
    bool pointerOverBoard = false;
    bool pointerOnWidget = false;
    PointerMode pointerMode = PointerMode::Auto;
    PrisonerMode prisonerMode = PrisonerMode::Auto;
    /// Ink for the two counts: the colour of the stones *counted*, which is what
    /// the bowls show — black stones sitting in White's bowl. It survives a
    /// stereo shader, where eyeInk() flattens every label to its own brightness
    /// and black against white is exactly a brightness difference.
    glm::vec4 prisonerInkBlack{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 prisonerInkWhite{1.0f, 1.0f, 1.0f, 1.0f};
    std::string prisonerTextB, prisonerTextW;
    void updatePointerState();
    /// Mirrors the GLFW input mode, so it is set on change rather than per frame.
    bool nativePointerHidden = false;
    /// Only used when the composite can go negative; see StereoComposite.
    mutable StereoComposite stereoComposite;
    TextAlign readoutAlign = TextAlign::Center;
    glm::vec4 readoutInk{0.0f, 0.0f, 0.0f, 1.0f};
    /// Ink for a readout describing a position that has been left. Follows
    /// readoutInk unless the config names one, so the feature is invisible until
    /// somebody asks for it.
    glm::vec4 readoutStaleInk{0.0f, 0.0f, 0.0f, 1.0f};
    bool haveStaleInk = false;
    std::string readoutText;

    /// The wait indicator: which wait, since when, and what it last read.
    WaitKind waitKind = WaitKind::None;
    float waitStarted = 0.0f;
    std::string waitText;
    bool showReadout = true;
    bool showWaitClock = true;
    glm::vec4 waitInk{0.0f, 0.0f, 0.0f, 1.0f};
    /// How long a wait has to last before it is worth mentioning. GNU Go answers
    /// in milliseconds; without this every move in a bot match flashes the mark
    /// for one frame.
    float waitGrace = 0.5f;
    /// The count and the mark state last drawn, so a frame is asked for when
    /// either changes and not once per idle tick. Wait::NOT_SHOWN while inside
    /// the grace period.
    int waitSecondShown = Wait::NOT_SHOWN;

    const AnalysisService* analysis = nullptr;
    /// Kept apart because they are undone differently: a label this overlay
    /// added is removed, whereas a point it merely tinted belongs to somebody
    /// else and only has its colour reset.
    std::vector<Position> analysisLabels;
    std::vector<Position> analysisTints;
    /// A suggested pass, handed from updateAnalysisOverlay() — which is where
    /// the report is read — to updateFloatingLabels(), which owns the margin.
    /// The two already run in that order, and the labels list is replaced
    /// wholesale, so it cannot be written from both.
    std::string passSuggestion;
    glm::vec4 passSuggestionInk{0.0f, 0.0f, 0.0f, 1.0f};
    AudioPlayer player;
    std::string pendingScreenshot;  // see requestScreenshot()

    void clearView();
};

/// The names `evaluation_moves` takes in user.json and on the command line.
/// `on`/`off` are still accepted: they were the whole vocabulary before the
/// third state, and both scenarios and keybindings use them.
const char* hintModeName(GobanView::HintMode mode);
std::optional<GobanView::HintMode> parseHintMode(std::string name);

#endif //GOBAN_GOBANVIEW_H
