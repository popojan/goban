#ifndef GOBAN_GOBANSHADER_H
#define GOBAN_GOBANSHADER_H

#include <string>
#include "AnalysisService.h"
#include "Metrics.h"
#include "Stereo.h"
#include "GobanModel.h"

class GobanView;

#include "OpenGL.h"
#include "Board.h"

#include "spdlog/spdlog.h"

GLuint shaderCompileFromString(GLenum type, const std::string& source);

GLuint make_buffer(GLenum target, const void *, GLsizei);

std::string createShaderFromFile(const std::string& filename);

class GobanShader {
public:
    explicit GobanShader(const GobanView& view): shadersReady(false), currentProgram(-1),
        width(0), height(0), gamma(1.0f), contrast(0.0f), eof(0.0725), dof(0.0925), view(view), animT(0.5f)
    {
        init();
    }
    void initProgram(const std::string& vprogram, const std::string& fprogram);
    void setMetrics(const Metrics &) const;
    void init();
    void destroy() const;
    void draw(const GobanModel&, int, float) const;
    int choose(int idx);
    void use() const;
    static void unuse() ;
    void setTime(float) const;
    void setCameraPan(glm::vec2) const;
    void setCameraDistance(float) const;
    void setStereoBase(float) const;
    /// Which eye the next draw renders: 0 left, 1 right. A no-op under a mono
    /// shader, which has no such uniform — the location is simply -1.
    void setEye(int) const;
    /// How the two eyes are combined; see Stereo::Anaglyph. Also a no-op in mono.
    void setAnaglyph(Stereo::Anaglyph) const;
    /// How much of each eye's own colour survives against its brightness.
    void setAnaglyphStrength(float) const;
    /// Per-channel crosstalk cancellation; see Stereo::Crosstalk.
    void setAnaglyphLeak(const Stereo::Crosstalk&) const;
    /// Per-eye gain, for filters that pass different amounts of light.
    void setAnaglyphBalance(const Stereo::EyeBalance&) const;
    /// Which channels reach which eye; see Stereo::Glasses.
    void setGlasses(Stereo::Glasses) const;
    /// How much green the colour modes use; see Stereo::DEFAULT_GREEN.
    void setAnaglyphGreen(float) const;
    void setRotation(glm::mat4x4) const;
    void setResolution(float, float);
    void setGamma(float);
    void setContrast(float);
    void setEof(float);
    void setDof(float);
    float getEof() const;
    float getDof() const;
    [[nodiscard]] float getGamma() const { return gamma;}
    [[nodiscard]] float getContrast() const { return contrast;}
    [[nodiscard]] bool isReady() const { return shadersReady;}
    /// Whether the selected shader renders an anaglyph, from the shader's own
    /// `"stereo"` entry in the config rather than from its file name: it is a
    /// property of the shader, and everything that has to follow it — the eye
    /// offset the overlay draws with, the greyscale its ink collapses to — is
    /// out here rather than in GLSL.
    [[nodiscard]] bool isStereo() const { return currentProgramStereo; }
    [[nodiscard]] float getStoneHeight() const { return currentProgramH; }
    /// The move-quality ink for the selected shader: the global `annotations`
    /// block with this shader's own laid over it.
    ///
    /// Here for the same reason `isStereo()` is — the palette is a property of
    /// the shader, but its consumer is the overlay, which bakes colour into the
    /// glyph buffers on the CPU. A `const vec3` in a GLSL partial could never
    /// reach it; the ray-traced board draws no letters, so the constant would
    /// sit unused in six fragment shaders. So it is declared in the shader's
    /// config entry beside `stereo` and `height`, and resolved out here.
    ///
    /// No invalidation of its own: `GobanView::switchShader()` already raises
    /// `UPDATE_ALL`, which rebuilds the glyph buffers — the mechanism that makes
    /// the anaglyph greyscale work is the one that repaints a new palette.
    [[nodiscard]] const QualityPalette& qualityPalette() const { return currentPalette; }
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
    GLint fsu_eye = -1;
    GLint fsu_anaglyph = -1;
    GLint fsu_anaglyphStrength = -1;
    GLint fsu_anaglyphLeak = -1;
    GLint fsu_anaglyphBalance = -1;
    GLint fsu_glasses = -1;
    GLint fsu_anaglyphGreen = -1;

    GLint iWhiteCapturedCount = -1;
    GLint iBlackCapturedCount = -1;
    GLint iWhiteReservoirCount = -1;
    GLint iBlackReservoirCount = -1;
    GLint iddc = -1;
    GLint fsu_cursor = -1;

    static const std::array<GLfloat, 16> vertexBufferData;
    static const GLushort elementBufferData[];

    GLint iCameraPan = -1;
    GLint iCameraDistance = -1;
    GLint iTime = -1;
    GLint iAnimT = -1;

    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;

    bool shadersReady;

    int currentProgram;
    float currentProgramH{};
    bool currentProgramStereo{false};
    QualityPalette currentPalette{};

    float width, height;
    float gamma, contrast;
    float eof, dof;

    const GobanView& view;

public:
    const float animT;
};

#endif
