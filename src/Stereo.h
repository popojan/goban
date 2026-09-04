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
#include <cctype>
#include <limits>
#include <optional>
#include <string>

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

/// The horizontal image shift that puts the **near point exactly at the screen
/// plane** — the stereoscopic window resting on the nearest thing in frame, so
/// nothing in the scene comes forward through it.
///
/// Substituting halfBase() into convergence() cancels the near point entirely:
///
///     convergence = f·e/dof = near   ⟺   dof = deviation · aspect
///
/// so the window needs no camera term at all, only the aspect ratio it is
/// measured against. That is why a fixed number could not do this job: it was
/// right at one shape of window and wrong at every other — 3.6% of the image
/// width behind the glass at 4:3 and 1.9% at 16:9, chosen by nobody. Exactly the
/// defect the stereo base had before it was tied to the near point.
///
/// `offset` moves the window off that resting place, as a fraction of image
/// width. Positive pushes the scene further behind the glass. Negative brings it
/// forward *through* the screen plane, which is legitimate stereoscopy and is
/// the more vivid picture — but the interface is drawn flat at that plane, so
/// anything in front of it collides with the menus. Nothing in the scene is
/// allowed forward of the window by default for that reason.
inline float window(float deviation, float aspect, float offset = 0.0f) {
    return (std::min(deviation, MAX_DEVIATION) + offset) * aspect;
}

/// How far the window may be shifted off the near point, either way. Small,
/// because the whole usable range is a few percent of image width.
constexpr float MAX_WINDOW_OFFSET = 0.05f;

inline float clampWindowOffset(float offset) {
    return std::min(MAX_WINDOW_OFFSET, std::max(-MAX_WINDOW_OFFSET, offset));
}

/// Camera-space depth of the **zero-parallax plane**: where the two eyes' images
/// of a point coincide, and so where the scene meets the glass. Nearer than this
/// is negative parallax — in front of the screen, out toward the viewer; further
/// is positive parallax, behind it.
///
/// From the same image model as everything above. The vertex shader shifts each
/// eye's image by `dof`, so the right eye sees a point at
/// `q0.x = dof + f·(x−e)/z` and the left at `q0.x = −dof + f·(x+e)/z`. Their
/// separation is `2·dof − 2·f·e/z`, which vanishes at `z = f·e/dof`.
///
/// This is the quantity `dof` decides and `halfBase()` deliberately does not.
/// It cancels in near-minus-far, so it changes nothing about how much depth the
/// eyes are asked to accept — only where that depth sits relative to the screen.
/// The two must not be traded against each other.
///
/// `dof == 0` means parallel cameras with no image shift, which converges only
/// at infinity: the whole scene then sits in front of the glass.
inline float convergence(float halfBase, float dof, float focal) {
    if (dof <= 0.0f) return std::numeric_limits<float>::infinity();
    return focal * halfBase / dof;
}

/** How the two eyes are combined into one anaglyph image.
 *
 * Orthogonal to Glasses below, which decides *which channels reach which eye*.
 * This decides how much colour is put into them. Exactly one eye can carry hue
 * in any arrangement — the one holding two channels — and which eye that is
 * comes from the glasses, not from here.
 *
 * `Gray` is the default and the only mode that works in either pair, because it
 * leaves green out altogether: green is the channel the two arrangements
 * disagree about, so a mode that never uses it cannot be wrong about it.
 *
 * The modes agree on one eye each — `Gray` and `HalfColor` send the same grey to
 * the eye that is not carrying colour, `HalfColor` and `Color` differ only in
 * whether the *other* eye gets brightness or its own channel — which is why the
 * shader's branches are not four.
 *
 * Beyond that they trade colour fidelity against *retinal rivalry*: the
 * discomfort of showing each eye a differently-coloured version of one object.
 * More colour is not simply better.
 */
enum class Anaglyph {
    Gray,        ///< Brightness only, green unused. Works in either pair of glasses.
    HalfColor,   ///< One eye keeps its colour, the other gets brightness.
    Color,       ///< Each eye keeps its own channels. Most colour, most rivalry.
    Dubois,      ///< Least-squares optimal projection. Red/cyan only; see duboisApplies().
};

