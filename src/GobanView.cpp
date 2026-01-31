#include "GobanView.h"
#include <GLFW/glfw3.h>
#include "AudioPlayer.hpp"
#include "ElementGame.h"

#include <cstdio>
#include <set>
#include "UserSettings.h"

GobanView::GobanView(GobanModel& m)
    :
    gobanShader(*this), gobanOverlay(*this), model(m), MAX_FPS(false), VIEWPORT_WIDTH(0), VIEWPORT_HEIGHT(0),
    translate(0.0, 0.0, 0.0), newTranslate(0.0, 0.0, 0.0), resolution(1024.0, 768.0), lastTime(0.0f),
    startTime(0.0f), animationRunning(false), isPanning(false), isZooming(false), isRotating(false),
    cam(1.0, 0.0, 0.0, 0.0), startX(0), startY(0), lastX(.0f), lastY(.0f), updateFlag(0),
    currentProgram(-1),	showLastMoveOverlay(true), showNextMoveOverlay(true)
{
    player.preload(config);
    player.init();

    initCam();
    updateTranslation();
    translate[0] = newTranslate[0];
    translate[1] = newTranslate[1];
    translate[2] = newTranslate[2];
    resetView();

    // Load saved shader (or default to 0) — must happen before setReady()
    int shaderIdx = 0;
    auto& settings = UserSettings::instance();
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
    auto& settings = UserSettings::instance();

    if (settings.hasCameraSettings()) {
        cam.rLast[0] = settings.getCameraRotationX();
        cam.rLast[1] = settings.getCameraRotationY();
        cam.rLast[2] = settings.getCameraRotationZ();
        cam.rLast[3] = settings.getCameraRotationW();
        newTranslate[0] = settings.getCameraTranslationX();
        newTranslate[1] = settings.getCameraTranslationY();
        newTranslate[2] = settings.getCameraTranslationZ();
    }

    if (settings.hasShaderSettings()) {
        gobanShader.setEof(settings.getShaderEof());
        gobanShader.setDof(settings.getShaderDof());
        gobanShader.setGamma(settings.getShaderGamma());
        gobanShader.setContrast(settings.getShaderContrast());
    }

    translate[0] = newTranslate[0];
    translate[1] = newTranslate[1];
    translate[2] = newTranslate[2];
    updateFlag |= UPDATE_SHADER;

    lastTime = 0.0;
    startTime = static_cast<float>(glfwGetTime());
    animationRunning = true;
    requestRepaint();
}

void GobanView::switchShader(int idx) {
    updateFlag |= GobanView::UPDATE_ALL;
    // Show loading message if UI is ready
    if (model.parent && model.parent->GetContext()) {
        model.parent->showMessage("Loading shaders...");
    }
    gobanShader.choose(idx);
    state.metricsReady = false;
    gobanShader.setReady();
    // Clear message after compilation
    if (model.parent && model.parent->GetContext()) {
        model.parent->clearMessage();
    }
}

void GobanView::saveView() {
    auto& settings = UserSettings::instance();

    settings.setCameraRotation(cam.rLast[0], cam.rLast[1], cam.rLast[2], cam.rLast[3]);
    settings.setCameraTranslation(newTranslate[0], newTranslate[1], newTranslate[2]);

    settings.setShaderEof(gobanShader.getEof());
    settings.setShaderDof(gobanShader.getDof());
    settings.setShaderGamma(gobanShader.getGamma());
    settings.setShaderContrast(gobanShader.getContrast());

    settings.save();
}

void GobanView::clearView() {
    std::remove("user.json");
    initCam();
    updateTranslation();
    translate[0] = newTranslate[0];
    translate[1] = newTranslate[1];
    translate[2] = newTranslate[2];
    gobanShader.setGamma(1.0);
    gobanShader.setContrast(0.0);
    gobanShader.setEof(0.0075);
    gobanShader.setDof(0.01);
    updateFlag |= UPDATE_SHADER;
    saveView();
    requestRepaint();
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
        glm::vec4 a(0.0f, 0.0f, -6.0f * (y - startY)/static_cast<float>(WINDOW_HEIGHT), 1.0);
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
        glm::vec4 a(
                -scale * (x - startX) / static_cast<float>(WINDOW_HEIGHT),
                 scale * (y - startY) / static_cast<float>(WINDOW_HEIGHT),
                 0.0, 1.0);
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
	shader.setPan(newTranslate);

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

    if(flags & UPDATE_SOUND_STONE) {
        board.setRandomStoneRotation();
        spdlog::debug("Playing stone sound in repaint");
        player.play("move", 1.0);
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
        updateLastMoveOverlay();
        updateNavigationOverlay();
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
        updateFlag |= UPDATE_OVERLAY | UPDATE_STONES;
    } else {
        updateLastMoveOverlay();
    }
    return showLastMoveOverlay;
}

bool GobanView::toggleNextMoveOverlay() {
    showNextMoveOverlay = !showNextMoveOverlay;
    if (!showNextMoveOverlay) {
        for (const auto& pos : navOverlays) {
            if (pos) board.removeBoardOverlay(pos);
        }
        updateFlag |= UPDATE_OVERLAY | UPDATE_STONES;
    } else {
        updateNavigationOverlay();
    }
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

	// Get current move number from SGF tree depth
	size_t moveNum = model.game.moveCount();

	if (moveNum > 0) {
		auto [move, moveIndex] = model.game.lastStoneMoveIndex();
		if (move == Move::NORMAL) {
			lastMove = move.pos;
			if (showLastMoveOverlay) {
				std::ostringstream ss;
				ss << moveIndex;  // Use the stone's actual move index, not total depth (which includes passes)
				spdlog::debug("updateLastMoveOverlay: setting '{}' at ({},{}) color={}",
					ss.str(), move.pos.col(), move.pos.row(), move.col == Color::BLACK ? "B" : "W");
				board.setOverlay(move.pos, ss.str(), move.col);
			}
		} else {
			spdlog::debug("updateLastMoveOverlay: lastStoneMoveIndex returned non-NORMAL");
		}
	} else {
		spdlog::debug("updateLastMoveOverlay: no moves");
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

	// Collect positions with explicit markup (these take precedence over variations)
	std::set<std::pair<int, int>> markupPositions;
	for (const auto& markup : model.state.markup) {
		if (markup.pos) {
			markupPositions.insert({markup.pos.col(), markup.pos.row()});
		}
	}

	// Clear pass variation label (set below if a pass variation exists)
	model.state.passVariationLabel.clear();

	// Get all variations (branches) from current position
	if (model.game.isNavigating()) {
		auto variations = model.game.getVariations();
		size_t viewPos = model.game.getViewPosition();
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
				// Pass variation - show in message label (no board position to overlay)
				bool isBlack = (move.col == Color::BLACK);
				model.state.msg = isBlack ? GameState::BLACK_PASS : GameState::WHITE_PASS;
				model.state.passVariationLabel = ss.str();
				spdlog::debug("Navigation overlay: pass variation {}", ss.str());
			}
			idx++;
		}
	}

	// Render SGF markup annotations (explicit markup takes precedence over variations)
	for (const auto& markup : model.state.markup) {
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
				Color stoneColor = board[markup.pos].stone;
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

	// UPDATE_STONES needed to upload mAnnotation material changes for grid patches
	requestRepaint(UPDATE_OVERLAY | UPDATE_STONES);
}

void GobanView::onBoardSized(int newBoardSize) {
	board.clear(newBoardSize);
	lastMove = Position(-1, -1);
	navOverlays.clear();
	markupOverlays.clear();
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
