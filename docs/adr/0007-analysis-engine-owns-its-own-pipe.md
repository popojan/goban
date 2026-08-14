# ADR-0007: Continuous analysis runs in a process of its own, configured from the kibitz engine

**Status:** Proposed
**Date:** 2026-08-14

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

## Consequences

- **Memory and compute double for the analysis-capable engine.** A second KataGo
  loads a second copy of the weights — nothing is shared across processes — and
  competes for the same device. On a weak machine this is strictly worse than no
  analysis, which is why (3) makes it opt-out-able and why the default must be
  considered carefully; see open question 1.
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
  on.

  *The remaining overlap is startup.* `animateIntro()` renders continuously at
  exactly the moment engines load their weights, which is #45's timeline. Two
  analysis-capable engines means two weight loads inside that same window, on top
  of the animation. Starting the analysis engine lazily — on first enable rather
  than at startup — costs nothing and avoids the one sustained collision.

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
- **`stop` is itself a command.** Cancelling a stream is not free, and an engine
  that ignores `stop` wedges the analysis thread. The scoring timeout precedent
  applies: bound it, and kill an engine that will not answer.
- Accepted: two engines may hold the same weights, and a user who wants exactly
  one KataGo must choose between analysis and playing against it.
- Accepted: the overlay can disagree with the coach about the score, since they
  are different processes with possibly different rulesets. Goban has no ruleset
  concept (see ADR-0003), so this is not newly broken, only newly visible.
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
  the overlay. It also does nothing for the two-positions-at-once problem. Worth
  revisiting only as the low-memory mode if open question 1 demands one.
- **Analyse on the coach between moves.** Same as above with the additional
  property that a wedged analysis stream takes down scoring and legality. This is
  the failure that took the whole chain down on 2026-08-13.
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

Deliberately unresolved. The decision above stands without them, but none should
be settled by accident during implementation.

1. **Is analysis on or off by default when an analysis-capable engine exists?**
   Off is safe and surprises nobody; on is what a user who configured KataGo
   expects. Entangled with the memory cost.
2. **Which protocol?** `kata-analyze`/`kata-analyze ... ownership true` is far
   richer (ownership map, score lead, PV) but KataGo-specific; `lz-analyze` is
   the portable subset. Probe for both, or declare KataGo the only supported
   analysis engine?
7. **Should the analysis instance get its own engine config?** A search tuned for
   a live overlay wants few threads, a small batch and a visit cap — the opposite
   of a playing configuration. Inheriting the kibitz engine's command line
   (decision 2) inherits its `default_gtp.cfg` too. An `analysis_parameters`
   override would fix it; whether that is worth a second configuration knob is
   open. Related: allowing the analysis instance on a CPU backend while the
   playing engine keeps the GPU, which sidesteps the contention above entirely
   on machines with cores to spare.
3. **What does the overlay show while the analysis engine is still loading?**
   Same question as the startup gap in the UX work, and it should get the same
   answer.
4. **Does analysis follow the cursor during review, or only the game position?**
   Following the cursor is the useful behaviour and the reason for a separate
   process — but it means re-sending the whole position on every arrow key.
5. **Ownership/territory display vs. the existing territory toggle.** They are
   different things (live estimate vs. final score) drawn on the same board, and
   `backlog/issue-49-realtime-analysis.md` assumes shader work (grid-line hiding
   at annotated points) that is not done.
6. **Where do win rate and score go?** The message box is already contended by
   results, comments, engine diagnostics and prompts — see the UX work; this
   feature should not be the fifth claimant.

## What this does not change

The coach, the kibitz engine, and the sync invariant are untouched. Space still
asks the kibitz engine for a move on the game thread, exactly as today. This ADR
only says where the *continuous* analysis stream lives.
