# ADR-0007: Continuous analysis runs in a process of its own, configured from the kibitz engine

**Status:** Accepted — Stage 1 implemented 2026-08-15
**Date:** 2026-08-14
**Revised:** 2026-08-14 — the six open questions are resolved into decisions
7–15; three narrower ones are left open. That was the last edit: from `Accepted`
onwards the append-only rule in [README](README.md) applies, and the
implementation log below is additive.

## Context

Issue #49 asks for a realtime analysis overlay: win rate, score estimate, and
the top few moves drawn on the board while the human thinks. The obvious way to
build it is "ask the kibitz engine" — it is already the engine nominated for
suggestions, and `docs/architecture.md` §3 already describes it as "the one
asked for suggestions and for analysis-mode replies".

That reading does not survive contact with three facts.

**The kibitz engine is the coach by default.** `PlayerManager::loadHumanPlayers()`
ends with `if (!engines.empty() && !kibitzConfigured) kibitz = coach;`
(`src/PlayerManager.cpp:215`). `coach` and `kibitz` are indices, not processes,
and on a stock `config/base.json` — one enabled engine carrying `main` — they
name the same pipe. So "analysis runs on the kibitz engine" means, for most
installs, "a streaming analysis command runs on the process that owns the
authoritative board, decides legality, and produces the final score". That is
[ADR-0001](0001-engine-exclusive-ui-actions.md)'s hazard restated: a command in
flight owns the engine's pipes until it replies, and an analysis stream is a
command that deliberately never replies until told to stop.

**Kibitz and analysis want opposite things from the engine's board.** A kibitz
request is `genmove`, which *plays* the move into that engine — which is
precisely why `syncOtherEngines()` skips the kibitz engine when it made the move
(`src/GameThread.cpp:419`). Analysis must leave the board untouched. Worse, the
two want to be at different positions at once: while the user reviews a variation,
the overlay should analyse the position under the cursor, but the coach must
stay at the game position or the next `play` desynchronises it. One process
cannot hold two positions.

**Analysis is a capability, not a role.** GNU Go has no `kata-analyze` and no
`lz-analyze`. Whatever the configuration says, the code must handle "the
nominated engine cannot analyse" — so a design that assumes analysis and kibitz
coincide needs the split case anyway, as a fallback rather than as the model.

Against that sits one real argument for reuse, and it should not be lost: the
user has already nominated an engine they trust for suggestions, a third role is
configuration friction, and **the move the overlay stars ought to be the move
Space actually plays**. An overlay recommending A while kibitz plays B is a bug
report waiting to happen.

## Decision

### The process

**1. Continuous analysis runs in a dedicated engine process with its own pipes.**
It is never the coach's process and never the kibitz engine's process, even when
all three are the same binary with the same command line.

**2. Its configuration defaults to the kibitz engine's.** The `analysis` role
resolves as: an engine explicitly carrying `"analysis": 1`, else the engine
carrying `"kibitz": 1`, else none. Only the *configuration* is inherited — the
process is separate. This keeps the common case zero-configuration and keeps the
overlay's recommendation consistent with what Space plays, without sharing a pipe.

**3. Analysis is opt-in and degrades to absent.** No analysis engine resolves, or
the resolved engine answers `unknown command` to the analysis probe, or the user
turned it off: the overlay is simply unavailable, and says so once. It never
falls back to borrowing the coach.

**4. The analysis engine is not a member of the sync invariant.** CLAUDE.md's
"all enabled engines stay in sync at the same position" governs engines that may
be asked to play or to score. The analysis engine is asked for neither. It is
driven to whatever position the *view* is at, which during review is not the game
position, and it is excluded from `syncOtherEngines()`.

**5. It gets its own thread, not the game thread.** ADR-0001 gives the game
thread exclusive ownership of the playing engines' pipes; this decision gives an
analysis thread exclusive ownership of the analysis engine's pipe. The two never
touch each other's. Results reach the UI the way everything else does — published
as plain immutable data, read per frame ([ADR-0006](0006-publish-a-game-snapshot.md)),
never by the UI thread reaching into the engine.

**6. Analysis yields to the game.** The stream is stopped while any playing
engine is searching, and resumed when the human is on move. This is not a
courtesy: on the GPU it is the difference between this feature and a
bot-versus-bot match, which never has two searches in flight at once — see the
contention consequence below. It costs little, because analysis is worth most
exactly when the human is thinking and the coach is idle.

### What position it analyses

**7. Analysis follows the review cursor, not the game position.** The overlay
reports on the position that is on screen, including inside a variation the user
navigated into rather than played into.

