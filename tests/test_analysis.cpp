// The two pure halves of the live evaluation overlay (ADR-0007): parsing an
// engine's analysis report, and working out how to move an engine from one
// position to another. Both are free functions over plain data for the same
// reason availableActions() is — so they test without a process, a thread or a
// model.
//
// The streaming transport underneath them is covered in tests/test_gtpclient.cpp
// against the mock engine; the whole feature is covered end to end in
// tests/scenarios/evaluation_*.scn.

#include <doctest/doctest.h>

#include "AnalysisService.h"

namespace {

Move stone(int col, int row, Color::Value color) {
    return {Position(col, row), Color(color)};
}

// A kata-analyze report line, in the shape KataGo actually emits: several `info`
// blocks on one line, `pv` running to the end of its block, and extension keys
// this parser does not read sitting between the ones it does.
const char* kKataLine =
    "info move Q16 visits 100 utility 0.12 winrate 0.6000 scoreMean 2.5 "
    "scoreStdev 12.0 scoreLead 2.5 prior 0.3 lcb 0.59 order 0 pv Q16 D4 Q4 "
    "info move D4 visits 40 utility 0.10 winrate 0.5500 scoreMean 1.5 "
    "scoreLead 1.5 order 1 pv D4 Q16";

// The portable subset. No scoreLead at all, which is the case that must not
// become a score of zero.
const char* kLzLine =
    "info move Q16 visits 100 winrate 0.6000 order 0 pv Q16 D4 "
    "info move D4 visits 40 winrate 0.5500 order 1 pv D4 Q16";

}  // namespace

TEST_CASE("a kata-analyze line parses into moves and a score") {
    AnalysisReport report;
    REQUIRE(parseAnalysisLine(kKataLine, Color::BLACK, report));

    CHECK(report.moves.size() == 2);
    CHECK(report.winrateBlack == doctest::Approx(0.60));
    REQUIRE(report.scoreLeadBlack.has_value());
    CHECK(*report.scoreLeadBlack == doctest::Approx(2.5));
    // Total visits, not the best move's: this is the position's search effort.
    CHECK(report.visits == 140);

    // The vertex survives the round trip. Q16 on a 19x19 is column 15 (the
    // letter I is skipped), row 15.
    CHECK(report.moves.front().move == Position(15, 15));
    CHECK(report.moves.front().order == 0);
    CHECK(report.moves.front().visits == 100);
}

TEST_CASE("winrate and score are converted to Black's frame of reference") {
    // Engines report for the side to move. A display fed the raw number flips
    // every move and reads as noise — the same class of bug as the prisoner
    // counts that shipped swapped.
    AnalysisReport asBlack;
    REQUIRE(parseAnalysisLine(kKataLine, Color::BLACK, asBlack));
    AnalysisReport asWhite;
    REQUIRE(parseAnalysisLine(kKataLine, Color::WHITE, asWhite));

    CHECK(asBlack.winrateBlack == doctest::Approx(0.60));
    CHECK(asWhite.winrateBlack == doctest::Approx(0.40));

    REQUIRE(asBlack.scoreLeadBlack.has_value());
    REQUIRE(asWhite.scoreLeadBlack.has_value());
    CHECK(*asBlack.scoreLeadBlack == doctest::Approx(2.5));
    CHECK(*asWhite.scoreLeadBlack == doctest::Approx(-2.5));
}

TEST_CASE("an engine with no score reports no score, not a score of zero") {
    // Engine::final_score() returns std::optional<float> for exactly this
    // reason: 0.0 is a legitimate result. lz-analyze has no scoreLead at all,
    // and reading that as a drawn game is the mistake that took the scoring
    // chain down on 2026-08-13.
    AnalysisReport report;
    REQUIRE(parseAnalysisLine(kLzLine, Color::BLACK, report));

    CHECK(report.moves.size() == 2);
    CHECK(report.winrateBlack == doctest::Approx(0.60));
    CHECK_FALSE(report.scoreLeadBlack.has_value());
}

