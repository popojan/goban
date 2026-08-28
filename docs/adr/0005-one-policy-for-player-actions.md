# ADR-0005: Every player action asks `availableActions()`, buttons and keys alike

**Status:** Accepted
**Date:** 2026-08-13

## Context

ADR-0002 step 5 extracted `availableActions()` (`src/UiActions.h`) as the single
policy deciding which actions a player is offered, after two bugs in one day
traced to the rules living inline in `ElementGame::OnUpdate()`. It established a
rule — *make the button ask the same question the command asks* — and wired
`cmdResign` and the `resign` command to a shared `GobanControl::canResign()`.

Only `resign` was ever wired up. The other eight actions kept hand-rolled guards
in their command handlers, and those guards drifted from the toolbar. A
systematic audit on 2026-08-13, cross-checking each `add(...)` in
`GobanControl::buildRegistry()` against the `UiActions` field the toolbar greys
the corresponding button by, found six live disagreements:

| Action | Toolbar required | Command actually checked |
|---|---|---|
| `toggle_territory` | `phase == Finished` | `shouldShowTerritory()` |
| `navigate_*`, `undo move` | not thinking, **not bot-bot locked**, UI ready | `isThinking()` only |
| `play once` | **not thinking**, not finished, **not locked** | `acceptsUiEvents()`, `isAtFinishedGame()` |
| `start` | an **engine** to move, not already playing | `phase != Finished` |
| `clear` | `hasMoves` | nothing |
| `save` | `hasUnsavedChanges` | nothing |

Two of these were reachable and visible:

- **Territory on a resigned game.** A resignation satisfies `phase == Finished`,
  so the button was enabled; `shouldShowTerritory()` excludes `+R`, so the
  command refused. An enabled button that did nothing — the exact failure mode
  the rule was written to prevent.
- **Navigation in a locked bot-versus-bot match.** The buttons were greyed, but
  the keys were not, and `isThinking()` does not mean what the command assumed:
  the game loop clears `playerToMove` *before* its 500 ms inter-move sleep, so
  `isThinking()` is false for that window on every single move. Pressing Left or
  Home there was accepted and called `enterReview()`, pausing a running match
  the toolbar was refusing to let anyone touch.

The rule decayed because it was a rule. Nothing in the code made a new command
consult the policy, and the cheapest way to write a handler was to restate the
condition inline.

## Decision

**Every command in the registry that corresponds to a `UiActions` field guards
itself with that field, read through `GobanControl::actions()`.** The handler
holds no policy of its own. Adding a player action means adding a field to
`UiActions`, a rule to `availableActions()`, and a case to
`tests/test_uiactions.cpp` — not a condition at a call site.

Two rules needed restating to make one answer serve both callers:

- **`territory` gains a `scoredEnd` input.** `availableActions()` must stay pure
  over plain data, so the predicate is passed in rather than the record. It is
  published by the game thread (see Consequences) rather than computed by the
  reader.
- **`pass` gains a review term.** Away from the end of the line a pass describes
  a *variation*, not a turn, and `GobanControl::boardClick()` has never consulted
  turn ownership there — its review branch runs before any such test. Phrasing
  `pass` as `humanToMove` alone would have refused a pass onto an engine's colour
  while a click on the same node went through. It now reads
  `(humanToMove || reviewingMidTree)`, with `reviewingMidTree` being
  `!atEndOfNavigation`. The engine-thinking lock stays *outside* that
  disjunction: a pass is a preserving action in ADR-0001's sense and a click
  refuses there too.

**The four navigation keys dispatch through the registry** rather than calling
`GameThread` directly, so they inherit the same guard.

## Consequences

- Buttons and keybindings cannot disagree, because there is one expression.
- `start` no longer works in a human-versus-human game. There is no engine to
  hand the turn to, and `pass` or a board click already start the game. This
  changed `game_phase_transitions.scn`, which had relied on the old behaviour.
- Navigation keys are now recorded by `ScenarioRecorder`, which they were not
  when they bypassed `command()`. Keyboard-driven review used to vanish from
  every bug report.
- `dumpState()` exposes the nine booleans as `can_*`, so a scenario can assert
  the policy directly. Previously "this button is greyed" was checkable only by
  pressing it and observing that nothing moved, which cannot distinguish a
  refusal from an action that legitimately had nothing to do.
- **A UI-thread walk of the SGF tree had to be removed to make this safe.**
  `uiInputs()` runs every frame; routing `shouldShowTerritory()` through it
  segfaulted in `SgfcProperty`'s destructor about one run in six, because that
  predicate walks a tree the game thread owns. `GobanModel::onBoardChange()`
  already computes the value on the game thread at every position change, so it
  now publishes it into an atomic. This removed the exposure *added here*, not
  the underlying hole: `dumpState()` and `uiInputs()` still call `moveCount()`,
  `getViewPosition()`, `isAtEndOfNavigation()`, `getVariations()` and
  `hasGameResult()` across the same boundary. That predates this ADR and wants
  its own decision.

## Alternatives rejected

**Fix only the two user-visible disagreements.** Smaller diff, and it was the
tempting option because `start`, `clear` and `save` diverge harmlessly today.
Rejected because it leaves the mechanism a rule again: the next command would be
written the same way as the last six, and the audit would have to be repeated.
The two visible bugs were not special — they were the two that happened to be
reachable yet.

**Document each divergence as intentional and test both sides.** Would have
pinned the current behaviour without changing it, and is honest about a disabled
button meaning "not offered" rather than "forbidden". Rejected because it doubles
the number of rules to keep in step, and the divergences were not intentional in
the first place — every one of them was an oversight that a test would have
frozen into a specification.

**Make `availableActions()` take `GobanModel`/`GameThread` so it can compute
`scoredEnd` itself.** Rejected for the reason UiActions.h already gives:
`isThinking()` reads a member only the game loop sets, so the engine-thinking
cases would stop being testable — and they are half of what the function decides.
Passing plain data in keeps `tests/test_uiactions.cpp` fixtureless.

**Let analysis mode's auto-reply follow the colour assignment**, which would have
made human-versus-human analysis mode useful and let its refusal be deleted.
Considered and rejected separately during this work: `docs/game-modes.md` claimed
that behaviour, but the code has always used `currentKibitz()` regardless of
assignment. Changing it would alter what analysis mode does in every existing
configuration to fix a case that gains nothing — Kibitz on demand already works
in an ordinary match, with two humans. The documentation was corrected to match
the code, and the refusal now explains itself.
