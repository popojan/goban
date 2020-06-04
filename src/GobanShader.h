#ifndef GOBAN_GOBANSHADER_H
#define GOBAN_GOBANSHADER_H

#include <string>
#include "glyphy/GlyphyFont.h"
#include "Metrics.h"
#include "GobanModel.h"

class GobanView;

#include "OpenGL.h"
#include "Board.h"
#include "GobanModel.h"
#include "Camera.h"

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "spdlog/spdlog.h"

GLuint shaderCompileFromString(GLenum type, const std::string& source);

GLuint make_buffer(GLenum target, const void *, GLsizei);

std::string createShaderFromFile(const std::string& filename);

class GobanShader {
public:
    GobanShader(const GobanView& view): shadersReady(false), shaderChanged(false),
        currentProgram(-1), ww(-1.0f), stoneh(-1.0f),
        width(0), height(0), gamma(1.0f), contrast(0.0f),
	view(view), animT(0.5f)
    {
        init();
    }
    void initProgram(const std::string& vprogram, const std::string& fprogram);
    void setMetrics(const Metrics &);
    void init();
    void destroy();
    void draw(const GobanModel&, const DDG::Camera&, int, float);
    int choose(int idx);
    void use();
    void unuse();
    void setTime(float);
    void setPan(glm::vec3);
    void setRotation(glm::mat4x4);
    void setResolution(float, float);
    void setGamma(float);
    void setContrast(float);
    float getGamma(){ return gamma;}
    float getContrast(){ return contrast;}
    bool isReady() { return shadersReady;}
    float getStoneHeight() const { return currentProgramH; }
    void setReady() { shadersReady = true; }
    int getCurrentProgram() const {return currentProgram;}
    bool shaderAttachFromString(GLuint program, GLenum type, const std::string& source);
private:
    GLuint gobanProgram = 0;
    GLuint iVertex = 0;
    GLint iMouse;
    GLint iDim;
    GLint iModelView;
	GLint iStone;
    GLint iResolution;
    GLint iStones;
    GLuint bufStones;
    GLuint uBlockIndex;
    GLuint blockBindingPoint = 1;
    GLuint vertexBuffer, elementBuffer;
    GLint iGamma;
    GLint iContrast;
    GLint fsu_fNDIM;
    GLint fsu_boardaa;
    GLint fsu_boardbb;
    GLint fsu_boardcc;
    GLint fsu_ww;
    GLint fsu_iww;
    GLint fsu_w;
    GLint fsu_h;
    GLint fsu_stoneRadius;
    GLint fsu_d;
    GLint fsu_stoneradius2;
    GLint fsu_dn;
    GLint fsu_b;
    GLint fsu_y;
    GLint fsu_px;
    GLint fsu_pxs;
    GLint fsu_r1;
    GLint fsu_r2;
    GLint fsu_r123r123;
    GLint fsu_rrr;
    GLint fsu_r1r1ir2ir2;
    GLint fsu_maxBound;
    GLint fsu_dw;
    GLint fsu_iscale;
    GLint fsu_bowlRadius;
    GLint fsu_bowlRadius2;
    GLint fsu_cc;

    GLint iWhiteCapturedCount;
    GLint iBlackCapturedCount;
    GLint iddc;


    int AA;

    static const std::array<GLfloat, 16> vertexBufferData;
    static const GLushort elementBufferData[];

    GLint iTranslate;
    GLint iTime;
    GLint iAnimT;
    GLint iAA;

    GLuint vertexShader;
    GLuint fragmentShader;

    bool shadersReady;
    bool shaderChanged;

    int currentProgram;
    float currentProgramH;

    float ww, stoneh;
    float width, height;
    float gamma, contrast;

    const GobanView& view;

public:
    const float animT;
};

#endif
