#ifndef GOBAN_GOBANSHADER_H
#define GOBAN_GOBANSHADER_H

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
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
        width(0), height(0), gamma(1.0f), contrast(0.0f),
        eof(Stereo::DEFAULT_DEVIATION), dof(0.0f), view(view), animT(0.5f)
    {
        init();
    }
    /// Joins any build still in flight. A std::thread destroyed while joinable
    /// calls std::terminate, and quitting during the very pause this class
    /// exists to hide is not an unlikely thing for a user to do.
    ~GobanShader() { joinBuild(); }

    GobanShader(const GobanShader&) = delete;
    GobanShader& operator=(const GobanShader&) = delete;

    void initProgram(const std::string& vprogram, const std::string& fprogram);
    void setMetrics(const Metrics &) const;
    void init();
    void destroy() const;
    void draw(const GobanModel&, int, float) const;
    int choose(int idx);

    /// Select a shader without blocking, when a shared GL context is available.
    ///
    /// Linking is the entire cost of selecting a shader — 2019 ms measured on
    /// Intel/Mesa with a cold driver cache against 19 ms to compile the fragment
    /// shader — and it is paid the first time *each* shader is used, not only at
    /// startup. Done on the UI thread it is a frozen window with nothing drawn
    /// and nothing to explain it.
    ///
    /// Falls back to choose() when there is no shared context, so behaviour on a
    /// driver or platform that will not give us one is exactly what it was.
    /// Returns the index that will be current once the build lands.
    int chooseAsync(int idx);

    /// Take delivery of a finished background build, on the UI thread. Returns
    /// true on the frame the program actually became current, which is the
    /// caller's cue to mark the view ready and repaint everything.
    ///
    /// The context-local half of the build happens here and cannot happen on the
    /// worker: glBindBufferRange() binds to the *context*, not to the program or
    /// the buffer, so a binding made on the worker's context leaves this one
    /// with no uniform buffer and the board draws from whatever was there.
    bool pollBuild();

    /// Whether a shader is being linked in the background right now.
    [[nodiscard]] bool isBuilding() const { return buildRunning; }

    /// Which shader is *selected* — the one being linked while a build is in
    /// flight, the current one otherwise. Widgets must ask this rather than
    /// getCurrentProgram(), which reports what is actually drawing and is -1
    /// until the first program lands.
    [[nodiscard]] int selectedProgram() const {
        return buildRunning ? buildTarget : currentProgram;
    }

    /// How long the build in flight has been going, in seconds. Reported to the
    /// user as a whole-second count — there is no way to estimate the total, so
    /// there is no honest progress bar to draw; a count that turns over once a
    /// second is the same answer ADR-0012 reached for the other waits.
    [[nodiscard]] double buildElapsed() const;
    void use() const;
    static void unuse() ;
    void setTime(float) const;
    void setCameraPan(glm::vec2) const;
    void setCameraDistance(float) const;
    void setStereoBase(float) const;
    /// The horizontal image shift, computed on the CPU (Stereo::window) like the
    /// base beside it. Uploaded every frame from shadeIt(), never from
    /// setMetrics(): it follows the aspect ratio, so a window resize changes it.
    void setStereoWindow(float) const;
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
    GLint fsu_cursorMark = -1;

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

    /// A shader entry resolved from the configuration: where its source is, and
    /// the appearance facts it declares. Carried rather than applied on the spot
    /// so an asynchronous build can take them on when the program actually
    /// arrives — see pollBuild().
    struct PendingShader {
        int index = -1;
        std::string vertex, fragment;
        float height = 0.0f;
        bool stereo = false;
        QualityPalette palette{};
    };

    [[nodiscard]] bool resolveShader(int idx, PendingShader& out) const;
    void applyShaderMetadata(const PendingShader&);
    void startBuild(const PendingShader&);

    /// The background link. Only two values cross the thread boundary — the
    /// program name the worker produced and the fact that it finished — which is
    /// why none of the ~50 uniform locations need publishing: they are queried
    /// on the UI thread in adoptProgram(), where a lookup in a linked program's
    /// symbol table costs microseconds. Keeping them on one thread is what makes
    /// this small enough to reason about.
    std::thread buildThread;
    std::atomic<bool> buildFinished{false};   ///< worker -> UI, the handover
    bool buildRunning = false;                ///< UI thread only
    GLuint buildResult = 0;                   ///< written by the worker, read after buildFinished
    int buildTarget = -1;                     ///< which shader index is being built
    std::chrono::steady_clock::time_point buildStarted;
    PendingShader pending;                    ///< what the worker is building
    PendingShader queued;                     ///< asked for while that was running
    bool hasQueued = false;

    /// Compile and link, touching nothing but locals and the GL context that is
    /// current. Safe on the worker because every object it makes — shaders, the
    /// program — is shared between contexts, and it sets no context state.
    /// Returns 0 on failure.
    static GLuint buildProgram(const std::string& vertexProgram,
                               const std::string& fragmentProgram);

    /// Install a linked program: the buffers, the binding point and every
    /// uniform location. UI thread only; see pollBuild().
    void adoptProgram(GLuint program);

    void joinBuild();

    const GobanView& view;

public:
    const float animT;
};

#endif
