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

    // Away from the end of the line the cursor is annotating a record, not
    // taking a turn, so whose colour it is does not enter into it: a pass there
    // creates a pass variation exactly as a board click creates a stone
    // variation. Board clicks have never consulted turn ownership mid-tree
    // (GobanControl::boardClick takes its review branch first), and phrasing
    // pass any other way would leave the two disagreeing about the same act.
    //
    // The engineThinking term stays outside the disjunction. Passing is a
    // *preserving* action in ADR-0001's sense, so it is refused while an engine
    // is mid-genmove — and boardClick's review branch already blocks there, so
    // letting the review term through would put pass and click back at odds in
    // the one case that actually corrupts engine state.
    const bool reviewingMidTree = !in.atEndOfNavigation;
    a.pass   = !in.engineThinking && (in.humanToMove || reviewingMidTree)
               && !finished && !in.aiVsAiLocked;
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

    // Territory is what a scored ending produced, so a resignation has none to
    // show — nothing was ever counted. The button used to test the phase alone,
    // which is true of a resignation too, so it lit up and did nothing. Both
    // terms are kept: `scoredEnd` follows the position, and `finished` refuses
    // the stale overlay after a user has pressed Start and played on past a
    // recorded score.
    a.territory = finished && in.scoredEnd;
    a.clear     = in.hasMoves;
    a.save      = in.hasUnsavedChanges;

    return a;
}
