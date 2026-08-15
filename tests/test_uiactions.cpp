// Tests for availableActions() — ADR-0002 step 5.
//
// This is the policy behind Pass, Resign, Undo and the rest: which actions the
// toolbar offers, and which the command layer accepts. It used to be inline in
// ElementGame::OnUpdate(), untested, and restated in prose by each command that
// guards the same action. Both UI bugs found by hand on 2026-08-09 lived there,
// and both have a named case below:
//
//   * "reviewing a finished game" — Start / Pass / Undo were greyed out on a
//     position that is no longer the end of the game.
//   * "resign agrees with the resign command" — the toolbar refused what the
//     keybinding performed.
//
// The function is pure over plain data on purpose (see UiActions.h): a policy
// phrased over GameThread could not be tested with an engine thinking, since
// only the game loop sets that. Keep it that way — these tests need no fixture,
// no engine and no graphics context, which is why they can be exhaustive.
#include <doctest/doctest.h>

#include <initializer_list>

#include "UiActions.h"

namespace {

/// A live game, human to move, everything ready. Each test perturbs one or two
/// fields so the case reads as its own sentence.
UiInputs playing() {
    UiInputs in;
    in.phase             = GamePhase::Playing;
    in.uiReady           = true;
    in.humanToMove       = true;
    in.atEndOfNavigation = true;
    in.hasMoves          = true;
    return in;
}

}  // namespace

TEST_CASE("nothing is offered until the UI is ready") {
    UiInputs in = playing();
    in.uiReady = false;

    const UiActions a = availableActions(in);
    CHECK_FALSE(a.start);
    CHECK_FALSE(a.pass);
    CHECK_FALSE(a.resign);
    CHECK_FALSE(a.undo);
    CHECK_FALSE(a.kibitz);
    CHECK_FALSE(a.navigate);
    CHECK_FALSE(a.territory);
    CHECK_FALSE(a.clear);
    CHECK_FALSE(a.save);
}

TEST_CASE("mid-game with a human to move, the play actions are live") {
    const UiActions a = availableActions(playing());
    CHECK(a.pass);
    CHECK(a.resign);
    CHECK(a.undo);
    CHECK(a.navigate);
    CHECK(a.kibitz);
    CHECK(a.clear);
    CHECK_FALSE(a.start);        // already playing
    CHECK_FALSE(a.territory);    // not over
}

TEST_CASE("reviewing a finished game leaves the board usable") {
    // The bug: rewinding a finished game greyed out Start, Pass, Undo and
    // Kibitz, because the toolbar tested state.reason — which enterReview()
    // leaves set — instead of the phase, which is back to Paused. The
    // keybindings for those same actions kept working the whole time.
    UiInputs in;
    in.uiReady           = true;
    in.phase             = GamePhase::Paused;
    in.humanToMove       = true;
    in.engineToMove      = false;
    in.atEndOfNavigation = false;   // a continuation lies ahead of the cursor
    in.hasMoves          = true;

    const UiActions a = availableActions(in);
    CHECK(a.undo);
    CHECK(a.navigate);
    CHECK(a.pass);
    CHECK(a.kibitz);
    // Resign is the exception, and for its own reason: it writes the record's
    // result and adds no node, so mid-tree it would contradict the moves that
    // follow.
    CHECK_FALSE(a.resign);
    CHECK_FALSE(a.territory);
}

TEST_CASE("Start is offered only when an engine is waiting to move") {
    UiInputs in;
    in.uiReady = true;
    in.phase   = GamePhase::Paused;
    in.hasMoves = true;

    in.engineToMove = false;
    CHECK_FALSE(availableActions(in).start);

    in.engineToMove = true;
    CHECK(availableActions(in).start);

    // Not while already playing, and not on a decided game.
    in.phase = GamePhase::Playing;
    CHECK_FALSE(availableActions(in).start);
    in.phase = GamePhase::Finished;
    CHECK_FALSE(availableActions(in).start);
}

TEST_CASE("a finished game offers territory and nothing to play") {
    UiInputs in = playing();
    in.phase = GamePhase::Finished;
    in.scoredEnd = true;   // decided on points, so there is territory to show

    const UiActions a = availableActions(in);
    CHECK(a.territory);
    CHECK_FALSE(a.pass);
    CHECK_FALSE(a.resign);
    CHECK_FALSE(a.kibitz);
    CHECK_FALSE(a.start);
    // Undo stays live: rewinding is how you leave a finished game.
    CHECK(a.undo);
    CHECK(a.navigate);
}

TEST_CASE("a resigned game has no territory to show") {
    // Found by the 2026-08-13 audit. `territory` tested the phase alone, which
    // a resignation satisfies just as a scored ending does — so the button lit
    // up on a resigned game and toggle_territory, guarded by its own
    // shouldShowTerritory() test, refused. An enabled button that does nothing.
    UiInputs in = playing();
    in.phase     = GamePhase::Finished;
    in.scoredEnd = false;    // +R: nothing was ever counted

    CHECK_FALSE(availableActions(in).territory);

    // And the stale overlay is refused once the user has played on past a
    // recorded score: the position is at the end, but the game is live again.
    UiInputs resumed = playing();
    resumed.scoredEnd = true;
    CHECK_FALSE(availableActions(resumed).territory);
}

