//
// Created by jan on 7.5.17.
//

#include "GobanShader.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include "GobanView.h"
#include <Shell.h>

extern Configuration config;

const GLushort GobanShader::elementBufferData[] = {0, 1, 2, 3};
const std::array<GLfloat, 16> GobanShader::vertexBufferData = { {
	-1.0f, -1.0f, 0.0f, 1.0f,
	1.0f, -1.0f, 0.0f, 1.0f,
	-1.0f, 1.0f, 0.0f, 1.0f,
	1.0f, 1.0f, 0.0f, 1.0f
} };

GLuint shaderCompileFromString(GLenum type, const std::string& source) {
    GLuint shader;
    GLint length;
    GLint result;

    shader = glCreateShader(type);
    length = static_cast<GLint>(source.length());
    const char * psource = source.c_str();
    glShaderSource(shader, 1, &psource, &length);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        char *log;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        log = new char[length];
        glGetShaderInfoLog(shader, length, &result, log);

        spdlog::error("shaderCompileFromString(): Unable to compile: {}", log);

        delete [] log;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool shaderAttachFromString(GLuint program, GLenum type, const std::string& source)
{
    GLuint shader = shaderCompileFromString(type, source);
    if (shader != 0) {
        glAttachShader(program, shader);
        glDeleteShader(shader);
        return true;
    }
    return false;
}

std::string createShaderFromFile(const std::string& filename) {
    std::stringstream ss;
    std::ifstream fin(filename);
    std::string s;
    while (getline(fin, s)||fin){
        ss << s << std::endl;
    }
    return ss.str();
}

void GobanShader::initProgram(const std::string& vertexProgram, const std::string& fragmentProgram) {

    shadersReady = false;
    if (gobanProgram != 0) {
        glDeleteProgram(gobanProgram);
    }
    gobanProgram = glCreateProgram();

    const std::string sVertexShader = createShaderFromFile(vertexProgram);
    const std::string sFragmentShader = createShaderFromFile(fragmentProgram);

    if(!shaderAttachFromString(gobanProgram, GL_VERTEX_SHADER, sVertexShader))
        spdlog::error("Vertex shader [{}] failed to compile. Err {}", vertexProgram, glGetError());
    if(!shaderAttachFromString(gobanProgram, GL_FRAGMENT_SHADER, sFragmentShader))
        spdlog::error("Fragment Shader [{}] failed to compile. Err {}", fragmentProgram, glGetError());

    glLinkProgram(gobanProgram);

    GLint result;
    glGetProgramiv(gobanProgram, GL_LINK_STATUS, &result);

    if (result == GL_FALSE) {
        GLint length;
        char *log;

        glGetProgramiv(gobanProgram, GL_INFO_LOG_LENGTH, &length);
        log = (char*)malloc(static_cast<std::size_t>(length));
        glGetProgramInfoLog(gobanProgram, length, &result, log);

        spdlog::error("sceneInit(): Program linking failed: {0}", log);
        free(log);

        glDeleteProgram(gobanProgram);
        gobanProgram = 0;
    }

    uBlockIndex = glGetUniformBlockIndex(gobanProgram, "iStoneBlock");
    glGenBuffers(1, &bufStones);
    glBindBuffer(GL_UNIFORM_BUFFER, bufStones);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * (Board::MAXBOARD*Board::MAXBOARD << 2), 0, GL_DYNAMIC_DRAW);
    glUniformBlockBinding(gobanProgram, uBlockIndex, blockBindingPoint);
    glBindBufferRange(GL_UNIFORM_BUFFER, blockBindingPoint, bufStones, 0, 4 * sizeof(float)* Board::BOARDSIZE);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    iStone = glGetUniformLocation(gobanProgram, "iStone");
    iMouse = glGetUniformLocation(gobanProgram, "iMouse");
    iDim = glGetUniformLocation(gobanProgram, "NDIM");
    iTranslate = glGetUniformLocation(gobanProgram, "iTranslate");
    iTime = glGetUniformLocation(gobanProgram, "iTime");
    iResolution = glGetUniformLocation(gobanProgram, "iResolution");
    iGamma = glGetUniformLocation(gobanProgram, "gamma");
    iContrast = glGetUniformLocation(gobanProgram, "contrast");
    iBlackCapturedCount = glGetUniformLocation(gobanProgram, "iBlackCapturedCount");
    iWhiteCapturedCount = glGetUniformLocation(gobanProgram, "iWhiteCapturedCount");
    iModelView = glGetUniformLocation(gobanProgram, "glModelViewMatrix");
    iAnimT = glGetUniformLocation(gobanProgram, "iAnimT");

    fsu_fNDIM = glGetUniformLocation(gobanProgram, "fNDIM");
    fsu_boardaa = glGetUniformLocation(gobanProgram, "boardaa");
    fsu_boardbb = glGetUniformLocation(gobanProgram, "boardbb");
    fsu_boardcc = glGetUniformLocation(gobanProgram, "boardcc");
    fsu_ww = glGetUniformLocation(gobanProgram, "ww");
    fsu_iww = glGetUniformLocation(gobanProgram, "iww");
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

    glUseProgram(gobanProgram);
    glUniform1f(iAnimT, animT);
    glUseProgram(0);
    shaderChanged = true;
}

