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
    explicit GobanShader(const GobanView& view): shadersReady(false), currentProgram(-1),
        width(0), height(0), gamma(1.0f), contrast(0.0f), view(view), animT(0.5f),
        currentProgramH(.0), eof(0.0075), dof(0.01)
    {
        init();
    }
    void initProgram(const std::string& vprogram, const std::string& fprogram);
    void setMetrics(const Metrics &) const;
    void init();
    void destroy() const;
    void draw(const GobanModel&, int, float);
    int choose(int idx);
    void use() const;
    static void unuse() ;
    void setTime(float) const;
    void setPan(glm::vec3) const;
    void setRotation(glm::mat4x4) const;
    void setResolution(float, float);
    void setGamma(float);
    void setContrast(float);
    void setEof(float);
    void setDof(float);
    float getEof();
    float getDof();
    [[nodiscard]] float getGamma() const { return gamma;}
    [[nodiscard]] float getContrast() const { return contrast;}
    [[nodiscard]] bool isReady() const { return shadersReady;}
    [[nodiscard]] float getStoneHeight() const { return currentProgramH; }
    void setReady() { shadersReady = true; }
    [[nodiscard]] int getCurrentProgram() const {return currentProgram;}
    bool shaderAttachFromString(GLuint program, GLenum type, const std::string& source);
private:
    GLuint gobanProgram = 0;
    GLuint iVertex = 0;
    GLint iDim = -1;
    GLint iModelView = -1;
    GLint iResolution = -1;
    GLuint bufStones = 0;
    GLuint uBlockIndex = 0;
    GLuint blockBindingPoint = 1;
    GLuint vertexBuffer = 0, elementBuffer = 0;
    GLint iGamma = -1;
    GLint iContrast = -1;
    GLint fsu_fNDIM = -1;
    GLint fsu_boardaa = -1;
    GLint fsu_boardbb = -1;
    GLint fsu_boardcc = -1;
    GLint fsu_wwx = -1;
    GLint fsu_wwy = -1;
    GLint fsu_w = -1;
    GLint fsu_h = -1;
    GLint fsu_stoneRadius = -1;
    GLint fsu_d = -1;
    GLint fsu_stoneradius2 = -1;
    GLint fsu_dn = -1;
    GLint fsu_b = -1;
    GLint fsu_y = -1;
    GLint fsu_px = -1;
    GLint fsu_pxs = -1;
    GLint fsu_r1 = -1;
    GLint fsu_r2 = -1;
    GLint fsu_r123r123 = -1;
    GLint fsu_rrr = -1;
    GLint fsu_r1r1ir2ir2 = -1;
    GLint fsu_maxBound = -1;
    GLint fsu_dw = -1;
    GLint fsu_iscale = -1;
    GLint fsu_bowlRadius = -1;
    GLint fsu_bowlRadius2 = -1;
    GLint fsu_cc = -1;

    GLint vsu_eof = -1;
    GLint vsu_dof = -1;

    GLint iWhiteCapturedCount = -1;
    GLint iBlackCapturedCount = -1;
    GLint iWhiteReservoirCount = -1;
    GLint iBlackReservoirCount = -1;
    GLint iddc = -1;
    GLint fsu_cursor = -1;

    static const std::array<GLfloat, 16> vertexBufferData;
    static const GLushort elementBufferData[];

    GLint iTranslate = -1;
    GLint iTime = -1;
    GLint iAnimT = -1;

    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;

    bool shadersReady;

    int currentProgram;
    float currentProgramH{};

    float width, height;
    float gamma, contrast;
    float eof, dof;

    const GobanView& view;

public:
    const float animT;
};

#endif