This is the decision that justifies decision 1. If analysis only ever tracked the
game position, the coach already holds that position, and the rejected
"time-slice one process" alternative below comes back into contention — a
separate process would be tidiness rather than necessity. Following the cursor is
also the case a Go player actually wants numbers in.

Three consequences are part of the decision, because the naive implementation of
each is wrong:

- **Sync by diff, not by replay.** Reusing `GameThread::syncEngineToPosition()`
  (`boardsize` → `clear_board` → setup stones → replay every move) would make an
  arrow key cost one command per move played so far. The analysis thread owns its
  own pipe and tracks the path it last sent, so it sends the difference: common
  prefix, then *k* × `undo`, then *m* × `play`. An arrow key is **one command**.
  Only a jump or a branch switch pays a replay, and that replay is abandonable.
  This is also more *correct* for KataGo, whose network reads recent-move history
  planes: reconstructing a position as setup stones changes the policy output,
  replaying moves does not.
- **Latest-wins, never a queue.** Holding an arrow key must *replace* the pending
  target, not enqueue one analysis per key repeat, behind a short debounce
  (~200 ms). This is not a concession to cost: analysing a position the user is
  already flicking past has no value, so discarding it is the correct semantics.
- **The path has to be published.** `GameSnapshot` carries `moveCount` and
  `lastStoneMove`, not the move list. Following the cursor needs a position
  identity plus the moves that reach it, published from the same funnel
  (`GobanModel::onBoardChange()`) as everything else in ADR-0006. That is an
  addition to an existing mechanism, not a new one.

### Defaults and configuration

**8. Off by default, discoverable, sticky, and started lazily.** The toggle is
present whenever an analysis-capable engine resolves; enabling it is what starts
the process; the choice persists in `user.json`. On-by-default would ship a
second weight load to every machine — including the ones issue #45 says this can
break — to save one click, and a sticky toggle costs the KataGo user that click
once ever, not once per session. Lazy start also keeps the second weight load out
of `animateIntro()`, which is the one sustained contention window (see below).

**9. Speak `lz-analyze`; treat `kata-analyze`'s extra fields as optional.**
KataGo's line format is lz-analyze's with extra keys, so parsing the shared
subset (`move`, `visits`, `winrate`, `order`, `pv`) and reading `scoreLead` and
`ownership` when present costs almost nothing over a KataGo-only parser, and
keeps Leela Zero, Sai and the KataGo forks working. Capability is decided by
`list_commands` at startup, **not** by the engine's name: the existing
`GtpEngine::supportsKataAnalyze()` (`src/player.cpp:166`) substring-matches
"kata" in the reported name and must be replaced, not extended. Each optional
field is offered only where it is reported: an engine that speaks `lz-analyze`
but not `ownership` gets win rate and suggested moves and no ownership toggle,
which is decision 12 applied per field rather than per engine.

**10. The analysis instance takes a configuration override, not a second engine
entry.** `analysis_parameters` (and, if needed, `analysis_command`) on the engine
it inherits from; absent means inherit unchanged. A search tuned for a live
overlay wants few threads, a small batch and a visit cap, which is the opposite
of a playing configuration and would otherwise arrive via the inherited
`default_gtp.cfg`. The decisive argument is not tuning, though: this is what lets
the analysis instance run a CPU/Eigen backend while the playing engine keeps the
GPU, which **sidesteps the contention consequence below entirely** on any machine
with cores to spare. That makes the knob the mitigation for this ADR's worst
risk, not a convenience.

### What the user sees

**11. Analysis gets its own display surface, and it is a third one.** Not
`#lblMessage`. CLAUDE.md's two message surfaces split on ownership —
application status versus game content — and this splits on a different axis that
now has to be named:

> `#lblMessage` carries **events**: things that happened once, that scroll past.
> The analysis surface carries **state**: a value that is continuously true.

A continuously-changing number in `#lblMessage` would be the fifth claimant *and*
the only one that repaints twice a second, flickering over results and SGF
comments. Build it in RmlUi — a bar and a label get layout, styling and
translation for free — and reserve `GobanOverlay`/glyphy for the on-board move
labels, which genuinely need board coordinates. The per-colour corners in
`grpPlayers`, which already carry the capture counts, are the natural home for a
per-colour evaluation. **Fix the frame of reference at display time:** KataGo
reports winrate for the side to move, so showing it raw flips every move and
reads as noise. Pick one colour and convert. This is the same shape as the
prisoner counts that shipped swapped.