/** Which eye each channel reaches, which is a property of the **glasses** and
 *  not of the mode.
 *
 * Red and blue are never in doubt: a red lens passes red and blocks blue, a
 * blue or cyan lens does the reverse. Green is the whole question, and the
 * answer decides where colour can live:
 *
 *  - **Red/cyan**: the cyan lens passes green, so green belongs to the *right*
 *    eye. The right eye then has two channels and is the one that can carry
 *    hue — which is what every published colour anaglyph method assumes.
 *  - **Red/blue**: the blue lens *blocks* green while the red lens leaks it, so
 *    green belongs to the *left* eye. The left eye has two channels and is the
 *    one that can carry hue. It is the mirror image, not a lesser case.
 *
 * Getting this backwards is not a subtle mistake: it delivers one eye's image
 * to the other eye, which is a second picture rather than a slightly wrong
 * colour. That shipped once — colour modes written for red/cyan, worn with
 * red/blue glasses, and the red lens saw two boards.
 *
 * Colour vision needs at least two channels, so **exactly one eye can carry
 * colour** in either arrangement, and which one flips with the glasses. That is
 * the whole content of half-colour: one eye coloured, one eye grey.
 */
enum class Glasses {
    RedCyan,   ///< Green to the right eye. What the published methods assume.
    RedBlue,   ///< Green to the left eye, because a blue lens blocks it.
};

inline const char* glassesName(Glasses g) {
    return g == Glasses::RedBlue ? "red-blue" : "red-cyan";
}

inline std::optional<Glasses> parseGlasses(std::string name) {
    for (char& c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (c == '_' || c == ' ' || c == '/') c = '-';
    }
    if (name == "red-cyan" || name == "redcyan" || name == "cyan") return Glasses::RedCyan;
    if (name == "red-blue" || name == "redblue" || name == "blue") return Glasses::RedBlue;
    return std::nullopt;
}

inline int glassesUniform(Glasses g) { return static_cast<int>(g); }

/// Which channels an eye's **image** occupies. Not merely which channels reach
/// the eye: in `Gray` the green channel holds a flat haze carrying neither eye,
/// so it belongs to nobody however the glasses split it.
///
/// The overlay masks its text with this, and the shader writes zero outside it.
/// Text in a channel the board is not using ghosts on its own.
struct EyeChannels { bool r = false, g = false, b = false; };

inline EyeChannels eyeChannels(Anaglyph mode, Glasses glasses, int eye) {
    const bool left = (eye == 0);
    if (mode == Anaglyph::Gray) {
        // Brightness only, and green left out entirely — which is what makes it
        // the mode that works in either pair of glasses.
        return left ? EyeChannels{true, false, false} : EyeChannels{false, false, true};
    }
    if (glasses == Glasses::RedBlue) {
        return left ? EyeChannels{true, true, false} : EyeChannels{false, false, true};
    }
    return left ? EyeChannels{true, false, false} : EyeChannels{false, true, true};
}

/// Whether this eye is the one that can carry hue: the one with two channels.
/// Which eye that is flips with the glasses, so nothing may assume it.
inline bool carriesColor(Anaglyph mode, Glasses glasses, int eye) {
    const auto ch = eyeChannels(mode, glasses, eye);
    return (static_cast<int>(ch.r) + static_cast<int>(ch.g) + static_cast<int>(ch.b)) > 1;
}

/// Dubois' matrices are fitted to red/cyan filters specifically. With red/blue
/// glasses they are simply the wrong projection — green would be sent to the eye
/// that cannot see it — so the mode degrades to half-colour rather than
/// pretending. Published matrices for amber/blue exist; these are not them.
inline bool duboisApplies(Glasses glasses) { return glasses == Glasses::RedCyan; }

inline const char* anaglyphName(Anaglyph mode) {
    switch (mode) {
        case Anaglyph::HalfColor: return "half-color";
        case Anaglyph::Color:     return "color";
        case Anaglyph::Dubois:    return "dubois";
        case Anaglyph::Gray:      break;
    }
    return "gray";
}

