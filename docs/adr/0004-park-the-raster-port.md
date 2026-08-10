# ADR-0004: Park the raster port; the classic ubershader stays the product look

**Status:** Accepted
**Date:** 2026-08-10

## Context

The `raster-port` branch (2026-08-09/10, forked from `camera-refactor`) set out
to move the Red Carpet look off the fullscreen ray-tracing ubershader onto a
data-driven multi-pass pipeline: a JSON pass graph with GLSL hooks, a
bake-at-load noise LUT, a Monte Carlo baked board lightmap (visibility + AO,
EMA accumulation), a table underlay pass, instanced ray-traced stone impostors,
and a glossy-reflections look. Each phase was benchmarked and A/B-pinned; the
classic look was kept byte-identical throughout, with every new look a separate
config entry.

The measured cost anatomy of the classic 46 ms frame (UHD 620, 1056×1050, 140
stones): shadow scans ≈11 ms, extra composite layers ≈14 ms, simplex noise
≈14 ms, traversal floor ≈17 ms. The phased pipeline reached 51.7 fps against
22.5 classic at default zoom in a window — ≈2.3×.

Three problems survived all tuning, because they are structural:

1. **Baked shadows lag and flicker.** The progressive MC bake restarts on
   every damage — each move, each capture landing in a bowl — and the EMA that
   provides cross-fade *is* the visible convergence. The bowls are worst
   because captures are exactly the event that re-damages them. Hiding the
   convergence means more samples per batch, which spends the frame time the
   bake exists to save.
2. **The reflections look reads wrong.** At top-down cameras a stone occludes
   its own planar reflection, so the look is carried by `uBoardBleed`, a
   deliberately view-independent glow around stone bases. It reads as
   reflections of surrounding stones from all sides — non-physical by
   construction, not by mistuning.
3. **The gain evaporates where it matters.** The 2.3× headline came from
   default-zoom window benches. In the fullscreen board-dominant view actually
   used, per-board-pixel work bounds the frame: 17→20 fps for the LUT alone,
   17→23.6 fps for the entire phase-4 pipeline.

## Decision

The raster port is **parked, not merged**. The branch stays at `raster-port`
for reference. Work continues on `ux`, forked from the branch's three-commit
harness prefix (`60d794a`): the headful bench scenario, the `screenshot`
scenario directive, and the multi-word command dispatch fix with
`camera_animating` in `dumpState()` — test infrastructure with no shader or
config entanglement. The classic Red Carpet ubershader remains the product
look, and effort shifts to UX (ADR-0003's GTP proxy, the analysis overlay,
beta readiness).

## Consequences

- The ~3,000 lines of pass-graph, bake, and impostor machinery in
  `GobanShader`/`GobanView` do not enter the mainline; future shader work does
  not pay for them.
- The classic look's known cost stays: ~17 fps fullscreen on integrated
  graphics. That is accepted as the price of correct, immediate shadows.
- Knowledge from the branch outlives it and is recorded here: the frame cost
  anatomy above; the verified scene conventions (default camera *above* the
  board, `lpos2` at screen lower-left, shadows and speculars always physically
  consistent — the suspected "shadow direction quirk" was three stacked
  observation errors, settled by commit `872b634`); and the debug methodology
  (axis-probe bakes, single-stone scenes, numeric centroid scripts — never
  eyeball a dense board).
- Reopening this direction has a bar, not a ban: correct shadows must be
  available the frame the position changes. Progressive per-damage
  accumulation cannot meet it; something like per-position caching or an
  async bake that holds the previous lightmap until the new one is complete
  might. Cherry-picking from `raster-port` is the starting point, not a
  rewrite.

## Alternatives rejected

- **Merge, defaulting to the classic look.** The branch is additive and the
  classic look byte-identical, so this was possible — but it carries the full
  machinery as dead weight through every future `GobanShader` change, for no
  default-look benefit.
- **Continue with the shelved mop-ups** (per-rect shader variants, baked
  stone-receiver shadows). They attack the right bottleneck — board-pixel
  work — but with the same progressive-bake economics, so the flicker and
  latency remain.
- **Fix the flicker with more samples per batch.** Directly trades away the
  fps gain that motivated the port.
- **Shadow maps / SDF / PRT instead of the MC bake.** Already rejected when
  the bake was chosen: the analytic sCircle integrators these would feed
  break for large, low light sources. Revisiting them means restarting the
  port, not repairing it.
