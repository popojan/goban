in vec3 rdbl;
in vec3 rdbr;
flat in vec3 rool;
flat in vec3 roor;

// Which eye this pass is drawing: 0 left (red channel), 1 right (green+blue).
//
// One pass per eye, rather than both eyes in one invocation. The ray work is
// identical either way — this shader called render() twice per fragment, and now
// two passes call it once each — but the depth buffer can then hold *one* eye's
// occlusion at a time, which is the only way an annotation can be hidden behind
// the stone that eye actually sees.
//
// Sharing the buffer forced a choice between two wrong answers. `min(dl, dr)`
// classified a pixel as a stone wherever *either* eye saw one, so the other
// eye's text was clipped along a silhouette it should have been drawn past —
// visible as a best-move letter with its right-hand side missing. `max()` swaps
// that for text painted over a stone. Neither is a display limitation; both were
// artefacts of two eyes writing one number.
uniform int eye;

// How much colour survives the split: 0 gray, 1 half-colour, 2 colour,
// 3 Dubois. The order is Stereo::Anaglyph's, and Stereo::anaglyphUniform() is
// what converts it, so the enum and this cannot drift.
uniform int anaglyph;

// How much of this eye's own colour survives against its plain brightness: 1 is
// the mode as published, 0 collapses it to grey. Both ghosting and rivalry scale
// with how different the two eyes' images are, so this is one continuous dial
// between them. See Stereo::DEFAULT_STRENGTH.
uniform float anaglyphStrength;

// Crosstalk cancellation, per channel corrected — see Stereo::Crosstalk, which
// carries the derivation. The short version: each eye pre-subtracts its own
// image from the channels the *other* eye reads, so what leaks through an
// imperfect filter cancels at the eye rather than doubling the picture.
//
// These go negative, which is the entire point and the reason the composite may
// need a float target: a fixed-point framebuffer clamps a negative fragment to
// zero before the blend can subtract it.
uniform vec3 anaglyphLeak;

// Per-eye gain: (left, right). Filters do not pass the same amount of light —
// a blue lens is markedly darker than a red one — and a sustained brightness
// mismatch between the eyes is its own source of rivalry and strain, quite apart
// from colour. See Stereo::EyeBalance.
uniform vec2 anaglyphBalance;

// Which channels reach which eye: 0 red/cyan, 1 red/blue. A property of the
// glasses, not of the mode — see Stereo::Glasses. Green is the whole question: a
// cyan lens passes it, so it belongs to the right eye; a blue lens blocks it
// while the red lens leaks it, so it belongs to the *left*. Whichever eye gets
// green has two channels, and is therefore the only eye that can carry hue.
uniform int glasses;

// How much of the green channel the colour modes use, 0..1. Real lenses disagree
// about green — a cheap pair passes it through *both* filters — so whichever eye
// owns it, the other sees a ghost proportional to how much is there. Scaling it
// trades colour against the double image continuously. See Stereo::DEFAULT_GREEN,
// and note this is not anaglyphStrength: grey has full green, so desaturating
// does nothing to this ghost.
uniform float anaglyphGreen;

// Dubois' least-squares projection (Eric Dubois, "A projection method to
// generate anaglyph stereo images"), which picks the channel mix that minimises
// the perceived error under real red/cyan filters rather than assuming the
// filters are ideal. It is why a Dubois anaglyph keeps recognisable colour with
// far less rivalry than the naive split.
//
// The negative coefficients are its crosstalk pre-compensation: they cancel what
// leaks through a real filter, which is the same job Stereo::Crosstalk does with
// a number you can measure yourself.
//
// They only survive if the composite is accumulated somewhere that can hold a
// value below zero. The two eyes are summed by additive blending in separate
// passes — that is what buys per-eye depth — and a fixed-point framebuffer clamps
// a negative fragment to zero before the blend ever sees it. So this mode is one
// of the two that route through StereoComposite's float target; on the direct
// path it silently lost them, giving (0.689, 0.778, 0.230) on the shipped wood
// where exact Dubois gives (0.655, 0.703, 0.150) — not a rounding error,
// especially in blue.
// Kept as named rows rather than a mat3, because `mat3(a, b, c)` builds from
// *columns* and `m[0]` reads one back — so writing the published rows into a
// mat3 and dotting with m[i] silently transposes the matrix, which is a
// different projection that still looks plausible.
const vec3 duboisLeftR  = vec3( 0.4155,  0.4710,  0.1670);
const vec3 duboisLeftG  = vec3(-0.0458, -0.0484, -0.0257);
const vec3 duboisLeftB  = vec3(-0.0545, -0.0614,  0.0128);
const vec3 duboisRightR = vec3(-0.0109, -0.0365, -0.0060);
const vec3 duboisRightG = vec3( 0.3756,  0.7333,  0.0111);
const vec3 duboisRightB = vec3(-0.0651, -0.1287,  1.2971);

