#include "GobanView.h"
#include <GLFW/glfw3.h>
#include "AudioPlayer.hpp"
#include "AppState.h"
#include "ElementGame.h"

#include <cmath>
#include <limits>
#include <cstdio>
#include <iomanip>
#include <set>
#include <sstream>
#include "Stereo.h"
#include "UserSettings.h"

GobanView::GobanView(GobanModel& m)
    :
    gobanShader(*this), gobanOverlay(*this), model(m), MAX_FPS(false), VIEWPORT_WIDTH(0), VIEWPORT_HEIGHT(0),
    resolution(1024.0, 768.0), lastTime(0.0f),
    startTime(0.0f), animationRunning(false), isPanning(false), isZooming(false), isRotating(false),
    cam(1.0, 0.0, 0.0, 0.0), startX(0), startY(0), lastX(.0f), lastY(.0f), updateFlag(0),
    currentProgram(-1),	showLastMoveOverlay(true), showNextMoveOverlay(true)
{
    player.preload(config);
    player.init();

    initCam();

    // Where the view starts, most specific first: where the user left it, then
    // the preset they saved, then the default shipped in the application config.
    // The last of these is why a fresh install has a considered opening view
    // without user.json needing to exist — see UserSettings::setDefaultCamera.
    auto& settings = UserSettings::instance();
    CameraState camToRestore;
    bool haveCamera = true;
    if (settings.hasCurrentCamera()) {
        camToRestore = settings.getCurrentCamera();
    } else if (settings.hasSavedCamera()) {
        camToRestore = settings.getSavedCamera();
    } else if (settings.hasDefaultCamera()) {
        camToRestore = settings.getDefaultCamera();
    } else {
        haveCamera = false;
    }
    if (haveCamera) {
        cam.rLast[0] = camToRestore.rotX;
        cam.rLast[1] = camToRestore.rotY;
        cam.rLast[2] = camToRestore.rotZ;
        cam.rLast[3] = camToRestore.rotW;
        cam.rLast.normalize();
        cameraPan = glm::vec2(camToRestore.panX, camToRestore.panY);
        cameraDistance = camToRestore.distance;
        baseCameraPan = cameraPan;
        baseCameraDistance = cameraDistance;
    }

    // The shipped default for the board readout's ink. base.json is where a
    // default that ships belongs; user.json is the runtime scratchpad, and
    // ElementGame applies the user's own choice over this if they made one.
    if (config) {
        const std::string configured = config->data
                .value("annotations", nlohmann::json::object())
                .value("readout_color", std::string());
        if (!configured.empty()) {
            if (const auto parsed = parseHexColor(configured)) {
                readoutInk = *parsed;
                readoutStaleInk = *parsed;
            } else {
                spdlog::warn("annotations.readout_color: '{}' is not #rgb, #rrggbb "
                             "or #rrggbbaa", configured);
            }
        }
        const std::string coordConfigured = config->data
                .value("annotations", nlohmann::json::object())
                .value("coordinate_color", std::string());
        if (!coordConfigured.empty()) {
            if (const auto parsed = parseHexColor(coordConfigured)) {
                coordinateInk = *parsed;
            } else {
                spdlog::warn("annotations.coordinate_color: '{}' is not #rgb, "
                             "#rrggbb or #rrggbbaa", coordConfigured);
            }
        }
        const auto annotations = config->data.value("annotations", nlohmann::json::object());
        if (annotations.contains("coordinate_offset")) {
            const float offset = annotations.value("coordinate_offset", 0.425f);
            if (offset > MAX_SAFE_COORD_OFFSET) {
                spdlog::warn("annotations.coordinate_offset {} puts the labels off "
                             "the wood; the margin is 0.85 spacings wide", offset);
            }
            coordOffset = offset;
        }
        // The wait indicator's ink follows the readout's unless it is given one
        // of its own — same two-level arrangement, and the same reason: these
        // are all annotation ink on the same wood, and a default that differs
        // from its neighbours by accident looks like a bug.
        waitInk = readoutInk;
        const std::string waitConfigured = annotations.value("wait_color", std::string());
        if (!waitConfigured.empty()) {
            if (const auto parsed = parseHexColor(waitConfigured)) {
                waitInk = *parsed;
            } else {
                spdlog::warn("annotations.wait_color: '{}' is not #rgb, #rrggbb "
                             "or #rrggbbaa", waitConfigured);
            }
        }
        const std::string glyph = annotations.value("wait_glyph", std::string());
        if (!glyph.empty()) waitGlyph = glyph;
        waitGlyphSyncing = annotations.value("wait_glyph_syncing", std::string());
        if (annotations.contains("wait_blink_period")) {
            const float period = annotations.value("wait_blink_period", 1.0f);
            // Not a fade: this is how long one fully-printed / fully-absent
            // cycle takes. Below a fifth of a second it is a strobe.
            if (period < Wait::MIN_BLINK_PERIOD) {
                spdlog::warn("annotations.wait_blink_period {} is too fast to read; "
                             "keeping {}", period, waitBlinkPeriod);
            } else {
                waitBlinkPeriod = period;
            }
        }
        if (annotations.contains("wait_grace")) {
            const float grace = annotations.value("wait_grace", 0.5f);
            // Zero is meaningful — a scenario wants the mark on the first frame —
            // so only a negative one is refused.
            if (grace < 0.0f) {
                spdlog::warn("annotations.wait_grace {} is negative; keeping {}",
                             grace, waitGrace);
            } else {
                waitGrace = grace;
            }
        }

        const std::string staleConfigured = config->data
                .value("annotations", nlohmann::json::object())
                .value("readout_stale_color", std::string());
        if (!staleConfigured.empty()) {
            if (const auto parsed = parseHexColor(staleConfigured)) {
                readoutStaleInk = *parsed;
                haveStaleInk = true;
            } else {
                spdlog::warn("annotations.readout_stale_color: '{}' is not #rgb, "
                             "#rrggbb or #rrggbbaa", staleConfigured);
            }
        }
        // The shipped anaglyph mode. A name that will not parse leaves the
        // built-in Gray standing rather than falling to whatever `0` happens to
        // mean, the same degradation the ink above gets.
        const std::string configuredAnaglyph = config->data.value("anaglyph", std::string());
        if (!configuredAnaglyph.empty()) {
            if (const auto parsed = Stereo::parseAnaglyph(configuredAnaglyph)) {
                anaglyphMode = *parsed;
            } else {
                spdlog::warn("anaglyph: '{}' is not gray, half-color, color or dubois",
                             configuredAnaglyph);
            }
        }
        if (config->data.contains("anaglyph_strength")) {
            anaglyphColorStrength = Stereo::clampStrength(
                    config->data.value("anaglyph_strength", Stereo::DEFAULT_STRENGTH));
        }
        const auto leak = config->data.value("anaglyph_leak", nlohmann::json::array());
        if (leak.is_array() && leak.size() == 3) {
            anaglyphCrosstalk = Stereo::clampCrosstalk(
                    {leak[0].get<float>(), leak[1].get<float>(), leak[2].get<float>()});
        }
        const std::string configuredGlasses = config->data.value("glasses", std::string());
        if (!configuredGlasses.empty()) {
            if (const auto parsed = Stereo::parseGlasses(configuredGlasses)) {
                glassesType = *parsed;
            } else {
                spdlog::warn("glasses: '{}' is not red-cyan or red-blue", configuredGlasses);
            }
        }
        const std::string configuredPointer = config->data.value("pointer_mode", std::string());
        if (!configuredPointer.empty()) {
            if (const auto parsed = parsePointerMode(configuredPointer)) {
                pointerMode = *parsed;
            } else {
                spdlog::warn("pointer_mode: '{}' is not auto, always or never",
                             configuredPointer);
            }
        }
        if (config->data.contains("pointer_mark")) {
            pointerMarkStrength = std::min(1.0f, std::max(0.0f,
                    config->data.value("pointer_mark", 1.0f)));
        }
        if (config->data.contains("anaglyph_green")) {
            anaglyphGreenLevel = Stereo::clampGreen(
                    config->data.value("anaglyph_green", Stereo::DEFAULT_GREEN));
        }
        const auto balance = config->data.value("anaglyph_balance", nlohmann::json::array());
        if (balance.is_array() && balance.size() == 2) {
            anaglyphEyeBalance = Stereo::clampBalance(
                    {balance[0].get<float>(), balance[1].get<float>()});
        }
    }

    // ...and the user's own choice over it, since picking between these means
    // looking at the board through the glasses.
    if (const auto parsed = Stereo::parseAnaglyph(settings.getAnaglyph())) {
        anaglyphMode = *parsed;
    }
    if (settings.getAnaglyphStrength() >= 0.0f) {
        anaglyphColorStrength = Stereo::clampStrength(settings.getAnaglyphStrength());
    }
    if (const auto parsed = Stereo::parseGlasses(settings.getGlasses())) {
        glassesType = *parsed;
    }
    if (const auto parsed = parsePointerMode(settings.getPointerMode())) {
        pointerMode = *parsed;
    }
    if (settings.getAnaglyphGreen() >= 0.0f) {
        anaglyphGreenLevel = Stereo::clampGreen(settings.getAnaglyphGreen());
    }
    if (settings.hasAnaglyphBalance()) {
        Stereo::EyeBalance balance;
        settings.getAnaglyphBalance(balance.left, balance.right);
        anaglyphEyeBalance = Stereo::clampBalance(balance);
    }
    if (settings.hasAnaglyphLeak()) {
        // Asked for explicitly, so an all-zero answer is honoured rather than
        // read as "unset" — turning the correction off is a choice.
        Stereo::Crosstalk leak;
        settings.getAnaglyphLeak(leak.r, leak.g, leak.b);
        anaglyphCrosstalk = Stereo::clampCrosstalk(leak);
    }

    // Load shader settings (separate from camera)
    if (settings.hasShaderSettings()) {
        gobanShader.setEof(settings.getShaderEof());
        gobanShader.setDof(settings.getShaderDof());
        gobanShader.setGamma(settings.getShaderGamma());
        gobanShader.setContrast(settings.getShaderContrast());
    }

    // Load saved shader (or default to 0) — must happen before setReady()
    int shaderIdx = 0;
    if (settings.hasShaderSettings()) {
        std::string savedName = settings.getShaderName();
        auto shaders = config->data.value("shaders", nlohmann::json::array());
        for (int i = 0; i < static_cast<int>(shaders.size()); i++) {
            if (shaders[i].value("name", "") == savedName) {
                shaderIdx = i;
                break;
            }
        }
    }
    gobanShader.choose(shaderIdx);

    updateFlag |= GobanView::UPDATE_ALL;  // Ensure full render on startup
    gobanShader.setReady();
    gobanOverlay.setReady();

    // Sync initial state from model to prevent stale default values (e.g., reservoir counts)
    state.reservoirBlack = model.state.reservoirBlack;
    state.reservoirWhite = model.state.reservoirWhite;
    state.capturedBlack = model.state.capturedBlack;
    state.capturedWhite = model.state.capturedWhite;
    // Initialize colorToMove to EMPTY so first OnUpdate() will sync with model and update player toggle indicators
    state.colorToMove = Color::EMPTY;
}

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
    baseCameraPan = cameraPan;
    baseCameraDistance = cameraDistance;
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
    baseCameraPan = cameraPan;
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
    baseCameraDistance = cameraDistance;
}

