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

2. **The two game waits are drawn on the board**: an elapsed second count in the
   wood margin, right-aligned, through the overlay's glyph pass.
   `#lblStatus` keeps engine loading (which names a specific engine, at startup,
   before the board means anything) and the message badge (a notification, and
   the log panel's only close affordance).

3. **Annotations are carved, not lit, and they do not move except to change what
   they say.** No fading, no pulsing, no dimming, no blinking. An animated alpha
   reads as a screen effect laid over the scene rather than as part of it; a
   blink adds motion that carries no information. The count turning over once a
   second is the only change, and it is the physical kind — the kind a clock
   beside a board makes.

4. **The margin's two ends carry meanings**: left is an *action* the player might
   take (the recommended pass), right is *program status* (the wait clock). The
   readout is centred by default, and its alignment stays the user's choice.

## Alternatives rejected

**Keep both surfaces and let the user choose.** This is what shipped, and it is
why the decision is being made now: with the board version behind a toggle that
defaults off, the *worse* surface was the default, and every scenario, every
screenshot and every new user got the panel. A choice between two renderings of
one fact is not a feature; it is a failure to decide, and it doubles the
surface that every later change to the evaluation has to be correct in.

**A mark beside the count.** Three versions were built and all three removed,
which is worth recording because each removal taught the rule above. A *pulsing*
mark went first: an annotation at half alpha is lit rather than carved. A
*static* mark replaced it and was wrong the other way — a wait with nothing
moving cannot be told from a freeze, which is the one failure this exists to
prevent. A *blinking* mark fixed that and was then redundant: the seconds are
already visibly lapsing, so the blink carried no information and cost the quiet.
What is left is the count alone, which says "working, this long" without
introducing a symbol the board has no other use for and no way to explain.

**Naming the thinking engine on the board.** The banner did name it, and with
several engines configured "thinking" does not say which. Dropped for now, and
the reason is measured rather than aesthetic: at roughly 0.35 grid units per
character the bottom margin has about ten characters free beside a centred
readout on 19x19 and **two** on 9x9 — and 9x9 is where a slow engine is most
likely to be running. Colour-coding by side was rejected separately: it would put
the meaning in a hue that `GobanOverlay::eyeInk()` flattens to brightness under
any stereo shader.

One mechanism was found and deliberately *not* built on. The analysis service
yields while a playing engine searches (`GameThread::analysisMayRun()`), keeping
its last report frozen — so making the readout treat "yielded" as stale would,
with the shipped transparent stale ink, blank it during every genmove and free
the whole margin for a name. That works, but the yield is *policy* — it exists to
avoid GPU contention (issue #45), not because a separate process cannot evaluate
— so a layout resting on it would break the day the policy is relaxed. The
left-is-action / right-is-status split does not depend on it.

**This is the one open question left by this ADR**, along with the readout's own
staleness: it currently tests only the position id, where the retired panel also
tested whether the search was still running. That is a real inconsistency, left
alone because fixing it changes what is on screen during every engine move.

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
