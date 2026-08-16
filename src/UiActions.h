/** \file
 *  \brief The one policy deciding what a player may do (ADR-0005).
 *
 * A pure function over plain data, so it unit-tests without a model, a thread or
 * a graphics context — and so both the toolbar and every command guard can read
 * the same answer. Read the struct comment below before adding a rule anywhere
 * else; three times a second condition was hand-rolled at a call site, and three
 * times it drifted.
 */
#ifndef GOBAN_UIACTIONS_H
#define GOBAN_UIACTIONS_H

#include "GamePhase.h"

/** \brief Which player actions are available right now, and why.
 *
 * ADR-0002 step 5. This is the policy that decides whether Pass, Resign, Undo
 * and friends are offered. It used to live inline in
 * `ElementGame::OnUpdate()`, phrased as "disabled when …", untested, and
 * duplicated in prose by the command handlers that guard the same actions.
 * Both UI bugs found by hand on 2026-08-09 lived there:
 *
 *  * Start / Pass / Undo / Kibitz greyed out while reviewing a finished game,
 *    because the block tested `state.reason` instead of the phase.
 *  * `cmdResign` disagreeing with the `resign` command, so the toolbar refused
 *    what the keybinding happily performed — a disabled button that looked like
 *    a guard and was not one.
 *
 * The fix for both was a rule ("make the button ask the same question the
 * command asks"), and rules like that decay. Now there is one answer:
 * `GobanControl::actions()` gathers the inputs once, and both the toolbar and
 * the command layer read the result.
 *
 * That was originally true of `resign` alone, and the other eight commands went
 * on guarding themselves — differently. A 2026-08-13 audit found six live
 * disagreements between a button and the command it invokes, including a
 * Territory button that was enabled on a resigned game and did nothing, and
 * navigation keys that were accepted in a locked bot-versus-bot match the
 * toolbar had greyed out. Every command in the registry now dispatches through
 * `GobanControl::actions()`; see ADR-0005.
 *
 * **Keep this a pure function of plain data.** It must not take a `GobanModel`
 * or a `GameThread`: `GameThread::isThinking()` reads a member only the game
 * loop sets, so a policy phrased over those types could not be tested in the
 * engine-thinking cases at all — and those are half of what it decides.
 * `tests/test_uiactions.cpp` depends on this staying true.
 */
struct UiInputs {
    GamePhase phase        = GamePhase::Setup;
    /// Engines have finished loading and no programmatic widget sync is running.
    bool uiReady           = false;
    bool engineThinking    = false;
    /// The game thread is replaying the record into the engines — after a board
    /// size change, a clear, a handicap or an SGF load. Distinct from
    /// `engineThinking`, which is a genmove in flight: no engine is thinking
    /// during a resync, so every guard phrased over that term alone was open.
    /// It has to be here because the resync is *slow* — KataGo rebuilding for a
    /// new board size takes seconds on a CPU backend, and the UI is fully live
    /// throughout: board drawn, overlay running, toolbar lit.
    bool enginesSyncing    = false;
    bool humanToMove       = false;
    bool engineToMove      = false;
    /// A bot-versus-bot match outside analysis mode: the human is a spectator.
    bool aiVsAiLocked      = false;
    bool tsumego           = false;
    /// Some node between the root and the cursor is marked BM — in a tsumego,
    /// the solver is inside a branch that has already gone wrong. Only `kibitz`
    /// reads it, and only in a puzzle: it is what separates "show me how this
    /// is punished", which is the whole point of being allowed to play a wrong
    /// move out, from asking for a move at a position that is already solved.
    bool onBadMovePath     = false;
    /// The cursor is at the end of the line being played, with no continuation
    /// ahead of it. Resignation writes the record's result and adds no node, so
    /// it is only truthful here. Its negation is "reviewing mid-tree", where a
    /// move describes a variation rather than a turn — see `pass` below.
    bool atEndOfNavigation = true;
    /// The position is the end of a game decided on points. False for a
    /// resignation, which records a result without ever scoring the board, so
    /// there is no territory to show. Mirrors GameRecord::shouldShowTerritory().
    bool scoredEnd         = false;
    bool hasMoves          = false;
    bool hasUnsavedChanges = false;
    /// An engine nominated itself for the analysis role and has not been found
    /// incapable. False means the evaluation overlay is not offered at all —
    /// which is the stock configuration, where nothing carries "kibitz" and the
    /// only engine is a GNU Go that cannot analyse (ADR-0007 decision 3).
    bool evaluationAvailable = false;
};

/// Phrased as "enabled when", deliberately: the call sites and the tests then
/// read positively, and the negation happens once, at `setElementDisabled()`.
struct UiActions {
    bool start     = false;  ///< Hand the turn to an engine.
    bool pass      = false;
    bool resign    = false;
    bool undo      = false;  ///< navigateBack under another name.
    /// Place a stone at the cursor. The same question `pass` asks — both are a
    /// move at the cursor, and CLAUDE.md requires them to agree — so it is
    /// defined as `pass` rather than restated.
    bool play      = false;
    bool kibitz    = false;  ///< Ask the kibitz engine for one move.
    bool navigate  = false;  ///< All four navigation buttons share one answer.
    bool territory = false;
    bool clear     = false;
    bool save      = false;
    bool evaluation = false;  ///< Toggle the live evaluation overlay.
};

UiActions availableActions(const UiInputs& in);

#endif // GOBAN_UIACTIONS_H
