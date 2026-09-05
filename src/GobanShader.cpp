#include "GobanShader.h"

#include <chrono>
#include <fstream>
#include "AppState.h"
#include "GobanView.h"
#include "Shadinclude.hpp"
#include "UserSettings.h"
#include <glm/gtc/type_ptr.hpp>

const GLushort GobanShader::elementBufferData[] = {0, 1, 2, 3};
const std::array<GLfloat, 16> GobanShader::vertexBufferData = { {
	-1.0f, -1.0f, 0.0f, 1.0f,
	1.0f, -1.0f, 0.0f, 1.0f,
	-1.0f, 1.0f, 0.0f, 1.0f,
	1.0f, 1.0f, 0.0f, 1.0f
} };

GLuint shaderCompileFromString(GLenum type, const std::string& source) {
    GLint length;
    GLint result;

    GLuint shader = glCreateShader(type);
    length = static_cast<GLint>(source.length());
    const char * psource = source.c_str();
    glShaderSource(shader, 1, &psource, &length);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char *log = new char[length];
        glGetShaderInfoLog(shader, length, &result, log);

        spdlog::error("shaderCompileFromString(): Unable to compile: {}", log);

        delete [] log;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool GobanShader::shaderAttachFromString(GLuint program, GLenum type, const std::string& source)
{
    GLuint *pshader = type == GL_VERTEX_SHADER ? &this->vertexShader : &this->fragmentShader;
    *pshader = shaderCompileFromString(type, source);
    if (*pshader != 0) {
        glAttachShader(program, *pshader);
        //glDeleteShader(*pshader);
        return true;
    }
    return false;
}

std::string createShaderFromFile(const std::string& filename) {
    return Shadinclude::load(filename);
}

/// Compile and link. Locals and shared objects only — it writes no member and
/// sets no context state, which is exactly what makes it safe to run on the
/// worker's context. Returns 0 if anything failed.
GLuint GobanShader::buildProgram(const std::string& vertexProgram,
                                 const std::string& fragmentProgram) {

    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    auto ms = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    const GLuint program = glCreateProgram();

    const std::string sVertexShader = createShaderFromFile(vertexProgram);
    const std::string sFragmentShader = createShaderFromFile(fragmentProgram);
    const auto tRead = Clock::now();

    const GLuint vs = shaderCompileFromString(GL_VERTEX_SHADER, sVertexShader);
    if (vs == 0)
        spdlog::error("Vertex shader [{}] failed to compile. Err {}", vertexProgram, glGetError());
    else
        glAttachShader(program, vs);
    const auto tVert = Clock::now();

    const GLuint fs = shaderCompileFromString(GL_FRAGMENT_SHADER, sFragmentShader);
    if (fs == 0)
        spdlog::error("Fragment Shader [{}] failed to compile. Err {}", fragmentProgram, glGetError());
    else
        glAttachShader(program, fs);
    const auto tFrag = Clock::now();

    glLinkProgram(program);

    // Querying the link status is what makes the driver finish the link, so the
    // timing below is only meaningful on this side of it — and the link is where
    // essentially the whole cost is. Measured on Intel/Mesa with a cold driver
    // cache: 19 ms to compile the fragment shader, 2019 ms to link it. That is
    // the frozen launch users report, and it is worth one line in last_run.log,
    // which is what a bug report is read from.
    GLint result = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    const auto tLink = Clock::now();
    spdlog::info("shader [{}] built: read {:.0f} ms, vertex {:.0f} ms, fragment {:.0f} ms, link {:.0f} ms",
                 fragmentProgram, ms(t0, tRead), ms(tRead, tVert), ms(tVert, tFrag), ms(tFrag, tLink));

    if (vs != 0) { glDetachShader(program, vs); glDeleteShader(vs); }
    if (fs != 0) { glDetachShader(program, fs); glDeleteShader(fs); }

    if (result == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length > 0 ? length : 1), '\0');
        glGetProgramInfoLog(program, length, &result, log.data());
        spdlog::error("sceneInit(): Program linking failed: {0}", log.c_str());
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

/// Install a linked program: its buffers, its binding point and every uniform
/// location. **UI thread only.** glBindBufferRange() below binds to the context
/// rather than to the program or the buffer, so doing this on the worker would
/// leave the drawing context with no uniform buffer bound at all — the board
/// would render from whatever happened to be there.
void GobanShader::adoptProgram(GLuint program) {
    if (program == 0) return;
    // The old program is kept alive until the new one is in hand, which is what
    // lets a shader switch keep drawing the previous board for the seconds the
    // link takes instead of blanking.
    if (gobanProgram != 0 && gobanProgram != program) {
        glDeleteProgram(gobanProgram);
    }
    gobanProgram = program;

    uBlockIndex = glGetUniformBlockIndex(gobanProgram, "iStoneBlock");
    glGenBuffers(1, &bufStones);
    glBindBuffer(GL_UNIFORM_BUFFER, bufStones);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * (Board::MAX_BOARD * Board::MAX_BOARD << 2), nullptr, GL_DYNAMIC_DRAW);
    glUniformBlockBinding(gobanProgram, uBlockIndex, blockBindingPoint);
    glBindBufferRange(GL_UNIFORM_BUFFER, blockBindingPoint, bufStones, 0, 4 * sizeof(float)* Board::BOARD_SIZE);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    iDim = glGetUniformLocation(gobanProgram, "NDIM");
    iCameraPan = glGetUniformLocation(gobanProgram, "cameraPan");
    iCameraDistance = glGetUniformLocation(gobanProgram, "cameraDistance");
    iTime = glGetUniformLocation(gobanProgram, "iTime");
    iResolution = glGetUniformLocation(gobanProgram, "iResolution");
    iGamma = glGetUniformLocation(gobanProgram, "gamma");
    iContrast = glGetUniformLocation(gobanProgram, "contrast");
    iBlackCapturedCount = glGetUniformLocation(gobanProgram, "iBlackCapturedCount");
    iWhiteCapturedCount = glGetUniformLocation(gobanProgram, "iWhiteCapturedCount");
    iBlackReservoirCount = glGetUniformLocation(gobanProgram, "iBlackReservoirCount");
    iWhiteReservoirCount = glGetUniformLocation(gobanProgram, "iWhiteReservoirCount");
    iModelView = glGetUniformLocation(gobanProgram, "glModelViewMatrix");
    iAnimT = glGetUniformLocation(gobanProgram, "iAnimT");

    fsu_fNDIM = glGetUniformLocation(gobanProgram, "fNDIM");
    fsu_boardaa = glGetUniformLocation(gobanProgram, "boardaa");
    fsu_boardbb = glGetUniformLocation(gobanProgram, "boardbb");
    fsu_boardcc = glGetUniformLocation(gobanProgram, "boardcc");
    fsu_wwx = glGetUniformLocation(gobanProgram, "wwx");
    fsu_wwy = glGetUniformLocation(gobanProgram, "wwy");
    fsu_w = glGetUniformLocation(gobanProgram, "w");
    fsu_h = glGetUniformLocation(gobanProgram, "h");
    iAnimT = glGetUniformLocation(gobanProgram, "iAnimT");
    fsu_stoneRadius = glGetUniformLocation(gobanProgram, "stoneRadius");
    fsu_d = glGetUniformLocation(gobanProgram, "d");
    fsu_stoneradius2 = glGetUniformLocation(gobanProgram, "stoneRadius2");
    fsu_dn = glGetUniformLocation(gobanProgram, "dn");
    fsu_b = glGetUniformLocation(gobanProgram, "b");
    fsu_y = glGetUniformLocation(gobanProgram, "y");
    fsu_px = glGetUniformLocation(gobanProgram, "px");
    fsu_pxs = glGetUniformLocation(gobanProgram, "pxs");
    fsu_r1 = glGetUniformLocation(gobanProgram, "r1");
    fsu_r2 = glGetUniformLocation(gobanProgram, "r2");
    fsu_r123r123 = glGetUniformLocation(gobanProgram, "r123r123");
    fsu_rrr = glGetUniformLocation(gobanProgram, "rrr");
    fsu_r1r1ir2ir2 = glGetUniformLocation(gobanProgram, "r1r1ir2ir2");
    fsu_maxBound = glGetUniformLocation(gobanProgram, "maxBound");
    fsu_dw = glGetUniformLocation(gobanProgram, "dw");
    fsu_iscale = glGetUniformLocation(gobanProgram, "iscale");
    fsu_bowlRadius = glGetUniformLocation(gobanProgram, "bowlRadius");
    fsu_bowlRadius2 = glGetUniformLocation(gobanProgram, "bowlRadius2");
    fsu_cc = glGetUniformLocation(gobanProgram, "cc");
    iddc = glGetUniformLocation(gobanProgram, "ddc");
    fsu_cursor = glGetUniformLocation(gobanProgram, "cursor");
    fsu_cursorMark = glGetUniformLocation(gobanProgram, "cursorMark");

    vsu_eof = glGetUniformLocation(gobanProgram, "eof");
    vsu_dof = glGetUniformLocation(gobanProgram, "dof");
    fsu_eye = glGetUniformLocation(gobanProgram, "eye");
    fsu_anaglyph = glGetUniformLocation(gobanProgram, "anaglyph");
    fsu_anaglyphStrength = glGetUniformLocation(gobanProgram, "anaglyphStrength");
    fsu_anaglyphLeak = glGetUniformLocation(gobanProgram, "anaglyphLeak");
    fsu_anaglyphBalance = glGetUniformLocation(gobanProgram, "anaglyphBalance");
    fsu_glasses = glGetUniformLocation(gobanProgram, "glasses");
    fsu_anaglyphGreen = glGetUniformLocation(gobanProgram, "anaglyphGreen");

    queryShaderParamLocations();

    glUseProgram(gobanProgram);
    glUniform1f(iAnimT, animT);
    glUseProgram(0);
}

