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
