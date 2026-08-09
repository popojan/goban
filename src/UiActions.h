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
    bool humanToMove       = false;
    bool engineToMove      = false;
    /// A bot-versus-bot match outside analysis mode: the human is a spectator.
    bool aiVsAiLocked      = false;
    bool tsumego           = false;
    /// The cursor is at the end of the line being played, with no continuation
    /// ahead of it. Resignation writes the record's result and adds no node, so
    /// it is only truthful here.
    bool atEndOfNavigation = true;
    bool hasMoves          = false;
    bool hasUnsavedChanges = false;
};

/// Phrased as "enabled when", deliberately: the call sites and the tests then
/// read positively, and the negation happens once, at `setElementDisabled()`.
struct UiActions {
    bool start     = false;  ///< Hand the turn to an engine.
    bool pass      = false;
    bool resign    = false;
    bool undo      = false;  ///< navigateBack under another name.
    bool kibitz    = false;  ///< Ask the kibitz engine for one move.
    bool navigate  = false;  ///< All four navigation buttons share one answer.
    bool territory = false;
    bool clear     = false;
    bool save      = false;
};

UiActions availableActions(const UiInputs& in);

#endif // GOBAN_UIACTIONS_H