**12. Absent data is absent. Never a placeholder.** While the analysis engine is
loading, or after a failed probe, the overlay shows nothing — no empty bar, no
zeroed score, no spinner on the board. Loading progress goes to `#lblStatus`,
which is where application status already lives. A winrate bar sitting at 50%
because nothing has been computed is indistinguishable from a genuine 50%; that
is CLAUDE.md's "a failed score is not a score of zero" restated for a new
subsystem, and it is the same mistake that cost a day on 2026-08-13.

The yield window (decision 6) is the one case that is *stale* rather than absent,
and it is different in kind: the numbers were true for a position the board no
longer shows. Blanking them once per move would flicker; leaving them unmarked
would assert them. They stay, visibly marked as stale, until the next report
replaces them.

**13. Live ownership and the Territory toggle are mutually exclusive, and look
different.** They are different claims about the board: Territory is a final,
scored fact gated on `scoredEnd`; ownership is a live estimate that is wrong by
construction early in the game. Territory keeps the existing opaque area markers;
ownership renders as continuous shading, so the two cannot be mistaken for each
other, and enabling either disables the other. Ownership is its own sub-toggle,
below win rate and score, because `kata-analyze ... ownership true` roughly
doubles the stream volume for something most users will not want on.

This also settles the shipping order. `backlog/issue-49-realtime-analysis.md`
treats shader work as a prerequisite for the whole feature. Half of it is already
done — grid-line hiding at annotated points shipped as `Board::mAnnotation` /
`cidAnnotation` (`src/Board.cpp:9`, `fragment/partial/scene/object/stones.glsl`)
— and the half that remains, the two-layer glyphy overlay, gates only the
**on-board move labels**. Win rate and score need no rendering work at all;
ownership shading needs a *different* rendering decision (open question 1), not
that one. **Ship the numbers first; the board annotations follow.**

### Transport and cost

**14. The redraw rate is decoupled from the analysis update rate.** The
overlay wakes the renderer via `glfwPostEmptyEvent()` only when a *displayed*
value changes materially, capped at roughly 2 Hz — not by returning a polling
interval from `ElementGame::getIdleTimeout()`. Without this, the argument in the
consequences below eats itself: it rests on issue #52's fix (a static board
blocks in `glfwWaitEvents()` and costs nothing), and a stream that dirties the UI
on every report turns the ray-traced renderer back on continuously, on the same
GPU, in exactly the window the argument claims is free. This is a decision rather
than an implementation detail because it is where issue #45's class of bug lives.

**15. The stream is not an `issueCommand()`, and stopping it is not a `stop`
command.** `GtpClient::issueCommand()` is request/response and structurally
cannot consume an unbounded stream; the analysis client needs its own reader loop
over `Process::readLine(line, timeoutMs)`, which already exists. And there is no
`stop` in the lz/kata-analyze extension — the convention is that **any** input
line terminates the stream, which `src/player.cpp:194` already exploits by
sending `name`. The timeout obligation stands as written: bound the wait for the
stream to go quiet, and kill an engine that will not, on the `scoringTimeout()`
precedent.

## Consequences

- **Memory and compute double for the analysis-capable engine.** A second KataGo
  loads a second copy of the weights — nothing is shared across processes — and
  competes for the same device. On a weak machine this is strictly worse than no
  analysis, which is why (3) makes it opt-out-able, why (8) makes it off by
  default, and why (10) exists to let the second instance run somewhere else.
- **GPU contention is a known, reported failure mode, and this feature is the
  worst case for it.** Issue #45: a single KataGo repeatedly failing `boardsize`
  on a GT 730 (1 GB VRAM, driver 474), under both CUDA and OpenCL, while
  standalone KataGo and Sabaki worked on the same machine. The differentiator was
  goban's own ray-traced renderer on the same GPU. Three things follow.

  *Idle contention no longer exists.* Issue #52 — 50% GPU load doing nothing —
  was a real bug (unconditional redraw) and is fixed: rendering is event driven,
  `getIdleTimeout()` returns -1 with nothing to draw, and the loop blocks in
  `glfwWaitEvents()`. A static board costs nothing, so the window in which
  analysis is most useful — the human thinking over a still position — is also
  the window in which the GPU is most free. That is the fact this feature rests
  on, and decision 14 is what stops the feature from spending it.

  *The remaining overlap is startup.* `animateIntro()` renders continuously at
  exactly the moment engines load their weights, which is #45's timeline. Two
  analysis-capable engines means two weight loads inside that same window, on top
  of the animation. Starting the analysis engine lazily — on first enable rather
  than at startup — costs nothing and avoids the one sustained collision; (8)
  makes that the default rather than an option.

  *A bot-versus-bot match is not the precedent it looks like.* Two engines in a
  match search **alternately**; there are never two searches in flight. Analysis
  as naively specified overlaps the coach's genmove by construction, making it
  harder on the device than the two-bot case people already run. Decision (6)
  removes the difference by serialising them, which is why it is a decision and
  not an optimisation.
