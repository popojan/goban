# ADR-0014: The engine answers after you have decided

**Status:** Proposed — nothing implemented
**Date:** 2026-09-04

Revisits ADR-0007 decision 14, which turned the board suggestions off by default.

## Context

ADR-0007 put the move suggestions behind a toggle that ships **off**, and the
reason was not clutter:

> The numbers are read *after* a move, so a player can invent their own and
> evaluate it post-hoc; stars on the board are read *before*, and once the engine
> has pointed at a point you cannot un-see it.

That is a real objection and it stands. But it condemns the feature to being
useless for the case it would help most — a weak player learning by playing
against a bot, who wants to know whether the move they were *about to make* was
any good. Turning the stars on answers that question by removing it: you no
longer have a move of your own to test.

The observation that unlocks this is that the objection is about **order, not
about the display**. Stars read before a decision replace the player's thinking;
the same stars read after it grade the player's thinking. Nothing about the
overlay has to change — only when it is allowed to appear.

## Decision

**1. A mode where the analysis runs but the suggestions are withheld until the
player has committed to a point.** The two existing toggles already separate
these concerns — `toggle_evaluation` decides whether the engine runs at all,
`toggle_evaluation_moves` whether the suggestions are drawn — so the engine is
already streaming and holding a current report while nothing is shown. Revealing
is immediate and needs no new plumbing.

**2. The reveal happens before the stone is committed, on a dwell.** Resting the
cursor on an intersection for a short time is the trigger: "I have been looking
at this point" correlates with having chosen it, and it adds no gesture.

**Not a third click.** A click that means something different in one mode than in
another is the shape of defect ADR-0005 exists to prevent, and this codebase has
paid for it repeatedly. A dedicated key remains a reasonable alternative and is
left open.

**Not the existing two-click "stone in hand".** Those two clicks are a
mis-click guard — pick up, then place — not choose-then-confirm. The first click
says the player reached for the board, not that they decided where. Overloading
it would make a safety mechanism carry a meaning it was not designed for.

**3. Revealing before the commit, rather than after the stone lands.** This is a
correctness matter as much as a design one. ADR-0007 established that a report
for another position is not drawn — `updateAnalysisOverlay()` compares
`positionId`, because the engine's opinion of one position painted over the
stones of another is a lie. Suggestions revealed *after* the move belong to the
parent position while the board has moved on, so that path would require
explicitly widening the rule and marking the display as an opinion about the
previous position. Revealing before the commit has no such problem: the position
has not changed and the report is current.

**4. The verdict on the played move is state, and belongs on the board.** Whether
the last move was the engine's first choice, and what it cost, stays true for as
long as that move is the last one — so by ADR-0012 it is state, not an event, and
the wood is where it goes.

`#lblMessage` is specifically wrong for it. `OnUpdate()` clears that line on every
position change carrying no comment, and the position changes when the opponent
replies — measured at 13 ms for GNU Go. The verdict would flash and vanish before
it could be read. This is the same failure that made a confirmation prompt
impossible to click during a bot match.

**5. The verdict speaks in the vocabulary that already exists.** The
move-quality ramp (`move_quality_loss`: a concession at 0.015, a blunder at 0.10)
already grades moves, and the last move already carries a number drawn on the
stone. Grading the played move on that same scale reuses both; inventing a second
scale would mean the same move could be described two ways.

**6. The verdict rides the last-move marker; it does not touch the margin.** The
bottom margin is full — the recommended pass at the left, the readout centred,
the wait clock at the right — and the space is measured: at roughly 0.35 grid
units per character there are about ten characters free beside a centred readout
on 19x19 and **two** on 9x9. A beginner playing a bot is on the small board, so
there is no room for a second item exactly where this feature is aimed.

So the verdict is drawn on the stone: the last move's number tinted by the
quality ramp, and carrying the engine's rank letter when the move was in its
list. The suggestions already label ranked moves `A`/`B`/`C`, so a move number
wearing an `A` says "first choice" in vocabulary the player has already learnt.

It is **additive**, not a replacement. The stone says what the move was worth;
the readout says where the game stands now. They are different subjects, and
replacing the readout would delete the win rate at the exact moment it is most
wanted. The timing divides cleanly too: after the move the engine re-analyses, so
the readout is briefly stale and blanks, while the verdict is available instantly
because it comes from the report already computed for the position just left.

**7. Praise is honest about what it is.** Matching the engine's first choice is
worth saying — the satisfaction is the point, and a learner needs the positive
signal as much as the negative one. But it is the engine's opinion at a given
search depth, not correctness: a superhuman bot with few visits is often wrong
about its own ordering. So the wording names it as the engine's choice, and the
claim is gated on the report being worth trusting rather than made at any visit
count.

## Consequences

- The suggestions could reasonably default **on** in this mode, which is what
  ADR-0007 decision 14 could not allow. That is the point of the change.
- Nothing about the analysis engine, its process, or its sync changes. This is a
  display rule and a trigger.
- The dwell timer is a new input concept on the board; it must not fire while the
  camera is being dragged, and it should not repaint on a timer any more often
  than the wait clock does.
- A tsumego shows no evaluation at all (ADR-0007), and that is unaffected: the
  first suggestion there *is* the answer to the problem, whatever the order of
  reveal.
- The verdict depends on the last-move overlay being drawn. With those markers
  switched off it has nowhere to live, and must either draw its own mark or be
  absent — a decision to make rather than inherit, given what happened when a
  toggle was put around a whole draw pass instead of around its own labels.
- Like the suggestions and the readout, the verdict stands down at a scored end:
  the board has finished making its claim.

## Open questions

1. **Dwell or a key?** Dwell reads as effortless and risks firing when the player
   is merely thinking with the cursor parked. A key is unambiguous and adds a
   thing to learn. Dwell is the starting choice, on feel.
2. **What makes a report trustworthy enough to praise?** A visit count, a
   wall-clock floor, or showing the visits and letting the player judge.
3. **Is the after-the-stone reveal worth widening the `positionId` rule?** It is
   what was originally asked for, and it is the more natural moment — the move is
   made, now show me. Decision 3 defers it rather than refusing it.
