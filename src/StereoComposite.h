/** \file
 *  \brief A float colour target for anaglyph composites that go negative.
 *
 * The two eyes are rendered in separate passes and summed by additive blending —
 * that is what gives each eye its own depth buffer, and so its own occlusion.
 * Summing works straight into the screen for as long as every contribution is
 * positive, which is every mode except Dubois and every configuration without
 * crosstalk cancellation.
 *
 * Those two produce **negative** contributions on purpose: they are how one
 * eye's image is pre-subtracted from the channels the other eye reads, so that
 * what leaks through an imperfect filter cancels instead of doubling the picture
 * (see Stereo::Crosstalk). A fixed-point framebuffer clamps a negative fragment
 * to zero *before* the blend, so on the direct path those terms are silently
 * discarded — the correction appears to be applied and does nothing.
 *
 * So when Stereo::needsSignedAccumulation() says so, the passes are accumulated
 * into an RGBA16F renderbuffer, where the sum can dip below zero and come back,
 * and the result is blitted to whatever framebuffer was bound before. The blit
 * is what clamps, once, at the end — which is the correct place.
 *
 * Renderbuffers rather than textures because nothing samples this; a blit reads
 * it. That also means no resolve shader, and so no second GLSL program to keep
 * in step with the first.
 *
 * Failure is not fatal and not fatal *quietly*: if the target cannot be created
 * the caller composites directly, having been told so, and loses the corrections
 * rather than the board.
 */
#ifndef GOBAN_STEREOCOMPOSITE_H
#define GOBAN_STEREOCOMPOSITE_H

#include "OpenGL.h"

class StereoComposite {
public:
    ~StereoComposite() { destroy(); }

    /// Binds a float colour target of this size, creating or resizing it as
    /// needed, and clears it. Returns false if it could not be set up — nothing
    /// is bound in that case and the caller must composite straight to the
    /// screen.
    bool begin(int w, int h);

    /// Blits the accumulated image to the framebuffer that was bound before
    /// begin(), clamping to displayable range in the process, and restores that
    /// binding. Safe to call only after a begin() that returned true.
    void end();

    void destroy();

private:
    bool ensure(int w, int h);

    GLuint fbo = 0;
    GLuint colorBuffer = 0;
    GLuint depthBuffer = 0;
    GLint  previousFbo = 0;
    int width = 0;
    int height = 0;
    /// Latched after a failed setup, so a driver that cannot give us a float
    /// target is asked once rather than once per frame.
    bool unavailable = false;
};

#endif // GOBAN_STEREOCOMPOSITE_H