void GobanShader::setGamma(float gamma) {
    spdlog::debug("setting gamma = {0}", gamma);
    this->gamma = gamma;
}

void GobanShader::setContrast(float contrast) {
    spdlog::debug("setting contrast = {0}", contrast);
    this->contrast = contrast;
}

void GobanShader::setMetrics(const Metrics &m) {

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
    float ww = m.squareSize;

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
    glUniform1f(fsu_ww, ww);
    glUniform1f(fsu_iww, 1.0f/ww);

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
    glUniform1f(fsu_dw, 0.015f*ww);
    glUniform1f(fsu_iscale, 0.2f/ww);
    glUniform1f(iGamma, gamma);
    glUniform1f(iContrast, contrast);
    glUniform1f(fsu_bowlRadius, br);
    glUniform1f(fsu_bowlRadius2, br2);
    glUniform3fv(fsu_cc, 2, m.bowlsCenters);
}

void GobanShader::destroy(void) {
    glDeleteProgram(gobanProgram);
}

void GobanShader::init() {
	vertexBuffer = make_buffer(GL_ARRAY_BUFFER, &vertexBufferData[0], sizeof(GLfloat)*vertexBufferData.size());
    elementBuffer = make_buffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferData, sizeof(elementBufferData));

	currentProgram = 0;

	choose(currentProgram);

    glEnable(GL_BLEND);
}

void GobanShader::draw(const GobanModel& model, const DDG::Camera& cam, int updateFlag, float time) {
    if(!shadersReady)
        return;
#ifndef DEBUG_NVIDIA
	glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
#endif
	//glUseProgram(gobanProgram);
	if (updateFlag & GobanView::UPDATE_BOARD) {
		unsigned size = view.board.getSize();
		glUniform1i(iDim, size);
		setMetrics(model.metrics);
	}
	else if(updateFlag & GobanView::UPDATE_SHADER) {
        setMetrics(model.metrics);
    }

	glPushAttrib(GL_ALL_ATTRIB_BITS);

    if (view.animationRunning) {
        glUniform1f(iTime, view.lastTime + time - view.startTime);
    }
	else {
		glUniform1f(iTime, animT);
	}
    glUniform2fv(iResolution, 1, glm::value_ptr(view.resolution));
    if (updateFlag & GobanView::UPDATE_STONES) {
        spdlog::debug("place stones via glBufferData()");
        glUniform1i(iBlackCapturedCount,  view.state.capturedBlack);
        glUniform1i(iWhiteCapturedCount, view.state.capturedWhite);
		glUniform3fv(iddc, 2 * Metrics::maxc, model.metrics.tmpc);
        glBindBuffer(GL_UNIFORM_BUFFER, bufStones);
        glBufferData(GL_UNIFORM_BUFFER, view.board.getSizeOf(), view.board.getStones(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        //if (boardChanged > 1) { //TODO sound
            //stoneSound();
            //stoneSound(true);
        //}
    }
    glUniform3fv(iTranslate, 1, glm::value_ptr(view.newTranslate));

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glVertexAttribPointer(iVertex, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat)* 4, (void*)0);
	glEnableVertexAttribArray(iVertex);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
	glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, (void*)0);
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

int GobanShader::choose(int idx) {

    using nlohmann::json;
    json shaders(config.data.value("shaders", json::array()));

    if(shaders.size() < 1) {
        spdlog::critical("No shader definition found.");
        return -1;
    }

    int newProgram = idx % shaders.size();

    json shader(shaders[newProgram]);

    std::string vertexFile(shader.value("vertex", ""));
    std::string fragmentFile(shader.value("fragment", ""));

    currentProgramH = shader.value("height", 0.0f);
    if(!vertexFile.empty() && !fragmentFile.empty()) {
        initProgram(vertexFile, fragmentFile);
        currentProgram = newProgram;
    } else {
        spdlog::warn("Shader [{}] must comprise both vertex and fragment programs.", newProgram);
    }
    return currentProgram;
}

void GobanShader::use() {
    glUseProgram(gobanProgram);
}

void GobanShader::unuse() {
    glUseProgram(0);
}

void GobanShader::setTime(float time) {
    glUniform1f(iTime, time);
}

void GobanShader::setPan(glm::vec3 pan) {
    glUniform3fv(iTranslate, 1, glm::value_ptr(pan));
}

void GobanShader::setRotation(glm::mat4x4 m) {
    glUniformMatrix4fv(iModelView, 1, 0, glm::value_ptr(m));
}

void GobanShader::setResolution(float w, float h) {
    width = w;
    height = h;
}
