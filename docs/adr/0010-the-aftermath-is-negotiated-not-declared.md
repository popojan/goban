# ADR-0010: The aftermath is negotiated, not declared

**Status:** Proposed — sketch for discussion, nothing implemented
**Date:** 2026-08-16

## Context

Two passes end the game. What happens next, in every ruleset that has been
written down, is a *negotiation*: the players agree which groups are dead, and if
they cannot agree, play resumes to settle it. Japanese 2003 does this by
hypothetical play; Chinese and AGA rules resume the actual game. The agreement is
the ruling — not one participant's opinion of it.

What goban does instead is ask the coach engine `final_status_list dead`, shade
whatever comes back, ask the same engine `final_score`, and write the number into
`RE`. The human is not consulted at any point. That is fine when the engine is
right, which is most of the time, and wrong in the two situations where scoring
actually matters:

- **A group whose life the human disputes.** GNU Go's reading is not strong, and
  it is the shipped default coach. A misjudged group swings the result by tens of
  points and the player has no recourse but to disbelieve the number.
- **An unsettled position.** Two players who pass with a genuine capturing race
  unresolved have not finished the game. The rules say play on; goban declares a
  winner.

There is also a practical cost the review turned up: the number is the engine's,
so no engine means no result. `final_status_list dead` on a sparse 19×19 hangs
GNU Go long enough to be killed (ADR-0009), and 10 s even on a sparse 9×9. We
already compute the territory *shading* ourselves from the dead list; only the
arithmetic is outsourced.

Prior art matters here. This is not unexplored: OGS has a stone-removal phase
where both players mark and agree, and it is among the things people praise it
for. What is genuinely rare is a *local* board, playing local engines, that does
the same — and that is the thing goban is, a board first and an engine client
second.

## Decision (proposed)

**Two passes enter a stone-removal phase rather than ending the game.**

1. **A new `GamePhase::Removing`**, between `Playing` and `Finished`. The board is
   live, the message line explains what to do, and `availableActions()` gets its
   own row: no moves, no navigation off the end, but toggling and two new
   actions — *accept* and *resume*.
2. **The engine proposes; the human disposes.** `final_status_list dead` seeds the
   marking. Clicking a group toggles its status, exactly as the shading already
   redraws. The engine's proposal is a starting point, not a verdict.
3. **Accept ends the game.** The agreed dead list is what the score is computed
   from — and it is computed *locally*, from the fill we already run, so a
   disagreement with the engine's own `final_score` is expected rather than a
   contradiction. The engine's number becomes a cross-check logged at `warn` when
   the two differ by more than a point.
4. **Resume returns to `Playing`** with the two passes still in the record, which
   is what the rules describe. The SGF keeps them; there is nothing special to
   represent.
5. **A ruleset appears here, and only here.** Not before: a setting whose only
   effect is a one-point arithmetic difference the user cannot verify is a
   setting they can only get wrong. Dispute resolution is the part that genuinely
   differs, so this is where `RU` earns a reader and where Japanese-vs-Chinese
   stops being a formula and starts being behaviour.
6. **Engine opponents accept by default.** A bot cannot negotiate over GTP —
   there is no protocol for it — so against an engine, `accept` is the human's
   alone and the engine is taken to agree. Said plainly in the message line
   rather than implied.

## Consequences

- Scoring stops depending on an engine answering. That removes the last hard
  dependency on `final_score`, and it makes the GNU Go stall a slow *proposal*
  rather than a failed *result*.
- We own the arithmetic, and therefore seki and handicap compensation. Seki is
  already handled (the engine's `seki` list feeds the fill). Handicap
  compensation is the remaining trap and differs by ruleset — which is the point
  of putting the ruleset here.
- A local count can disagree with the engine's. That is not a bug to be hidden;
  it is the negotiation working. It does mean `RE` is ours, and a game reloaded
  elsewhere may be scored differently.
- Real work: a phase, a click mode, group toggling in the shading, two toolbar
  actions, five translations, an SGF representation for the agreed dead stones
  (`TB`/`TW`, or nothing and re-derive), and the ruleset reader.
- It changes something users already know how to do. Today two passes give an
  immediate answer; afterwards they give a screen that must be dismissed. That is
  the cost, and it is the reason a "don't ask me, just score it" preference
  probably has to exist from day one.

## Alternatives rejected

- **Keep asking the engine, add a manual override.** A "mark this group dead"
  affordance without a phase. Cheaper, and it is roughly what Sabaki and KaTrain
  offer — but the result is still declared the moment the second pass lands, so
  the override is an edit to a decision already made, and `RE` has already been
  written and autosaved.
- **Count locally, silently, and never ask.** Removes the engine dependency
  without the interface work. Rejected because it takes the human *further* out
  of the loop, not closer, while making us wrong on our own authority instead of
  the engine's.
- **Follow Japanese 2003 hypothetical play literally.** Correct, and unusable: it
  requires the engine to play out a hypothetical continuation under rules no GTP
  engine exposes. Resuming actual play is what Chinese and AGA rules say and what
  players actually do.
- **Do nothing.** Defensible — it has not produced a bug report, and the number is
  usually right. It is on this list because "usually right, no recourse when
  wrong" is the state of every scoring implementation in this class of program,
  and matching it is a choice rather than a default.

## Open questions

- Where does the agreed dead list live in the SGF? `TB`/`TW` is the obvious
  answer and is what most software writes; re-deriving from the final position
  plus the moves is smaller but loses the agreement itself.
- Does `Removing` need to survive a save and reload, or is it strictly
  transient? Transient is simpler and probably right — a reloaded game is under
  review, not mid-negotiation.
- Does the local count become authoritative for *loaded* games too, where `RE`
  already exists and was computed by somebody else? Almost certainly not: a
  record's own result outranks our recomputation.