TEST_CASE("a pv is consumed whole, not read as further keys") {
    // The failure this pins: `pv Q16 D4 Q4` followed by the next `info` block.
    // A parser that took one token per key would read D4 and Q4 as keys, fall
    // out of step, and attribute the second block's visits to the first move.
    AnalysisReport report;
    REQUIRE(parseAnalysisLine(kKataLine, Color::BLACK, report));
    REQUIRE(report.moves.size() == 2);
    CHECK(report.moves[0].visits == 100);
    CHECK(report.moves[1].visits == 40);
    CHECK(report.moves[1].move == Position(3, 3));   // D4
}

TEST_CASE("ownership and pvVisits are consumed whole too") {
    // Both are lists. `ownership` is the one Stage 2 will ask for, and it runs
    // to the end of the line with several hundred numbers in it.
    const char* line =
        "info move Q16 visits 100 winrate 0.6000 order 0 pvVisits 100 60 20 "
        "pv Q16 D4 Q4 ownership 0.5 -0.5 0.25 -0.25";
    AnalysisReport report;
    REQUIRE(parseAnalysisLine(line, Color::BLACK, report));
    REQUIRE(report.moves.size() == 1);
    CHECK(report.moves[0].visits == 100);
    CHECK(report.moves[0].move == Position(15, 15));
}

TEST_CASE("a line that is not a report is rejected rather than half-read") {
    AnalysisReport report;
    CHECK_FALSE(parseAnalysisLine("", Color::BLACK, report));
    CHECK_FALSE(parseAnalysisLine("= MockGtp", Color::BLACK, report));
    CHECK_FALSE(parseAnalysisLine("info", Color::BLACK, report));
    // An `info` block whose move never parses yields nothing, rather than a
    // report about an INVALID vertex.
    CHECK_FALSE(parseAnalysisLine("info move ZZ99 visits 10 winrate 0.5 order 0",
                                  Color::BLACK, report));
    // A malformed number costs its own field, not the whole line.
    AnalysisReport partial;
    REQUIRE(parseAnalysisLine("info move Q16 visits nonsense winrate 0.7 order 0",
                              Color::BLACK, partial));
    CHECK(partial.winrateBlack == doctest::Approx(0.70));
    CHECK(partial.moves.front().visits == 0);
}

TEST_CASE("the best move is the position's value") {
    // `order 0` says which block that is, and it is not always the first.
    const char* line =
        "info move D4 visits 40 winrate 0.5500 order 1 pv D4 "
        "info move Q16 visits 100 winrate 0.6000 order 0 pv Q16";
    AnalysisReport report;
    REQUIRE(parseAnalysisLine(line, Color::BLACK, report));
    CHECK(report.winrateBlack == doctest::Approx(0.60));

    // Without any order key, the most-visited block wins.
    const char* unordered =
        "info move D4 visits 40 winrate 0.5500 pv D4 "
        "info move Q16 visits 100 winrate 0.6000 pv Q16";
    AnalysisReport fallback;
    REQUIRE(parseAnalysisLine(unordered, Color::BLACK, fallback));
    CHECK(fallback.winrateBlack == doctest::Approx(0.60));
}