// The flat green the Gray mode holds the channel at. It is not carrying either
// eye — it is a haze that keeps the image off pure magenta — and it must be
// emitted by exactly one of the two passes, or the additive blend doubles it.
const float grayGreen = 0.1;

// This eye's contribution to the composite. The other eye's pass adds its own,
// so every mode writes zero into the channels it does not own — anything else
// would be one eye bleeding into the other's image, which is ghosting by
// construction.
vec3 eyeContribution(vec3 c)
{
    // The same (r+g+b)/3 the overlay's eyeInk() uses. Two brightness
    // conventions would let a label and the board under it disagree about how
    // dark a thing is.
    float lum = (c.r + c.g + c.b) / 3.0;

    // Desaturate toward that brightness first, so every mode below sees an
    // already-weakened colour and the dial works on all of them at once —
    // including Dubois, whose matrices are linear and so commute with it.
    c = mix(vec3(lum), c, anaglyphStrength);

    bool redBlue = (glasses == 1);

    if (eye == 0) {
        // Gray leaves green alone in both arrangements — it carries neither eye,
        // which is exactly why this mode works in either pair of glasses. The
        // flat haze rides with the left eye because one pass has to emit it and
        // the additive blend would otherwise double it.
        if (anaglyph == 0) return vec3(lum, grayGreen, 0.0);

        if (redBlue) {
            // Green is the left eye's here, so *this* is the eye with two
            // channels and the only one that can show hue. Half-colour and
            // colour agree: the coloured eye always keeps its own channels; what
            // they differ about is the other eye, below. Dubois lands here too,
            // its red/cyan matrices being the wrong projection for these lenses.
            return vec3(c.r, anaglyphGreen * c.g, 0.0);
        }
        if (anaglyph == 2) return vec3(c.r, 0.0, 0.0);
        if (anaglyph == 3) return vec3(dot(duboisLeftR, c),
                                       anaglyphGreen * dot(duboisLeftG, c),
                                       dot(duboisLeftB, c));
        // Half-colour: this is the grey eye under red/cyan.
        return vec3(lum, 0.0, 0.0);
    }

    // Blue alone, in every mode and both arrangements — no lens disagrees about
    // blue. What differs is what is put in it.
    if (anaglyph == 0) return vec3(0.0, 0.0, lum);

    if (redBlue) {
        // The grey eye here. Full colour hands it the scene's own blue, which on
        // a yellow-brown board is nearly nothing and reads as a dark, unfusable
        // mess; half-colour hands it brightness, which is what makes the pair
        // fuse. Dubois degrades to the latter.
        return (anaglyph == 2) ? vec3(0.0, 0.0, c.b) : vec3(0.0, 0.0, lum);
    }
    if (anaglyph == 3) return vec3(dot(duboisRightR, c),
                                   anaglyphGreen * dot(duboisRightG, c),
                                   dot(duboisRightB, c));
    // Half-colour and colour agree here: the coloured eye keeps its channels.
    return vec3(0.0, anaglyphGreen * c.g, c.b);
}

// What this eye must pre-subtract so its own image cancels where it leaks into
// the other eye's channels. Driven by this eye's brightness, applied to the
// channels this eye does *not* own — see Stereo::Crosstalk for why it cannot be
// applied to the channels the leak actually arrives in.
vec3 crosstalkCorrection(vec3 c)
{
    float lum = (c.r + c.g + c.b) / 3.0;
    bool redBlue = (glasses == 1);
    // A channel's correction is driven by the eye that does *not* own it, so the
    // ownership of green flips this with the glasses exactly as it flips the
    // composite above.
    // The green terms scale with anaglyphGreen for the same reason the image
    // does: they cancel a ghost whose size is what the dial just changed, and a
    // correction fitted at full green would cut a hole at half of it.
    if (eye == 0) {
        return redBlue ? vec3(0.0, 0.0, anaglyphLeak.b * lum)
                       : vec3(0.0, anaglyphGreen * anaglyphLeak.g * lum,
                              anaglyphLeak.b * lum);
    }
    return redBlue ? vec3(anaglyphLeak.r * lum, anaglyphGreen * anaglyphLeak.g * lum, 0.0)
                   : vec3(anaglyphLeak.r * lum, 0.0, 0.0);
}

void main(void)
{
    vec3 c = render(eye == 0 ? rool : roor,
                    normalize(eye == 0 ? rdbl : rdbr));
    gl_FragDepth = sceneDepth;

    // Balance before both of the following, not after: the crosstalk correction
    // is fitted to how bright the image actually is, so a gain applied afterwards
    // would leave the cancellation sized for a picture that is no longer on
    // screen. The flat green inside eyeContribution() is deliberately outside
    // this — it carries neither eye, so scaling it by one eye's gain would be
    // meaningless.
    c *= (eye == 0) ? anaglyphBalance.x : anaglyphBalance.y;

    glFragColor = eyeContribution(c) - crosstalkCorrection(c);
}
