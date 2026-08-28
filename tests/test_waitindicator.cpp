// The two pure halves of the board's wait indicator (see src/WaitIndicator.h):
// the blink curve, and the glyph atlas the overlay is warmed with.
//
// Both are free functions over plain data for the same reason availableActions()
// is — so they test without a GL context, a font, or a thread. The placement and
// the wiring are covered end to end in tests/scenarios/wait_indicator.scn.

#include <doctest/doctest.h>

#include "WaitIndicator.h"

#include <set>

TEST_CASE("a wait kind names itself for dumpState") {
    CHECK(std::string(waitKindName(WaitKind::None)) == "none");
    CHECK(std::string(waitKindName(WaitKind::Thinking)) == "thinking");
    CHECK(std::string(waitKindName(WaitKind::Syncing)) == "syncing");
}

TEST_CASE("nothing is drawn until the wait is worth mentioning") {
    // GNU Go answers a genmove in 13 ms. Without the grace period every move of
    // a bot-versus-bot match puts the mark on screen for a single frame, which
    // is exactly the "something is broken" reading the indicator exists to
    // prevent.
    CHECK(Wait::displayedSecond(0.0f, 0.5f) == Wait::NOT_SHOWN);
    CHECK(Wait::displayedSecond(0.49f, 0.5f) == Wait::NOT_SHOWN);
    CHECK(Wait::displayedSecond(0.5f, 0.5f) == 0);
    // The clock runs from the true start, not from the end of the grace period,
    // so the first count shown is honest rather than half a second short.
    CHECK(Wait::displayedSecond(1.2f, 0.5f) == 1);
    CHECK(Wait::displayedSecond(30.9f, 0.5f) == 30);
}

TEST_CASE("a zero grace shows the mark at once") {
    // What a scenario configures, so that it can assert the mark was placed
    // without having to wait out a delay that exists for real engines.
    CHECK(Wait::displayedSecond(0.0f, 0.0f) == 0);
    CHECK(Wait::displayedSecond(0.4f, 0.0f) == 0);
    CHECK(Wait::displayedSecond(1.0f, 0.0f) == 1);
}

TEST_CASE("the count is the repaint gate, so it must be stable within a second") {
    // A frame is drawn when this value changes. If it were not constant across
    // a second, a wait would cost twenty rebuilds of every glyph buffer per
    // second to draw twenty identical frames.
    for (int i = 0; i < 20; ++i) {
        const float t = 3.0f + static_cast<float>(i) * 0.05f;
        CHECK(Wait::displayedSecond(t, 0.5f) == 3);
    }
    CHECK(Wait::displayedSecond(4.0f, 0.5f) == 4);
}

TEST_CASE("the mark blinks hard, never partially") {
    // The rule the first attempt broke. markVisible() returns a bool and there
    // is no alpha anywhere in this file: a board annotation is fully carved or
    // absent. Half the period on, half off.
    const float p = 1.0f;
    CHECK(Wait::markVisible(0.0f, 0.0f, p) == true);
    CHECK(Wait::markVisible(0.49f, 0.0f, p) == true);
    CHECK(Wait::markVisible(0.51f, 0.0f, p) == false);
    CHECK(Wait::markVisible(0.99f, 0.0f, p) == false);
    // ...and back on as the next second turns over, so the mark and the count
    // keep step rather than drifting against each other.
    CHECK(Wait::markVisible(1.0f, 0.0f, p) == true);
    CHECK(Wait::markVisible(1.4f, 0.0f, p) == true);
    CHECK(Wait::markVisible(1.6f, 0.0f, p) == false);
}

TEST_CASE("the mark is absent for the whole grace period") {
    // Nothing at all is drawn before the wait is worth mentioning — not a faint
    // mark, not a placeholder.
    CHECK(Wait::markVisible(0.0f, 0.5f, 1.0f) == false);
    CHECK(Wait::markVisible(0.49f, 0.5f, 1.0f) == false);
    // Phase is measured from the moment it appears, so it comes up printed
    // rather than arriving mid-cycle and blinking straight out again.
    CHECK(Wait::markVisible(0.5f, 0.5f, 1.0f) == true);
}

