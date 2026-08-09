#include "UiActions.h"

UiActions availableActions(const UiInputs& in) {
    UiActions a;

    // Nothing is offered before the engines are up, or while a programmatic
    // widget sync is in flight — a change event fired by repopulating a
    // dropdown must not read as the user choosing something.
    if (!in.uiReady) return a;

    const bool finished = in.phase == GamePhase::Finished;
    const bool playing  = in.phase == GamePhase::Playing;
    // isThinking() is true only for engines, so this is "a human may act now".
    const bool humanTurn = in.humanToMove && !in.engineThinking;

    // Start hands the turn to an engine, so it needs one to be to move — and
    // there is nothing to start if the game is already running or over.
    a.start = !finished && !playing && in.engineToMove;

    a.pass   = humanTurn && !finished && !in.aiVsAiLocked;
    a.kibitz = !in.engineThinking && !finished && !in.aiVsAiLocked;

    // Resigning writes the result onto the record's root and adds no node, so
    // unlike a stone or a pass it cannot describe a branch: applied anywhere
    // but the end of the line, it would label a game whose recorded moves then
    // contradict it. A finished game is already decided, and a tsumego is a
    // puzzle rather than a game to concede.
    //
    // The aiVsAiLocked term settles a disagreement this step exposed: the
    // toolbar greyed Resign in a locked bot-bot match, while the `resign`
    // command never tested it, so the keybinding still resigned. Taking the
    // stricter side — the human is a spectator there, exactly as for pass and
    // undo. Reachable only via the explicit AI-vs-AI toggle, since two engines
    // playing means humanToMove() is false anyway.
    a.resign = humanTurn && !finished && !in.tsumego && in.atEndOfNavigation
               && !in.aiVsAiLocked;

    // Undo is navigateBack under another name, so it follows the navigation
    // buttons rather than whose turn it is. Reviewing a finished game is
    // precisely when it is wanted.
    a.navigate = !in.engineThinking && !in.aiVsAiLocked;
    a.undo     = a.navigate;

    a.territory = finished;
    a.clear     = in.hasMoves;
    a.save      = in.hasUnsavedChanges;

    return a;
}
