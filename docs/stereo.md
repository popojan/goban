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

### Where the screen plane actually lands

The shader shifts each eye's image by `dof`, so a point at depth `z` is seen by
the right eye at `q0.x = dof + f·(x−e)/z` and by the left at `−dof + f·(x+e)/z`.
Their separation is `2·dof − 2·f·e/z`, which vanishes at

```
convergence = f·e/dof                       // Stereo::convergence()
```

Everything nearer than that is in front of the glass, everything further behind
it. `GobanView::stereoConvergence()` reports it and `dumpState()` publishes it as
`stereo_convergence`, beside `stereo_board_near`/`stereo_board_far` — because the
question anyone asks of it is a comparison, not a distance.

Substituting `e = dev·aspect·near/f` cancels the near point entirely:

```
convergence = near   ⟺   dof = dev · aspect      // Stereo::window()
```

So the window needs no camera term — only the aspect ratio it is measured
against — and that is what it is set to. **The window rests on the nearest thing
in frame at every zoom and every aspect ratio**, so the scene recedes behind the
glass and nothing comes forward through it.

`dof` is now an *offset* from that resting place, in fractions of image width,
default 0 and clamped to ±0.05. Positive sinks the scene further back; negative
brings it forward through the screen plane.

### Why the near point, and not further forward

A fixed `dof` was wrong in a way no single screenshot shows. The shipped 0.0925
put the near point **3.6% of the image width behind the glass at 4:3 and 1.9% at
16:9** — the whole board behind the screen by an amount nobody chose, different on
every monitor. Everything in the scene was in positive parallax, which is why the
RmlUi interface, flat at the screen plane by construction, appeared to float in
front of the board.

Forward of the near point is more vivid and was tried: the board reads better
with the plane nearer its middle. It is rejected because **the interface is drawn
at the screen plane**, so anything in negative parallax intersects the menus —
the classic window violation, with the window frame being the UI itself. The near
point is therefore the furthest forward the window can go, and it is where it
sits.

**Measured** before the change, 19×19 at the default camera, 1024×768, board
spanning 2.42 to 3.99:

| `dof` (old absolute) | convergence | disparity at the board (measured / predicted) |
|---|---|---|
| 0.0925 (was shipped) | 1.16 | +41 / +40 px near, +46 / +49 px far |
| 0.0450 | 2.39 | +5 / +4 px near, +9 / +12 px far |
| 0 (no shift) | ∞ | −30 / −31 px near, −25 / −22 px far |

Positive is behind the screen. Measured by cross-correlating the red and blue
channels of a `gray` anaglyph screenshot — the two columns agreeing across a sign
flip is the check that matters, not either column alone.

The old arrangement also put the far field 6.9% of the image width behind the
screen, which on a 2 m projection is 139 mm of positive parallax: past the
interocular distance, so the eyes diverge. Projection is the case the 1/30
ceiling is chosen for, so the fixed window contradicted the very rule the base
follows.

`stereo_convergence_ratio` reports convergence over the near point — 1 when the
window is resting on it. A ratio rather than a distance because that is the
invariant: it holds at every zoom and aspect, where a distance is camera
specific.

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
| `GobanView::cameraBasis()` | the camera model the vertex shaders build, in one place |
| `GobanView::stereoWindow()` | the image shift, derived like the base and uploaded beside it |
| `GobanView::stereoConvergence()` | where the scene meets the glass |
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
physical: **green is the only channel real lenses disagree about.** Red and blue
are never in doubt — a red lens passes red and blocks blue, the other lens does
the reverse — so a mode that leaves green out cannot be wrong about anything.

Which eye owns green is a property of the **glasses**, not of the mode, and that
is what `glasses` selects:

- **red/cyan**: the cyan lens passes green, so green belongs to the *right* eye.
- **red/blue**: the blue lens blocks green while the red lens leaks it, so green
  belongs to the *left* eye.