TEST_CASE("an arrow key costs one command, not one per move played") {
    // This is the decision the whole separate process rests on: analysis
    // follows the review cursor, and it can only do that cheaply because the
    // common prefix is already on the engine's board.
    std::vector<Move> path;
    for (int i = 0; i < 5; ++i) {
        path.push_back(stone(i, i, i % 2 == 0 ? Color::BLACK : Color::WHITE));
    }

    SUBCASE("unchanged position sends nothing") {
        const auto plan = planIncrementalSync(path, path);
        CHECK(plan.undos == 0);
        CHECK(plan.playFrom == path.size());   // nothing left to play
    }

    SUBCASE("one move forward is one play") {
        std::vector<Move> shorter(path.begin(), path.end() - 1);
        const auto plan = planIncrementalSync(shorter, path);
        CHECK(plan.undos == 0);
        CHECK(plan.playFrom == 4);
    }

    SUBCASE("one move back is one undo") {
        std::vector<Move> shorter(path.begin(), path.end() - 1);
        const auto plan = planIncrementalSync(path, shorter);
        CHECK(plan.undos == 1);
        CHECK(plan.playFrom == shorter.size());
    }

    SUBCASE("stepping into a variation undoes only past the divergence") {
        std::vector<Move> variation(path.begin(), path.begin() + 3);
        variation.push_back(stone(9, 9, Color::WHITE));
        const auto plan = planIncrementalSync(path, variation);
        CHECK(plan.undos == 2);     // moves 4 and 5
        CHECK(plan.playFrom == 3);  // then the one new move
    }

    SUBCASE("a fresh engine replays everything") {
        const auto plan = planIncrementalSync({}, path);
        CHECK(plan.undos == 0);
        CHECK(plan.playFrom == 0);
    }

    SUBCASE("a different colour at the same point is a different move") {
        // Move identity has to include the colour, or a variation that plays
        // the same point for the other side would be taken for the same
        // position and nothing would be sent at all.
        std::vector<Move> recoloured = path;
        recoloured.back() = stone(4, 4, Color::WHITE);
        const auto plan = planIncrementalSync(path, recoloured);
        CHECK(plan.undos == 1);
        CHECK(plan.playFrom == 4);
    }
}

TEST_CASE("only playable moves are counted against the engine") {
    // `undo` is counted against what was actually sent. If an unsendable move
    // were skipped at the point of sending instead of filtered here, the two
    // would be out of step by one for the rest of the game — and the engine
    // would then be undoing somebody else's move.
    const std::vector<Move> path = {
        stone(3, 3, Color::BLACK),
        {Move::PASS, Color::WHITE},
        {Move::RESIGN, Color::BLACK},
        stone(4, 4, Color::WHITE),
        {Move::INVALID, Color::BLACK},
    };
    const auto playable = playableMoves(path);
    REQUIRE(playable.size() == 3);
    CHECK(playable[0] == Move::NORMAL);
    CHECK(playable[1] == Move::PASS);
    CHECK(playable[2] == Move::NORMAL);
}

// --- What the analysis thread is given ----------------------------------------

#include "GobanModel.h"

TEST_CASE("changing komi republishes the snapshot") {
    // Komi is not a position change, so nothing on the ordinary publish path
    // covers it — and the snapshot is where the analysis thread reads it from.
    // Without this, setting komi before the first move left the overlay scoring
    // the game against whatever komi was current at the last *board* change,
    // silently and by exactly the difference.
    GobanModel model(13);
    REQUIRE(model.snapshot()->komi != doctest::Approx(7.5f));

    model.onKomiChange(7.5f);
    CHECK(model.snapshot()->komi == doctest::Approx(7.5f));

    // And it is refused once play has begun, as it always has been — so the
    // published value cannot drift away from the record's either.
    model.start();
    model.onKomiChange(0.5f);
    CHECK(model.snapshot()->komi == doctest::Approx(7.5f));
}

// --- What the board draws -----------------------------------------------------

namespace {

/// A report of `n` suggestions, ranked, each losing `lossStep` more win rate
/// than the one before. Positions run along the first row.
AnalysisReport rankedReport(int n, double lossStep = 0.0,
                            Color::Value toMove = Color::BLACK) {
    AnalysisReport report;
    for (int i = 0; i < n; ++i) {
        AnalysisMove m;
        m.move = {Position(i, 0), Color(toMove)};
        m.order = i;
        m.visits = 100 - i;
        // In Black's frame throughout, as the parser leaves them. For White to
        // move the better move is the *lower* number for Black.
        m.winrateBlack = toMove == Color::BLACK ? 0.60 - lossStep * i
                                                : 0.40 + lossStep * i;
        report.moves.push_back(m);
    }
    return report;
}

std::pair<int, int> key(int col, int row) { return {col, row}; }

}  // namespace

