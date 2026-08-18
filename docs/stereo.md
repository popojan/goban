# Stereoscopy

Everything the anaglyph shaders and the glyph overlay need to agree about, in
one place. `src/Stereo.h` is the implementation; this is the reasoning behind it.

## Sources

From the klub stereoskopické fotografie library, by **Matěj Boháč**:

- *Less is More! Keeping the Deviation Under Control* — <http://stereoskopie.cz/deviation.php>
- *Výpočet maximální deviace* (three parts) — <http://stereoskopie.cz/deviace_1.php>
- *Stereoskopické okno* — <http://stereoskopie.cz/okno.php>
- *Orthostereo, hyperstereo, hypostereo*

Plus Petr Duchoň, *Stereoskopická fotografie* (ČSF 1985). PDFs of all of these
live in `res/stereo.zip`, which is deliberately **not** tracked — third-party
articles, 8.6 MB. Nothing below depends on having them.

## The image model

The shaders trace rays through a plane the vertex shader calls `q0`:

```
q0.x ∈ [-aspect, aspect]      so the image is 2*aspect wide
q0.y ∈ [-1, 1]
focal length f = 3            (GobanView::FOCAL_LENGTH, matches the shaders)
```

A point at camera-space depth `z` and lateral offset `x` lands at `q0.x = f*x/z`.
Two eyes at ±`e` (half the stereo base) therefore separate it by `2*f*e/z`.

## Deviation

**Deviation** is the horizontal separation between the two images of a point,
as a fraction of the image width — and what must be bounded is the *difference*
between the nearest and the furthest point in frame:

```
deviation = base * f * (1/near - 1/far)  /  image_width
          = base * f / near              when far = infinity
```