void GobanView::resetView() {
    auto& settings = UserSettings::instance();

    DDG::Quaternion targetRot = cam.rLast;
    glm::vec2 targetPan = cameraPan;
    float targetDist = cameraDistance;

    // The user's own preset if they saved one, otherwise the shipped default.
    // Without the second branch, `reset camera` on a fresh install — or after
    // `delete camera` — reset to wherever the view already was, which looks like
    // a broken command.
    const bool haveSaved = settings.hasSavedCamera();
    if (haveSaved || settings.hasDefaultCamera()) {
        const CameraState preset = haveSaved ? settings.getSavedCamera()
                                             : settings.getDefaultCamera();
        targetRot[0] = preset.rotX;
        targetRot[1] = preset.rotY;
        targetRot[2] = preset.rotZ;
        targetRot[3] = preset.rotW;
        targetRot.normalize();
        targetPan = glm::vec2(preset.panX, preset.panY);
        targetDist = preset.distance;
    }

    if (settings.hasShaderSettings()) {
        gobanShader.setEof(settings.getShaderEof());
        gobanShader.setDof(settings.getShaderDof());
        gobanShader.setGamma(settings.getShaderGamma());
        gobanShader.setContrast(settings.getShaderContrast());
    }

    updateFlag |= UPDATE_SHADER;
    animateCamera(targetRot, targetPan, targetDist);
}

void GobanView::switchShader(int idx) {
    updateFlag |= GobanView::UPDATE_ALL;
    gobanShader.choose(idx);
    state.metricsReady = false;
    gobanShader.setReady();
    // Force OnUpdate to re-evaluate the game state message
    state.msg = GameState::NONE;
}

void GobanView::saveView() {
    auto& settings = UserSettings::instance();

    CameraState camState;
    camState.rotX = cam.rLast[0];
    camState.rotY = cam.rLast[1];
    camState.rotZ = cam.rLast[2];
    camState.rotW = cam.rLast[3];
    camState.panX = cameraPan.x;
    camState.panY = cameraPan.y;
    camState.distance = cameraDistance;
    settings.setSavedCamera(camState);

    settings.setShaderEof(gobanShader.getEof());
    settings.setShaderDof(gobanShader.getDof());
    settings.setShaderGamma(gobanShader.getGamma());
    settings.setShaderContrast(gobanShader.getContrast());

    settings.save();
}

void GobanView::saveCurrentView() {
    auto& settings = UserSettings::instance();

    CameraState camState;
    camState.rotX = cam.rLast[0];
    camState.rotY = cam.rLast[1];
    camState.rotZ = cam.rLast[2];
    camState.rotW = cam.rLast[3];
    camState.panX = cameraPan.x;
    camState.panY = cameraPan.y;
    camState.distance = cameraDistance;
    settings.setCurrentCamera(camState);
    // Note: caller is responsible for settings.save()
}

void GobanView::clearView() {
    // Whichever file settings actually live in — not the hardcoded default.
    // A scripted run redirects persistence to scenario-user.json precisely so it
    // cannot touch the developer's real session, and `delete camera` was the one
    // path that ignored that and deleted user.json anyway.
    std::remove(UserSettings::instance().getSettingsFile().c_str());

    // Default camera: same values as initCam()
    DDG::Quaternion targetRot(-1.0, 1.0, 0.0, 0.0);
    targetRot.normalize();
    glm::vec2 targetPan(0.0f, 0.0f);
    float targetDist = 3.5f;

    gobanShader.setGamma(1.0);
    gobanShader.setContrast(0.0);
    gobanShader.setEof(0.0725);
    gobanShader.setDof(0.0925);
    updateFlag |= UPDATE_SHADER;

    animateCamera(targetRot, targetPan, targetDist);
}

void GobanView::animateCamera(const DDG::Quaternion& targetRotation,
                              const glm::vec2& targetPan, float targetDistance,
                              float duration) {
    cameraAnim.startRotation = cam.rLast;
    cameraAnim.targetRotation = targetRotation;
    cameraAnim.startCameraPan = cameraPan;
    cameraAnim.targetCameraPan = targetPan;
    cameraAnim.startCameraDistance = cameraDistance;
    cameraAnim.targetCameraDistance = targetDistance;
    cameraAnim.startTime = static_cast<float>(glfwGetTime());
    cameraAnim.duration = duration;
    cameraAnim.active = true;
    requestRepaint();
}

void GobanView::zoomRelative(float percentage) {
    // Scale camera distance proportionally.
    // Closer to board → smaller steps; distance stays positive.
    cameraDistance *= (1.0f + percentage * 0.2f);
    baseCameraDistance = cameraDistance;
    requestRepaint();
}

