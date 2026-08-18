#include "StereoComposite.h"

#include <spdlog/spdlog.h>

bool StereoComposite::ensure(int w, int h) {
    if (unavailable) return false;
    if (fbo != 0 && w == width && h == height) return true;

    destroy();
    width = w;
    height = h;

    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &colorBuffer);
    glGenRenderbuffers(1, &depthBuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
    // Half floats: the composite needs to hold values below zero between the two
    // eye passes, and 16 bits of it is far more than a board needs.
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA16F, w, h);

    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, colorBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthBuffer);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        // Once, not once per frame. The caller keeps drawing without the
        // corrections, which is a worse anaglyph rather than no board at all.
        spdlog::warn("stereo: no float composite target (framebuffer status 0x{:x}); "
                     "Dubois and crosstalk cancellation will be clamped", status);
        destroy();
        unavailable = true;
        return false;
    }
    return true;
}

bool StereoComposite::begin(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    // Saved rather than assumed to be zero: this runs inside RmlUi's render, and
    // what it has bound is its business, not ours.
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    if (!ensure(w, h)) return false;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    // Both buffers, and the colour to zero: the passes *accumulate* into this,
    // so anything left from last frame would be added to rather than replaced.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return true;
}

void StereoComposite::end() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
    // The blit is the clamp: float in, normalised fixed point out, once, at the
    // end — after every contribution has been summed, which is the only place a
    // negative term can have done its job first.
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
}

void StereoComposite::destroy() {
    if (depthBuffer) glDeleteRenderbuffers(1, &depthBuffer);
    if (colorBuffer) glDeleteRenderbuffers(1, &colorBuffer);
    if (fbo) glDeleteFramebuffers(1, &fbo);
    depthBuffer = colorBuffer = fbo = 0;
    width = height = 0;
}