TEST_CASE("passing mid-tree describes a variation, not a turn") {
    // Away from the end of the line a pass creates a pass variation, exactly as
    // a board click creates a stone variation — and GobanControl::boardClick
    // has never consulted turn ownership there. Phrasing `pass` as humanToMove
    // alone would have refused a pass onto an engine's colour while a click on
    // the same node went through, which is the inconsistency this removes.
    UiInputs in = playing();
    in.phase             = GamePhase::Paused;
    in.atEndOfNavigation = false;
    in.humanToMove       = false;   // an engine is nominally to move here

    CHECK(availableActions(in).pass);

    // At the end of the line it is a turn again, so ownership decides.
    UiInputs atEnd = in;
    atEnd.atEndOfNavigation = true;
    CHECK_FALSE(availableActions(atEnd).pass);

    // Reviewing overrides turn ownership and nothing else. In particular it does
    // not override the engine-thinking lock: a pass is a preserving action in
    // ADR-0001's sense, and boardClick's review branch refuses there too, so
    // allowing it would put pass and click back at odds in exactly the case
    // that corrupts engine state.
    UiInputs thinking = in;  thinking.engineThinking = true;
    UiInputs locked   = in;  locked.aiVsAiLocked = true;
    UiInputs over     = in;  over.phase = GamePhase::Finished;
    CHECK_FALSE(availableActions(thinking).pass);
    CHECK_FALSE(availableActions(locked).pass);
    CHECK_FALSE(availableActions(over).pass);
}

TEST_CASE("an engine mid-genmove locks navigation and kibitz") {
    UiInputs in = playing();
    in.engineThinking = true;

    const UiActions a = availableActions(in);
    CHECK_FALSE(a.navigate);
    CHECK_FALSE(a.undo);
    CHECK_FALSE(a.kibitz);
    // Nor may a human act: GTP traffic while the engine thinks corrupts it.
    CHECK_FALSE(a.pass);
    CHECK_FALSE(a.resign);
}

TEST_CASE("a bot-bot match outside analysis mode locks the human out") {
    UiInputs in = playing();
    in.aiVsAiLocked = true;

    const UiActions a = availableActions(in);
    CHECK_FALSE(a.pass);
    CHECK_FALSE(a.resign);
    CHECK_FALSE(a.undo);
    CHECK_FALSE(a.navigate);
    CHECK_FALSE(a.kibitz);
    // Saving and clearing are not play actions and stay available.
    CHECK(a.clear);
}

TEST_CASE("a tsumego is a puzzle, not a game to concede") {
    UiInputs in = playing();
    in.tsumego = true;

    const UiActions a = availableActions(in);
    CHECK_FALSE(a.resign);
    CHECK(a.pass);        // passing a problem is merely wrong, not corrupting
}

TEST_CASE("resign agrees with the conditions the resign command applies") {
    // The drift this step exists to prevent: GobanControl::canResign() now
    // reads this same field rather than restating the rules. Pin all four
    // conditions so the two cannot come apart again.
    UiInputs in = playing();
    CHECK(availableActions(in).resign);

    UiInputs notReady = in;      notReady.uiReady = false;
    UiInputs midTree  = in;      midTree.atEndOfNavigation = false;
    UiInputs over     = in;      over.phase = GamePhase::Finished;
    UiInputs puzzle   = in;      puzzle.tsumego = true;
    UiInputs notHuman = in;      notHuman.humanToMove = false;
    UiInputs thinking = in;      thinking.engineThinking = true;
    UiInputs locked   = in;      locked.aiVsAiLocked = true;

    CHECK_FALSE(availableActions(notReady).resign);
    CHECK_FALSE(availableActions(midTree).resign);
    CHECK_FALSE(availableActions(over).resign);
    CHECK_FALSE(availableActions(puzzle).resign);
    CHECK_FALSE(availableActions(notHuman).resign);
    CHECK_FALSE(availableActions(thinking).resign);
    // The one condition the button had and the command did not, until this
    // step made them the same expression.
    CHECK_FALSE(availableActions(locked).resign);
}

TEST_CASE("clear and save follow the record, not the turn") {
    UiInputs in = playing();

    in.hasMoves = false;
    CHECK_FALSE(availableActions(in).clear);
    in.hasMoves = true;
    CHECK(availableActions(in).clear);

    in.hasUnsavedChanges = false;
    CHECK_FALSE(availableActions(in).save);
    in.hasUnsavedChanges = true;
    CHECK(availableActions(in).save);
}