TEST_CASE("the quality ramp runs from the best move to a blunder") {
    const glm::vec4 best = moveQualityColor(0.0);
    const glm::vec4 mid  = moveQualityColor(0.03);
    const glm::vec4 bad  = moveQualityColor(0.20);

    // Green through amber to red: red rises, green falls.
    CHECK(best.g > best.r);
    CHECK(bad.r > bad.g);
    CHECK(mid.r > best.r);
    CHECK(mid.g > bad.g);

    // Clamped at both ends, so a negative loss cannot escape the ramp and an
    // enormous one cannot run past red.
    CHECK(moveQualityColor(-1.0).g == doctest::Approx(best.g));
    CHECK(moveQualityColor(99.0).r == doctest::Approx(bad.r));
}

TEST_CASE("suggestions with no label of their own get rank letters") {
    const auto labels = evaluationLabels(rankedReport(3), {}, {}, DEFAULT_EVAL_LABELS);
    REQUIRE(labels.size() == 3);
    CHECK(labels[0].text == "A");
    CHECK(labels[1].text == "B");
    CHECK(labels[2].text == "C");
    CHECK(labels[0].pos == Position(0, 0));
}

TEST_CASE("a suggestion on a recorded variation is tinted, not relabelled") {
    // The engine's best move is very often a move already in the record. The
    // variation keeps its own "3a" and simply takes on the colour, so nothing
    // is lost and colour means quality everywhere on the board.
    const auto labels = evaluationLabels(rankedReport(3), {key(0, 0)}, {},
                                         DEFAULT_EVAL_LABELS);
    REQUIRE(labels.size() == 3);
    CHECK(labels[0].text.empty());        // tint only
    CHECK(labels[0].pos == Position(0, 0));

    // Letters do not skip: the first move that actually gets a label is 'A'.
    // B and C with no A on the board reads as broken.
    CHECK(labels[1].text == "A");
    CHECK(labels[2].text == "B");
}

TEST_CASE("explicit SGF markup is left entirely alone") {
    // The user's own annotation outranks both the engine and the variation
    // labels, as it already outranks variations today.
    const auto labels = evaluationLabels(rankedReport(3), {}, {key(1, 0)},
                                         DEFAULT_EVAL_LABELS);
    REQUIRE(labels.size() == 2);
    CHECK(labels[0].pos == Position(0, 0));
    CHECK(labels[1].pos == Position(2, 0));   // the marked point is skipped
    CHECK(labels[0].text == "A");
    CHECK(labels[1].text == "B");
}

TEST_CASE("the board shows at most maxLabels suggestions") {
    CHECK(evaluationLabels(rankedReport(9), {}, {}, 5).size() == 5);
    CHECK(evaluationLabels(rankedReport(9), {}, {}, 1).size() == 1);
    CHECK(evaluationLabels(rankedReport(9), {}, {}, 0).empty());
}

TEST_CASE("loss is never negative, whichever colour is to move") {
    // Both win rates are in Black's frame, so for White the better move is the
    // lower number. Subtracting the wrong way round would colour the engine's
    // own best move as a blunder on every White turn.
    for (const auto toMove : {Color::BLACK, Color::WHITE}) {
        const auto labels = evaluationLabels(rankedReport(3, 0.05, toMove), {}, {},
                                             DEFAULT_EVAL_LABELS);
        REQUIRE(labels.size() == 3);
        const glm::vec4 bestColor = moveQualityColor(0.0);
        CHECK(labels[0].color.g == doctest::Approx(bestColor.g));
        CHECK(labels[0].color.r == doctest::Approx(bestColor.r));
        // ...and the ones behind it get progressively worse, not better.
        CHECK(labels[1].color.r > labels[0].color.r);
        CHECK(labels[2].color.g < labels[1].color.g);
    }
}

TEST_CASE("moves the engine never searched are not drawn") {
    AnalysisReport report = rankedReport(3);
    report.moves[1].visits = 0;
    const auto labels = evaluationLabels(report, {}, {}, DEFAULT_EVAL_LABELS);
    REQUIRE(labels.size() == 2);
    CHECK(labels[0].pos == Position(0, 0));
    CHECK(labels[1].pos == Position(2, 0));
}
