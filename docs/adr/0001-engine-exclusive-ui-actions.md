# ADR-0001: Engine-exclusive UI actions are classified by what they do to the current game

**Status:** Accepted
**Date:** 2026-08-08

## Context

A GTP command in flight owns that engine's stdin/stdout until the engine replies.
Standard GTP has **no portable way to abort one**: `stop` exists only in the
analysis extension (`lz-analyze` / `kata-analyze`), not for `genmove`, and GNU Go
has nothing at all. A `genmove` can legitimately run for minutes.

From this follows the hard constraint:

> Exactly one thread may be in a GTP transaction with a given engine at a time.

The game loop calls `player->genmove()`, a blocking read. `GameThread::interrupt()`
ends in `thread->join()`. So any UI-thread code that called `interrupt()` while an
engine was thinking froze the entire application until the engine replied. Two
actions did exactly that — **Open SGF** and **New Game** — and both were reported
as hangs by the user.

The naive fixes are all wrong in interesting ways. Refusing the action is poor UX.
Retrying on the UI thread still leaves the UI thread doing GTP. Letting both
threads talk to the engine desynchronises the stream and silently corrupts
results.

## Decision

Classify every UI action that needs exclusive engine access by **what happens to
the in-flight genmove result**, and handle each class differently.

**Class 1 — preserves the current game** (board click, navigation, pass, resign)
The pending genmove is still *valid*: it is being computed for a position that
will still exist. Its result must be played, not discarded.
→ **Refused while an engine is thinking.**

**Class 2 — discards or replaces the current game** (New Game, clear, Open SGF,
switch game within a collection)
The pending genmove is *worthless*: it belongs to a position that is about to
cease to exist.
→ **Deferred to the game thread** via `GameThread::runWhenEngineFree()`. The game
thread discards the arriving move and then runs the action, holding sole
ownership of the pipes. The UI shows `Waiting for <engine>...` and stays live.

**Class 3 — terminates everything** (Quit)
Nothing needs preserving.
→ **Kill the engine processes** (`shutdown()`), which unblocks the read via EOF.

And the rule both hangs broke:

> **No UI-thread wait for a genmove, ever.**

## Consequences

- The UI never blocks on an engine. Both reported hangs are gone.
- A Class 2 action lands *after* the engine's move arrives, so the board changes
  a moment after the click rather than instantly. This is accepted: the
  alternative is a frozen window.
- The engine keeps its process and its loaded weights. Killing and respawning
  would have cost a full network reload — tens of seconds for KataGo, i.e. worst
  precisely when the engine is slowest.
- Deferred actions **coalesce**: only the most recent is kept, since they all
  discard the game anyway.
- Class 2 actions had to be split into an engine/model half and a UI half
  (`GobanControl::finishGameReplacement`), because the deferred half runs on the
  game thread and RmlUi is not thread safe.
- The per-command GTP timeout (added the same day) is what bounds the worst case:
  a wedged engine can no longer defer a Class 2 action forever.
- `interrupt()` is a no-op when called from the game thread itself — joining
  there would deadlock on self-join, and setting `interruptRequested` would kill
  the loop the caller still needs.

## Alternatives rejected

- **Refuse the action while thinking.** Uniform and trivial, but with a slow
  engine the user simply cannot open a file for a minute. Rejected on UX.
- **Kill and respawn the engine.** Unblocks instantly and the load re-syncs
  everything anyway, so it is semantically clean — but the respawn cost lands
  exactly when the engine is slowest, and no respawn path exists.
- **Retry from the UI thread each frame.** Keeps the UI responsive, but leaves
  GTP work on the UI thread and races between the `isThinking()` check and
  `interrupt()`. Treats the symptom.
- **Let both threads talk to the engine.** Breaks the single-owner constraint;
  a late reply is read as the answer to the next command. Silently wrong results
  are worse than a hang.
- **Generalise `NavCommand` with per-action fields.** It is already effectively a
  tagged union; adding `path` / `gameIndex` / `startAtRoot` arms would grow it per
  action. A `std::function<void()>` task queue has no union problem.

## Invariants established

Recorded in CLAUDE.md, enforced by
`tests/scenarios/load_while_engine_thinking.scn`:

- No UI-thread wait for a genmove.
- Class 2 actions run on the game thread and discard the pending move.
- Quiescence (`GobanControl::isIdle`) accounts for engine thinking, queued
  navigation, **and** a deferred action still running.

## Footnote

Implementing this surfaced a separate pre-existing bug: after the initial engine
sync the loop fell through into `genmove` even for a *paused* loaded game,
because the `!model` early-out had been evaluated before `enginesSynced` flipped.
For a loaded SGF the active player is a `LocalHumanPlayer`, whose `genmove()`
blocks on a condition variable forever — silently wedging the loop so queued
navigation was never drained, while `isThinking()` reported false because the
stuck player is not an engine. Fixed by re-evaluating from the top of the loop
after syncing.
