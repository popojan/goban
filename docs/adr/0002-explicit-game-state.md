# ADR-0002: Replace the lifecycle flags with explicit state machines

**Status:** Accepted 2026-08-09 — `GamePhase` steps 1–2 implemented, steps 3–5 outstanding
**Date:** 2026-08-09

## Context

Game lifecycle is currently carried by loose booleans with no single owner:

| Flag | Refs | Writes | Written from |
|---|---|---|---|
| `model.isGameOver` | 43 | 8 real | GobanModel, GameNavigator, GameThread |
| `model.started` | 33 | 5 | GobanModel, GameNavigator, GameThread, GobanControl |
| `holdsStone` | 20 | 9 | GobanControl, GobanModel |
| `syncingUI` | 18 | 4 | GobanControl, ElementGame |
| `tsumegoMode` | 17 | 5 | GobanControl, GameThread, ElementGame |
| `interruptRequested` | 16 | 4 | GameThread |
| `hasThreadRunning` | 11 | 3 | GameThread |
| `enginesSynced` | 10 | 5 | GameThread |
| `navigationInProgress` | 9 | RAII | GameNavigator |
| `deferredPending` | 8 | 4 | GameThread |

Three modules write the two central flags. Nothing enforces which combinations
are legal, and CLAUDE.md's Design Invariants section — 20+ hand-written rules —
exists precisely because the state machine is implicit. Documentation is standing
in for a type.

This is not theoretical. Of nine bugs fixed on 2026-08-08/09, **four trace
directly to implicit state**:

- `clearSession()` cleared `gameInDocument` but not the coupled condition, so
  archiving mid-game silently dropped every later move.
- `appendGameToDocument()` cleared the external-file reference on one path out of
  four.
- The game loop tested `!model || isGameOver` *before* `enginesSynced` flipped,
  then fell through into `genmove` on a paused game and wedged on a human
  player's blocking `genmove()`.
- A move arriving after an interrupt was played instead of discarded.

A fifth — the Open/New-Game hang — was engine *ownership* rather than game state,
and is addressed by ADR-0001.

Two further data points. Fixing the hang **added two more flags**
(`deferredPending`, `deferredDone`), so the pressure is still increasing. And
until 2026-08-08 there were no tests at all; there are now 138 unit tests, 4
scenarios and an interaction recorder. Restructuring state without that net is
how regressions get introduced, so the moment this becomes affordable is now.

## Proposed decision

Do **not** build one mega-enum. Three distinct machines are conflated today, and
separating them keeps each small, independently testable, and independently
shippable.

**1. `GamePhase` — what the game itself is doing**
`Setup` → `Playing` → `Paused` → `Finished`, plus `Setup` ← any (new game).
Replaces `started` and `isGameOver`. Owned solely by `GobanModel`; every change
goes through `transitionTo()`, which rejects and logs illegal transitions.

**2. `EngineSync` — whether engines match the record**
`Unsynced` → `Syncing` → `Synced`. Replaces `enginesSynced`. Owned by
`GameThread`. This is where today's "checked the flag before it flipped" bug
lived.

**3. `LoopState` — the game thread's own lifecycle**
`Stopped` → `Running` → `Stopping`. Names what `hasThreadRunning`,
`interruptRequested` and `deferredPending` already express jointly. Genuinely
thread state, so it stays inside `GameThread`.

**Deliberately left alone**, because they are orthogonal and folding them in
would recreate the mess in a new shape:
`GameMode` (MATCH/ANALYSIS), `aiVsAi`, `tsumegoMode`, `holdsStone`, and the
`GameRecord` persistence flags (`gameHasNewMoves`, `gameInDocument`,
`unsavedChanges`).

`syncingUI` is **not** in scope as a target. It is a symptom of
`ElementGame::OnUpdate()` (317 lines, every frame) emitting change events; it
should die when that is decomposed, which is the step *after* this one. Attacking
it directly would be treating the rash.

## Migration, in shippable steps

1. **Derive, don't change.** Add `GamePhase` as a read-only accessor computed
   from today's flags. Add tests pinning current behaviour at every call site.
   Zero behaviour change — this is the safe wedge, and it makes the real
   transition table visible.
2. **Invert.** Make the phase authoritative; keep `started`/`isGameOver` as
   deprecated accessors so nothing breaks at once. Route all writes through
   `transitionTo()` with a legal-transition table. This is where regressions
   would hide, so it lands with scenario coverage for each transition first.