TEST_CASE("a degenerate blink period does not divide by zero or wedge on") {
    // annotations.wait_blink_period is a user knob; GobanView warns and keeps
    // its own value, but the pure function must not depend on that.
    for (float bad : {0.0f, -1.0f, 0.001f}) {
        // Whatever it clamps to, it must still be a blink: on somewhere,
        // off somewhere, and never NaN.
        bool sawOn = false, sawOff = false;
        for (int i = 0; i < 200; ++i) {
            const float t = static_cast<float>(i) * 0.01f;
            (Wait::markVisible(t, 0.0f, bad) ? sawOn : sawOff) = true;
        }
        CHECK(sawOn);
        CHECK(sawOff);
    }
}

TEST_CASE("a degenerate grace does not produce a nonsense count") {
    // annotations.wait_grace is a user knob; GobanView refuses a negative one,
    // but the pure function must not depend on that having happened.
    CHECK(Wait::displayedSecond(1.0f, -1.0f) == 1);
    CHECK(Wait::displayedSecond(0.0f, -1.0f) == 0);
}

TEST_CASE("the atlas contains every character the ordinary overlays draw") {
    const std::string atlas = Wait::atlasWith({});
    // Move numbers and the coordinate margin.
    for (char c : std::string("0123456789ABCDEFGHJKLMNOPQRST")) {
        CHECK(atlas.find(c) != std::string::npos);
    }
    // The evaluation readout ("B 62% B+4.5") and this indicator's count ("12s").
    for (char c : std::string("BW+-.%s")) {
        CHECK(atlas.find(c) != std::string::npos);
    }
}

TEST_CASE("a configured glyph is folded into the atlas") {
    // The point of the whole change: the atlas used to be a string literal, so a
    // character named in config/base.json could never reach it and simply did
    // not appear, with nothing said anywhere.
    const std::string atlas = Wait::atlasWith({"\xE2\x97\x8F"});   // U+25CF ●
    CHECK(atlas.find("\xE2\x97\x8F") != std::string::npos);
    CHECK(atlas.find(Wait::BASE_ATLAS) == 0);   // and the base survives, in place
}

TEST_CASE("folding in a glyph the atlas already has does not duplicate it") {
    const std::string base = Wait::atlasWith({});
    // "O" is the shipped default precisely because it is already there — the
    // shipped overlay font is Roboto, 98 glyphs, ASCII only.
    CHECK(Wait::atlasWith({"O"}) == base);
    CHECK(Wait::atlasWith({"O", "O", "12s"}) == base);
}

TEST_CASE("several configured strings are all folded in, in order") {
    const std::string atlas = Wait::atlasWith({"\xE2\x97\x8F", "\xE2\x97\x8B"});
    CHECK(atlas.find("\xE2\x97\x8F") != std::string::npos);   // ●
    CHECK(atlas.find("\xE2\x97\x8B") != std::string::npos);   // ○
    CHECK(atlas.find("\xE2\x97\x8F") < atlas.find("\xE2\x97\x8B"));
}

TEST_CASE("utf8 characters split the way the glyph buffer decodes them") {
    // A second decoding rule here would let the atlas and the draw disagree
    // about what a byte sequence means, and the symptom would be a glyph that
    // is in the atlas by every test we could write and still does not appear.
    const auto chars = Wait::utf8Chars("a\xE2\x97\x8F""b");
    REQUIRE(chars.size() == 3);
    CHECK(chars[0] == "a");
    CHECK(chars[1] == "\xE2\x97\x8F");
    CHECK(chars[2] == "b");

    CHECK(Wait::codepoint("a") == 0x61);
    CHECK(Wait::codepoint("\xE2\x97\x8F") == 0x25CF);
    CHECK(Wait::codepoint("\xE2\x97\x8B") == 0x25CB);
    CHECK(Wait::codepoint("\xC3\xA9") == 0xE9);          // two-byte é
    CHECK(Wait::codepoint("\xF0\x9F\x9F\xA2") == 0x1F7E2);  // four-byte 🟢
}

TEST_CASE("a truncated utf8 sequence does not read past the end") {
    // Configuration is a file a human edits; a half-written multibyte character
    // must not walk off the string.
    const auto chars = Wait::utf8Chars("\xE2\x97");
    REQUIRE(chars.size() == 1);
    CHECK(chars[0] == "\xE2\x97");
    CHECK(Wait::utf8Chars("").empty());
    CHECK(Wait::codepoint("") == 0);
}
