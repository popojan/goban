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
