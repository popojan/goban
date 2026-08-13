# ADR-0006: The UI reads a published snapshot, not the SGF tree

**Status:** Accepted (stages 1 and 2 of 3 done)
**Date:** 2026-08-13

## Context

`GameRecord` is an SGF tree the game thread mutates freely — moves, navigation,
branch promotion, loading — and its const accessors take no lock. The UI thread
read it anyway, from two places: `GobanControl::uiInputs()`, which runs every
frame behind `ElementGame::syncActionAvailability()`, and `dumpState()`, which
runs on every scenario step and every recorded command.

This was a real data race, not a theoretical one. Adding
`shouldShowTerritory()` to `uiInputs()` for ADR-0005 segfaulted in
`SgfcProperty`'s destructor about one run in six, because `isGameFinished()`
builds and destroys a vector of `shared_ptr<ISgfcProperty>` per node while the
game thread is free to be deleting those nodes.

The existing `GameRecord::mutex` protects almost nothing at this boundary. It
locks twelve mutating methods but **not** `navigateToChild()`,
`navigateToTreePath()`, `markBadMove()`, `promoteCurrentPathToMainLine()`,
`removeGameResult()` or `annotate()` — the last three declared `const` while
mutating the tree — and no reader locks at all. In practice it serialises the
game thread against itself.

## Decision

**Whoever owns the record computes a `GameSnapshot` and publishes it; the UI
thread reads the snapshot and never touches the tree.**

`GobanModel::publishSnapshot()` builds the struct and swaps it behind a mutex
held only for the pointer assignment. `GobanModel::snapshot()` returns a
`shared_ptr<const GameSnapshot>`, so a reader gets a consistent set of fields
that cannot shift underneath it mid-read.

The publish point is `GobanModel::onBoardChange()` — the funnel every position
change already passes through: moves, all six navigation commands, SGF load,
game switch, scoring and handicap. Two paths change the record without a board
change and publish for themselves: `createNewRecord()` and `onBoardSized()`,
which is what the new-game path notifies instead.

`hasUnsavedChanges` is **not** in the snapshot. Saving is not a position change,
so it has no publish point; `GameRecord::unsavedChanges` became
`std::atomic<bool>` instead. That is the rule for anything the UI needs which is
a plain scalar and changes off the position-change path.

## Consequences

- The per-frame race is gone by construction rather than by locking.
- It removes a per-frame *cost* as well: `moveCount()` walks to the root and
  `getLoadedMovesCount()` walks the entire main line. The UI was paying both
  every frame, at a price proportional to the length of the game. The snapshot
  computes them once per position change.
- **A missed publish point shows up as stale UI**, which is a new failure mode.
  It is cheap to detect: the scenario suite asserts `move_count`, `board_size`
  and friends after every action, and it caught both missed points on the first
  run of the change. Anything that changes what the UI displays without going
  through `onBoardChange()` must publish for itself.
- **Stage 2 is done.** `boardClick()`, the `pass` command, `keyPress()`'s
  navigation block and `prev_game`/`next_game` now take one snapshot and decide
  from it. Taking a single snapshot matters as much as not reading the tree:
  reading the fields one at a time would let the position shift between the
  branches of the same decision. The snapshot gained `colorToMove`,
  `variationMoves`, `onBadMovePath`, `atFinishedGame` and `loadedGameCount`.

  Two readers were deliberately left: `GobanModel::hasGameWorthKeeping()` and
  `GobanControl::saveCurrentGame()`. Both run on an explicit user action —
  confirming a replacement, or quitting — rather than during play, and
  `hasGameWorthKeeping()` is unit-tested against records that were never
  published, so moving it would mean changing those tests to publish first.

  **Stage 3 remains:** `model.state.comment` and
  `model.state.markup` are `std::string`/`std::vector` written by the game
  thread in `syncStateAfterNavigation()` and read by the UI thread with no
  synchronisation at all. A reallocation concurrent with a read is a
  use-after-free. That is the `ElementGame::OnUpdate()` message tail, which has
  little coverage, so it should follow coverage rather than precede it.

## Alternatives rejected

**Recursive mutex over every accessor.** Mechanical and complete, and recursive
is required because the predicates nest (`shouldShowTerritory` calls four other
public accessors). Rejected on hold time: `saveAs()` writes the whole SGF to disk
under the lock and autosave runs on the game thread after moves, so a UI thread
locking to read `moveCount()` every frame would stall on disk I/O once per save.
`run_scenarios.sh` already documents that a large daily session file slows
autosave enough to blow scenario waits — that becomes visible jank. Deadlock risk
was *not* the objection: lock order is only ever `GobanModel::mutex` →
`GameRecord::mutex`, since `GameRecord` does not know `GobanModel`.

**`std::shared_mutex` with an internal/external split.** Concurrent readers, and
textbook-correct. Rejected because `shared_mutex` is not recursive and the
predicates nest heavily, so roughly twenty-five accessors would each need an
unlocked twin — a larger diff than the snapshot — while writers still block every
reader for the duration of a save or a load. The same stall, more code.

**Leave it and document.** It had not crashed in production, only under the load
the new scenarios generate. Rejected because "has not crashed yet" is what a data
race looks like right up until it does, and the fix also removes a per-frame cost
that grows with game length.
