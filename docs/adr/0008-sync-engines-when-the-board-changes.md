# ADR-0008: Engines sync when the board changes, not when the player moves

**Status:** Accepted
**Date:** 2026-08-15

## Context

Changing the board size froze the application for several seconds — but not at
the moment of the change. The board redrew instantly, the toolbar lit up, the
evaluation overlay carried on. The freeze arrived later, on the first attempt to
play: the stone came out of the bowl, and then nothing, for long enough to click
three more times.

`GameThread::clearGame()` left the engines `EngineSync::Unsynced` with the game
loop *stopped*, and the comment said so plainly: "this can sit Unsynced
indefinitely — until the user's first move starts it." The first click called
`model.start()` and `run()`, the loop woke, and only then did it replay the
position into every engine. On a CPU KataGo, rebuilding for a new board size is
seconds.

Nothing surfaced any of it. `isIdle()` counted `Syncing`, so scenarios waited
correctly and the suite stayed green; `uiInputs()` never gathered it, so the
policy could not grey anything; `isThinking()` was false throughout, because no
genmove was in flight. `playLocalMove()` found `playerToMove` null and fell
through to `queuedMove` — **a single slot, not a queue** — so each further click
overwrote the last. Four clicks produced one stone.

The decisive observation was that **the SGF load path already does the right
thing** (`loadSGF`: "Start game thread early … Must mark Unsynced BEFORE
run()"). Two entry points to the same state had grown apart, and only one of
them made the user pay.

## Decision

**`clearGame()` starts the game loop itself**, so the replay runs while the
board is empty and the player is idle. New game, board size and handicap all
take that path.

Placement is load-bearing in two ways:

- **After `setFixedHandicap()`**, because the replay carries the setup stones it
  places. Starting earlier would race the handicap onto the board behind the
  replay's back.
- **Behind `if (!isRunning())`**, because `clearGame()` also runs *on* the game
  thread when a discarding action was deferred past a genmove (ADR-0001), and
  `run()` takes `playerMutex`, which that path may already hold.

Two supporting changes, each of which is useless alone:

- **`UiInputs` gains `enginesSyncing`**, and `availableActions()` folds it
  together with `engineThinking` into one `engineBusy` term. That is the
  question every guard there meant to ask. A new action `a.play` covers board
  clicks, which were not policy-driven at all; it is *assigned* from `a.pass`
  rather than restated, because a move at the cursor is a move at the cursor and
  the two drifting apart is what ADR-0005 exists to prevent.
- **`getIdleTimeout()` covers `isSyncingEngines()`**, and `#lblStatus` gains
  `tplStatusSyncing`. Without the timeout the main loop blocks in
  `glfwWaitEvents()` for the whole sync and draws no frame, so a message would
  never be painted; without the message there is nothing to paint. This is the
  same trap as the exit hang in ADR-0007's log: no input arrives, so nothing
  repaints, so nothing can be reported.

## Consequences

The wait moves to where it costs nothing, and when it is still visible — a
resync already under way — it is named rather than silent.

**The move that starts a sync cannot be refused, only the ones during it.** With
the engines `Unsynced` and the loop stopped there is nobody to do the work, so
refusing the click that would start it deadlocks the game. `a.play` is therefore
true for `Unsynced` and false only for `Syncing`. In the shipped design that
distinction is mostly moot, since the sync now begins with the board change —
but it is why the first move is accepted and queued, and why the stone stays in
hand while it is pending. That reads as honest rather than broken: the move is
waiting, not lost.

**`Unsynced` still must not make `isIdle()` false.** The state is now short-lived
on this path, but the rule is unchanged and for the original reason: nothing
guarantees anyone will act on it, so waiting for it to clear can hang.

### Rejected: refuse the click and tell the user to wait

The first proposal, and what was asked for before the eager sync was found. It
treats the symptom — the user still waits, just with the toolbar greyed. It also
cannot apply to the click that starts the sync, so the worst case survives it.

### Rejected: keep it lazy but show a spinner

Same objection. The wait is avoidable, and an avoidable wait that is merely
well-labelled is still an avoidable wait.

### Correction, 2026-08-16: the sync must not start before the new record exists

Shipped broken, and found the next morning. `clearGame()` started the loop
itself, but `newGameNow()` installs the empty record *after* `clearGame()`
returns — so the replay raced `createNewRecord()` and could hand the engines the
game being discarded. The board drew empty; GNU Go still held all 37 stones of
the previous game and answered `? illegal move (command: play B F5)` when the
player opened on F5, which was that game's own first move.

The start therefore moved out to `GameThread::startSyncingNewGame()`, whose only
call site is after `createNewRecord()`. It also checks its precondition —
`moveCount > 0` means the previous record is still installed — and logs an error
rather than syncing, because getting this wrong is invisible from the UI and
surfaces much later as an engine refusing a legal-looking move.

Worth recording that the reproduction failed: three runs against real GNU Go,
replaying the actual recorded game, never fired the race. The window is a few
instructions wide and the original session had KataGo analysing alongside. So
the scenario named below pins the behaviour, not the race — the ordering is
guarded structurally, by there being one call site, and by that check.

### Accepted cost: work that may be discarded

Changing the board size twice in quick succession syncs twice. Each `clearGame()`
interrupts the loop first, so the second supersedes the first rather than racing
it, and the wasted work is bounded by how fast a person can use a dropdown.