Whichever eye holds two channels is the only eye that can carry hue — colour
vision needs two — so exactly one eye is the coloured one, and which one flips.
Getting this backwards delivers one eye's picture to the other, which appears as
a **second image** rather than a wrong colour. If the colour modes double up and
`gray` does not, this is the first setting to change.

### When green reaches both eyes

The split above is the clean model. Cheap dyed lenses do not honour it: their
passbands are broad and overlapping, and green sits in the middle of the visible
spectrum, so both filters pass a good part of it. Measured on one such pair —
giving green to the right eye put a second picture in the **red** lens; giving it
to the left put a second picture in the **blue** lens. Both, symmetrically.

That is not a bug in either assignment. It means green carries to both eyes
whoever owns it, so with those lenses only red and blue are cleanly separated —
one channel per eye, one scalar per eye — and hue, needing two, is impossible.
`gray` is then not a fallback but the correct answer, and its flat green is
exactly right: it is the only arrangement that puts nothing in the channel both
eyes can see.

Short of that, ghosting is proportional to *how much* green is present, so
`anaglyph_green` (0–1) trades colour against the double image continuously, and
somewhere below 1 is the most colour a given pair will carry. On a wooden board
the loss is mild: suppressing green skews the image warm, which is where it
already lives.

**`anaglyph_green` is not `anaglyph_strength`.** The latter desaturates toward
luminance, and luminance has *full* green — measured under red/blue half-colour,
`strength 0` left mean green at 106 of 255 where `green 0` took it to 0.1. One
moves colour toward grey; the other moves the disputed channel toward black. The
ghost only answers to the second.

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

## The mouse pointer

A native pointer is composited by the window system at the screen plane, with no
disparity at all. Under an anaglyph it therefore cannot sit at the depth of the
point it indicates: fuse the board and you see two pointers, fuse the pointer and
the board doubles. There is no value to tune — a 2D overlay has no depth — so the
only fix is to draw the pointer *in* the scene.

It rides the grid's own coverage accumulator in `scene/object/board.glsl`, which
means it inherits each eye's disparity, the board's antialiasing and its
occlusion by stones without a second code path, and it is inked like a grid line
because it is the same act: a mark on the wood.

**Shape.** Four ticks, turned a quarter turn from the grid so no arm is ever
parallel to a line, gapped at the centre so the intersection it names stays
clear, and reaching past a stone's radius so they still read on an occupied
point. The two obvious shapes are unavailable: on this board a disc already means
a stone and an upright cross already means the grid.

**Flat on the wood, not floating above it.** A marker in a plane above the board
has different disparity from the point beneath it, so the eyes are given two
fusion targets a few pixels apart — the same discomfort as the annotation
clipping above. Lying on the surface there is one.

**It moves like the stone it precedes.** The mark snaps to an intersection —
naming a point is the whole job — but carries the same "imprecise hand" offset a
stone in hand gets, from the *same* function (`Board::fuzzyOffset()`). So it
slides a little within the point as the mouse moves and then jumps to the next,
rather than sitting rigidly on the lattice. Measured with the shipped constants:
about 1 px of drift per 8 px of mouse travel.

**It gives way to the stone in hand.** Where a ghost stone is already showing
where the click will land, the mark is not drawn — two indicators for one point.
But it does not give way merely because a stone is *held*: on a point the rules
refuse there is no ghost stone, so the mark is the only thing left, and its
presence there is what says "not here".

**The native pointer is hidden positionally.** Only while it is over the board,
and only when the board is drawing its own: over the interface a screen-plane
pointer is *correct*, that interface being flat at the screen plane itself, so it
is never taken away where it works.

`pointer auto` (the default) draws the mark only under a stereo shader — where
the native pointer is genuinely wrong. `pointer always` extends it to mono, where
the native pointer is merely ordinary and a mark lying on the wood is nicer; that
also hides the native pointer there, which is why it is opt-in. `pointer never`
keeps the window system's pointer everywhere.