- **A new position-change consumer.** The analysis thread has to be told where the
  view is, including during review. That is a second subscriber to the same
  position-change signal the snapshot already carries, and it must tolerate the
  user navigating faster than the engine answers — every position change cancels
  the outstanding analysis.
- **Decision 7 makes decision 4 load-bearing, not merely true.** Once analysis
  follows the cursor, the analysis engine is *permanently* at a position nobody
  else is at, so it can never be borrowed for a kibitz move, a legality check or
  a score — not "should not", *cannot*. Any future code that reaches for "an
  engine that is already loaded" must exclude it explicitly.
- Accepted: two engines may hold the same weights, and a user who wants exactly
  one KataGo must choose between analysis and playing against it.
- Accepted: the overlay can disagree with the coach about the score, since they
  are different processes with possibly different rulesets. Goban has no ruleset
  concept (see ADR-0003), so this is not newly broken, only newly visible.
- Accepted: a third display surface. CLAUDE.md's "two surfaces, and they do not
  overlap" becomes three, with the event-versus-state split in (11) as the rule
  that keeps them from colliding. That invariant moves into CLAUDE.md when the
  surface exists, not before.
- Good: nothing in the existing game loop changes. No genmove is interrupted, no
  pipe is shared, and the feature can be built and tested without touching
  `gameLoop()`.

## Alternatives rejected

- **Use the kibitz engine's process directly.** The proposal this ADR started
  from. Loses on the three facts in Context: it is the coach by default, its
  board is mutated by `genmove` and must stay at the game position, and it may
  not be analysis-capable. Its *intent* — one nominated engine, no extra
  configuration, consistent recommendations — is preserved by (2).
- **Time-slice one process**, running analysis in the game loop's idle windows
  and stopping it before every real command. One process, no extra memory. Loses
  because the only reliable idle window is the 500 ms inter-move sleep; every
  navigation, `play`, and `final_score` would have to stop and restart the
  stream, and a `stop` that is slow to answer stalls the game rather than just
  the overlay. It also does nothing for the two-positions-at-once problem, which
  decision 7 makes the central one. Worth revisiting only as the low-memory mode
  if the memory cost proves unaffordable in practice.
- **Analyse on the coach between moves.** Same as above with the additional
  property that a wedged analysis stream takes down scoring and legality. This is
  the failure that took the whole chain down on 2026-08-13.
- **Track the game position only, and hide the overlay during review.** The
  cheap half of decision 7: no path diffing, no new published field, and the
  overlay is never wrong about what it describes. Rejected because it removes the
  reason for a separate process — the coach is already at that position — and
  because review is where the numbers are worth most. The cost that motivated it
  turned out to be an artefact of reusing `syncEngineToPosition()`; incremental
  `undo`/`play` makes an arrow key one command.
- **Bind the overlay to `GameMode::ANALYSIS`.** Superficially attractive: a mode
  called Analysis showing analysis. Rejected because that flag means something
  unrelated — Sabaki-style turn handling, where the human plays either colour and
  the engine auto-replies — and it is refused outright for human-versus-human
  matches (`GameThread.cpp:1065`). Enabling a display feature would change *who
  plays*. Given what `coach`/`kibitz`-as-indices already cost, the overlay gets
  its own toggle and a name that is not "analysis mode".
- **Put win rate and score in `#lblMessage`.** Zero new UI. Rejected by (11): it
  is already contended by results, comments, diagnostics and prompts, and this
  would be the only claimant that repaints continuously.
- **A separate lightweight estimator inside goban** (no engine at all). Goban has
  no evaluation function and no ruleset concept; territory flood-fill is local
  but dead-stone determination already comes from an engine. Building one is a
  different project.
- **Reuse the stderr regex filters as the analysis channel.** They exist and
  already parse KataGo's output, but their result is collected once per completed
  move into the SGF comment (`GameThread::collectEngineComments()`), which is
  post-hoc annotation, not a live feed. The filters stay useful for move
  comments; they are not the transport for this.

## Open questions

The six that shipped with this proposal are resolved above. Three narrower ones
remain, and none of them blocks the numbers-only first stage.