/// The synchronous path, unchanged in effect: build, then install. Still used
/// when there is no shared context, and by the fallback inside chooseAsync().
void GobanShader::initProgram(const std::string& vertexProgram, const std::string& fragmentProgram) {
    const GLuint program = buildProgram(vertexProgram, fragmentProgram);
    shadersReady = false;
    if (program == 0) {
        gobanProgram = 0;
        return;
    }
    adoptProgram(program);
}

int GobanShader::chooseAsync(int idx) {
    GLFWwindow* shared = AppState::GetShaderContext();
    if (shared == nullptr) {
        // No shared context: exactly the old behaviour, freeze included. Better
        // than not selecting the shader at all.
        return choose(idx);
    }

    PendingShader s;
    if (!resolveShader(idx, s)) return currentProgram;

    // Already holding it — the same dedupe choose() does, and the reason the
    // startup path links once rather than three times.
    if (s.index == currentProgram && gobanProgram != 0 && !buildRunning) {
        applyShaderMetadata(s);
        return currentProgram;
    }

    if (buildRunning) {
        // One link at a time. Remember the last thing asked for rather than
        // dropping it: a build takes seconds, and cycling shaders with the
        // keyboard is exactly how several requests arrive inside one. Only the
        // most recent matters — the ones in between were never seen.
        queued = s;
        hasQueued = true;
        return currentProgram;
    }

    startBuild(s);
    return currentProgram;
}