3. **Delete the compatibility accessors**, one file at a time.
4. **Repeat for `EngineSync`**, which is small and mostly local to `GameThread`.
5. Only then decompose `ElementGame::OnUpdate()`, retiring `syncingUI`.

Each step ends with a green suite and is independently committable. `LoopState`
is mostly renaming and can ride along with step 4.

## Consequences

- Illegal combinations become unrepresentable rather than merely undocumented,
  and several CLAUDE.md invariants turn into a transition table plus tests.
- Transitions become greppable and loggable in one place — today, answering "who
  ended the game?" means reading three files.
- Cost: a mechanical but wide diff touching GobanModel, GameThread,
  GameNavigator and GobanControl. Steps 2 and 3 are the risky ones.
- Accepted: a temporary period where both the phase and the deprecated accessors
  exist, which is uglier than either end state.
- The scenario harness must gain per-transition coverage before step 2. That is
  worth having regardless.

## Alternatives rejected

- **One combined enum** for game, engines and thread. Fewer types, but the three
  change on different threads at different rates; a single enum would need
  sub-states immediately.
- **Target `syncingUI` first.** It is the most visible workaround, but it caused
  none of the nine bugs. Its cause is `OnUpdate`, which is step 5.
- **Do features first, refactor later.** Every feature adds writers to these
  flags, so the refactor only grows. The test net that makes this affordable
  exists as of today.
- **Big-bang rewrite.** The realistic failure mode for a solo hobby project is
  stalling halfway. Hence the derive-then-invert sequence, where step 1 is
  behaviour-preserving and each later step ships on its own.

## What would make us abandon this

If step 1 shows the derived phase needs more than about five states, or that
call sites disagree about what "started" means in ways that cannot be reconciled,
then the flags are encoding something genuinely richer and this ADR should be
superseded rather than forced through.

## Implementation log

### Step 1 — derive. Done 2026-08-09.

`GamePhase` (`src/GamePhase.h`) and `GobanModel::phase()` derive the phase from
`started`/`isGameOver`; nothing writes it, so behaviour is unchanged.
`GobanControl::dumpState()` reports `phase` *next to* the two raw flags rather
than instead of them, so a scenario can catch the derived value and its inputs
disagreeing. The transition table is now written down as tests:
`tests/test_gamephase.cpp` (19 cases: the flags→phase truth table, then one case
per operation that writes those flags) and
`tests/scenarios/game_phase_transitions.scn`, which walks the same lifecycle
through the real application.

Four states were enough, so the abandon criterion did not trigger. Three
findings, all preserved as-is for step 2 to decide deliberately:

1. **`started` is not a function of the phase.** `Finished` is reached two ways
   and they are not equivalent: ending a live game sets `isGameOver` and leaves
   `started` true, while loading a finished SGF leaves it false. Code
   distinguishes them today — `GobanControl::setKomi()` and
   `GameThread::setFixedHandicap()` gate on `!started` alone, so they behave
   differently on a game that just ended than on the same game reloaded.

   This is the one thing that complicates the migration plan: step 2 keeps
   `started` as a deprecated accessor, which is only possible if it is
   derivable. Either clear `started` when the game finishes (a small deliberate
   behaviour change — the audit says the only visible effect is that komi
   becomes editable on a freshly finished game, which the UI disables anyway),
   or accept that `started` outlives the phase and keep it as a separate bit
   until step 3 deletes its call sites. Prefer the former; decide it in step 2,
   not silently.

2. **A freshly constructed model reports `Finished`**, because `isGameOver`
   defaults to true as a stand-in for "not ready yet" until `onBoardSized()`
   runs. Nothing depends on it — the game loop's other guard, `!model`, covers
   that window — but the initial phase should be `Setup`.

3. **The flags cannot separate `Setup` from `Paused`; the record has to.** With
   both clear, an empty record (at the root, no continuation) is `Setup` and
   anything else is `Paused`. Note that `moveCount()` is the *view* position and
   so reads 0 at the root of a loaded game too — the discriminator needs
   `hasNextMove()` as well. A corollary: "new game" is two steps, and
   `onBoardSized()` on its own lands in `Paused` because it clears the flags but
   leaves the record; only `createNewRecord()` completes the transition.

### Step 2 — invert. Done 2026-08-09.

`GobanModel::gamePhase` is now the authoritative state and the `started` /
`isGameOver` booleans are deleted. Every change goes through one private
`transitionTo()`, reached only via named transitions that say what happened
rather than which flag moved: `start()`, `pause()`, `enterReview()`,
`endGame(reason)`, `createNewRecord()`. `isStarted()` and `isGameOver()` remain
as compatibility accessors — now pure functions of the phase — so step 3 is a
mechanical substitution at ~40 read sites rather than a behaviour change.

