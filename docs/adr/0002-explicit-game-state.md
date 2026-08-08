# ADR-0002: Replace the lifecycle flags with explicit state machines

**Status:** Accepted 2026-08-09 — implementation not started
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