void GobanShader::startBuild(const PendingShader& s) {
    GLFWwindow* shared = AppState::GetShaderContext();
    if (shared == nullptr) return;

    pending = s;
    buildTarget = s.index;
    buildResult = 0;
    buildFinished.store(false, std::memory_order_relaxed);
    buildRunning = true;
    buildStarted = std::chrono::steady_clock::now();

    buildThread = std::thread([this, shared]() {
        // The worker owns this context for its whole life; nothing else ever
        // makes it current, so there is no handover to get wrong.
        glfwMakeContextCurrent(shared);
        const GLuint program = buildProgram(pending.vertex, pending.fragment);
        // The UI thread is about to use this program on its own context. Shared
        // objects need the producing context to have finished before the
        // consuming one may rely on them, and glFinish is the blunt instrument
        // that guarantees it without a sync object.
        glFinish();
        glfwMakeContextCurrent(nullptr);
        buildResult = program;
        buildFinished.store(true, std::memory_order_release);
    });
}

void GobanShader::joinBuild() {
    if (buildThread.joinable()) buildThread.join();
    buildRunning = false;
}

double GobanShader::buildElapsed() const {
    if (!buildRunning) return 0.0;
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now() - buildStarted).count();
}

bool GobanShader::pollBuild() {
    if (!buildRunning || !buildFinished.load(std::memory_order_acquire)) return false;

    joinBuild();
    const GLuint program = buildResult;
    buildResult = 0;
    buildFinished.store(false, std::memory_order_relaxed);

    const double took = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - buildStarted).count();

    bool adopted = false;
    if (program == 0) {
        // The link failed and said so in the log. Leave whatever was already
        // current alone rather than dropping to a blank board — on a shader
        // switch that is the previous shader, which is the better answer than
        // nothing, and at startup there is nothing to lose.
        spdlog::error("Shader {} could not be built; keeping the current one.", buildTarget);
    } else {
        // The appearance facts land with the program, not when it was asked
        // for. Applying them at the request would flip isStereo() — and with it
        // the overlay's two eyes and its ink — seconds before the shader that
        // needs them is actually drawing.
        applyShaderMetadata(pending);
        adoptProgram(program);
        currentProgram = buildTarget;
        spdlog::info("shader {} ready after {:.1f} s", buildTarget, took);
        adopted = true;
    }

    // Whatever was asked for while this one was linking.
    if (hasQueued) {
        const PendingShader next = queued;
        hasQueued = false;
        if (next.index != currentProgram || gobanProgram == 0) {
            startBuild(next);
        }
    }
    return adopted;
}