**There is no rejection table, and that is the finding.** Step 2 was specified
as "a legal-transition table which rejects and logs illegal transitions", but
every ordered pair of phases turns out to be reachable through some supported
action: new game from anywhere, Start on a paused *or* finished game (promoting
a variation from a finished position does exactly this), navigating off the end
of a finished game, navigating back onto it. A matrix that permits everything is
worse than none — it reads like a guarantee and provides none. So the type does
the work instead: the states are mutually exclusive, which is what the old
`started && isGameOver` pair was not, and the single writer gives the lifecycle
one log stream. The only precondition worth enforcing turned out to be
`endGame(NO_REASON)`, which is refused: a finished game always has a result.

Three deliberate behaviour changes, each pinned by a test:

1. **`started` no longer outlives the game.** This is finding 1 from step 1,
   resolved by normalising rather than by carrying an extra bit. Ending a game
   under play now leaves `isStarted()` false, matching a loaded finished game.
   Knock-on effects, all audited: `playLocalMove()` stops queuing a move for a
   finished game (the ADR-0001 stale-move class); `onPlayerChange()` stops
   annotating in the window between a double pass and RE being written;
   `setFixedHandicap()`'s `started` guard becomes unreachable, its callers
   already running after the phase has left `Playing`. `setHandicap()`,
   `operator bool()` and `cmdStart` are unaffected — each is separately guarded
   by `state.reason` or `hasGameResult()`.

2. **Komi is editable in `Setup` and `Paused` only.** The old `!started` guard
   refused a game that had just ended but allowed the same game reloaded from
   SGF, because loading leaves `started` false. Rather than propagate the
   laxer of the two, both are refused: RE was scored with the komi in force, so
   editing it afterwards corrupts the record. `GobanControl::setKomi()` and
   `GobanModel::onKomiChange()` carry the same guard and must stay in step.

3. **A fresh model starts in `Setup`, not `Finished`** — finding 2 from step 1.
   The game loop is held off by its other guard, `!model`, which still holds.

One consequence worth knowing: the navigation restore path now calls
`endGame()`, which sets `state.reason` from `isResignationResult()`. Previously
it set only the flag, so a position could be "over" with `reason ==
NO_REASON` after a start/navigate-back/navigate-forward sequence. The phase and
the reason can no longer disagree.

**Next: step 3** — delete `isStarted()` / `isGameOver()`, one file at a time,
replacing each read with the phase predicate it actually means. Several are not
straight substitutions: `!isGameOver()` at a *call* site usually means "the game
is playable", which is `Setup || Paused || Playing`, and spelling that out is
the point of the exercise.

**Two harness bugs surfaced**, both found by the step-2 scenario asserting state
immediately after every transition — which no earlier scenario did, because
earlier ones assert against a bot match where engine latency hid the windows.
Neither is caused by this ADR; both would have corrupted any future scenario.

- `hasPendingNavigation()` reported false between `processNavigationQueue()`
  popping a command and `GameNavigator` raising its own `navigationInProgress`
  flag, so `wait_idle` could return *during* a navigation and read the board
  before it had applied. Fixed with an in-flight counter incremented under the
  queue mutex — the same shape `processDeferredTask()` already used. Reproduced
  roughly one run in eight; zero in twenty-five after the fix.
- `wait_idle` does not cover territory scoring, so the SGF `RE` property lags
  `game_over`: it is written by the scoring pass, not by the closing move. This
  one is *not* fixed. The obvious predicate — "territory requested but not
  ready" — is permanently true when scoring fails, which would hang every
  `wait_idle` instead. Scenarios must `wait_until has_result true` before
  asserting anything that depends on the record being final.

### Step 3 — delete the compatibility accessors. Done 2026-08-09.

`isStarted()` and `isGameOver()` are gone; `phase()` is the only way to ask. The
`started` and `game_over` keys are gone from `dumpState()` too — one key, one
truth — and the scenarios that asserted them now say `expect phase <name>`.

The prediction in the step-2 log, that `!isGameOver()` would expand into a
spelled-out set of phases, was wrong: `Setup || Paused || Playing` is just
`!= Finished`, so writing it out gains nothing over the comparison. The value of
this step turned out to be elsewhere — three redundant conditions that only
became visible once the states were a type:

- `GameThread::gameLoop()`'s `!model || model.isGameOver()` is exactly
  `phase() != Playing`. `operator bool()` *is* "Playing", so the second half
  could never add a case. It now says what it means.
