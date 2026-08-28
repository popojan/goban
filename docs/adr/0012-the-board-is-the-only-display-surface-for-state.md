# ADR-0012: State is shown on the board, not in a panel over it

**Status:** Accepted — implemented 2026-08-28
**Date:** 2026-08-28

Supersedes ADR-0007 decision 11 (the evaluation panel as a third surface) and the
"three surfaces" invariant that came out of it.

## Context

Two things over the board were saying things the board could say itself.

**The evaluation panel** (`#grpAnalysis`) arrived with ADR-0007 as the display
for the live evaluation: a win-rate bar and two labels, bottom right, on a
`#000000b0` backing. The on-board readout came later and was written down as an
experiment — `GobanView.h` said so in as many words: *"The panel is small,
overlays a board that took some trouble to render, and is the one piece of the
interface not in the scene; this is the experiment in putting it there."* It
shipped behind `toggle_evaluation_board`, defaulting **off**, so the panel stayed
the thing users actually saw.

**The engine-thinking banner** (`tplStatusThinking`, `tplStatusSyncing` in
`#lblStatus`) arrived because a genmove was completely silent — 30.9 s measured
for one kibitz from the stock 9x9 KataGo on a CPU backend, reported as "nothing
happens". The text was right about the problem and wrong about where to say it.
It was already recorded as a placeholder pending a polish pass.

Both are chrome laid over a ray-traced board, and the board is the product. The
question is not whether to keep saying these things — both exist because saying
nothing was a real failure — but where.

## Decision

**The board carries state; the status line carries only what is about the
application.**

Concretely:

1. **The evaluation panel is retired.** `#grpAnalysis`, its stylesheet block, its
   three templates and `ElementGame::syncEvaluationPanel()` are gone. The
   readout on the wood is the only display, and **there is no placement
   setting**: `toggle_evaluation_board`, `GobanView::showEvaluationOnBoard` and
   `UserSettings::evaluationOnBoard` are removed. `toggle_evaluation` decides
   whether the evaluation exists at all; `evaluation_align` still decides where
   along the edge it sits, because that is taste rather than a choice between
   two implementations of the same thing.

2. **The two game waits are drawn on the board**: a static mark and an elapsed
   second count in the wood margin, through the overlay's glyph pass.
   `#lblStatus` keeps engine loading (which names a specific engine, at startup,
   before the board means anything) and the message badge (a notification, and
   the log panel's only close affordance).

3. **Annotations are carved, not lit.** No pulsing, no fading, no dimming. A
   board annotation is present or it is absent; an animated opacity reads as a
   screen effect laid over the scene rather than as part of it. The count
   ticking over once a second is the whole animation, and it is the physical
   kind — what a clock beside a board does.

## Alternatives rejected

**Keep both surfaces and let the user choose.** This is what shipped, and it is
why the decision is being made now: with the board version behind a toggle that
defaults off, the *worse* surface was the default, and every scenario, every
screenshot and every new user got the panel. A choice between two renderings of
one fact is not a feature; it is a failure to decide, and it doubles the
surface that every later change to the evaluation has to be correct in.

**A pulsing mark for "thinking"** (the original sketch, and the first
implementation). Rejected on the rule in decision 3. It was built, looked at,
and taken out; the blink curve and its tests went with it.

**Naming the thinking engine on the board.** The banner did name it, and with
several engines configured "thinking" does not say which. Dropped for now: the
mark is anonymous, and the engine name remains in the log. Colour-coding the
mark by side was considered and rejected separately — it would put the mark's
meaning in its hue, which `GobanOverlay::eyeInk()` flattens to brightness under
any stereo shader. **This is the one open question left by this ADR.**

**Dimming the readout when it is stale** (ADR-0007 decision 13, "stale is
dimmed rather than blanked, because blanking would flicker once per move"). The
shipped `annotations.readout_stale_color` of `#00000000` already blanked it, and
that is deliberate rather than a slip: a half-faded number reads as a fault, and
the information is either correct and fully present or it is absent. The flicker
that decision 13 feared is accepted as the cost. `readout_stale_color` remains,
so anyone preferring the dimmed behaviour can set it.

## Consequences

- **The win-rate bar is gone.** The readout is text — `B 62% B+4.5` — and there
  is no diegetic equivalent of the bar yet. This is a real loss of an
  at-a-glance graphic and is the second open question.
- **The board readout is not translated.** It composes `B 62%` in C++ where the
  panel used templates. The notation is the same in every language the project
  ships, but this is now the only place a user-visible string bypasses the
  template mechanism.
- The "three surfaces" invariant becomes two, and the tense split that justified
  the third (`#lblMessage` carries events, `#grpAnalysis` carries state) is
  restated: **state goes on the board**.
- `evaluation_overlay.scn` and `evaluation_tsumego.scn` change. The latter found
  a real consequence: `expect eval_board_text ""` had been passing because the
  readout was never drawn at all, and with the readout always live it needs a
  frame — `wait_until`, the documented idiom for keys that report what was drawn.
- Anything added later that wants to show continuous state has no panel to join.
  That is the point.