// Half the stereo base, in world units, computed on the CPU (Stereo.h) — not
// the raw `eof` preference. The shader used to scale that preference by the
// camera distance itself, which is the wrong quantity: the depth budget is set
// by the *near point*, and the board is a fixed-size object, so zooming in
// shrinks the near point far faster than the distance does.
//
// Uploaded from shadeIt() on every frame, beside the camera it depends on. It
// spent one afternoon in setMetrics(), which runs only on a board or shader
// change: the board's stereo base then froze at whatever the camera was when
// the shader was last switched, while the overlay's followed the camera — so
// the two agreed until the first zoom and never again.
void GobanShader::setStereoBase(float halfBase) const {
    glUniform1f(vsu_eof, halfBase);
}

void GobanShader::setStereoWindow(float window) const {
    glUniform1f(vsu_dof, window);
}

void GobanShader::setGamma(float value) {
    spdlog::debug("setting gamma = {0}", value);
    this->gamma = value;
}

void GobanShader::setContrast(float value) {
    spdlog::debug("setting contrast = {0}", value);
    this->contrast = value;
}

void GobanShader::setMetrics(const Metrics &m) const {

    if(!shadersReady || m.fNDIM <= 0)
        return;

    float fNDIM(m.fNDIM);

    float boardaa(sqrtf(2.0f/width/height));
    float boardbb(sqrtf(1.0f/width/height));
    float r1 = m.stoneRadius;
    float px = m.px;
    float d = m.d;
    float b = m.b;
    float w = m.w;
    float h = m.h;
    float y = m.y;
    float r = m.stoneSphereRadius;
    float br = m.innerBowlRadius;
    float br2 = m.br2;
    float wwx = m.squareSizeX;
    float wwy = m.squareSizeY;

    float r2 = b*r1/(2.0f*sqrtf(r1*r1-px*px)); //ellipsoid
    float dn[3];
    dn[0] = dn[2] = 0.0f; dn[1] = d;
    float pxs = 0.5f*(0.5f*w - px);
    float rrr[3];
    rrr[0] = rrr[2] = r2*r2*r1*r1; rrr[1] = r1*r1*r1*r1;
    float maxBound[3];
    maxBound[0] = maxBound[2] = 1.2f;
    maxBound[1] = h;

    glUniform1f(fsu_fNDIM, fNDIM);
    glUniform1f(fsu_boardaa, boardaa);
    glUniform1f(fsu_boardbb, boardbb);
    glUniform1f(fsu_boardcc, 8.0f*boardaa);
    glUniform1f(fsu_wwx, wwx);
    glUniform1f(fsu_wwy, wwy);

    glUniform1f(fsu_w, w); //initial width
    glUniform1f(fsu_h, h); //initial height
    glUniform1f(fsu_stoneRadius, r); //sphere
    glUniform1f(fsu_d, d);
    glUniform1f(fsu_stoneradius2, r*r);

    glUniform3fv(fsu_dn, 1, dn);
    glUniform1f(fsu_b, b);
    glUniform1f(fsu_y, y);
    glUniform1f(fsu_px, px);
    glUniform1f(fsu_pxs, pxs);
    glUniform1f(fsu_r1, r1);
    glUniform1f(fsu_r2, r2);
    glUniform1f(fsu_r123r123, r1*r1*r2*r2*r1*r1);
    glUniform3fv(fsu_rrr, 1, rrr);
    glUniform1f(fsu_r1r1ir2ir2, r1*r1/(r2*r2));
    glUniform3fv(fsu_maxBound, 1, maxBound);
    glUniform1f(fsu_dw, 0.015f*wwx);
    glUniform1f(fsu_iscale, 0.2f/wwx);
    glUniform1f(iGamma, gamma);
    glUniform1f(iContrast, contrast);
    glUniform1f(fsu_bowlRadius, br);
    glUniform1f(fsu_bowlRadius2, br2);
    glUniform3fv(fsu_cc, 4, m.bowlsCenters);
    // `dof` is *not* uploaded here. It used to be, harmlessly, while it was a
    // constant; now it is derived from the aspect ratio and the deviation, so it
    // changes when the window is resized — and setMetrics() runs only on a board
    // or shader change. That is exactly how the stereo base froze at the last
    // shader switch for an afternoon. It goes up from shadeIt(), beside the base.
}

