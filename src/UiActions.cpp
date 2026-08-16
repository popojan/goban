#include "UiActions.h"

UiActions availableActions(const UiInputs& in) {
    UiActions a;

    // Nothing is offered before the engines are up, or while a programmatic
    // widget sync is in flight — a change event fired by repopulating a
    // dropdown must not read as the user choosing something.
    if (!in.uiReady) return a;

    const bool finished = in.phase == GamePhase::Finished;
    const bool playing  = in.phase == GamePhase::Playing;

    // "An engine has the game thread", which is the question every guard below
    // meant to ask and only half of which it was asking. A genmove in flight is
    // one way; the initial sync after a board size change, a clear or an SGF
    // load is the other, and it is the slower of the two — seconds of KataGo
    // rebuilding, with the whole UI live and nothing thinking.
    //
    // Nothing reached the loop during that window: playLocalMove() found
    // playerToMove null, fell through to `queuedMove`, and each further click
    // overwrote the single slot. Four clicks produced one stone, which is what
    // it took to notice.
    const bool engineBusy = in.engineThinking || in.enginesSyncing;
    // isThinking() is true only for engines, so this is "a human may act now".
    const bool humanTurn = in.humanToMove && !engineBusy;

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
    a.pass   = !engineBusy && (in.humanToMove || reviewingMidTree)
               && !finished && !in.aiVsAiLocked;
    // Defined as pass, not restated as an equal-looking expression: they are one
    // act at one point, and the two drifting apart is the failure this whole
    // file exists to prevent. A click on a *finished* game is not a play — it
    // means "clear" — and boardClick() handles that before asking.
    a.play   = a.pass;
    // A solved puzzle has nothing left to ask. The board click is already
    // refused there — "Solved — stay blocked" — and the button that says
    // *Hint* in Czech was not, so it played the engine's move on past the
    // answer and started the game while it was at it. Anywhere else in a
    // tsumego it stands: at the root it is a hint, and inside a branch that has
    // gone wrong it is "show me how this is punished", which is the reason a
    // wrong move may be played out at all.
    const bool solvedEnd = in.tsumego && in.atEndOfNavigation && !in.onBadMovePath;
    a.kibitz = !engineBusy && !finished && !in.aiVsAiLocked && !solvedEnd;

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
    a.navigate = !engineBusy && !in.aiVsAiLocked;
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

    // Deliberately free of every term above. The evaluation overlay is a
    // display, not a move: it follows the review cursor, it is worth most on a
    // finished game being reviewed, and it never touches a playing engine's
    // pipe. The only question is whether there is an engine that can answer.
    // Tsumego is the one exception — an overlay that stars the correct move
    // solves the puzzle for the reader.
    a.evaluation = in.evaluationAvailable && !in.tsumego;

    return a;
}