/// Spellings are forgiving on the two that have them — `grey`/`gray` and the
/// hyphen in `half-color`/`halfcolor`/`half_color` — because this is typed at a
/// command prompt and into a config file, not selected from a list.
inline std::optional<Anaglyph> parseAnaglyph(std::string name) {
    for (char& c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (c == '_' || c == ' ') c = '-';
    }
    if (name == "gray" || name == "grey" || name == "mono") return Anaglyph::Gray;
    if (name == "half-color" || name == "half-colour" || name == "halfcolor"
        || name == "halfcolour" || name == "half") return Anaglyph::HalfColor;
    if (name == "color" || name == "colour" || name == "full") return Anaglyph::Color;
    if (name == "dubois") return Anaglyph::Dubois;
    return std::nullopt;
}

/// The order the shader's `anaglyph` uniform takes, so the enum and the GLSL
/// cannot drift apart silently.
inline int anaglyphUniform(Anaglyph mode) { return static_cast<int>(mode); }

/// How much of each eye's own colour survives, against its plain brightness.
/// 1 is the mode as published, 0 collapses it to `Gray` whatever the mode.
///
/// This is the cheap half of coping with imperfect glasses, and it works because
/// **both ghosting and retinal rivalry scale with how different the two eyes'
/// images are**. Desaturating toward brightness shrinks that difference
/// continuously, so it is one dial from "colour I can enjoy" to "colour I can
/// fuse". It is a palliative — it reduces the leak rather than cancelling it,
/// which is what `leak` below does.
constexpr float DEFAULT_STRENGTH = 1.0f;

inline float clampStrength(float s) { return std::min(1.0f, std::max(0.0f, s)); }

/** Crosstalk cancellation, per channel: how much of the *other* eye's image to
 *  subtract so the leak through a real filter cancels at the eye.
 *
 * The direction is the thing to get right, and it is not the obvious one. Light
 * reaching an eye through the wrong channel **cannot be removed from the channel
 * the eye cannot see**. It has to be pre-subtracted from the channel the eye
 * *does* see:
 *
 *   left eye  receives  R + α·(right image, carried in G and B)
 *   so R must be written as  L − α·Rt   — a negative red term driven by the
 *   *right* eye's pass.
 *
 *   right eye receives  G,B + β·(left image, carried in R)
 *   so G,B must be written as  Rt − β·L — negative green and blue terms driven
 *   by the *left* eye's pass.
 *
 * So each component here names **the channel being corrected**: `.r` is applied
 * to red and driven by the right eye, `.g` and `.b` are applied to green and
 * blue and driven by the left eye. Green and blue are separate because they leak
 * differently — a red filter blocks blue well and green badly, which is the
 * whole reason `Gray` puts the right eye in blue alone.
 *
 * This is exactly the structure of Dubois' negative off-diagonal coefficients,
 * generalised to a number you can measure for the glasses actually on your face:
 * published Dubois can do no better than the filters it was fitted to.
 *
 * Zero by default, so nothing happens until asked for — and, since only these
 * terms and Dubois' own can go negative, zero is also what keeps the ordinary
 * modes on the cheap direct compositing path.
 */
struct Crosstalk {
    float r = 0.0f;   ///< Subtracted from red, driven by the right eye.
    float g = 0.0f;   ///< Subtracted from green, driven by the left eye.
    float b = 0.0f;   ///< Subtracted from blue, driven by the left eye.

    [[nodiscard]] bool any() const { return r > 0.0f || g > 0.0f || b > 0.0f; }
};

/// A correction beyond this is not cancelling a ghost, it is drawing a hole
/// where the other eye's image is — the subtraction outruns the signal and the
/// image goes black in the bright parts. Clamped rather than refused, because it
/// is tuned by eye against a moving target.
constexpr float MAX_CROSSTALK = 0.5f;

inline Crosstalk clampCrosstalk(Crosstalk c) {
    c.r = std::min(MAX_CROSSTALK, std::max(0.0f, c.r));
    c.g = std::min(MAX_CROSSTALK, std::max(0.0f, c.g));
    c.b = std::min(MAX_CROSSTALK, std::max(0.0f, c.b));
    return c;
}