void GobanShader::destroy() const {
    glDeleteProgram(gobanProgram);
}

void GobanShader::init() {

	vertexBuffer = make_buffer(GL_ARRAY_BUFFER, &vertexBufferData[0], sizeof(GLfloat)*vertexBufferData.size());
	elementBuffer = make_buffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferData, sizeof(elementBufferData));

	// Deliberately does *not* build a program. This runs from the constructor,
	// which is before anybody has read which shader the user actually saved, so
	// it could only ever link number 0 — and GobanView's own constructor then
	// links the real one a few lines later. On a cold driver cache that made the
	// wasted link the expensive one: measured 1969 ms to link, against 17 ms to
	// compile the fragment shader it discards. `currentProgram` stays -1, which
	// is what choose() reads as "nothing built yet".

    glEnable(GL_BLEND);
}

void GobanShader::draw(const GobanModel& model, int updateFlag, float time) const {
    if(!shadersReady)
        return;
#ifndef DEBUG_NVIDIA
	glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
#endif
	//glUseProgram(gobanProgram);
	if (updateFlag & GobanView::UPDATE_BOARD) {
		int size = view.board.getSize();
		glUniform1i(iDim, size);
		setMetrics(model.metrics);
	}
	else if(updateFlag & GobanView::UPDATE_SHADER) {
        setMetrics(model.metrics);
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);

    // Unconditionally, like the camera and unlike the stone data: a toggle is
    // rare but it must not need a particular flag to be in the frame that
    // carries it. Uploading two integers per frame costs nothing measurable,
    // and the alternative is the bug where a value travels on one flag while
    // what it affects travels on another.
    uploadShaderParams();

    if (view.animationRunning) {
        glUniform1f(iTime, view.lastTime + time - view.startTime);
    } else {
		glUniform1f(iTime, animT);
	}
    glUniform2fv(iResolution, 1, glm::value_ptr(view.resolution));
    if (updateFlag & GobanView::UPDATE_STONES) {
        spdlog::debug("place stones via glBufferData()");
        glUniform1i(iBlackCapturedCount,  view.capturedBlackShown);
        glUniform1i(iWhiteCapturedCount, view.capturedWhiteShown);
        glUniform1i(iBlackReservoirCount,  static_cast<int>(view.state.reservoirBlack / 2));
        glUniform1i(iWhiteReservoirCount, static_cast<int>(view.state.reservoirWhite / 2));
        glUniform4fv(iddc, 2 * Metrics::maxc, model.metrics.tmpc);

        glBindBuffer(GL_UNIFORM_BUFFER, bufStones);
        glBufferData(GL_UNIFORM_BUFFER, view.board.getSizeOf(), view.board.getStones(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    // Outside UPDATE_STONES, beside the camera. It used to live inside that
    // branch, and the mouse-move path raises the flag only while a stone is in
    // hand — so the one case the pointer mark exists for is exactly the case in
    // which the uniform went stale and the mark stayed where the mouse had last
    // been holding something.
    {
        const Position coord = view.getBoardCoordinate(view.lastX, view.lastY);
        const int size = view.board.getSize();
        // The point it names, plus the same imprecise-hand offset a stone in
        // hand gets — Board::fuzzyOffset(), not a second copy of the arithmetic.
        // Snapping rigidly was the first version and read as a cursor stuck to a
        // lattice; drifting freely names a place the board has no name for. This
        // does what the stone does: slides within the point, then jumps to the
        // next one.
        const glm::vec2 fuzz = view.board.fuzzyOffset(coord);
        const float cur[2] = {
            static_cast<float>(coord.col()) + fuzz.x - static_cast<float>(size) / 2.0f,
            static_cast<float>(coord.row()) + fuzz.y - static_cast<float>(size) / 2.0f,
        };
        glUniform2fv(fsu_cursor, 1, cur);
        glUniform1f(fsu_cursorMark, view.pointerMark());
    }

    glUniform2fv(iCameraPan, 1, glm::value_ptr(view.cameraPan));
    glUniform1f(iCameraDistance, view.cameraDistance);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glVertexAttribPointer(iVertex, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat)* 4, static_cast<void *>(nullptr));
    glEnableVertexAttribArray(iVertex);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, static_cast<void *>(nullptr));
    glDisableVertexAttribArray(iVertex);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glPopAttrib();
}

GLuint make_buffer(GLenum target, const void *buffer_data, GLsizei buffer_size) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, buffer_size, buffer_data,  GL_DYNAMIC_DRAW);
    glBindBuffer(target, 0);
    return buffer;
}

