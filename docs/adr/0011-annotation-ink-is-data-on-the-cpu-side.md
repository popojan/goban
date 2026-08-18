# ADR-0011: The move-quality palette is data in the shader's config entry, not a constant in its GLSL

**Status:** Accepted — implemented 2026-08-17
**Date:** 2026-08-17

## Context

The best-moves overlay colours each suggestion by win-rate loss against the best
move, on a three-stop ramp built in `moveQualityColor()`. The stops shipped as
literals in `src/AnalysisService.cpp`, with a comment inviting exactly the change
that prompted this: *"Expect to tune these by eye against a lit wooden board —
they are the most likely thing here to look wrong."*

They did look wrong, in two ways at once, and the second explains the first.
The board is lit wood, `(0.824, 0.635, 0.282)` — mean brightness **0.58**. The
original stops came out at mean 0.43 (green), **0.65** (amber) and 0.45 (red).
The amber was *lighter than the board it was printed on*, and green and red were
the same grey as each other.

That greyness is not a curiosity. `GobanOverlay::eyeInk()` reduces every label to
`(r+g+b)/3` under a stereo shader, because anaglyph is greyscale and a green
label would otherwise vanish from the red eye. So under stereo the ramp is read
as **ink density**, and a ramp whose ends share a density says nothing — which is
what "in BW/stereo they almost disappear" reported. In colour, meanwhile, the
fully-saturated stops read as signal lights rather than as annotation on wood.

Retuning the numbers is a one-line change. The question this ADR settles is
where the retuned numbers should *live*, because tuning ink against a board is
not a thing anyone gets right in one pass, and a rebuild per attempt is the wrong
loop.

## Decision

**The palette is data, in the `annotations` block of `config/base.json`, and a
shader entry may carry its own `annotations` block that overrides it.
`GobanShader::choose()` resolves the two, beside `stereo` and `height`.**

The split within it:

- **The three stops are shader-scoped.** They are an appearance judgement about
  a particular board, and a shader that redefines the board dark needs its own.
- **The two thresholds are global.** `move_quality_loss` is a Go judgement — ten
  points of win rate is a blunder under any board, under any shader — so a
  shader entry has no standing to hold an opinion about them, and
  `resolveQualityPalette()` reads them from the global block alone.

The shipped stops are `#0f521f`, `#7a4d0d`, `#a83d38`: deep green, ochre, brick
red, at mean brightness 0.17, 0.28, 0.37. Every one sits below the wood, and they
**increase monotonically**, so the ramp survives `eyeInk()` — the best move is
the most firmly printed and a blunder fades toward the board. That ordering is a
constraint, not a preference, and it is the thing a dark-board shader must invert.

## Alternatives rejected

### A `const vec3` in a GLSL partial, shared through the include system

The natural reading, and the one raised first: the shader is what decides the
board is wood-coloured, so the ink that has to sit on that board belongs beside
it — and `Shadinclude` means a shared `partial/annotations.glsl` would be one
copy, not six, so the "six copies of one value" objection recorded against
per-shader ink does not apply.

The objection that does apply is the **CPU/GPU boundary**. The consumer of these
colours is `add_text()`, which bakes them into the glyph vertex buffers on the
CPU; the ray-traced fragment shader never draws a letter. A constant in a GLSL
partial would therefore sit unread in six fragment shaders, existing solely for a
pipeline with no way to fetch it. Recovering it means text-scraping
`Shadinclude::load()`'s output for `vec3(...)` — which makes the palette's
authoring format regex-parsed GLSL, breaks silently on a reformat, and has no
error path.

This is the same reasoning already recorded on `GobanShader::isStereo()`:

> from the shader's own `"stereo"` entry in the config rather than from its file
> name: it is a property of the shader, and everything that has to follow it —
> the eye offset the overlay draws with, the greyscale its ink collapses to — is
> out here rather than in GLSL.

`height` is the same shape: a physical property of the rendered stone that the
CPU must act on, declared in JSON beside the file paths. The palette joins them.

### Leaving the stops as C++ literals

Defensible — they are half of a mapping whose other half (the thresholds) is a
Go judgement, and a config knob invites a bright palette that dies under
anaglyph, which is precisely the failure being repaired. Rejected because the
tuning loop is the point: these are meant to be adjusted by eye against a lit
board, and a rebuild per adjustment is not that. The hazard is answered by
keeping the C++ values as the fallback and documenting the monotonicity
constraint at the config key rather than by withholding the knob.

### Per-shader from the start, with no global default

Rejected as premature: no shipped shader redefines the board, so six entries
would carry identical palettes. The global block is the default and the override
is a pure addition, which is the arrangement
[`CLAUDE.md`](../../CLAUDE.md) already predicted for annotation ink and the same
two-level shape as the camera (`config/base.json` ships, `user.json` overrides).

## Consequences

- **No new invalidation.** `GobanView::switchShader()` already raises
  `UPDATE_ALL`, which rebuilds the glyph buffers — the mechanism that makes the
  anaglyph greyscale work repaints a new palette for free.
- **`moveQualityColor()` stays pure over plain data**, taking a `QualityPalette`
  with the shipped values as its default argument, for the reason
  `availableActions()` does: it tests without a GL context, a config singleton or
  a thread. `resolveQualityPalette()` is likewise a pure function over two
  `nlohmann::json` objects, so the merge is testable in `goban_core` even though
  its caller lives in the OpenGL target.
- **Malformed config degrades per stop, not per array.** A typo in the third
  colour leaves the two that parsed, and warns. Dropping all three would hide the
  typo; applying none would revert a palette the user had half-tuned.
- **Inverted thresholds are refused, not applied.** `slightly < blunder` and both
  positive, or the ramp runs backwards and every move reads as its opposite.
- **A user can still configure an illegible palette.** Nothing in a hex string
  says "this must stay below the wood and rise monotonically". The constraint is
  documented at the key and pinned for the *shipped* values by
  `tests/test_analysis.cpp`; it is not enforced against user input, which is the
  same latitude `readout_color` already has.
- The stops are now specified in two places that must agree — `config/base.json`
  and the `QualityPalette` defaults. The config wins wherever it is present, so
  the C++ values are reached only by a user who deleted the key.
