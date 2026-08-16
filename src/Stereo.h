/** \file
 *  \brief The stereoscopic depth budget: one implementation, shared by the ray
 *         traced board and the glyph overlay.
 *
 * The rules here are from the stereophotography literature kept in
 * `res/stereo.zip` — Matěj Boháč, *Less is More! Keeping the Deviation Under
 * Control* and *Výpočet maximální deviace* (klub stereoskopické fotografie).
 * Two facts from those articles do all the work:
 *
 *  1. **Deviation** is the horizontal separation of the two images of a point,
 *     expressed as a fraction of the image width, and what must be bounded is
 *     the *difference* between the nearest and the furthest point in frame —
 *     `d = b·f·(1/x − 1/y)` on film, with the far point at infinity reducing it
 *     to `d = b·f/x`. It is set by the **near point**, not by the distance to
 *     the subject, and the near point "is often not the subject in the
 *     foreground, but the blades of grass at the bottom edge".
 *  2. The generally respected ceiling is **1/30 of the image width**, and being
 *     more conservative than that costs nothing: "a picture with an exaggerated
 *     amount of space becomes unusable, whereas restrained depth harms nothing".
 *
 * Our "film" is the shader's `q0` plane: `q0.x` spans [-aspect, aspect], so the
 * image is `2·aspect` wide, and the focal length is `FOCAL_LENGTH` in the same
 * units. A point at camera-space depth `z` lands at `q0.x = f·x/z`, so with a
 * half-base `e` the two eyes separate it by `2·f·e/z` — which is where every
 * formula below comes from.
 *
 * The horizontal image shift (`dof`, the stereoscopic window) deliberately does
 * not appear: it slides the whole depth range in or out of the screen and
 * cancels in the near-minus-far difference. It decides where the window sits;
 * these functions decide how much depth may pass through it.
 */
#ifndef GOBAN_STEREO_H
#define GOBAN_STEREO_H

#include <algorithm>

namespace Stereo {

/// The historically respected ceiling, as a fraction of image width. Projection
/// is the demanding case and this is its value; a desk monitor tolerates more,
/// but nothing is gained by spending the margin.
constexpr float MAX_DEVIATION = 1.0f / 30.0f;

/// What we ask for by default — comfortably inside the ceiling, which is the
/// article's own advice. A user who wants more depth can have it up to
/// MAX_DEVIATION and no further.
constexpr float DEFAULT_DEVIATION = 1.0f / 40.0f;

/// Deviation produced by a half-base `e`, as a fraction of image width.
/// `nearPoint` and `farPoint` are depths along the view axis; pass a huge
/// `farPoint` for a scene that runs to the horizon, which is the safe reading.
inline float deviation(float halfBase, float aspect, float nearPoint,
                       float farPoint, float focal) {
    if (nearPoint <= 0.0f || aspect <= 0.0f) return 0.0f;
    const float invFar = (farPoint > nearPoint) ? 1.0f / farPoint : 0.0f;
    return focal * halfBase * (1.0f / nearPoint - invFar) / aspect;
}

/// The half-base that puts `nearPoint` at exactly `deviation` of the image
/// width, with the far point at infinity — the inverse of the above, and the
/// conservative direction: a finite far point only ever needs *more* base to
/// reach the same deviation.
///
/// This is the whole fix. The base has to scale with the **near point**; the
/// board is a fixed-size object, so as the camera comes in, the near point
/// shrinks far faster than the camera distance does — at which point a base
/// tied to the camera distance runs away. Measured with the shipped
/// configuration: 1/20 of the image width at the default zoom, 1/12 zoomed in,
/// where the ceiling is 1/30.
inline float halfBase(float deviation, float aspect, float nearPoint, float focal) {
    if (focal <= 0.0f || nearPoint <= 0.0f) return 0.0f;
    return std::min(deviation, MAX_DEVIATION) * aspect * nearPoint / focal;
}

} // namespace Stereo

#endif // GOBAN_STEREO_H