/** How much of the green channel the colour modes actually use, 0 to 1.
 *
 * Green is the one channel real lenses disagree about. The clean model says a
 * cyan lens passes it and a blue lens blocks it, so it belongs to one eye or the
 * other — but cheap dyed lenses have broad, overlapping passbands, and green sits
 * in the middle of the spectrum. Measured on one such pair: giving green to the
 * right eye put a second picture in the *red* lens, and giving it to the left put
 * a second picture in the *blue* lens. Both, symmetrically. Green was reaching
 * both eyes whoever owned it.
 *
 * Which kills the clean split, but not the picture. Ghosting is proportional to
 * how much green is there, so scaling it down trades colour against the double
 * image continuously, and somewhere below 1 is the most colour a given pair of
 * lenses will carry. On a wooden board the loss is mild — suppressing green skews
 * the image warm, which is where it already lives.
 *
 * **This is not `strength`.** That desaturates toward luminance, and grey has
 * *full* green: turning it down leaves green's amplitude untouched and the ghost
 * exactly where it was. The two dials look similar and do opposite things — one
 * moves colour toward grey, this one moves the disputed channel toward black.
 *
 * Applied only where green carries an eye's image, never to `Gray`'s flat haze,
 * which is not anybody's picture and is what makes that mode ghost-free.
 */
constexpr float DEFAULT_GREEN = 1.0f;

inline float clampGreen(float g) { return std::min(1.0f, std::max(0.0f, g)); }

/// Whether a mode's colour depends on green at all. `Gray` does not — it is the
/// mode defined by leaving green out — so the dial is inert there, and saying so
/// keeps a user from tuning a knob that cannot move.
inline bool greenCarriesImage(Anaglyph mode) { return mode != Anaglyph::Gray; }

/** Per-eye gain, for glasses whose two filters do not pass the same amount of
 *  light.
 *
 * They rarely do. A blue lens is markedly darker than a red one, so with
 * red/blue glasses the right eye receives a dimmer picture than the left of the
 * same scene. The eyes do not average that away: a sustained brightness
 * mismatch is a source of rivalry and strain in its own right, independent of
 * colour, and it is the complaint people describe as "3D gives me a headache".
 *
 * This is the one adjustment that genuinely helps a red/blue setup, where the
 * colour modes cannot help at all — each eye there receives exactly one channel,
 * and colour vision needs two.
 *
 * Applied to the eye's colour *before* the mode's projection, so the crosstalk
 * terms scale with it: a brighter image leaks proportionally more, and a
 * correction fitted at one gain would be wrong at another.
 */
struct EyeBalance {
    float left = 1.0f;
    float right = 1.0f;

    [[nodiscard]] bool isUnity() const { return left == 1.0f && right == 1.0f; }
};

/// Gain is a multiplier on light, so zero is black and negative is meaningless;
/// past 4 the highlights are so far into clipping that the picture is flat.
constexpr float MIN_BALANCE = 0.1f;
constexpr float MAX_BALANCE = 4.0f;

inline EyeBalance clampBalance(EyeBalance b) {
    b.left  = std::min(MAX_BALANCE, std::max(MIN_BALANCE, b.left));
    b.right = std::min(MAX_BALANCE, std::max(MIN_BALANCE, b.right));
    return b;
}

/// Whether the composite can go **negative**, and so needs to accumulate in a
/// float target rather than straight into the screen.
///
/// A fixed-point framebuffer clamps a negative fragment to zero before the blend
/// ever sees it, which silently discards exactly the terms that cancel a ghost.
/// Only Dubois' own coefficients and an explicit `Crosstalk` produce them, so
/// every other configuration stays on the direct path — the one already in use,
/// already verified, and one framebuffer cheaper.
inline bool needsSignedAccumulation(Anaglyph mode, const Crosstalk& leak) {
    return mode == Anaglyph::Dubois || leak.any();
}

} // namespace Stereo

#endif // GOBAN_STEREO_H