- `GobanControl::boardClick()`'s `if (isGameOver()) … else if (!isGameOver())`
  had an else-if that was structurally the negation of its own if.
- `GobanModel::onBoardChange()` re-tested `isGameOver()` inside a branch already
  guarded by it.

The three identical "restore Finished at the end of a finished game" blocks in
`GameNavigator`, and the two identical "show the result" blocks, collapsed into
`restoreFinishedStateAtEnd()` and `showEndOfGameResult()`.

**One inconsistency found and deliberately left**, pinned by a test:
`state.reason` is not part of the phase and outlives it. `enterReview()` drops
`Finished` but leaves the reason set, and the UI's own notion of "over" —
`ElementGame::OnUpdate()` and `GobanControl::setHandicap()` — is
`state.reason != NO_REASON` rather than the phase. So after navigating back from
a finished game the phase says `Paused` while the toolbar still greys out Start,
Pass, Resign and Undo, even though those commands check the phase and would
work. This predates the ADR: the old code cleared `isGameOver` there and left
the reason alone in exactly the same way. The fix is for `enterReview()` to
clear it, as `start()` already does — a UI behaviour change that wants its own
commit rather than a refactor's.

**`GamePhase` is now complete.** Remaining: step 4 (`EngineSync` and
`LoopState`, both local to `GameThread`) and step 5 (decompose
`ElementGame::OnUpdate()`, retiring `syncingUI`).

### Step 4 — EngineSync and LoopState. Done 2026-08-09.

Both are private to `GameThread`, so this step touches no other file except
`GobanControl::isIdle()` and `dumpState()`.

**`EngineSync`** replaces `enginesSynced`. The third state is not decoration:
the bool could not distinguish work *pending* from work *happening*, and the
difference decides whether a caller waiting for quiescence should wait.
`newGameNow()` interrupts the loop and *then* marks the engines out of date, so
after a new game the state is `Unsynced` with nothing running — indefinitely,
until the user's first move starts the loop. Treating that as busy would stall
`wait_idle` forever. `Syncing`, by contrast, is held only while the game thread
is replaying the record, and is left unconditionally at the end of the replay,
failure included, so it cannot strand anyone. `isIdle()` now waits on `Syncing`
alone — a real gap closed, of the same family as the navigation one in step 2.

**`LoopState`** replaces `hasThreadRunning` and `interruptRequested`.
`Stopping` is the state those two encoded jointly and neither named: the thread
is alive, because an engine mid-genmove cannot be aborted, but it will exit as
soon as that call returns.

**`deferredPending` is not folded in**, contrary to the plan above. A deferred
task can be queued while the loop runs perfectly happily, and it never means
"stopping" — it is work waiting, not lifecycle. Folding it in would recreate
exactly the conflation this ADR exists to remove, which is the same reasoning
the decision used to exclude `tsumegoMode` and friends. The two meet in one
derived question, now named `shouldDiscardMove()`.

Collapsing two independent bits into one enum introduces a hazard the bits did
not have: an unconditional write to `Running` can swallow a `Stopping` that
arrived in between. So the loop announces itself with a compare-exchange from
`Stopped` only, once, before the first iteration rather than inside it. That
also closes a latent deadlock — an interrupt landing between `gameLoop()`
clearing the old flag and its first loop test used to leave `run()` waiting on
`engineStarted` forever, because `hasThreadRunning` was never set.

`interrupt()` now leaves `Stopped` after a successful join, where the old code
left `interruptRequested` set until something else happened to clear it. On the
timeout path it stays `Stopping`, which is exactly what that path means. It also
enters `Stopping` by compare-exchange from `Running` only: the mirror of the
same hazard, and one this refactor briefly introduced. Writing it
unconditionally would claim an already-exited loop was still alive, and the
timeout poll would spin out its full deadline and report failure for a thread
that was ready to join — reachable after a timed-out interrupt whose loop later
exited on its own. Latent today, since no caller passes a timeout, but the
timeout path is documented API.

The wider lesson for step 5: collapsing independent bits into one enum removes
illegal combinations and adds lost-update hazards in their place. Every write
to such an enum wants to be conditional on the state it expects.

`dumpState()` reports `engine_sync` and `loop_state`.

**Remaining: step 5** — decompose `ElementGame::OnUpdate()` and retire
`syncingUI`. That is a rendering-layer change with no test harness behind it,
so it is a different kind of risk from steps 1–4 and should be planned on its
own.
