//
// Created by jan on 7.5.17.
//

#include "GobanShader.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include "GobanView.h"
#include <Shell.h>

extern "C" {
#include "glyphy/matrix4x4.h"
}

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

        std::cerr << "shaderCompileFromString(): Unable to compile: " << log << std::endl;
        delete [] log;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

void shaderAttachFromString(GLuint program, GLenum type, const std::string& source)
{
    GLuint shader = shaderCompileFromString(type, source);
    if (shader != 0) {
        glAttachShader(program, shader);
        glDeleteShader(shader);
    }
}
#ifdef OPTIMIZE_SHADERS

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

std::string createShader(const std::string& fname, bool optimize, glslopt_ctx* ctx, glslopt_shader_type shaderType) {

    std::stringstream ss;
    std::ifstream fin(fname);
    std::string s;
    //ss << "#version 300 es" << std::endl;
    while (getline(fin, s)||fin){
        ss << s << std::endl;
    }
    if (optimize) {
        glslopt_shader* shader = glslopt_optimize(ctx, shaderType, ss.str().c_str(), 0);
        std::string ret;
        if (glslopt_get_status(shader)) {
            ret = glslopt_get_output(shader);
        }
        else {
            ret = glslopt_get_log(shader);
        }
        glslopt_shader_delete(shader);
        std::ofstream fout(fname + ".opt");
        if (shaderType = glslopt_shader_type::kGlslOptShaderFragment) {
            std::string fs(ret);
            replace(fs, "uniform vec4 iStones[361]", "layout(std140) uniform iStoneBlock{vec4 iStones[361];}");
            fout << fs;
            return fs.c_str();
        }
        else {
            fout << ret;
            return ret;
        }
    }
    return ss.str().c_str();
}
#else
std::string createShader(const std::string& fname, bool optimize) {
    (void)optimize;
    std::stringstream ss;
    std::ifstream fin(fname);
    std::string s;
    while (getline(fin, s)||fin){
        ss << s << std::endl;
    }
    return ss.str();
}

#endif

void GobanShader::initProgram(int which) {
    console->info("preShaderInitProgram = {0}", glGetError());
    const char* vprogram[4] = {
        "data/glsl/vertex.glsl",
        "data/glsl/vertex.anaglyph.glsl",
        "data/glsl/vertex.glsl",
        "data/glsl/vertex.glsl"
    };
    const char* program[4] = {
        "data/glsl/fragment.glsl",
        "data/glsl/fragment.anaglyph.glsl",
	"data/glsl/fragment.2D.glsl",
        "data/glsl/fragment.25D.glsl"
    };
    shadersReady = false;
    if (gobanProgram != 0) {
        glDeleteProgram(gobanProgram);
    }
    gobanProgram = glCreateProgram();
#ifdef OPTIMIZE_SHADERS
    glslopt_ctx* ctx = OPTIMIZE ? glslopt_initialize(glslopt_target::kGlslTargetOpenGLES30) : 0;
    const std::string sVertexShader = createShader(VERTEX_FILE, OPTIMIZE, ctx, glslopt_shader_type::kGlslOptShaderVertex);
    const std::string sFragmentShader = createShader(FRAGMENT_FILE, OPTIMIZE, ctx, glslopt_shader_type::kGlslOptShaderFragment);
    glslopt_cleanup(ctx);
#else
    const std::string sVertexShader = createShader(vprogram[std::max(0, which % 4)], OPTIMIZE);
    const std::string sFragmentShader = createShader(program[std::max(0, which % 4)], OPTIMIZE);
#endif
    shaderAttachFromString(gobanProgram, GL_VERTEX_SHADER, sVertexShader);
    shaderAttachFromString(gobanProgram, GL_FRAGMENT_SHADER, sFragmentShader);
    glLinkProgram(gobanProgram);

    GLint result;
    glGetProgramiv(gobanProgram, GL_LINK_STATUS, &result);

    if (result == GL_FALSE) {
        GLint length;
        char *log;

        glGetProgramiv(gobanProgram, GL_INFO_LOG_LENGTH, &length);
        log = (char*)malloc(static_cast<std::size_t>(length));
        glGetProgramInfoLog(gobanProgram, length, &result, log);

        console->info("sceneInit(): Program linking failed: {0}", log);
        free(log);

        glDeleteProgram(gobanProgram);
        gobanProgram = 0;
    }

    console->info("postShaderGetProgram = {0}", glGetError());

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
    iAA = glGetUniformLocation(gobanProgram, "AA");
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

    AA = 1;
    glUniform1i(iAA, AA);
    glUniform1f(iAnimT, animT);
    glUseProgram(0);
    console->info("postShaderUniformsLocations = {0}", glGetError());
    shaderChanged = true;

}

void GobanShader::setGamma(float gamma) {
    this->gamma = gamma;
}

void GobanShader::setContrast(float contrast) {
    this->contrast = contrast;
}

void GobanShader::setMetrics(const Metrics &m) {

    //std::cerr << width << " x " << height << " ... " << shadersReady << " ---" << m.fNDIM << std::endl;
    if(!shadersReady || m.fNDIM <= 0)
        return;

    float fNDIM(m.fNDIM);

    float boardaa(sqrt(2.0f/width/height));
    float boardbb(sqrt(1.0f/width/height));
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

    float r2 = b*r1/(2.0f*sqrt(r1*r1-px*px)); //ellipsoid
    float dn[3];
    dn[0] = dn[2] = 0.0f; dn[1] = d;
    float pxs = 0.5f*(0.5f*w - px);
    float rrr[3];
    rrr[0] = rrr[2] = r2*r2*r1*r1; rrr[1] = r1*r1*r1*r1;
    float maxBound[3];
    maxBound[0] = maxBound[2] = 1.2f;
    maxBound[1] = h;

    //ww = 2.0f/(fNDIM - 1.0f + 2.0f*0.85f);
    //float stoneRadius = 0.25f * (w*w/h + h);
    glUniform1f(fsu_fNDIM, fNDIM);
    glUniform1f(fsu_boardaa, boardaa); //aa
    glUniform1f(fsu_boardbb, boardbb); //aa
    glUniform1f(fsu_boardcc, 8.0f*boardaa); //aa
    glUniform1f(fsu_ww, ww); //square size
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

const std::array<float, 4> GobanShader::programH = { { 0.85f, 0.85f, 0.0f, 0.85f } };

void GobanShader::init() {
	std::cerr << "preShaderInit = " << glGetError() << std::endl;
	
	vertexBuffer = make_buffer(GL_ARRAY_BUFFER, &vertexBufferData[0], sizeof(GLfloat)*vertexBufferData.size());
    elementBuffer = make_buffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferData, sizeof(elementBufferData));

	currentProgram = 0;
    currentProgramH = programH[0];

    initProgram(currentProgram);

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
        glUniform1i(iBlackCapturedCount,  view.state.capturedBlack);
        glUniform1i(iWhiteCapturedCount, view.state.capturedWhite);
		glUniform3fv(iddc, 2 * model.metrics.maxc, model.metrics.tmpc);
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

void GobanShader::cycleShaders() {
    currentProgram = (currentProgram + 1) % 4;
    initProgram(currentProgram);
    currentProgramH = programH[currentProgram];
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
    glUniformMatrix4fv(iModelView, 1,  false, glm::value_ptr(m));
}

void GobanShader::setResolution(float w, float h) {
    width = w;
    height = h;
}