TEST_CASE("the evaluation overlay asks only whether an engine can answer") {
    // Deliberately free of every other term. It is a display, not a move: it
    // follows the review cursor rather than the turn, it is worth most on a
    // finished game being reviewed, and it never touches a playing engine's
    // pipe (ADR-0007 decisions 4 and 7). So none of the conditions that gate
    // Pass, Resign or Undo apply to it.
    UiInputs in = playing();
    in.evaluationAvailable = true;
    CHECK(availableActions(in).evaluation);

    UiInputs thinking = in;  thinking.engineThinking = true;
    UiInputs locked   = in;  locked.aiVsAiLocked = true;
    UiInputs over     = in;  over.phase = GamePhase::Finished;
    UiInputs midTree  = in;  midTree.atEndOfNavigation = false;
    UiInputs notHuman = in;  notHuman.humanToMove = false;

    CHECK(availableActions(thinking).evaluation);
    CHECK(availableActions(locked).evaluation);
    CHECK(availableActions(over).evaluation);
    CHECK(availableActions(midTree).evaluation);
    CHECK(availableActions(notHuman).evaluation);

    // Three things do refuse it. No engine nominated itself — the stock
    // configuration, where nothing carries "kibitz" and the only engine is a
    // GNU Go that cannot analyse.
    UiInputs none = in;      none.evaluationAvailable = false;
    CHECK_FALSE(availableActions(none).evaluation);

    // A tsumego: an overlay that stars the correct move solves the puzzle.
    UiInputs puzzle = in;    puzzle.tsumego = true;
    CHECK_FALSE(availableActions(puzzle).evaluation);

    // And startup, like everything else.
    UiInputs notReady = in;  notReady.uiReady = false;
    CHECK_FALSE(availableActions(notReady).evaluation);
}

TEST_CASE("kibitz stays available while reviewing mid-tree") {
    // Deliberately *not* gated on atEndOfNavigation, unlike resign. Away from
    // the end a move is a variation rather than a turn, which `pass` and board
    // clicks already honour — kibitz is the third move-producing action and
    // must agree with them. docs/game-modes.md's "undo and try a different move
    // in a match" is this workflow, and it notes the engine will not *auto*
    // respond at a historical position: asking once is the tool that leaves.
    //
    // Refusing here would be the tempting fix for the bug where kibitz mid-tree
    // silently did nothing. It was the wrong one — the fault was in how the
    // request was delivered, not in whether it was allowed.
    UiInputs in = playing();
    in.atEndOfNavigation = false;
    CHECK(availableActions(in).kibitz);

    // The terms it does have, unchanged.
    UiInputs thinking = in;  thinking.engineThinking = true;
    UiInputs over     = in;  over.phase = GamePhase::Finished;
    UiInputs locked   = in;  locked.aiVsAiLocked = true;
    CHECK_FALSE(availableActions(thinking).kibitz);
    CHECK_FALSE(availableActions(over).kibitz);
    CHECK_FALSE(availableActions(locked).kibitz);
}

TEST_CASE("a resync locks the same things a genmove does") {
    // The gap that let four clicks produce one stone. A board size change, a
    // clear or an SGF load sends the game thread off to replay the record into
    // every engine, and on a CPU KataGo that is seconds — with the whole UI
    // live, the board drawn and the overlay running. No engine is *thinking*
    // there, so every guard phrased over engineThinking alone was open, and a
    // click went into queuedMove: a single slot, overwritten by the next one.
    UiInputs in = playing();
    in.enginesSyncing = true;

    CHECK_FALSE(availableActions(in).play);
    CHECK_FALSE(availableActions(in).pass);
    CHECK_FALSE(availableActions(in).kibitz);
    CHECK_FALSE(availableActions(in).resign);
    CHECK_FALSE(availableActions(in).navigate);
    CHECK_FALSE(availableActions(in).undo);

    // A display is not a move: the evaluation overlay never touches the game
    // thread, so it is as available mid-resync as it is mid-genmove.
    UiInputs evalReady = in;  evalReady.evaluationAvailable = true;
    CHECK(availableActions(evalReady).evaluation);

    // And it lifts. This is what distinguishes it from a stuck flag.
    in.enginesSyncing = false;
    CHECK(availableActions(in).play);
}

TEST_CASE("a click and a pass are the same question") {
    // CLAUDE.md requires them to agree — a move at the cursor is a move at the
    // cursor. Defined rather than restated, so this holds by construction; the
    // case is here to fail loudly if somebody splits them again.
    for (bool midTree : {false, true}) {
        for (bool syncing : {false, true}) {
            for (bool thinking : {false, true}) {
                for (bool human : {false, true}) {
                    UiInputs in = playing();
                    in.atEndOfNavigation = !midTree;
                    in.enginesSyncing    = syncing;
                    in.engineThinking    = thinking;
                    in.humanToMove       = human;
                    const UiActions a = availableActions(in);
                    CHECK(a.play == a.pass);
                }
            }
        }
    }

    // Including on a finished game, where both are false — a click there means
    // "clear", which boardClick() resolves before it asks.
    UiInputs over = playing();
    over.phase = GamePhase::Finished;
    CHECK_FALSE(availableActions(over).play);
    CHECK_FALSE(availableActions(over).pass);
}