bool GobanShader::resolveShader(int idx, PendingShader& out) const {
    using nlohmann::json;
    json shaders(config->data.value("shaders", json::array()));

    if(shaders.empty()) {
        spdlog::critical("No shader definition found.");
        return false;
    }

    out.index = static_cast<int>(idx % shaders.size());
    json shader(shaders[out.index]);

    out.vertex = shader.value("vertex", "");
    out.fragment = shader.value("fragment", "");

    out.name = shader.value("name", std::string());
    out.height = shader.value("height", 0.0f);
    // Declared by the shader rather than inferred from its vertex file: the
    // overlay has to draw the same two eyes, and a path comparison is not a
    // fact about the shader.
    out.stereo = shader.value("stereo", 0) != 0;
    // Third of the same kind. Whether the scene has bowls in it cannot be read
    // off the fragment path without parsing the include graph, and the CPU has
    // to know: PrisonerMode::Auto draws the counts in the margin exactly where
    // the shader does not draw the pile.
    out.bowls = shader.value("bowls", 0) != 0;
    // Same shape, and the same reason: an appearance fact the CPU has to act
    // on, declared by the shader rather than inferred. The global block is the
    // default and no shipped shader overrides it, so this normally resolves to
    // exactly what `annotations` says.
    out.palette = resolveQualityPalette(
            config->data.value("annotations", json::object()),
            shader.value("annotations", json::object()));
    // Fourth of the same kind, and the one that is a *setting* rather than a
    // fact: what the shader offers comes from the config, what it is set to
    // comes from user.json, keyed by the same name that restores the selection.
    // ADR-0017.
    out.params = resolveShaderParams(
            config->data.value("shader_params", json::object()),
            shader,
            UserSettings::instance().getShaderParams(out.name));

    if(out.vertex.empty() || out.fragment.empty()) {
        spdlog::warn("Shader [{}] must comprise both vertex and fragment programs.", out.index);
        return false;
    }
    return true;
}

void GobanShader::applyShaderMetadata(const PendingShader& s) {
    currentProgramH = s.height;
    currentProgramStereo = s.stereo;
    currentProgramBowls = s.bowls;
    currentProgramName = s.name;
    currentPalette = s.palette;
    currentParams = s.params;
    // Cleared rather than resized: these are locations in a program that may not
    // be the one currently adopted. adoptProgram() fills them, and uploadShaderParams()
    // does nothing while they are empty, which is the right behaviour during a
    // build — the old program is still drawing and its own locations are gone.
    currentParamLocations.clear();
}