void GobanView::mouseMoved(float x, float y) {
    lastX = x;
    lastY = y;
    if (isRotating){
        cam.motion(x, y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        requestRepaint();
    }
    else if (isZooming) {
        float delta = -6.0f * (y - startY) / static_cast<float>(WINDOW_HEIGHT);
        cameraDistance = baseCameraDistance * std::exp(delta);
        requestRepaint();
    }
    else if (isPanning) {
        float scale = 2.0f * cameraDistance / FOCAL_LENGTH;
        float dx = -scale * (x - startX) / static_cast<float>(WINDOW_HEIGHT);
        float dy =  scale * (y - startY) / static_cast<float>(WINDOW_HEIGHT);
        glm::mat4 m(cam.setView());
        glm::vec3 cu = glm::normalize(glm::vec3(m * glm::vec4(1, 0, 0, 0)));
        glm::vec3 cv = glm::normalize(glm::vec3(m * glm::vec4(0, 1, 0, 0)));
        cameraPan.x = baseCameraPan.x + dx * cu.x + dy * cv.x;
        cameraPan.y = baseCameraPan.y + dx * cu.z + dy * cv.z;
        requestRepaint();
    }
}

void GobanView::initCam() {
    cam.rLast[0]=-1.0;
    cam.rLast[1]=1.0;
    cam.rLast[2]=0.0;
    cam.rLast[3]=0.0;
    cam.rLast.normalize();
    cameraPan = glm::vec2(0.0f, 0.0f);
    cameraDistance = 3.5f;
    baseCameraPan = cameraPan;
    baseCameraDistance = cameraDistance;
}

void GobanView::reshape(int width, int height) {
	WINDOW_WIDTH = width;
	WINDOW_HEIGHT = height;
	resolution = glm::vec2(static_cast<float>(width), static_cast<float>(height));

	// Set the viewport to match the window dimensions
	glViewport(0, 0, width, height);

	VIEWPORT_WIDTH = static_cast<float>(width);
	VIEWPORT_HEIGHT = static_cast<float>(height);

	gobanShader.setResolution(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
}

void GobanView::requestRepaint(int what) {
    // The wake decision comes from fetch_or's *previous* value, not from a
    // separate read. Reading updateFlag and then OR-ing it is two operations,
    // and between them the UI thread can exchange the flag to UPDATE_NONE and
    // block in glfwWaitEvents(): the read said "not idle, someone will draw", the
    // exchange took the bits that someone was going to draw, and our bits landed
    // afterwards with nobody left to notice them. Nothing then repaints until an
    // unrelated input event arrives — and a repaint is what plays the stone
    // sound, so the symptom is a move that is silent and, until you move the
    // mouse, invisible.
    //
    // With fetch_or the two cases are exhaustive: if our bits landed before the
    // exchange, the exchange takes them and draws them; if after, we observe
    // UPDATE_NONE and post. Called from the game thread on every move, so this
    // is not a theoretical ordering.
    const int previous = updateFlag.fetch_or(what);
    if (previous == UPDATE_NONE && !animationRunning) {
        glfwPostEmptyEvent();  // Wake the event loop only if it was idle
    }
}

void GobanView::shadeIt(float time, const GobanShader& shader, int flags, int eye) const {
	shader.use();

	// Both of these have to be set *after* use(): glUniform applies to whatever
	// program is currently bound, so setting them from the caller's loop —
	// where the program is not bound yet — silently writes them nowhere, and
	// every mode renders as mode zero.
	shader.setEye(eye);
	shader.setAnaglyph(anaglyphMode);
	shader.setAnaglyphStrength(anaglyphColorStrength);
	shader.setAnaglyphLeak(anaglyphCrosstalk);
	shader.setAnaglyphBalance(anaglyphEyeBalance);
	shader.setGlasses(glassesType);
	shader.setAnaglyphGreen(anaglyphGreenLevel);
	shader.setTime(lastTime);
	shader.setRotation(cam.setView());
	shader.setCameraPan(cameraPan);
	shader.setCameraDistance(cameraDistance);
	// Every frame, with the camera: the base is a function of where the camera
	// is, and the overlay recomputes it every frame too. See setStereoBase().
	shader.setStereoBase(stereoHalfBase());

	if (flags & UPDATE_SHADER) {
		spdlog::debug("setMetrics");
		shader.setMetrics(model.metrics);
	}

	shader.draw(model, flags, time);
	shader.unuse();
}

void GobanView::Render(int w, int h)
{
	if(!gobanShader.isReady())
        return;

	float time = static_cast<float>(glfwGetTime());

	if (WINDOW_HEIGHT != h || WINDOW_WIDTH != w) {
		reshape(w, h);
		startTime = time;
		lastTime = 0.0;
		animationRunning = true;
		// OR, not assign. UPDATE_ALL is (1|2|4|8|16|32) and does *not* include
		// UPDATE_SOUND_STONE (64) — sound is an event, not a surface, so it has
		// no business being redrawn wholesale. Assigning therefore threw away a
		// stone sound that had been requested and not yet played, if the window
		// happened to be resized in between.
		updateFlag |= UPDATE_ALL;
    // Ensure viewport is set correctly (RmlUi may have changed it)
	}

  glDisable(GL_BLEND);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

    // Atomically grab and clear the pending flags so nothing is lost
    // if the game thread sets new flags while we render.
    int flags = updateFlag.exchange(UPDATE_NONE);

    // Smooth camera animation via quaternion slerp + pan/distance interpolation
    if (cameraAnim.active) {
        float t = (time - cameraAnim.startTime) / cameraAnim.duration;
        if (t >= 1.0f) {
            cam.rLast = cameraAnim.targetRotation;
            cameraPan = cameraAnim.targetCameraPan;
            cameraDistance = cameraAnim.targetCameraDistance;
            baseCameraPan = cameraPan;
            baseCameraDistance = cameraDistance;
            cameraAnim.active = false;
        } else {
            t = t * t * (3.0f - 2.0f * t); // smoothstep easing
            cam.rLast = DDG::slerp(cameraAnim.startRotation, cameraAnim.targetRotation, t);
            cameraPan = glm::mix(cameraAnim.startCameraPan, cameraAnim.targetCameraPan, t);
            cameraDistance = glm::mix(cameraAnim.startCameraDistance, cameraAnim.targetCameraDistance, t);
        }
        // Camera uniforms are set unconditionally in shadeIt — no UPDATE_SHADER needed
    }

    if(flags & UPDATE_SOUND_STONE) {
        board.setRandomStoneRotation();
        spdlog::debug("Playing stone sound in repaint");
        player.play("move", 1.0);
    }

	// Update overlays before stone upload so annotation material changes
	// (grid-erasing patches) are included in the glBufferData upload.
	if (flags & UPDATE_OVERLAY){
        updateLastMoveOverlay();
        updateNavigationOverlay();
        // After the navigation overlay, never before: it tints the labels that
        // pass has just written.
        updateAnalysisOverlay();
        updateFloatingLabels();
	}

	if (flags & UPDATE_STONES) {
	    board.updateStones(model.board);
        updateCursor();

        double vol = board.collision;
        if(vol > 0) {
            flags |= UPDATE_OVERLAY;
            if(player.playbackCount() < 5){
	            player.play("clash", vol);
            }
            board.collision = false;
	    }
	}

	// The overlay's glyph buffers are shared by both eyes, so they are built
	// once, before either pass — and before the board, because a pass that draws
	// the text has to have it.
	if (flags & UPDATE_OVERLAY){
        gobanOverlay.Update(board, model);
	}

	// One pass per eye under an anaglyph shader, board and text together.
	//
	// The eyes cannot share a depth buffer. It holds one number per pixel, and a
	// stone in the left eye's image sits where bare board is in the right's, so
	// any single value is wrong for one of them: the old `min(dl, dr)` in
	// partial/stereo/on.glsl classified the pixel as a stone wherever *either*
	// eye saw one, which clipped the other eye's annotation along a silhouette
	// it should have been drawn past. Rendering an eye at a time gives each its
	// own occlusion, and the depth clear between them is what keeps the second
	// from being tested against the first.
	//
	// It costs nothing: the stereo fragment shader always called render() twice
	// per pixel, and two passes call it once each. Measured on the 19x19
	// benchmark, before and after, in tests/bench/.
	const int eyes = gobanShader.isStereo() ? 2 : 1;
	unsigned glyphs = 0;

	// Dubois and crosstalk cancellation work by subtracting one eye's image from
	// the channels the other eye reads, so their contributions go *negative* —
	// and a fixed-point framebuffer clamps a negative fragment to zero before the
	// additive blend can subtract it, which would leave the correction looking
	// applied and doing nothing. Those configurations accumulate into a float
	// target instead, resolved by a blit that clamps once at the end.
	//
	// Only those. Every other mode is positive throughout and stays on the direct
	// path, which is one framebuffer cheaper and already verified on real
	// glasses.
	const bool signedComposite = gobanShader.isStereo()
	        && Stereo::needsSignedAccumulation(anaglyphMode, anaglyphCrosstalk);
	const bool compositing = signedComposite
	        && stereoComposite.begin(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

	for (int eye = 0; eye < eyes; ++eye) {
		if (gobanShader.isStereo()) {
			// The eyes are summed, not masked into fixed channels. Masking was
			// enough while both were grey and owned one channel each, but a
			// Dubois composite puts *both* eyes in all three — that is what its
			// crosstalk correction is — so the channel each eye may write is no
			// longer a constant. The first pass establishes the frame by
			// overwriting; the second adds to it.
			if (eye == 0) {
				glDisable(GL_BLEND);
			} else {
				glClear(GL_DEPTH_BUFFER_BIT);
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
			}
		}

		shadeIt(time, gobanShader, flags, eye);

		// Text blends over the board normally; only the board pass above is
		// additive. Set explicitly rather than assumed — the second eye's pass
		// leaves GL_ONE, GL_ONE behind it.
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);

		// Every piece of text in the program shares this pass: move numbers, the
		// variation labels, SGF markup, the coordinate margin, the evaluation's
		// A/B/C and its board readout. It used to run only when one of the two
		// move-marker toggles was on, which was true when it was written — back then
		// the markers *were* the overlay. Everything since joined the same buffers
		// without revisiting the gate, so turning both markers off took the
		// coordinates, the markup and the whole evaluation display down with them.
		// The two toggles are honoured where they belong: updateLastMoveOverlay()
		// and updateNavigationOverlay() simply do not place their labels.
		if (time - startTime >= gobanShader.animT) {
			gobanOverlay.use();
			glyphs += gobanOverlay.draw(model, cam, 0, eye);
			gobanOverlay.unuse();
			animationRunning = false;
		}

		glUseProgram(0);

		if (time - startTime >= gobanShader.animT) {
			gobanOverlay.use();
			glyphs += gobanOverlay.draw(model, cam, 1, eye);
			gobanOverlay.unuse();
			animationRunning = false;
		}
	}

	// Resolve before restoring state: the blit is what turns the signed
	// accumulation into something displayable, and it must happen while the
	// float target is still bound.
	if (compositing) stereoComposite.end();

	if (gobanShader.isStereo()) {
		// RmlUi draws the rest of the interface after this returns and does not
		// set either of these itself. GobanShader::draw() and
		// GobanOverlay::draw() both bracket themselves in
		// glPushAttrib/glPopAttrib, which restores the state to whatever was
		// current when they were entered — ours — so nothing else puts it back.
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (time - startTime >= gobanShader.animT) {
		// What actually went to the screen, for the same reason sounds_played
		// counts what was heard: every state key describing an overlay stayed
		// true while nothing was drawn, so the suite could not see this at all.
		// Only stored on a frame that ran the pass — a frame skipped for the
		// intro animation says nothing about the overlay either way. Still
		// doubles under a stereo shader: the eye loop is out here now, but each
		// label is still drawn once per eye.
		overlayGlyphsDrawn.store(glyphs);
	}

	glEnable(GL_BLEND);
}

bool GobanView::toggleLastMoveOverlay() {
    showLastMoveOverlay = !showLastMoveOverlay;
    if (!showLastMoveOverlay) {
        if (lastMove) {
            board.removeOverlay(lastMove);
        }
    }
    requestRepaint(UPDATE_OVERLAY | UPDATE_STONES);
    return showLastMoveOverlay;
}

bool GobanView::toggleNextMoveOverlay() {
    showNextMoveOverlay = !showNextMoveOverlay;
    if (!showNextMoveOverlay) {
        for (const auto& pos : navOverlays) {
            if (pos) board.removeBoardOverlay(pos);
        }
    }
    requestRepaint(UPDATE_OVERLAY | UPDATE_STONES);
    return showNextMoveOverlay;
}

void GobanView::setTsumegoMode(bool enabled) {
    tsumegoMode = enabled;
    if (enabled) {
        showLastMoveOverlay = true;
        showNextMoveOverlay = false;
        // Clear navigation overlays (spoilers)
        for (const auto& pos : navOverlays) {
            if (pos) board.removeBoardOverlay(pos);
        }
    } else {
        showLastMoveOverlay = true;
        showNextMoveOverlay = true;
        updateNavigationOverlay();
    }
    updateLastMoveOverlay();
    requestRepaint(UPDATE_OVERLAY | UPDATE_STONES);
}


void GobanView::zoomToStones() {
    using namespace glm;
    const auto& metrics = model.metrics;
    int boardSize = board.getSize();

    // Collect world-space positions of all stones (using fuzzy placement offsets)
    // Read from view's board (not model.board) — shader renders view.board.glStones
    std::vector<vec3> stones;
    for (int col = 0; col < boardSize; ++col) {
        for (int row = 0; row < boardSize; ++row) {
            const auto& pt = board[Position(col, row)];
            if (pt.stone != Color::EMPTY) {
                float x = pt.x * metrics.squareSizeX;
                float z = pt.y * metrics.squareSizeY;
                stones.push_back({x, 0.0f, z});
            }
        }
    }
    if (stones.empty()) return;

    // Camera basis
    mat4 m = cam.setView();
    vec3 cw = normalize(vec3(m * vec4(0, 0, 1, 0)));
    vec3 up = normalize(vec3(m * vec4(0, 1, 0, 0)));
    vec3 cu = normalize(cross(up, cw));
    vec3 cv = cross(cw, cu);
    float ratio = resolution.x / resolution.y;
    float r = metrics.stoneRadius + std::max(metrics.squareSizeX, metrics.squareSizeY);

    // Initial center from orthographic projection onto screen axes
    float puMin = dot(stones[0], cu), puMax = puMin;
    float pvMin = dot(stones[0], cv), pvMax = pvMin;
    for (size_t i = 1; i < stones.size(); ++i) {
        float pu = dot(stones[i], cu);
        float pv = dot(stones[i], cv);
        puMin = std::min(puMin, pu); puMax = std::max(puMax, pu);
        pvMin = std::min(pvMin, pv); pvMax = std::max(pvMax, pv);
    }
    float puMid = (puMax + puMin) * 0.5f;
    float pvMid = (pvMax + pvMin) * 0.5f;
    float t = -(puMid * cu.y + pvMid * cv.y) / cw.y;
    vec3 center = puMid * cu + pvMid * cv + t * cw;
    vec2 targetPan(center.x, center.z);

    // Initial distance from world-space bbox size (so first iteration is reasonable)
    float puHalf = (puMax - puMin) * 0.5f + r;
    float pvHalf = (pvMax - pvMin) * 0.5f + r;
    float worldHalf = std::max(puHalf / ratio, pvHalf);
    float targetDist = std::max(0.5f, FOCAL_LENGTH * worldHalf);
    for (int iter = 0; iter < 8; ++iter) {
        // Screen-space bbox from two levels per stone:
        // - board plane (y=0): stone base, no radius
        // - equator (y=stoneHeight*0.5*h): widest point, full stoneRadius
        // stoneHeight is shader-dependent (0.85 for 3D, 0.0 for 2D)
        float quMin = 1e9f, quMax = -1e9f, qvMin = 1e9f, qvMax = -1e9f;
        vec3 panOff(targetPan.x, 0.0f, targetPan.y);
        float stoneH = gobanShader.getStoneHeight() * 0.5f * metrics.h;
        vec3 yOff(0.0f, stoneH, 0.0f);
        for (const auto& s : stones) {
            vec3 o0 = s - panOff;
            // Board level (no radius)
            float d0 = std::max(0.01f, dot(o0, cw) + targetDist);
            float qu0 = FOCAL_LENGTH * dot(o0, cu) / d0;
            float qv0 = FOCAL_LENGTH * dot(o0, cv) / d0;
            quMin = std::min(quMin, qu0); quMax = std::max(quMax, qu0);
            qvMin = std::min(qvMin, qv0); qvMax = std::max(qvMax, qv0);
            // Equator level (full radius)
            vec3 oH = o0 + yOff;
            float dH = std::max(0.01f, dot(oH, cw) + targetDist);
            float rScr = FOCAL_LENGTH * r / dH;
            float quH = FOCAL_LENGTH * dot(oH, cu) / dH;
            float qvH = FOCAL_LENGTH * dot(oH, cv) / dH;
            quMin = std::min(quMin, quH - rScr); quMax = std::max(quMax, quH + rScr);
            qvMin = std::min(qvMin, qvH - rScr); qvMax = std::max(qvMax, qvH + rScr);
        }
        float quMid = (quMax + quMin) * 0.5f;
        float qvMid = (qvMax + qvMin) * 0.5f;
        float quHalf = (quMax - quMin) * 0.5f;
        float qvHalf = (qvMax - qvMin) * 0.5f;

        // Scale distance so the bbox fits the viewport
        float scale = std::max(quHalf / ratio, qvHalf);

        // Shift pan to center the screen-space bbox on board plane
        // Use current targetDist (before scaling) since quMid was measured at this distance
        float shiftU = quMid * targetDist / FOCAL_LENGTH;
        float shiftV = qvMid * targetDist / FOCAL_LENGTH;

        if (scale > 0.01f)
            targetDist *= scale;
        targetDist = std::max(targetDist, 0.01f);

        spdlog::debug("zoomToStones[{}]: pan=({:.3f},{:.3f}) dist={:.3f} "
                      "qu=[{:.3f},{:.3f}]/{:.3f} qv=[{:.3f},{:.3f}]/1.0 mid=({:.4f},{:.4f})",
                      iter, targetPan.x, targetPan.y, targetDist,
                      quMin, quMax, ratio, qvMin, qvMax, quMid, qvMid);

        // Converged when centered and fitting
        if (std::abs(quMid) < 0.0001f && std::abs(qvMid) < 0.0001f
            && std::abs(scale - 1.0f) < 0.0001f)
            break;

        // Apply pan correction with damping to prevent oscillation
        vec3 worldShift = shiftU * cu + shiftV * cv;
        float ty = -worldShift.y / cw.y;
        worldShift += ty * cw;
        float shiftLen = length(vec2(worldShift.x, worldShift.z));
        float damping = (shiftLen > 1.0f) ? 0.5f : 1.0f;
        targetPan.x += damping * worldShift.x;
        targetPan.y += damping * worldShift.z;
    }

    targetDist = std::clamp(targetDist, 0.01f, 10.0f);
    animateCamera(cam.rLast, targetPan, targetDist);
}

Position GobanView::getBoardCoordinate(float x, float y) const {
    glm::vec2 p = boardCoordinate(x, y);
    float xx = p.x/model.metrics.squareSizeX + 0.5f*model.metrics.fNDIM;
    float yy = p.y/model.metrics.squareSizeY + 0.5f*model.metrics.fNDIM;
    int px = static_cast<int>(floorf(xx));
    int py = static_cast<int>(floorf(yy));
    Position ret(px, py);
    ret.x = xx;
    ret.y = yy;
    return ret;
}

glm::vec2 GobanView::boardCoordinate(float x, float y) const {
    using namespace glm;
    mat4 m = cam.setView();
    // Match shader camera model: ta = pan on board plane, camera behind along viewDir
    vec3 ta = vec3(cameraPan.x, 0.0f, cameraPan.y);
    vec3 viewDir = normalize(vec3(m * vec4(0, 0, 1, 0)));
    vec3 roo = ta - cameraDistance * viewDir;
    vec3 up = normalize(vec3(m * vec4(0, 1, 0, 0)));
    vec3 cw = viewDir;  // forward = toward board
    vec3 cu = normalize(cross(up, cw));
    vec3 cv = cross(cw, cu);
    float ratio = resolution.x / resolution.y;
    vec2 q0 = vec2(ratio * 2.0f * (x / resolution.x - 0.5f), 2.0f * (0.5f - y / resolution.y));
    vec3 rdb = q0.x * cu + q0.y * cv + FOCAL_LENGTH * cw;
    // Intersect ray with board plane (y=0)
    auto t = -roo.y / rdb.y;
    vec3 ip = roo + rdb * t;
    return {ip.x, ip.z};
}

float GobanView::stereoNearPoint() const {
    using namespace glm;
    // Same camera model as boardCoordinate() above and as the vertex shaders.
    const mat4 m = cam.setView();
    const vec3 ta(cameraPan.x, 0.0f, cameraPan.y);
    const vec3 cw = normalize(vec3(m * vec4(0, 0, 1, 0)));
    const vec3 roo = ta - cameraDistance * cw;
    const vec3 up = normalize(vec3(m * vec4(0, 1, 0, 0)));
    const vec3 cu = normalize(cross(up, cw));
    const vec3 cv = cross(cw, cu);
    const float aspect = resolution.y > 0.0f ? resolution.x / resolution.y : 1.0f;

    float nearest = std::numeric_limits<float>::max();

    // The board box, as the scene shaders build it: half-extents (1, 0.25,
    // squareSizeY/squareSizeX) about the origin.
    const float halfZ = model.metrics.squareSizeX > 0.0f
                      ? model.metrics.squareSizeY / model.metrics.squareSizeX : 1.0f;
    const vec3 half(1.0f, 0.25f, halfZ);
    for (int i = 0; i < 8; ++i) {
        const vec3 corner((i & 1) ? half.x : -half.x,
                          (i & 2) ? half.y : -half.y,
                          (i & 4) ? half.z : -half.z);
        const float z = dot(corner - roo, cw);
        if (z > 0.0f) nearest = std::min(nearest, z);
    }

    // ...and the table, which passes under the board and continues toward the
    // viewer. Its nearest visible point is where the bottom edge of the frame
    // meets it, which is what binds once the camera pulls back. The shallower
    // of the two table heights the shaders use, because a shallower plane is
    // met sooner and so is the conservative one.
    constexpr float TABLE_Y = -0.2f;
    for (const float qx : {-aspect, 0.0f, aspect}) {
        const vec3 rd = normalize(qx * cu - cv + FOCAL_LENGTH * cw);
        if (rd.y < -1e-6f) {
            const float t = (TABLE_Y - roo.y) / rd.y;
            if (t > 0.0f) nearest = std::min(nearest, dot(t * rd, cw));
        }
    }

    // A camera inside the board has no honest answer; keep it finite rather
    // than letting the base run away.
    return std::max(nearest, 0.05f);
}

float GobanView::stereoDeviation() const {
    const float aspect = resolution.y > 0.0f ? resolution.x / resolution.y : 1.0f;
    // Far point at infinity: the table runs to the horizon, and it is the
    // reading that cannot flatter us.
    return Stereo::deviation(stereoHalfBase(), aspect, stereoNearPoint(),
                             std::numeric_limits<float>::infinity(), FOCAL_LENGTH);
}

float GobanView::stereoHalfBase() const {
    const float aspect = resolution.y > 0.0f ? resolution.x / resolution.y : 1.0f;
    return Stereo::halfBase(gobanShader.getEof(), aspect, stereoNearPoint(), FOCAL_LENGTH);
}

void GobanView::animateIntro() {
    lastTime = 0.0;
    startTime = static_cast<float>(glfwGetTime());
    animationRunning = true;
    requestRepaint(UPDATE_BOARD|UPDATE_STONES|UPDATE_OVERLAY);
}

void GobanView::Update() {
	// First: everything below compares against `board`, and a resize handed over
	// by the game thread has to land before those comparisons, not after.
	applyPendingResize();

	int newProgram = gobanShader.getCurrentProgram();
	if (currentProgram != newProgram) {
		// UPDATE_OVERLAY too: switching to or from an anaglyph shader changes
		// the ink every label is built with (GobanOverlay::eyeInk), and that
		// colour lives in the glyph buffers rather than in a uniform.
		updateFlag |= UPDATE_SHADER | UPDATE_OVERLAY;
		currentProgram = newProgram;
	}
	if (board.getSize() != model.board.getSize()) {
		// Only request UPDATE_BOARD for dimension change
		// Don't request UPDATE_STONES - model.board might not have stones yet
		updateFlag |= UPDATE_BOARD;
	}
	if (board.positionNumber.load() != model.board.positionNumber.load()) {
		updateFlag |= UPDATE_STONES | UPDATE_OVERLAY;
		board.positionNumber.store(model.board.positionNumber.load());
	}
}

void GobanView::moveCursor(float x, float y) {
    Position coord = getBoardCoordinate(x, y);
    model.setCursor(coord);
    if(model.state.holdsStone) {
        updateFlag |= UPDATE_STONES | UPDATE_OVERLAY;
    }
    else if (pointerMark() > 0.0f) {
        // The mark follows the mouse, so it needs a frame even with no stone in
        // hand — which is the whole case it exists for.
        updateFlag |= UPDATE_SOME;
    }
    // Settled here rather than in the next frame's OnUpdate: the gate would
    // otherwise describe where the mouse was, and a scenario asserting it
    // straight after a move would read the previous position.
    updatePointerState();
}

bool GobanView::ghostStoneVisible() const {
    return model.state.holdsStone && model.isPointOnBoard(model.cursor)
           && model.isLegalMove(Move(model.cursor, state.colorToMove));
}

void GobanView::updateCursor(){
    Position cursor = model.cursor;

    if(state.holdsStone != model.state.holdsStone) {
        board.setRandomStoneRotation();
        state.holdsStone = model.state.holdsStone;
    }
    // The ghost stone appears exactly where a click would place one, and
    // nowhere else. It used to ask only whether the point was empty, which is
    // most of the rule but not the rule: a ko ban or a self-capture drew a
    // stone the click then sent to the engine anyway, to be refused. Both ends
    // ask GobanModel::isLegalMove() now — see GobanControl::placeStone().
    if(ghostStoneVisible()) {
        board.placeCursor(cursor, state.colorToMove);
    }
}


void GobanView::updateLastMoveOverlay() {
	// Always clear old overlay first
	if (lastMove) {
		spdlog::debug("updateLastMoveOverlay: clearing old at ({},{})", lastMove.col(), lastMove.row());
		board.removeOverlay(lastMove);
		lastMove = Position(-1, -1);
	}

	// From the published snapshot, never from the record: this runs inside
	// Render() on the UI thread, and the game thread owns the SGF tree. See
	// GameSnapshot and ADR-0006.
	const auto snap = model.snapshot();

	if (snap->lastStoneMove == Move::NORMAL) {
		const Move& move = snap->lastStoneMove;
		lastMove = move.pos;
		if (showLastMoveOverlay) {
			std::ostringstream ss;
			ss << snap->lastStoneMoveNumber;  // The stone's own index, not the tree depth, which counts passes too
			spdlog::debug("updateLastMoveOverlay: setting '{}' at ({},{}) color={}",
				ss.str(), move.pos.col(), move.pos.row(), move.col == Color::BLACK ? "B" : "W");
			board.setOverlay(move.pos, ss.str(), move.col);
		}
	} else {
		spdlog::debug("updateLastMoveOverlay: no stone move behind the cursor");
	}
}

void GobanView::updateNavigationOverlay() {
	// Clear previous navigation overlays
	for (const auto& pos : navOverlays) {
		if (pos) {
			board.removeBoardOverlay(pos);
		}
	}
	navOverlays.clear();

	// Clear previous markup overlays (both board-level and stone-level)
	for (const auto& pos : markupOverlays) {
		if (pos) {
			board.removeBoardOverlay(pos);  // Clears layer 0 (board)
			board.removeOverlay(pos);        // Clears layers 1-2 (stones)
		}
	}
	markupOverlays.clear();

	// One snapshot for every pass below. Reading model.state.markup directly
	// walked a std::vector the game thread is free to be replacing; taking it
	// twice would also let the loops disagree about what is on the board. The
	// variation list comes from here too — getVariations() allocated a vector of
	// shared_ptr<ISgfcNode> per child, on the UI thread, per repaint.
	const auto snap = model.snapshot();

	// Collect positions with explicit markup (these take precedence over variations)
	std::set<std::pair<int, int>> markupPositions;
	for (const auto& markup : snap->markup) {
		if (markup.pos) {
			markupPositions.insert({markup.pos.col(), markup.pos.row()});
		}
	}

	// Get all variations (branches) from current position
	if (snap->navigating) {
		const auto& variations = snap->variationMoves;
		size_t viewPos = snap->viewPosition;
		size_t nextMoveNum = viewPos + 1;

		spdlog::debug("updateNavigationOverlay: viewPos={}, found {} variations",
			viewPos, variations.size());

		// Label variations: no letter if single child, otherwise newest gets highest letter
		// child[0]=newest/main → 'a', child[1] → 'b', child[2] → 'c'
		size_t idx = 0;
		for (const auto& move : variations) {
			std::ostringstream ss;
			ss << nextMoveNum;
			if (variations.size() > 1) {
				char letter = 'a' + static_cast<char>(idx);
				ss << letter;
			}

			if (move == Move::NORMAL) {
				// Skip positions that have explicit markup (markup takes precedence)
				if (markupPositions.count({move.pos.col(), move.pos.row()})) {
					idx++;
					continue;
				}
				if (showNextMoveOverlay) {
					board.setBoardOverlay(move.pos, ss.str());
				}
				navOverlays.push_back(move.pos);
				spdlog::debug("Navigation overlay: {} at ({},{})",
					ss.str(), move.pos.col(), move.pos.row());
			} else if (move == Move::PASS) {
				spdlog::debug("Navigation overlay: pass variation {}", ss.str());
			}
			idx++;
		}
	}

	// Render SGF markup annotations (explicit markup takes precedence over variations)
	for (const auto& markup : snap->markup) {
		if (!markup.pos) continue;

		std::string text;
		switch (markup.type) {
			case MarkupType::LABEL:
				text = markup.label;
				break;
			case MarkupType::TRIANGLE:
				text = "^";  // ASCII triangle approximation
				break;
			case MarkupType::SQUARE:
				text = "#";  // ASCII square approximation
				break;
			case MarkupType::CIRCLE:
				text = "O";  // ASCII circle
				break;
			case MarkupType::MARK:
				text = "X";  // X marker
				break;
		}

		if (!text.empty()) {
			if (showLastMoveOverlay || showNextMoveOverlay) {
				// Check if there's a stone at this position
				// Use model.board (always up-to-date) not view.board (synced later in render)
				Color stoneColor = model.board[markup.pos].stone;
				if (stoneColor != Color::EMPTY) {
					// Use stone-level overlay (renders on top of stone)
					board.setOverlay(markup.pos, text, stoneColor);
				} else {
					// Use board-level overlay (renders on empty point with grid patch)
					board.setBoardOverlay(markup.pos, text);
				}
			}
			markupOverlays.push_back(markup.pos);
		}
	}

}

void GobanView::updateAnalysisOverlay() {
	// Undo the previous pass first, and differently for the two kinds: a label
	// this overlay added is removed outright, while a point it merely tinted
	// belongs to the navigation overlay and only loses its colour.
	for (const auto& pos : analysisLabels) {
		if (pos) board.removeBoardOverlay(pos);
	}
	analysisLabels.clear();
	for (const auto& pos : analysisTints) {
		if (pos) board.setOverlayTint(pos, std::nullopt);
	}
	analysisTints.clear();
	// Cleared here rather than at each of the four early returns below, so a
	// stale "pass" cannot outlive the report that produced it.
	passSuggestion.clear();

	if (!showAnalysisOverlay || !analysis) return;
	const auto report = analysis->report();
	if (!report) return;

	// Only for the position on screen. A report for a position that has since
	// been left describes a different board, and drawing it would put the
	// engine's opinion of one position onto the stones of another.
	const auto snap = model.snapshot();
	if (report->positionId != snap->positionId) return;

	// A scored end has already made a claim about every point, and it made it in
	// the same place these labels write to: setBoardOverlay() puts mAnnotation
	// into glStones[idx], which is the float updateArea() fills with mBlackArea
	// or mWhiteArea. The damage is not a redraw away either — updateArea() only
	// writes when a point's influence *changes*, so the shading never comes
	// back, and removeBoardOverlay() then resets the point to mEmpty. Turning
	// the suggestions on erased the territory for good.
	//
	// Standing down is the answer rather than sharing the channel, for the
	// reason the readout stands down here (ADR-0007 decision 13): the game is
	// over and counted, and what to play next is not a question the board is
	// being asked. A resignation counted nothing, has no territory, and keeps
	// its suggestions — scoredEnd follows the cursor, so navigating back off the
	// end brings them back too.
	if (snap->scoredEnd) return;

	// The two claims already on the board. Markup is the user's own annotation
	// and is left alone entirely; a variation label is kept and tinted.
	std::set<std::pair<int, int>> markupPositions;
	for (const auto& markup : snap->markup) {
		if (markup.pos) markupPositions.insert({markup.pos.col(), markup.pos.row()});
	}
	std::set<std::pair<int, int>> labelledPositions;
	for (const auto& pos : navOverlays) {
		if (pos) labelledPositions.insert({pos.col(), pos.row()});
	}

	for (const auto& label : evaluationLabels(*report, labelledPositions,
	                                          markupPositions, DEFAULT_EVAL_LABELS,
	                                          gobanShader.qualityPalette())) {
		// No point to sit on, so it goes to the margin. updateFloatingLabels()
		// runs immediately after this and picks it up.
		if (label.pass) {
			passSuggestion = label.text;
			passSuggestionInk = label.color;
			continue;
		}
		// A suggestion can only land on an empty point — the engine proposes
		// legal moves — but the board is a frame behind the model during a
		// capture, so check rather than assume.
		if (model.board[label.pos].stone != Color::EMPTY) continue;
		if (label.text.empty()) {
			board.setOverlayTint(label.pos, label.color);
			analysisTints.push_back(label.pos);
		} else {
			board.setBoardOverlay(label.pos, label.text, label.color);
			analysisLabels.push_back(label.pos);
		}
	}
}

void GobanView::updateFloatingLabels() {
	// One list, one setter: setFloatingLabels() replaces wholesale, so the
	// readout and the coordinates have to be built together.
	std::vector<FloatingLabel> labels;
	readoutText.clear();

	const auto snap = model.snapshot();
	// A scored game has a *result*, and it is already on the message line. An
	// estimate beside it is contradictory — KataGo's scoreLead and GNU Go's
	// final_score disagreed by a tenth of a point on screen — redundant, and in
	// the way of the territory patches that fill the board there. A resignation
	// scores nothing, so it keeps the readout, and navigating back off the end
	// brings it back because scoredEnd follows the cursor.
	// No placement choice any more: the evaluation is drawn on the board, or the
	// analysis is off and there is no report to draw. The RmlUi panel that used
	// to be the alternative is gone — see ADR-0012.
	if (analysis && !snap->scoredEnd) {
		if (const auto report = analysis->report()) {
			// The margin runs from row -0.85 to row 0 — 0.85 grid spacings of
			// wood on every board size, because the constant in Metrics::calc()
			// is in grid units. Centre of that strip is -0.425.
			constexpr float MARGIN_ROW = -0.425f;
			// The same size as a move number. Larger read as shouting; matching
			// the annotations already on the board makes it belong to the same
			// object rather than sit on top of one.
			const float size = 0.8f / static_cast<float>(model.getBoardSize());
			const float lastCol = static_cast<float>(model.getBoardSize()) - 1.0f;
			const float halfN = 0.5f * lastCol;

			// Alignment is an aesthetic choice, so it is the user's. The anchor
			// moves with it: right-aligned text hanging off the middle of the
			// board would look like a mistake, so each alignment anchors to the
			// grid line it belongs against.
			float anchorCol = halfN;
			if (readoutAlign == TextAlign::Left)  anchorCol = 0.0f;
			if (readoutAlign == TextAlign::Right) anchorCol = lastCol;

			std::ostringstream text;
			// Anchored to Black, always — the same convention the panel uses,
			// and chess's for the same reason: a figure that keeps its side is
			// comparable across moves, so "62% then 48%" reads as the swing your
			// move caused. Naming whoever leads instead never drops below 50%
			// and hides exactly that. The score below keeps Go's own convention
			// of naming the leader, because that is how a result is written.
			text << "B " << static_cast<int>(std::lround(report->winrateBlack * 100.0))
			     << "%";
			if (report->scoreLeadBlack) {
				const double lead = *report->scoreLeadBlack;
				// One space, not a run of them: the scenario runner re-joins an
				// expected value with single spaces, so a wider gap could not be
				// asserted. How this should actually be spaced on a board edge
				// is a look-at-it question anyway.
				text << " " << (lead >= 0.0 ? "B+" : "W+")
				     << std::fixed << std::setprecision(1) << std::fabs(lead);
			}

			// add_text centres on the point, so this is the middle of the board
			// edge. Black on wood, like every other board-level label.
			// Describing a position that has been navigated away from. The panel
			// dims in that case; here the ink is a second setting rather than a
			// factor over the first, because scaling an alpha the user has
			// already tuned has no defensible default. It equals readoutInk
			// unless someone says otherwise, so this changes nothing by itself.
			const bool stale = report->positionId != snap->positionId;
			readoutText = text.str();
			labels.push_back({glm::vec2(anchorCol, MARGIN_ROW), readoutText, size,
			                  stale ? readoutStaleInk : readoutInk, 0u, readoutAlign});
		}
	}
	// The engine's advice is to stop playing. It belongs to the *suggestions*
	// feature, not the readout — it is a judgement about a move, read before you
	// play, which is why the two toggles are separate — so it appears with the
	// board labels and disappears with them, whatever the readout is doing.
	if (!passSuggestion.empty()) {
		constexpr float MARGIN_ROW = -0.425f;
		const float size = 0.8f / static_cast<float>(model.getBoardSize());
		const float lastCol = static_cast<float>(model.getBoardSize()) - 1.0f;
		// The end of the margin the readout is not using. Right by default,
		// because the readout is centred by default and the eye finds the end of
		// a line there; left when the user has pushed the readout right, since
		// two right-aligned strings share one anchor and would overprint.
		const bool readoutOnTheRight = !readoutText.empty()
		                               && readoutAlign == TextAlign::Right;
		labels.push_back({glm::vec2(readoutOnTheRight ? 0.0f : lastCol, MARGIN_ROW),
		                  passSuggestion, size, passSuggestionInk, 0u,
		                  readoutOnTheRight ? TextAlign::Left : TextAlign::Right});
	}
	// The wait indicator: a mark and how long it has been waiting.
	//
	// The mark blinks and the count ticks, and both are discrete. Nothing here
	// fades: a board annotation is carved or it is not there, and an opacity
	// that breathes reads as a screen effect laid over the scene rather than as
	// part of it. Blinking the *presence* keeps that rule — the mark is only
	// ever fully printed or fully absent, one state at a time, which is what a
	// clock's colon does. See Wait::markVisible().
	//
	// Two labels rather than one string, laid out left to right from a single
	// anchor: the mark left-aligned on it, the count left-aligned a fixed gap
	// along. The gap is in grid units, so it scales with the board exactly as
	// the glyph size (0.8/N of a square) does, and they cannot drift apart.
	waitText.clear();
	if (waitKind != WaitKind::None) {
		const float elapsed = static_cast<float>(glfwGetTime()) - waitStarted;
		const int second = Wait::displayedSecond(elapsed, waitGrace);
		if (second != Wait::NOT_SHOWN) {
			constexpr float MARGIN_ROW = -0.425f;
			constexpr float GAP = 0.7f;
			const float size = 0.8f / static_cast<float>(model.getBoardSize());
			const float lastCol = static_cast<float>(model.getBoardSize()) - 1.0f;
			// The end of the margin nothing else is using. Left by default — the
			// readout is centred and the pass suggestion is right — and moved to
			// the right only when the user has pulled the readout left onto it.
			const bool readoutOnTheLeft = !readoutText.empty()
			                              && readoutAlign == TextAlign::Left;
			const float anchor = readoutOnTheLeft ? lastCol - 2.0f : 0.0f;

			const std::string& glyph = (waitKind == WaitKind::Syncing && !waitGlyphSyncing.empty())
			                           ? waitGlyphSyncing : waitGlyph;
			const std::string count = std::to_string(second) + "s";

			// The count holds its place whether the mark is showing or not, so
			// nothing slides sideways twice a second.
			if (Wait::markVisible(elapsed, waitGrace, waitBlinkPeriod)) {
				labels.push_back({glm::vec2(anchor, MARGIN_ROW), glyph, size,
				                  waitInk, 0u, TextAlign::Left});
			}
			labels.push_back({glm::vec2(anchor + GAP, MARGIN_ROW), count, size,
			                  waitInk, 0u, TextAlign::Left});
			// What a scenario can assert on, since the glyphs themselves are
			// unreachable from a headless run. The count is always part of it;
			// the mark comes and goes, which is why an assertion on this key
			// should not pin the whole string.
			waitText = Wait::markVisible(elapsed, waitGrace, waitBlinkPeriod)
			           ? glyph + " " + count : count;
		}
	}
	if (showCoordinates) {
		const int N = static_cast<int>(model.getBoardSize());
		const float size = 0.8f / static_cast<float>(N);
		// Out into the margin by `coordOffset` spacings. The wood runs 0.85
		// past the outermost line on every board size, so 0.425 sits in the
		// middle of it. Top and left, always — fixed whether or not the readout
		// is on, so nothing ever moves and the bottom edge stays free for it.
		const float MARGIN = coordOffset;
		for (int col = 0; col < N; ++col) {
			labels.push_back({glm::vec2(static_cast<float>(col),
			                            static_cast<float>(N - 1) + MARGIN),
			                  std::string(1, Position::columnLabel(col)), size,
			                  coordinateInk, 0u, TextAlign::Center});
		}
		for (int row = 0; row < N; ++row) {
			labels.push_back({glm::vec2(-MARGIN, static_cast<float>(row)),
			                  std::to_string(Position::rowLabel(row)), size,
			                  coordinateInk, 0u, TextAlign::Center});
		}
	}

	gobanOverlay.setFloatingLabels(std::move(labels));
}

void GobanView::setWaitIndicator(WaitKind kind) {
	if (kind != waitKind) {
		waitKind = kind;
		waitStarted = static_cast<float>(glfwGetTime());
		waitSecondShown = Wait::NOT_SHOWN;
		if (kind == WaitKind::None) waitText.clear();
		// A wait ending has to repaint too, or the last frame drawn keeps the
		// indicator on a board that is no longer waiting for anything.
		requestRepaint(UPDATE_OVERLAY);
		return;
	}
	if (kind == WaitKind::None) return;

	// Still waiting. A frame is worth drawing when the count changes or the mark
	// blinks, and not otherwise — twenty identical frames a second would be
	// twenty rebuilds of every glyph buffer for no visible difference.
	// getIdleTimeout() offers the ticks; this decides which of them are worth
	// anything. Same shape as the evaluation's publish gate, which deliberately
	// asks for no repaint when the displayed values have not moved.
	const float elapsed = static_cast<float>(glfwGetTime()) - waitStarted;
	const int second = Wait::displayedSecond(elapsed, waitGrace);
	const bool mark = Wait::markVisible(elapsed, waitGrace, waitBlinkPeriod);
	if (second != waitSecondShown || mark != waitMarkShown) {
		waitSecondShown = second;
		waitMarkShown = mark;
		requestRepaint(UPDATE_OVERLAY);
	}
}

bool GobanView::toggleCoordinates() {
	setCoordinates(!showCoordinates);
	return showCoordinates;
}

void GobanView::setCoordinates(bool shown) {
	if (showCoordinates == shown) return;
	showCoordinates = shown;
	requestRepaint(UPDATE_OVERLAY);
}

void GobanView::setAnaglyph(Stereo::Anaglyph mode) {
	if (anaglyphMode == mode) return;
	anaglyphMode = mode;
	// The board is repainted with a new composite, and that is all: the overlay
	// ink deliberately does not follow. Every mode gives the left eye the red
	// channel alone, so a green label is invisible to it whatever the board
	// does — which is the rule GobanOverlay::eyeInk() already enforces.
	requestRepaint(UPDATE_ALL);
}

void GobanView::setAnaglyphStrength(float strength) {
	const float next = Stereo::clampStrength(strength);
	if (anaglyphColorStrength == next) return;
	anaglyphColorStrength = next;
	requestRepaint(UPDATE_ALL);
}

void GobanView::setAnaglyphLeak(const Stereo::Crosstalk& leak) {
	anaglyphCrosstalk = Stereo::clampCrosstalk(leak);
	requestRepaint(UPDATE_ALL);
}

void GobanView::setAnaglyphGreen(float green) {
	const float next = Stereo::clampGreen(green);
	if (anaglyphGreenLevel == next) return;
	anaglyphGreenLevel = next;
	requestRepaint(UPDATE_ALL);
}

const char* pointerModeName(PointerMode mode) {
    switch (mode) {
        case PointerMode::Always: return "always";
        case PointerMode::Never:  return "never";
        case PointerMode::Auto:   break;
    }
    return "auto";
}

std::optional<PointerMode> parsePointerMode(const std::string& name) {
    std::string n;
    for (char c : name) n += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (n == "auto" || n == "stereo") return PointerMode::Auto;
    if (n == "always" || n == "on" || n == "yes") return PointerMode::Always;
    if (n == "never" || n == "off" || n == "no") return PointerMode::Never;
    return std::nullopt;
}

void GobanView::setPointerMode(PointerMode mode) {
	if (pointerMode == mode) return;
	pointerMode = mode;
	// Re-asks both questions: whether to draw the mark, and whether the native
	// pointer should be back. Neither follows from the mode alone.
	requestRepaint(UPDATE_SOME);
	updatePointerState();
}

void GobanView::setPointerOnWidget(bool onWidget) {
	if (pointerOnWidget == onWidget) return;
	pointerOnWidget = onWidget;
	updatePointerState();
}

void GobanView::updatePointerState() {
	const bool over = !pointerOnWidget && model.isPointOnBoard(model.cursor);
	if (pointerOverBoard != over) {
		pointerOverBoard = over;
		// Any flag will do: the board is redrawn on every frame that renders at
		// all, and the cursor uniform is uploaded with the camera rather than
		// behind UPDATE_STONES.
		requestRepaint(UPDATE_SOME);
	}
	// Hidden only where the drawn mark replaces it. If the mark is off — mono,
	// or strength 0 — the native pointer stays, because taking it away without
	// putting anything in its place is worse than the parallax it suffers from.
	// diegeticPointer(), not pointerMark(): with a stone in hand the mark stands
	// down in favour of the ghost stone, and reading the mark alone would hand
	// the native pointer back the moment you picked a stone out of the bowl.
	const bool hide = diegeticPointer();
	if (hide != nativePointerHidden) {
		nativePointerHidden = hide;
		if (GLFWwindow* window = AppState::GetWindow()) {
			glfwSetInputMode(window, GLFW_CURSOR,
			                 hide ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
		}
	}
}

void GobanView::setGlasses(Stereo::Glasses g) {
	if (glassesType == g) return;
	glassesType = g;
	// UPDATE_ALL, not a bare repaint: the overlay masks its text into the
	// channels its eye owns, and those just changed, so the glyph buffers have to
	// be rebuilt as well as the board redrawn.
	requestRepaint(UPDATE_ALL);
}

void GobanView::setAnaglyphBalance(const Stereo::EyeBalance& balance) {
	anaglyphEyeBalance = Stereo::clampBalance(balance);
	requestRepaint(UPDATE_ALL);
}

void GobanView::setReadoutColor(const glm::vec4& color) {
	if (readoutInk == color) return;
	readoutInk = color;
	// The stale ink shadows it until it is set explicitly — otherwise changing
	// the colour would silently leave the stale one at the old value.
	if (!haveStaleInk) readoutStaleInk = color;
	requestRepaint(UPDATE_OVERLAY);
}

void GobanView::setCoordinateOffset(float spacings) {
	if (coordOffset == spacings) return;
	coordOffset = spacings;
	requestRepaint(UPDATE_OVERLAY);
}

void GobanView::setCoordinateColor(const glm::vec4& color) {
	if (coordinateInk == color) return;
	coordinateInk = color;
	requestRepaint(UPDATE_OVERLAY);
}

void GobanView::setReadoutStaleColor(const glm::vec4& color) {
	haveStaleInk = true;
	if (readoutStaleInk == color) return;
	readoutStaleInk = color;
	requestRepaint(UPDATE_OVERLAY);
}

const char* GobanView::alignName(TextAlign align) {
	switch (align) {
		case TextAlign::Left:   return "left";
		case TextAlign::Right:  return "right";
		case TextAlign::Center: return "center";
	}
	return "center";
}

std::optional<TextAlign> GobanView::parseAlign(const std::string& name) {
	if (name == "center" || name == "centre") return TextAlign::Center;
	if (name == "left")  return TextAlign::Left;
	if (name == "right") return TextAlign::Right;
	return std::nullopt;
}

void GobanView::setEvaluationAlign(TextAlign align) {
	if (readoutAlign == align) return;
	readoutAlign = align;
	requestRepaint(UPDATE_OVERLAY);
}

bool GobanView::toggleAnalysisOverlay() {
	setAnalysisOverlay(!showAnalysisOverlay);
	return showAnalysisOverlay;
}

void GobanView::setAnalysisOverlay(bool shown) {
	if (showAnalysisOverlay == shown) return;
	showAnalysisOverlay = shown;
	// UPDATE_STONES as well as UPDATE_OVERLAY: a label sets the annotation
	// material, and that has to reach the stone upload or the grid stays drawn
	// under it.
	requestRepaint(UPDATE_OVERLAY | UPDATE_STONES);
}

void GobanView::onBoardSized(int newBoardSize) {
	// Hand it over; do not do it here. This runs on the game thread, and during
	// startup on the engine-loader thread, with the board already on screen —
	// while board.clear() assigns a std::string into all 361 points and the four
	// overlay vectors below are walked by updateNavigationOverlay() once per
	// repaint. See pendingBoardSize.
	pendingBoardSize.store(newBoardSize);
	// Only request UPDATE_BOARD (for shader dimension) and UPDATE_OVERLAY
	// Don't request UPDATE_STONES - let onBoardChange handle that when stones are ready
	requestRepaint(UPDATE_BOARD | UPDATE_OVERLAY);
}

void GobanView::applyPendingResize() {
	const int newBoardSize = pendingBoardSize.exchange(-1);
	if (newBoardSize < 0) return;
	board.clear(newBoardSize);
	lastMove = Position(-1, -1);
	navOverlays.clear();
	markupOverlays.clear();
	analysisLabels.clear();
	analysisTints.clear();
}

void GobanView::onStonePlaced(const Move& move) {
    spdlog::debug("onStonePlaced: move={}, type={}", move.toString(),
        move == Move::NORMAL ? "NORMAL" : (move == Move::PASS ? "PASS" : "OTHER"));

    if (move == Move::NORMAL) {
        // Overlay updates happen on the UI thread in Render (updateLastMoveOverlay).
        // Just request sound and repaint.
        requestRepaint(UPDATE_SOUND_STONE | UPDATE_STONES | UPDATE_OVERLAY);
    }
}

void GobanView::onGameMove(const Move& move, const std::string& comment) {
    // Delegate visual/audio to onStonePlaced
    onStonePlaced(move);
}

void GobanView::onBoardChange(const Board& newBoard) {
	// Model already has the board (GobanModel::onBoardChange stores it).
	// UI thread copies from model.board and updates overlays in Render().
	requestRepaint(UPDATE_BOARD | UPDATE_STONES | UPDATE_OVERLAY);
}
