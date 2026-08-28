# ADR-0009: A killed engine is restarted and resynchronised

**Status:** Accepted
**Date:** 2026-08-16

## Context

`GtpClient::issueCommand()` bounds every command with a timeout and, when it
fires, kills the engine (ADR-0001's `setCommandTimeout()`, issue #45). That is
right, and the reason is not impatience: standard GTP has no way to abort a
command in flight, so an engine that answers *late* delivers its reply to
whatever command is read next. Silently wrong results are worse than a dead
engine.

What was missing was the other half. `terminateProcess()` had no counterpart
anywhere in the codebase. Once an engine had been killed, `failed_` latched and
every later command returned failure immediately, for the rest of the session,
with one log line to say so.

The case that reaches this is not exotic. GNU Go does not answer
`final_status_list dead` on a sparse 19×19 — it reads life and death
exhaustively and simply never comes back — so a game that ends in a double pass
on a thin board kills the coach. The coach is the referee: it holds the
authoritative board, decides legality, and does the scoring. After that the
player could keep clicking and nothing would happen, because every move was being
offered to a dead process.

Two things made it hard to see. The engine is not *gone* — the `Player` object,
the dropdown entry and the name all remain — and the failure surfaces only as
refused moves, which look like a stuck board rather than a missing engine.

## Decision

**An engine killed for not answering is respawned, and every engine is then
resynchronised from the record.**

- `GtpClient::revive()` starts a replacement process from the spawn parameters
  resolved once in the constructor, so a restart cannot pick a different binary
  than the original did. It clears `failed_` only after the new stderr reader is
  attached.
- `GameThread::reviveFailedEngines()` runs at the top of the game loop, before
  anything that might speak GTP. It is the only thread allowed to do this, for
  exactly the reason ADR-0001 gives: it swaps the pipes.
- A revived engine holds an empty board, so the loop sets
  `EngineSync::Unsynced`. That is the existing machinery — coach first, then
  everyone else — rather than a second, per-engine replay path.
- **Navigation waits for synced engines.** `processNavigationQueue()` leaves a
  command queued while `engineSync != Synced`, because `BACK` issues `undo` and
  `FORWARD` issues `play` against whatever position the engine still holds.
  `TO_TREE_PATH` is exempt: it sets the cursor and syncs the coach itself, and
  startup deliberately queues it before the initial sync so that the sync lands
  on the restored position rather than on the root.
- Bounded: `MAX_REVIVES` (3) per engine per session, then it is left down with an
  error. It is logged once, not on every loop iteration.
- The user is told. The message reaches `#pnlLog` through the spdlog sink at
  `warn`, so no new "show this error" path was added.

`terminated_` was split from the kill it used to imply. It now means only
"shut down on purpose" — teardown, quitting — and such an engine is deliberately
*not* revivable. The timeout path calls a new private `killProcess()`, which
kills and reaps without raising it.

## Consequences

- A wedged engine costs one timeout and a resync instead of the session.
- Every engine is replayed after any single engine is revived. That is heavier
  than strictly necessary, and on a CPU KataGo it is seconds — but it is the one
  path that already exists, is already covered by `isIdle()`, and already leaves
  the invariant "all engines sit at the same position" true.
- The kill now reaps the child. It did not before, so every killed engine left a
  zombie; with restarts that would have been one per attempt.
- Exposed a latent SIGPIPE: `~GtpClient` wrote `quit` into the stdin of a process
  it had itself killed, and only `main.cpp`'s process-wide `SIG_IGN` kept that
  from killing the application. Making the kill synchronous made it reliable
  rather than a lost race, so the destructor now skips the polite `quit` for an
  engine that is already dead, and `goban_tests` installs the same disposition —
  a library in `goban_core` should not depend on a signal handler installed by
  the executable.
- A scoring failure still latches (`Board::territoryFailed`), so the revived
  engine is not immediately asked the question that killed it.

## Alternatives rejected

- **Do not kill on timeout; keep reading.** This is what the code did before
  issue #45 and it hung the game thread outright. The late-reply desynchronisation
  is real and unfixable within GTP.
- **Kill, but only for scoring, and keep the engine for play.** Attractive,
  because `applyTerritory()` already degrades gracefully — but the pipe is
  shared. A `final_status_list` that arrives three minutes later would be read
  as the answer to a `genmove`, which is precisely the failure the kill exists
  to prevent.
- **Revive lazily, at the next command.** It would put process spawning inside
  `issueCommand()`, reachable from any caller on any thread, which is the
  ownership rule ADR-0001 exists to keep. The game loop already has a
  once-per-iteration slot for exactly this kind of housekeeping.
- **Resync only the revived engine.** Cheaper, and wrong in one case that
  matters: `syncEngineToPosition()` reads the record, so a per-engine call has
  to be ordered against the coach's own state anyway. Unsynced already expresses
  "the engines and the record disagree", and having one meaning is worth more
  than the saved replays.
- **Unlimited restarts.** An engine that fails every command would be respawned
  ten times a second — a fork bomb driven by a misconfigured binary.
