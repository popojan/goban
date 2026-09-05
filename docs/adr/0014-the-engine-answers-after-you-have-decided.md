# ADR-0014: The engine answers after you have decided

**Status:** Accepted — implemented 2026-09-05
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

**2. A stone in hand is the precondition, the dwell is the trigger.** Resting the
cursor on an intersection for a short time, *while holding a stone*, is what
reveals. Neither signal is strong alone — reaching for the board says nothing
about where, and a parked cursor says nothing about intent — but together they
say "I have chosen this point", which is exactly the moment being waited for.
This also leaves the mis-click guard doing only its own job.

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

**4. The verdict lives only while the move is being decided, and nothing is shown
after the commit.** The engine's answer is consumed at decision time: the letter,
the tinted candidates and the readout are all read before the stone is placed. By
the time it lands there is nothing left to learn from it.

Two alternatives were considered and both fail.

`#lblMessage` is wrong because `OnUpdate()` clears that line on every position
change carrying no comment, and the position changes when the opponent replies —
measured at 13 ms for GNU Go. The verdict would flash and vanish. This is the
same failure that made a confirmation prompt impossible to click during a bot
match.

**Tinting the move number after the commit fails for the same measured reason.**
`updateLastMoveOverlay()` clears the previous marker and labels only the single
most recent stone, so a tint on the player's move survives about one frame before
the opponent's reply takes the marker over. Grading *every* played stone would
avoid that, but it is a different feature — post-hoc review of a whole game, with
a permanent visual load — and not this one.

The cost is worth stating: a move played without waiting for the reveal gets no
feedback at all. That is consistent rather than regrettable. The player chose not
to ask.

**5. The verdict speaks in the vocabulary that already exists.** The
move-quality ramp (`move_quality_loss`: a concession at 0.015, a blunder at 0.10)
already grades moves and already tints the suggestions. Grading the aimed-at
point on that same scale reuses it; inventing a second scale would mean the same
move could be described two ways.

**6. The verdict is drawn on the stone, not in the margin.** The
bottom margin is full — the recommended pass at the left, the readout centred,
the wait clock at the right — and the space is measured: at roughly 0.35 grid
units per character there are about ten characters free beside a centred readout
on 19x19 and **two** on 9x9. A beginner playing a bot is on the small board, so
there is no room for a second item exactly where this feature is aimed.

So the verdict is drawn at the point being aimed at: tinted by the quality ramp,
and carrying the engine's rank letter when the point is in its list. The
suggestions already label ranked moves `A`/`B`/`C`, so the aimed-at point wearing
an `A` says "first choice" in vocabulary the player has already learnt — and
seeing it there, before committing, is the praise.

**It is the evaluation's own label, not the last-move marker's.** It merely lands
on the same point. That matters because gating it on the last-move toggle would
make one feature vanish for a reason belonging to another — the same coupling
that once let switching off two markers take the coordinates, the markup and the
whole evaluation display with them. The rule that came out of that stands: a
toggle belongs where its own labels are placed.

Faking the other toggle on instead — drawing move numbers while the menu still
says they are off — only inverts the problem into a setting that disagrees with
the board, which is what ADR-0005 exists to prevent.

**A move number is a fact about the record, so it may not appear before the move
is in it.** An unconfirmed stone therefore carries no number, and the labels the
mode draws are its own:

| phase | on the stone |
|---|---|
| stone in hand, not committed | nothing yet |
| dwell elapsed, report trustworthy | the rank letter on the aimed-at point, tinted; the other candidates likewise |
| committed | the mode's labels go; the ordinary move number appears |

The reveal is transient by construction, so it never has to coexist with the move
number — which is also why nothing has to be overridden or borrowed.

**7. The delta goes in the readout, not on the stone.** While a stone is held and
aimed at a point, the readout in the bottom margin shows *that candidate's*
numbers instead of the position's, and returns to the position when the stone is
placed or put back.

There is nowhere else to put it — two characters beside a centred readout on 9x9
— but the stronger reason is that the suggestions do not print their loss today,
they are *tinted* by it. A verdict that printed a number would put two scales for
one quantity on the board. Rank is carried by the letter, magnitude by the
colour, and the exact figure is asked for by aiming at the point. Lizzie answers
the same question the same way: hover a candidate to see its statistics.

It is **additive**, not a replacement. The stone says what the move was worth;
the readout says where the game stands now. They are different subjects, and
replacing the readout would delete the win rate at the exact moment it is most
wanted. The timing divides cleanly too: after the move the engine re-analyses, so
the readout is briefly stale and blanks, while the verdict is available instantly
because it comes from the report already computed for the position just left.

**8. Praise is honest about what it is.** Matching the engine's first choice is
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
- The verdict is visible whenever the mode is on, whatever the last-move and
  next-move markers are set to. It shares their point, never their toggle.
- Like the suggestions and the readout, the verdict stands down at a scored end:
  the board has finished making its claim.

## Implementation log — 2026-09-05

`evaluation_moves` became three-state (`off`/`on_demand`/`always`), migrated from
the old boolean on read, and the menu item became a select because one checkbox
cannot show three states. `annotations.hint_dwell_ms` (800) is the dwell.

Two things the design did not anticipate.

**A point the engine never searched has no number**, and that is most of a
beginner's moves — the silence lands exactly where the feature was aimed. So
`kata-analyze` is asked with `minmoves` (`hint_min_moves`, 30) for wider
coverage, and candidates below `hint_min_visits_fraction` (0.05) of the best
move's visits are dropped again: a win rate from a single visit is noise, and
showing it is worse than the silence it replaces. `minmoves` is an extension, and
an engine that rejects it would otherwise retire the evaluation for the session,
so a refusal is caught once and the command asked plainly thereafter.

**The scripted `click` did not move the cursor.** A real click always lands where
the cursor already is; the scripted one skipped that, so the ghost stone, the
pointer mark and this dwell all read the previous point.

### Still open: the delta after the move, Fritz-style

Remembering the previous readout and subtracting the new one would give the cost
of a move that was played without waiting for the reveal — which is the gap
decision 4 leaves open, and how infinite analysis reports move deltas elsewhere.

It works in **review**: stepping through a game, each position can be compared
with the one before it. It does **not** work live against a fast opponent — the
position after the player's move exists for about 13 ms before GNU Go replies,
which is not long enough for the engine to evaluate it at all, let alone for the
difference to be read. The same measurement that decided decisions 4 and 6.

## Open questions

1. **Dwell or a key?** Dwell reads as effortless and risks firing when the player
   is merely thinking with the cursor parked. A key is unambiguous and adds a
   thing to learn. Dwell is the starting choice, on feel.
2. **What makes a report trustworthy enough to praise?** A visit count, a
   wall-clock floor, or showing the visits and letting the player judge.
3. **Is the after-the-stone reveal worth widening the `positionId` rule?** It is
   what was originally asked for, and it is the more natural moment — the move is
   made, now show me. Decision 3 defers it rather than refusing it.
4. **Is showing nothing after the commit too austere?** Decision 4 says the
   answer was consumed while deciding, and the measurement backs it — a mark on
   the last move alone survives one frame against a 13 ms opponent. But a player
   who plays quickly then gets no feedback at all, and grading every stone is a
   whole separate feature rather than a small extension of this one. Whether that
   feature is wanted is a question this ADR does not settle. Needs looking at.