1. **What does continuous ownership shading render as?** The board carries one
   float per point, and it is already doing two jobs: a small enum of material
   values (`Board::mEmpty` 0, `mAnnotation` 1, `mBlackArea` 2, `mWhiteArea` 3,
   `mBlack` 5, `mWhite` 6 — the backlog's claim that 1.0 is free is stale, the
   grid-hiding material exists) plus a fractional offset, `mDeltaCaptured` 0.5,
   added on top. Territory is binary and fits an enum slot; a continuous
   ownership value in [-1, 1] does not, and the one free slot (4.0) buys nothing.
   Whether ownership extends the fractional convention that `mDeltaCaptured`
   started, or takes a second channel, is a rendering decision of its own and
   blocks only ownership — not win rate or score.
2. **Does analysis run in tsumego mode?** An overlay that stars the correct move
   destroys the puzzle. Off is almost certainly right, but tsumego already
   carries blanket exclusions elsewhere (`suppressSessionCopy`), and whether this
   is one more of those or a user-visible choice should be settled when tsumego's
   UX is next touched rather than guessed at here.
3. **The stream's visit budget and report cadence.** Decision 14 caps the
   *redraw* rate; the engine-side `maxVisits` and report interval that keep a
   live search from saturating a device are a measurement question on real
   hardware, not an argument. Settle them with numbers.

## Implementation log — Stage 1, 2026-08-15

Win rate and score, no board annotations, as decision 13 scoped it. The feature
is called **Evaluation** in the interface, deliberately not "analysis": that word
was already spent on `GameMode::ANALYSIS`, which changes who plays.

`src/AnalysisService.{h,cpp}` in `goban_core`, owned by `ElementGame` and
declared after `engine` so it is torn down first. `GtpClient` gained
`streamCommand()`, `stopStreaming()` and `setQuiet()`; `GameSnapshot` gained
`positionId`, `pathMoves`, `komi` and the setup stones; `GameThread` gained
`analysisMayRun()`; `PlayerManager` records the analysis bot's configuration.

**Where the plan was wrong, and what was done instead:**

- **The position is polled, not observed.** The plan had the service subscribe to
  the position-change signal. `GobanModel::onBoardChange()` both publishes the
  snapshot *and* fans out to observers, so an observer's position in that list
  decides whether it sees the new snapshot or the old one — a correctness
  question answered by a registration order nothing else depends on. Polling
  `snapshot()` on the loop's 100 ms tick makes latest-wins automatic and the
  ordering irrelevant, and 100 ms is invisible next to the 200 ms debounce.

- **`scoreLead` had to move into the per-move block, and `order` had to default
  to -1.** Both were found by the first run of `tests/test_analysis.cpp`, and
  both are the same underlying mistake — assuming a key's position in the line.
  KataGo emits `scoreLead` *before* `order`, so a report-level "keep the one from
  the best block" test reads `order` while it is still at its default and the
  last block parsed wins. And defaulting `order` to 0 makes "the engine did not
  say" indistinguishable from "this is the best move", so the most-visited
  fallback could never fire. This is why the parser tests assert on a line with
  the extension keys interleaved rather than on a tidy one.

- **A pre-existing hang in the main loop, made reachable here.** `main()` sets
  `AppState::RequestExit()` from inside a loop iteration and then, with nothing
  to render, blocks in `glfwWaitEvents()` for an event that will never arrive;
  the loop condition is not re-evaluated until the next iteration. Latent for as
  long as it has existed, because whatever ends a run normally dirties the view.
  Decision 14's publish gate requests *no* repaint when the displayed values have
  not changed, which is precisely a clean frame — so a scenario ending while the
  overlay was running hung for the full 120 s harness timeout. Fixed in
  `main.cpp` by not entering the blocking wait once an exit is requested. Found
  by `evaluation_overlay.scn`, diagnosed from a thread dump.

- **A third status branch was needed, and a fourth string was not.** The existing
  loading indicator is gated on `enginesLoaded`, which is long true by the time
  the user enables this, so the analysis engine could not reuse that branch. It
  does reuse the *template*: "Loading <engine>…" is exactly what is happening, and
  it costs no new string in five languages.

- **`run_scenarios.sh` learned a `# config:` directive.** The harness had one bot
  list per run, so the two-processes-from-one-configuration case could only have
  been run by hand — meaning the documented one-command suite would silently not
  cover the feature.

Not done, and unchanged from the three open questions above: on-board move
labels, ownership shading, and the visit-budget numbers. Tsumego is settled for
now in the narrow way decision 13's scope allows — `availableActions()` refuses
the toggle while `tsumegoMode` is set, since an overlay that stars the correct
move solves the puzzle.

## What this does not change

The coach, the kibitz engine, and the sync invariant are untouched. Space still
asks the kibitz engine for a move on the game thread, exactly as today. This ADR
only says where the *continuous* analysis stream lives, what position it tracks,
and where its output is allowed to appear.