void GobanShader::queryShaderParamLocations() {
    currentParamLocations.assign(currentParams.size(), -1);
    for (size_t i = 0; i < currentParams.size(); ++i) {
        const GLint loc = glGetUniformLocation(gobanProgram, currentParams[i].name.c_str());
        currentParamLocations[i] = loc;
        // ADR-0017 decision 7. The one real advantage of metadata in a GLSL
        // comment is that it cannot drift from the uniform; this is what buys
        // that back. -1 means either a typo in the config or a uniform the
        // compiler eliminated because nothing reads it — both are the author's
        // mistake and both are silent otherwise.
        if (loc < 0) {
            spdlog::warn("Shader parameter '{}' is declared in the configuration "
                         "but the linked program has no such uniform (typo, or "
                         "nothing in the shader reads it).", currentParams[i].name);
        }
    }
}

void GobanShader::uploadShaderParams() const {
    for (size_t i = 0; i < currentParamLocations.size(); ++i) {
        if (currentParamLocations[i] >= 0) {
            glUniform1i(currentParamLocations[i], currentParams[i].value ? 1 : 0);
        }
    }
}

bool GobanShader::setShaderParam(const std::string& name, bool value) {
    for (auto& p : currentParams) {
        if (p.name == name) {
            p.value = value;
            return true;
        }
    }
    return false;
}

int GobanShader::choose(int idx) {
    PendingShader s;
    if (!resolveShader(idx, s)) return currentProgram;

    applyShaderMetadata(s);

    // Link only when we are not already holding this very program. Linking is
    // where a shader costs — measured 2019 ms cold on Intel/Mesa against 19 ms
    // to compile the fragment shader — and the same program used to be linked
    // three times on the way to the first frame: from GobanShader's constructor,
    // from GobanView's constructor for the saved shader, and again when the
    // shader dropdown syncs itself to what is already selected. The driver's own
    // cache made repeats cheap (90 ms) rather than free, and warm that was still
    // 278 ms of every launch.
    //
    // The metadata above is applied either way: it is nearly free, and it comes
    // from the configuration rather than from the program object, so a reload
    // with the same index must still be able to change it.
    if (s.index != currentProgram || gobanProgram == 0) {
        initProgram(s.vertex, s.fragment);
    }
    currentProgram = s.index;
    return currentProgram;
}

void GobanShader::use() const {
    glUseProgram(gobanProgram);
}

void GobanShader::unuse() {
    glUseProgram(0);
}

void GobanShader::setTime(float time) const {
    glUniform1f(iTime, time);
}

void GobanShader::setCameraPan(glm::vec2 pan) const {
    glUniform2fv(iCameraPan, 1, glm::value_ptr(pan));
}

void GobanShader::setCameraDistance(float dist) const {
    glUniform1f(iCameraDistance, dist);
}

void GobanShader::setRotation(glm::mat4x4 m) const {
    glUniformMatrix4fv(iModelView, 1, 0, glm::value_ptr(m));
}

void GobanShader::setResolution(float w, float h) {
    width = w;
    height = h;
}

void GobanShader::setEye(int eye) const {
    glUniform1i(fsu_eye, eye);
}

void GobanShader::setAnaglyph(Stereo::Anaglyph mode) const {
    glUniform1i(fsu_anaglyph, Stereo::anaglyphUniform(mode));
}

void GobanShader::setAnaglyphStrength(float strength) const {
    glUniform1f(fsu_anaglyphStrength, strength);
}

void GobanShader::setAnaglyphLeak(const Stereo::Crosstalk& leak) const {
    glUniform3f(fsu_anaglyphLeak, leak.r, leak.g, leak.b);
}

void GobanShader::setAnaglyphBalance(const Stereo::EyeBalance& balance) const {
    glUniform2f(fsu_anaglyphBalance, balance.left, balance.right);
}

void GobanShader::setGlasses(Stereo::Glasses g) const {
    glUniform1i(fsu_glasses, Stereo::glassesUniform(g));
}

void GobanShader::setAnaglyphGreen(float green) const {
    glUniform1f(fsu_anaglyphGreen, green);
}

void GobanShader::setEof(float val) {
    eof = val;
}

void GobanShader::setDof(float val) {
    dof = val;
}

float GobanShader::getEof() const {
    return eof;
}

float GobanShader::getDof() const {
    return dof;
}
