#include "GobanView.h"
#include <GLFW/glfw3.h>
#include "AudioPlayer.hpp"
#include "ElementGame.h"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <set>
#include <sstream>
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
    bool wasIdle = (updateFlag == UPDATE_NONE && !animationRunning);
    updateFlag |= what;
    if (wasIdle) {
        glfwPostEmptyEvent();  // Wake the event loop only if it was idle
    }
}

void GobanView::shadeIt(float time, const GobanShader& shader, int flags) const {
	shader.use();

	shader.setTime(lastTime);
	shader.setRotation(cam.setView());
	shader.setCameraPan(cameraPan);
	shader.setCameraDistance(cameraDistance);

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
		updateFlag = UPDATE_ALL;
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
        updateEvaluationReadout();
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

    shadeIt(time, gobanShader, flags);

   	glEnable(GL_BLEND);

	glEnable(GL_DEPTH_TEST);
	if (flags & UPDATE_OVERLAY){
        gobanOverlay.Update(board, model);
	}

	if (time - startTime >= gobanShader.animT) {
		if (showLastMoveOverlay || showNextMoveOverlay) {
			gobanOverlay.use();
			gobanOverlay.draw(model, cam, 0);
			gobanOverlay.unuse();
		}
		animationRunning = false;
    }

    glUseProgram(0);

	if (time - startTime >= gobanShader.animT) {
		if (showLastMoveOverlay || showNextMoveOverlay) {
			gobanOverlay.use();
			gobanOverlay.draw(model, cam, 1);
			gobanOverlay.unuse();
		}
		animationRunning = false;
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

void GobanView::animateIntro() {
    lastTime = 0.0;
    startTime = static_cast<float>(glfwGetTime());
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
}

void GobanView::updateCursor(){
    Position cursor = model.cursor;

    if(state.holdsStone != model.state.holdsStone) {
        board.setRandomStoneRotation();
        state.holdsStone = model.state.holdsStone;
    }
    if(model.state.holdsStone && model.isPointOnBoard(cursor)){
        auto& np = model.board[cursor];
        if(np.stone == Color::EMPTY)
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

	if (!showAnalysisOverlay || !analysis) return;
	const auto report = analysis->report();
	if (!report) return;

	// Only for the position on screen. A report for a position that has since
	// been left describes a different board, and drawing it would put the
	// engine's opinion of one position onto the stones of another.
	const auto snap = model.snapshot();
	if (report->positionId != snap->positionId) return;

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
	                                          markupPositions, DEFAULT_EVAL_LABELS)) {
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

void GobanView::updateEvaluationReadout() {
	std::vector<FloatingLabel> labels;
	readoutText.clear();

	const auto snap = model.snapshot();
	// A scored game has a *result*, and it is already on the message line. An
	// estimate beside it is contradictory — KataGo's scoreLead and GNU Go's
	// final_score disagreed by a tenth of a point on screen — redundant, and in
	// the way of the territory patches that fill the board there. A resignation
	// scores nothing, so it keeps the readout, and navigating back off the end
	// brings it back because scoredEnd follows the cursor.
	if (showEvaluationOnBoard && analysis && !snap->scoredEnd) {
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
			text << (report->winrateBlack >= 0.5 ? "B " : "W ")
			     << static_cast<int>(std::lround(
			            (report->winrateBlack >= 0.5 ? report->winrateBlack
			                                         : 1.0 - report->winrateBlack) * 100.0))
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
	gobanOverlay.setFloatingLabels(std::move(labels));
}

void GobanView::setReadoutColor(const glm::vec4& color) {
	if (readoutInk == color) return;
	readoutInk = color;
	// The stale ink shadows it until it is set explicitly — otherwise changing
	// the colour would silently leave the stale one at the old value.
	if (!haveStaleInk) readoutStaleInk = color;
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

bool GobanView::toggleEvaluationOnBoard() {
	setEvaluationOnBoard(!showEvaluationOnBoard);
	return showEvaluationOnBoard;
}

void GobanView::setEvaluationOnBoard(bool shown) {
	if (showEvaluationOnBoard == shown) return;
	showEvaluationOnBoard = shown;
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
	board.clear(newBoardSize);
	lastMove = Position(-1, -1);
	navOverlays.clear();
	markupOverlays.clear();
	analysisLabels.clear();
	analysisTints.clear();
	// Only request UPDATE_BOARD (for shader dimension) and UPDATE_OVERLAY
	// Don't request UPDATE_STONES - let onBoardChange handle that when stones are ready
	requestRepaint(UPDATE_BOARD | UPDATE_OVERLAY);
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
