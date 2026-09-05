# ADR-0015: Tsumego is a game mode, and the menu reports it rather than offering it

**Status:** Accepted — implemented 2026-09-05
**Date:** 2026-09-05

## Context

`GameMode` was `MATCH` / `EXPLORE`, and tsumego was a separate
`std::atomic<bool>` on `GobanModel`, written at five call sites. Two independent
variables, so their product was representable — and one cell of it is wrong.

**`tsumego + EXPLORE` plays the puzzle for the solver.** Explore's defining
property is that the kibitz engine answers *every* move
(`GameThread.cpp`, the `gameMode == GameMode::EXPLORE` branch). In a puzzle the
record already answers, on the solution path and on a refuted one alike. So the
engine's reply lands on top of the recorded refutation, at a position the
problem has an opinion about. Read from the code, not reproduced from a session —
which is the point: nothing in the UI could reach that state deliberately, and
nothing stopped it being reached by accident (a session restored with both
booleans set, an SGF whose result flipped the mode under a puzzle).

The UI was meanwhile already *pretending* they were one control: a single menu
item, checked if either was set, its label swapped between `tplAnalysisMode` and
`tplTsumegoMode`. A control that renders two variables as one is a statement
about what they should have been.

Two smaller faults fell out of the same shape. `finalizeGameLoad()` decided the
mode from `hasGameResult()` alone, so a load could silently drop a tsumego the
file chooser had just asked for; and after the Explore rename (5204bef) the
menu's checkmark asked `OnMenuToggle("toggle_analysis_mode", …)` for a class no
element carried any more, so it had quietly stopped tracking anything at all.

ADR-0007 left this open explicitly: *"tsumego already has a scatter of
special cases … should be settled when tsumego's own design is written down"*.
This is that.

## Decision

**`GameMode` has three values, `GameThread::gameMode` is the source of truth, and
`GobanModel::tsumegoMode` is the copy it publishes.**

- `GameThread::applyGameMode()` is the **one writer** — of the enum, of the
  published flag, and of `GameRecord::suppressSessionCopy`, which now follows the
  mode instead of being set beside it at three call sites, one of which missed.
  Same rule as `GobanModel::transitionTo()` and `LoopState`.
- `setGameMode()` holds the policy and nothing else does: `EXPLORE` is still
  refused for a human-versus-human game, **`TSUMEGO` is never refused** — a
  puzzle ignores player assignment entirely.
- The mode survives a load *because nothing between the request and
  `finalizeGameLoad()` writes it*. `applyLoadedGame()`'s reset to `MATCH` is
  gone; that reset was what made the ordering fragile.

**The menu shows a three-value select, and Tsumego is the value it only
reports.** A puzzle is entered by opening one and left by starting a new game, in
both directions and by every route:

- The `tsumego` **option** carries RmlUi's `disabled` attribute, so it cannot be
  picked — while `SetSelection()` ignores `disabled`, which is what lets the mode
  still be *displayed*.
- The **whole select** is greyed while a puzzle is open (`UiActions::gameMode`,
  the wrapper's `pointer-events: none`), so Match and Explore cannot be picked
  either.
- `game_mode` and `toggle_explore_mode` share one body and refuse the same
  cases with a message, so a script and a keybinding cannot reach what the
  control refuses (ADR-0005).

**Persistence is one key.** `user.json`'s `tsumego_mode` and `analysis_mode`
booleans become `game_mode`, migrated on read, tsumego winning if a stale file
carries both. `UserSettings` stores the enum, not its name.

## Consequences

- The bad cell is unrepresentable. `tsumego + EXPLORE` cannot be reached from
  the menu, from a script, from an SGF's result, or from a restored session.
- Leaving a puzzle *through the menu* is not offered, and that is the point
  rather than a gap: picking Match mid-puzzle would restore `promote=true` in
  `GobanControl::playVariationAt()`, so the solver's next attempt would promote
  itself over the recorded answer — the failure `tsumego_mode.scn` already pins.
  The cost accepted is a mode you can see and not change; `clear` is the exit,
  and it is one menu away.
- `gameMode` became `std::atomic<GameMode>`. It was a plain enum member read by
  the UI thread every frame and written by the game thread whenever a load was
  deferred past a genmove — a race that predates this change and was invisible.
- The enum's **declaration order is the option order** in five `goban.rml`
  files, because `OnUpdate()` casts the mode straight to a selection index. This
  is the same coupling `selectEvaluationMoves` already has, and nothing enforces
  it; it is noted in `GameMode.h`.
- The menu holding all this was renamed **Analyze → Review**, and its `Engine`
  group to `Analysis` — the playing engines are configured under Game, so the
  analysis engine is the extra one and naming its group after the role rather
  than the category is what disambiguates it. Two further UX questions raised at
  the same time are **deferred, not settled**: the Prisoners readout does not
  belong in a review menu, and the Analysis group would read better as
  On/Background/Off, or named after the engine, with a submenu when several are
  configured.

## Alternatives rejected

**Keep tsumego a flag, and forbid the bad pair with a guard.** A guard is a
fourth thing to keep in step with three writers, and the writers were the
problem. Making the state unrepresentable costs less than policing it — the same
argument ADR-0002 made for `GamePhase` against `started && isGameOver`.

**Make Tsumego a fully choosable mode.** Tempting, since `setGameMode(TSUMEGO)`
would work mechanically: it is the *record* that cannot follow. A puzzle's main
line is the answer that defines "correct" for every later verdict, and an
ordinary game has no such line — so entering from the menu would produce a
puzzle with no answer, and leaving would re-arm the promotion that overwrites
one. `load_tsumego` and the file chooser's toggle stay the only doors.

**Hide the Tsumego option unless a problem is open.** Nothing would be dimmed,
but the list changes length under the pointer, and the mode then has no
rendering at all in the state where it is actually in force. A greyed option
that can still be selected programmatically shows the truth in every state.

**`NLOHMANN_JSON_SERIALIZE_ENUM` for the settings round-trip.** The project
already uses nlohmann::json, so this is the obvious way to avoid a hand-written
`gameModeName()` / `parseGameMode()` pair. Its `from_json` falls back to the
*first* listed pair on an unknown string rather than reporting failure, so
`"game_mode": "analysis"` would silently mean Match with nothing logged —
against "Fail early". `std::optional` has to be able to say "that is not a
mode". (C++26 reflection, P2996, would generate the table from the enumerators;
no shipping compiler has it.)

**Leave the mode select where the toggle was, in a menu still called Analyze.**
The name described one of its four groups. Deferred is not the same as rejected
for the two UX questions above, but the rename is not deferred: the menu is the
one place a user goes to review, and it now says so.