**Ceiling: 1/30 of the image width** (Boháč; "MOFD", 1.2 mm on a 36 mm frame).
Projection is the demanding case and this is its number. Being more conservative
costs nothing — *"a picture with an exaggerated amount of space becomes
unusable, whereas restrained depth harms nothing"* — so `Stereo::
DEFAULT_DEVIATION` asks for 1/40.

Inverted, that is the whole of what the code computes:

```
halfBase = min(wanted, 1/30) * aspect * near / f        // Stereo::halfBase()
```

### It is set by the near point

Not by the distance to the subject. The board is a fixed-size object, so coming
closer shrinks the nearest point in frame far faster than it shrinks the camera
distance. The rule that scaled the base with `cameraDistance` measured:

| camera distance | near point | deviation | verdict |
|---|---|---|---|
| 3.1 (default) | 2.4 | 1/20 | over |
| 1.5 | 0.7 | 1/12 | over, 2.5× |
| 20 | 17 | 1/29 | just inside |

— an error that *grows* as you approach, so a check at one zoom says nothing
about another. `GobanView::stereoNearPoint()` asks the board box **and** the
table's near edge, because the nearest point is often not the subject but
"the blades of grass at the bottom edge": the board binds at ordinary zoom, the
table once the camera pulls back.

## The window is not the deviation

`dof` is the horizontal image shift (HIT) — the stereoscopic window. It slides
the whole depth range through the screen plane and cancels in near-minus-far, so
it decides where the scene sits relative to the glass, never how much depth the
eyes must accept. Parallel cameras, never toe-in: toe-in keystones each eye
differently, which is a vertical disparity the eyes cannot fuse away.

## Two traps, both found the hard way

**Do not normalize a ray direction in a vertex shader.** `rdb`/`rdbl`/`rdbr` are
varyings, so the fragment gets the interpolation of the four corner values, and
interpolating unit vectors is not interpolating directions — it is only correct
when the corners have equal length. Mono gets away with it, since `(±aspect, ±1,
f)` all have the same length; `dof` breaks that symmetry, so pre-normalizing
warped each eye and shrank the window to **85%** of the configured value (0.849
predicted, 0.853 measured at 4:3, `dof` 0.0925), varying across the screen. The
fragment shader normalizes anyway; that is where it belongs.

**One depth buffer, two eyes: take the nearer.** Depth here is a layer rather
than a distance — `partial/algorithm/render.glsl` classifies a pixel as 0.25
(board from below), 0.5 (a stone) or 0.75 (everything else), and the overlay
draws at 0.4 (label on a stone) or 0.6 (label on the board). The stereo shader
calls `render()` twice into one buffer, so writing `gl_FragDepth` inside it let
the second eye win: wherever the eyes disagreed about a stone, a best-move
letter showed straight through it. `render()` reports into `sceneDepth`;
`stereo/on.glsl` takes `min(dl, dr)`.

## In the code

| Where | What |
|---|---|
| `src/Stereo.h` | the two formulas, the ceiling, the default |
| `GobanView::stereoNearPoint()` | board box + the table's near edge |
| `GobanView::stereoHalfBase()` | the one base, uploaded to the shader **every frame** from `shadeIt()` and used by `GobanOverlay` for the same two eyes |
| `GobanOverlay::draw()` | one pass per eye: eye offset, off-axis frustum for the window, colour mask, depth writes off |
| `GobanOverlay::eyeInk()` | anaglyph is greyscale, so a label keeps only its brightness |
| `config/base.json` | `"stereo": 1` on the anaglyph shaders; `eof` is the deviation asked for, `dof` the window |

Scenarios assert `stereo_deviation` (bounded at 1/30), `stereo_near`,
`stereo_base` and `overlay_glyphs` — which doubles under a stereo shader, since
every label is drawn once per eye. See `tests/scenarios/stereo_depth_budget.scn`
and `tests/test_stereo.cpp`. **Assert at more than one zoom**: that is the
property the old rule lacked, and a single screenshot cannot see it.

## Combining the eyes: anaglyph modes

Everything above is about *where* the two images go. This is about how they are
carried in one picture, and it is where a real pair of glasses stops behaving
like the ideal filters the textbooks assume.

The left eye always owns red. What differs is what the right eye gets:

| Mode | Right eye | Left eye | Glasses |
|---|---|---|---|
| `gray` | blue only, green held at 0.1 | brightness | red/blue **and** red/cyan |
| `half-color` | its own green and blue | brightness | red/cyan |
| `color` | its own green and blue | its own red | red/cyan |
| `dubois` | least-squares mix | least-squares mix | red/cyan |

`gray` is the default because it is the forgiving one, and the reason is
physical: **a red filter blocks blue well and green badly.** Keeping the right
eye's image out of green means the worst leak has nothing to carry. Every other
mode must use green — half the colour information is there — so all three need a
genuinely cyan right lens. If the colour modes double up and `gray` does not, the
glasses are the reason and no amount of tuning will fix it.

Beyond the channels, the modes trade colour fidelity against **retinal
rivalry**: the discomfort of showing each eye a differently-coloured version of
one object. More colour is not simply better.

### Two dials for imperfect glasses

`anaglyph_strength` (0–1) is how much of each eye's own colour survives against
its plain brightness. Both ghosting and rivalry scale with how *different* the
two eyes' images are, so this is one continuous dial from the mode as published
down to `gray`. It reduces the leak; it does not cancel it.

`anaglyph_leak [r g b]` cancels it. Each eye pre-subtracts its own image from the
channels the other eye reads, so what gets through the wrong filter meets a
matching hole. The direction is the part worth stating carefully, because it is
not the obvious one:

> Light reaching an eye through the wrong channel **cannot be removed from the
> channel that eye cannot see.** It has to be pre-subtracted from the channel it
> *can*.
>
> The left eye receives `R + α·(right image, carried in G and B)`, so red must be
> written as `L − α·Rt` — a negative red term driven by the **right** eye's pass.
> Symmetrically, green and blue carry `Rt − β·L`, driven by the **left** eye's.

Each component names the channel being corrected, so `g` is the one to raise
first. Useful values are small — on the shipped wood, `0.12` already takes green
to nearly zero — and too much draws a dark hole where the other eye's image is,
which is why it is clamped at 0.5.

This is exactly the structure of Dubois' negative off-diagonal coefficients,
generalised to a number you can measure for the glasses on your face. Published
Dubois can do no better than the filters it was fitted to.

### Why the composite sometimes needs a float target

Dubois and any non-zero `anaglyph_leak` produce **negative** contributions — that
is what a cancellation is. The two eyes are summed by additive blending in
separate passes (see below), and a fixed-point framebuffer clamps a negative
fragment to zero *before* the blend, so on the direct path those terms are
silently discarded: the correction looks applied and does nothing. Measured on
the shipped wood, clamped Dubois gave `(0.689, 0.778, 0.230)` where exact Dubois
gives `(0.655, 0.703, 0.150)` — not a rounding error, especially in blue.

So `Stereo::needsSignedAccumulation()` routes exactly those configurations
through `StereoComposite`, an `RGBA16F` renderbuffer resolved by a blit — the
blit being the clamp, once, at the end, after every contribution has had its
chance to subtract. Everything else stays on the direct path, which is one
framebuffer cheaper and already verified against real glasses.

## One pass per eye

The stereo fragment shader renders **one eye per pass**, and the two are summed.
It used to render both in a single invocation, which forced the two eyes to share
one depth buffer, and one number per pixel cannot describe two different
occlusions: `min(dl, dr)` classified a pixel as a stone wherever *either* eye saw
one, so the other eye's annotation was clipped along a silhouette it should have
been drawn past — a best-move letter with its right-hand side missing. `max()`
only swaps that for text painted over a stone.

Rendering an eye at a time gives each its own depth, with a depth clear between
them. It is not slower: the shader always called `render()` twice per fragment,
and two passes call it once each. Measured on `tests/bench/`, 19×19 with 140
stones at 1024×768 — mono unchanged at 28.8 fps, stereo **12.4 → 14.9 fps**,
because halving the work per invocation improves occupancy.

The overlay's eye loop is driven by the caller for the same reason: each eye's
text must be tested against that eye's board.
