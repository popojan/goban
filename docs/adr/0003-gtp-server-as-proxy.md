# ADR-0003: Goban's GTP server role is a transparent proxy, and the launcher owns the game

**Status:** Proposed
**Date:** 2026-08-09

## Context

The community has asked for GTP interoperability, sketched in
`backlog/gtp-interface.md`. Goban is already a GTP **client**: it orchestrates
several engines at once (coach, kibitz, and up to two players). Serving GTP as
well appears to give one game two owners, and that tension is the thing this
decision has to resolve.

The backlog sketch avoids it rather than answering it. `runGtpMode()` builds a
fresh `GobanModel` and `GameThread` and never enters the GUI path, so the two
roles simply never coexist — then `--gtp --visual` reopens the question and
treats the GUI as a passive renderer. It is not passive: it has menus, `clear`,
dropdowns that replace the game, and (since 2026-08-09) a modal confirmation
that blocks waiting for a human.

Three facts shape the answer.

**GTP has no observer role.** Every peer on a GTP connection is an engine the
controller may ask to move; there is no display or spectate command. So "goban
as a board" cannot be built as a passive listener — there is nothing to listen
to.

**A GTP engine sees the whole game, not just its own side.** The controller
sends `play` for the opponent's moves too. Anything that speaks the engine side
of the protocol can render the entire match.

**The standard tools are controllers, and they are the referee.**
`gogui-twogtp`, gomill's `ringmaster`, Sabaki. They own the game, decide the
result, and drive one engine per pipe. Goban is not going to displace them and
should not try.

## Decision

**1. The GTP server role is a transparent proxy, never a playing engine.**

```
controller ──GTP──> goban --gtp --proxy "gnugo --mode gtp" ──GTP──> gnugo
                      └── renders the match live
```

Goban speaks the engine side to the controller, forwards to one downstream
engine, and renders what passes through. Any existing pipeline gains a 3D live
view by wrapping one engine's command line, with no changes on its side. That
interoperability, not tournament participation, is the value.

**2. The process that launched goban owns the game.** Launched by a script:
the controller is the referee and the human at the screen is a spectator — the
camera works, nothing that affects the game does. Launched by a human: goban
orchestrates exactly as it does today. One owner, decided at startup, never
negotiated.

**3. In proxy mode goban is non-authoritative.** It never vetoes a move, never
truncates a position, never prompts. Where its local rules disagree with the
downstream engine's, that is a rendering problem to log, not a refusal.

**4. Goban does not become a match runner.** Batch games and result tables are
`ringmaster`'s job. Goban's contribution is the view.

## Consequences

- The GUI degradation has exactly one implementation point, thanks to ADR-0002
  step 5: an `externallyDriven` input to `availableActions()`
  (`src/UiActions.h`) turns off every game-affecting action at once, tested.
  `GobanModel::hasGameWorthKeeping()` must never raise a prompt in this mode —
  there is no human to answer, and an inbound `clear_board` would otherwise
  hang the session.
- **Non-authoritative contradicts assumptions the code makes today.**
  `GameRecord::replayMoves()` stops at the first move it rejects and
  `isValidMove()` gates placement; the ko/snapback bug (CLAUDE.md, Go Rules)
  showed exactly how that silently truncates a position. Those paths need a
  mode where a disagreement renders as best it can.
- **Territory and score are the real casualty.** Goban has no local scoring:
  the territory flood-fill is local, but dead-stone determination
  (`final_status_list dead`) and the final score (`final_score`) both come from
  a GTP engine. In proxy mode the only engine present belongs to the
  controller's conversation, and anything goban injects there spends the
  controller's clock. Worse, goban has no ruleset concept at all — the ruleset
  is baked into each bot's command line in `config/base.json`
  (`--japanese-rules`, `-r japanese`) — so a score obtained from a *separate*
  local coach could contradict the match's official result. And the controller
  never tells an engine the result, so goban cannot simply be told.
- Accepted: the proxy sits in the `genmove` path and must add no measurable
  latency, which bounds how much rendering happens synchronously.
- Accepted: two GTP roles in one binary, with the client half well-worn and the
  server half new. They share `GtpClient` for the downstream leg.

## Alternatives rejected

- **Goban as a playing engine**, answering `genmove` itself. From a configured
  bot it is a pointless wrapper — run the bot directly. From a human at the 3D
  board it is genuinely interesting ("play me over GTP") but has no tournament
  story, since no controller's time control survives a human thinking, and it
  is a different product. Not excluded forever; excluded from this decision.
- **A passive display peer** that is never asked to move. There is no such role
  in GTP; it amounts to registering as an engine and hoping nobody calls
  `genmove`.
- **A non-GTP side channel** — tailing the SGF `twogtp` writes, or a private
  socket. Less protocol work, but no interoperability story, another surface to
  maintain, and GTP is what was asked for.
- **GTP as a general remote-control API** for goban: load an SGF, switch to
  analysis mode, navigate. GTP has no vocabulary for any of it, and `--script`
  plus the command registry already does this properly (see ADR-0004 if
  written; the registry is the scripting surface).
- **Both roles live at once**, an external peer driving while the human also
  plays. Two owners of one game means a conflict rule for every action.
  Ownership by launcher reduces it to one question asked once.

## Open questions

These are deliberately unresolved; the decision above stands without them, but
none should be settled by accident during implementation.

1. **Is `--gtp` always `--proxy <engine>`?** Or is a degenerate downstream-less
   mode worth having — `genmove` returns an error, and goban is display-only
   for tools willing never to ask?
2. **Territory and score in proxy mode**: show none, run a separate local coach,
   or inject `final_status_list` on the downstream pipe between controller
   commands? Entangled with (5).
3. **Which configured engines survive in proxy mode** — coach, kibitz, neither?
   Simplest is neither, which follows from (2) answering "none".
4. **What happens on `quit`?** Does goban save the SGF, to the session document
   or to a path given on the command line? The controller writes its own record
   and will not ask.
5. **Does this finally force a ruleset concept**, or does the proxy simply never
   claim a score? Related: goban supports no aftermath play — a double pass ends
   the game immediately, with no dead-stone agreement phase.
6. **What may the human spectator still do?** Camera, certainly. Save the SGF?
   Navigate the completed game after the controller has gone?
7. **Time controls.** `time_settings` and `time_left` presumably pass through
   untouched — but if goban ever injects a command of its own, whose clock pays
   for it?

## What this does not change

Human-launched behaviour is untouched: goban orchestrates its engines exactly as
it does today, including bot-versus-bot matches. This ADR only says what happens
when something else launched the process.
