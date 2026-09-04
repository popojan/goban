// The stereoscopic depth budget — src/Stereo.h.
//
// The rules come from the stereophotography literature kept in res/stereo.zip
// (Matěj Boháč, *Keeping the Deviation Under Control* and *Výpočet maximální
// deviace*): the deviation is the near-to-far separation as a fraction of image
// width, it is set by the **near point** rather than by the distance to the
// subject, and 1/30 of the image width is the ceiling.
//
// The case that matters is the sweep at the bottom. The rule this replaced tied
// the stereo base to the *camera distance*, which passes an inspection at one
// zoom and fails at another — the board is a fixed-size object, so coming
// closer shrinks the near point far faster than it shrinks the distance.
#include <doctest/doctest.h>

#include <cmath>

#include "Stereo.h"

namespace {

// The camera model the shaders and GobanOverlay share: focal length 3 in units
// where the image is 2*aspect wide.
constexpr float F = 3.0f;
constexpr float ASPECT = 16.0f / 9.0f;

/// What the old rule produced: half-base proportional to the camera distance.
float legacyHalfBase(float eof, float cameraDistance) {
    return eof * cameraDistance / F;
}

/// The board is about two units across, so the nearest corner sits roughly this
/// far in front of the point the camera is aimed at. Deliberately crude — the
/// point of the sweep is the *shape* of the relationship, not a board.
float nearPointAt(float cameraDistance) {
    return cameraDistance - 0.8f;
}

} // namespace

TEST_CASE("halfBase and deviation are inverses") {
    for (float nearPoint : {0.5f, 1.0f, 2.5f, 10.0f}) {
        for (float want : {1.0f / 60.0f, 1.0f / 40.0f, 1.0f / 30.0f}) {
            const float b = Stereo::halfBase(want, ASPECT, nearPoint, F);
            const float got = Stereo::deviation(b, ASPECT, nearPoint,
                                                std::numeric_limits<float>::infinity(), F);
            CHECK(got == doctest::Approx(want).epsilon(1e-5));
        }
    }
}

TEST_CASE("the ceiling is a ceiling, whatever is asked for") {
    // user.json ships whatever the user last set, and the setting predates the
    // rule change — 0.0725 was the shipped default and it meant something else.
    // Asking for more than 1/30 must not deliver more than 1/30.
    const float greedy = Stereo::halfBase(0.0725f, ASPECT, 2.4f, F);
    const float capped = Stereo::halfBase(Stereo::MAX_DEVIATION, ASPECT, 2.4f, F);
    CHECK(greedy == doctest::Approx(capped));
    CHECK(Stereo::deviation(greedy, ASPECT, 2.4f,
                            std::numeric_limits<float>::infinity(), F)
          <= doctest::Approx(Stereo::MAX_DEVIATION));
}

TEST_CASE("a nearer far point needs no more base than infinity does") {
    // deviation() takes the far point because a finite one produces less
    // deviation for the same base; halfBase() assumes infinity, which is the
    // safe direction and the one the table's horizon actually gives us.
    const float b = Stereo::halfBase(Stereo::MAX_DEVIATION, ASPECT, 2.0f, F);
    const float atInfinity = Stereo::deviation(b, ASPECT, 2.0f,
                                               std::numeric_limits<float>::infinity(), F);
    const float atTwenty   = Stereo::deviation(b, ASPECT, 2.0f, 20.0f, F);
    CHECK(atTwenty < atInfinity);
    CHECK(atInfinity == doctest::Approx(Stereo::MAX_DEVIATION));
}

TEST_CASE("degenerate cameras produce no base rather than an infinite one") {
    CHECK(Stereo::halfBase(Stereo::MAX_DEVIATION, ASPECT, 0.0f, F) == 0.0f);
    CHECK(Stereo::halfBase(Stereo::MAX_DEVIATION, ASPECT, -1.0f, F) == 0.0f);
    CHECK(Stereo::halfBase(Stereo::MAX_DEVIATION, ASPECT, 2.0f, 0.0f) == 0.0f);
    CHECK(Stereo::deviation(0.05f, ASPECT, 0.0f, 10.0f, F) == 0.0f);
}

TEST_CASE("the deviation holds across the zoom range — and did not before") {
    // The whole reason the rule changed. Note that the legacy row is not merely
    // "worse": it passes nowhere here, and its failure *grows* as the camera
    // comes in, which is exactly the case a single screenshot does not catch.
    constexpr float LEGACY_EOF = 0.0725f;   // the shipped value, in its old meaning

    bool legacyEverExceeded = false;
    float legacyWorst = 0.0f;

    for (float d : {1.2f, 1.6f, 2.2f, 3.1f, 5.0f, 9.0f, 20.0f}) {
        const float nearPoint = nearPointAt(d);
        REQUIRE(nearPoint > 0.0f);

        const float now = Stereo::deviation(
            Stereo::halfBase(Stereo::DEFAULT_DEVIATION, ASPECT, nearPoint, F),
            ASPECT, nearPoint, std::numeric_limits<float>::infinity(), F);
        CHECK(now <= doctest::Approx(Stereo::MAX_DEVIATION));
        CHECK(now == doctest::Approx(Stereo::DEFAULT_DEVIATION).epsilon(1e-4));

        const float legacy = Stereo::deviation(legacyHalfBase(LEGACY_EOF, d),
                                               ASPECT, nearPoint,
                                               std::numeric_limits<float>::infinity(), F);
        if (legacy > Stereo::MAX_DEVIATION) legacyEverExceeded = true;
        legacyWorst = std::max(legacyWorst, legacy);
    }

    CHECK(legacyEverExceeded);
    // Zoomed in, it was more than twice the ceiling: 1/12 of the image width
    // where 1/30 is the limit.
    CHECK(legacyWorst > 2.0f * Stereo::MAX_DEVIATION);
}

TEST_CASE("the window rests on the near point, at every zoom and aspect") {
    // The identity the derived window is built on. Substituting halfBase() into
    // convergence() cancels the near point, so `dof = deviation * aspect` puts
    // the zero-parallax plane exactly on the nearest thing in frame — with no
    // camera term left in it.
    //
    // This is what a fixed `dof` could not do. Measured before the change, the
    // shipped 0.0925 put the near point 3.6% of the image width behind the glass
    // at 4:3 and 1.9% at 16:9 — the whole scene behind the screen by an amount
    // nobody chose, and different on every monitor.
    for (float aspect : {4.0f / 3.0f, 16.0f / 9.0f, 16.0f / 10.0f, 1.0f, 21.0f / 9.0f}) {
        for (float d : {1.2f, 1.6f, 2.2f, 3.1f, 5.0f, 9.0f, 20.0f}) {
            const float nearPoint = nearPointAt(d);
            REQUIRE(nearPoint > 0.0f);

            const float base = Stereo::halfBase(Stereo::DEFAULT_DEVIATION, aspect, nearPoint, F);
            const float win  = Stereo::window(Stereo::DEFAULT_DEVIATION, aspect);
            CHECK(Stereo::convergence(base, win, F)
                  == doctest::Approx(nearPoint).epsilon(1e-4));
        }
    }
}

TEST_CASE("the window offset moves the scene, and only the scene") {
    const float nearPoint = nearPointAt(3.1f);
    const float base = Stereo::halfBase(Stereo::DEFAULT_DEVIATION, ASPECT, nearPoint, F);

    const float atNear = Stereo::convergence(base, Stereo::window(Stereo::DEFAULT_DEVIATION, ASPECT), F);
    // Positive pushes the window back, so the plane moves away from the camera
    // and the scene sinks further behind the glass.
    const float pushed = Stereo::convergence(
        base, Stereo::window(Stereo::DEFAULT_DEVIATION, ASPECT, +0.01f), F);
    // Negative pulls it forward, bringing the near part of the scene out through
    // the screen plane — vivid, and where it collides with the flat interface.
    const float pulled = Stereo::convergence(
        base, Stereo::window(Stereo::DEFAULT_DEVIATION, ASPECT, -0.01f), F);

    CHECK(pushed < atNear);
    CHECK(pulled > atNear);

    // The offset is bounded, and the deviation is untouched by any of it — the
    // window slides the range through the screen, it does not stretch it.
    CHECK(Stereo::clampWindowOffset(+10.0f) == doctest::Approx(Stereo::MAX_WINDOW_OFFSET));
    CHECK(Stereo::clampWindowOffset(-10.0f) == doctest::Approx(-Stereo::MAX_WINDOW_OFFSET));
    for (float offset : {-0.01f, 0.0f, +0.01f}) {
        (void) offset;
        CHECK(Stereo::deviation(base, ASPECT, nearPoint,
                                std::numeric_limits<float>::infinity(), F)
              == doctest::Approx(Stereo::DEFAULT_DEVIATION).epsilon(1e-4));
    }
}

TEST_CASE("an anaglyph mode survives a round trip through its name") {
    // The name is what goes into user.json and what comes back out of it, so a
    // mode whose name does not parse would be silently reset to gray on the next
    // start — with the setting still sitting in the file, looking applied.
    for (const auto mode : {Stereo::Anaglyph::Gray, Stereo::Anaglyph::HalfColor,
                            Stereo::Anaglyph::Color, Stereo::Anaglyph::Dubois}) {
        const auto parsed = Stereo::parseAnaglyph(Stereo::anaglyphName(mode));
        REQUIRE(parsed);
        CHECK(*parsed == mode);
    }
}

TEST_CASE("anaglyph names are forgiving where they have to be") {
    // Typed at a command prompt and into a config file, not picked from a list.
    CHECK(Stereo::parseAnaglyph("grey") == Stereo::Anaglyph::Gray);
    CHECK(Stereo::parseAnaglyph("GRAY") == Stereo::Anaglyph::Gray);
    CHECK(Stereo::parseAnaglyph("half_color") == Stereo::Anaglyph::HalfColor);
    CHECK(Stereo::parseAnaglyph("half-colour") == Stereo::Anaglyph::HalfColor);
    CHECK(Stereo::parseAnaglyph("Dubois") == Stereo::Anaglyph::Dubois);
    CHECK(Stereo::parseAnaglyph("colour") == Stereo::Anaglyph::Color);

    // And unforgiving where it matters: a typo must be distinguishable from a
    // choice, or a misspelling in base.json silently selects mode zero.
    CHECK_FALSE(Stereo::parseAnaglyph("duboys"));
    CHECK_FALSE(Stereo::parseAnaglyph(""));
    CHECK_FALSE(Stereo::parseAnaglyph("optimal"));
}

TEST_CASE("the uniform order is the enum order") {
    // The GLSL branches on these integers by hand; nothing links the two but
    // this. Gray must be 0 in particular — it is what an uninitialised uniform
    // and a failed parse both fall back to.
    CHECK(Stereo::anaglyphUniform(Stereo::Anaglyph::Gray) == 0);
    CHECK(Stereo::anaglyphUniform(Stereo::Anaglyph::HalfColor) == 1);
    CHECK(Stereo::anaglyphUniform(Stereo::Anaglyph::Color) == 2);
    CHECK(Stereo::anaglyphUniform(Stereo::Anaglyph::Dubois) == 3);
}

TEST_CASE("the colour dial is bounded, and zero means gray in any mode") {
    CHECK(Stereo::clampStrength(0.5f) == doctest::Approx(0.5f));
    CHECK(Stereo::clampStrength(-1.0f) == doctest::Approx(0.0f));
    CHECK(Stereo::clampStrength(4.0f) == doctest::Approx(1.0f));
    // Above 1 would oversaturate rather than "more colour": the shader mixes
    // toward the eye's own colour, so t > 1 extrapolates past it into values the
    // scene never contained.
    CHECK(Stereo::DEFAULT_STRENGTH == doctest::Approx(1.0f));
}

TEST_CASE("crosstalk cancellation is bounded and never negative") {
    const auto c = Stereo::clampCrosstalk({-0.2f, 0.05f, 9.0f});
    // A negative correction would *add* the other eye's image — ghosting on
    // purpose — so it clamps to zero rather than being taken literally.
    CHECK(c.r == doctest::Approx(0.0f));
    CHECK(c.g == doctest::Approx(0.05f));
    // And past the ceiling it stops cancelling a ghost and starts cutting a hole
    // where the other eye's image is.
    CHECK(c.b == doctest::Approx(Stereo::MAX_CROSSTALK));
}

TEST_CASE("only a composite that can go negative pays for a float target") {
    // This is the predicate that keeps the ordinary modes on the direct path —
    // the one already verified against real glasses — so it is worth pinning in
    // both directions.
    const Stereo::Crosstalk none;
    CHECK_FALSE(none.any());
    CHECK_FALSE(Stereo::needsSignedAccumulation(Stereo::Anaglyph::Gray, none));
    CHECK_FALSE(Stereo::needsSignedAccumulation(Stereo::Anaglyph::HalfColor, none));
    CHECK_FALSE(Stereo::needsSignedAccumulation(Stereo::Anaglyph::Color, none));

    // Dubois always, because its own off-diagonal coefficients are negative.
    CHECK(Stereo::needsSignedAccumulation(Stereo::Anaglyph::Dubois, none));

    // ...and any explicit correction, in any mode, including gray.
    const Stereo::Crosstalk green{0.0f, 0.08f, 0.0f};
    CHECK(green.any());
    CHECK(Stereo::needsSignedAccumulation(Stereo::Anaglyph::Gray, green));
}

TEST_CASE("gray is the only mode that spares the green channel") {
    // Which is what makes it work in either pair of glasses: green is the one
    // channel the two arrangements disagree about, so a mode that never uses it
    // cannot be wrong about it.
    for (const auto g : {Stereo::Glasses::RedCyan, Stereo::Glasses::RedBlue}) {
        const auto left = Stereo::eyeChannels(Stereo::Anaglyph::Gray, g, 0);
        const auto right = Stereo::eyeChannels(Stereo::Anaglyph::Gray, g, 1);
        CHECK(left.r);
        CHECK_FALSE(left.g);
        CHECK_FALSE(right.g);
        CHECK(right.b);
        // And neither eye can show hue, in either pair.
        CHECK_FALSE(Stereo::carriesColor(Stereo::Anaglyph::Gray, g, 0));
        CHECK_FALSE(Stereo::carriesColor(Stereo::Anaglyph::Gray, g, 1));
    }
}

TEST_CASE("green changes eyes with the glasses, and colour follows it") {
    // The correction that came from a real pair of glasses: a blue lens blocks
    // green while the red lens leaks it, so under red/blue green reaches the
    // LEFT eye. Sending it to the right eye there does not merely look wrong —
    // it puts the right eye's picture into the left eye, which is a second board.
    const auto mode = Stereo::Anaglyph::HalfColor;

    const auto cyanRight = Stereo::eyeChannels(mode, Stereo::Glasses::RedCyan, 1);
    CHECK(cyanRight.g);
    CHECK(cyanRight.b);
    CHECK_FALSE(Stereo::eyeChannels(mode, Stereo::Glasses::RedCyan, 0).g);

    const auto blueLeft = Stereo::eyeChannels(mode, Stereo::Glasses::RedBlue, 0);
    CHECK(blueLeft.r);
    CHECK(blueLeft.g);
    CHECK_FALSE(Stereo::eyeChannels(mode, Stereo::Glasses::RedBlue, 1).g);

    // Exactly one eye can carry hue, and which one flips with the glasses.
    CHECK(Stereo::carriesColor(mode, Stereo::Glasses::RedCyan, 1));
    CHECK_FALSE(Stereo::carriesColor(mode, Stereo::Glasses::RedCyan, 0));
    CHECK(Stereo::carriesColor(mode, Stereo::Glasses::RedBlue, 0));
    CHECK_FALSE(Stereo::carriesColor(mode, Stereo::Glasses::RedBlue, 1));

    // Blue is never in dispute: no lens disagrees about it.
    for (const auto g : {Stereo::Glasses::RedCyan, Stereo::Glasses::RedBlue}) {
        CHECK(Stereo::eyeChannels(mode, g, 1).b);
        CHECK_FALSE(Stereo::eyeChannels(mode, g, 0).b);
    }
}

TEST_CASE("no channel is ever written by both eyes") {
    // The property that makes an anaglyph an anaglyph. A channel claimed twice
    // is one eye's image delivered to both, which is the double picture.
    for (const auto mode : {Stereo::Anaglyph::Gray, Stereo::Anaglyph::HalfColor,
                            Stereo::Anaglyph::Color, Stereo::Anaglyph::Dubois}) {
        for (const auto g : {Stereo::Glasses::RedCyan, Stereo::Glasses::RedBlue}) {
            const auto l = Stereo::eyeChannels(mode, g, 0);
            const auto r = Stereo::eyeChannels(mode, g, 1);
            // Bound first: doctest cannot decompose && inside a CHECK.
            const bool bothRed = l.r && r.r;
            const bool bothGreen = l.g && r.g;
            const bool bothBlue = l.b && r.b;
            CHECK_FALSE(bothRed);
            CHECK_FALSE(bothGreen);
            CHECK_FALSE(bothBlue);
            // The left eye always owns red; that much never moves.
            CHECK(l.r);
        }
    }
}

TEST_CASE("glasses names survive a round trip, and Dubois knows its limits") {
    for (const auto g : {Stereo::Glasses::RedCyan, Stereo::Glasses::RedBlue}) {
        const auto parsed = Stereo::parseGlasses(Stereo::glassesName(g));
        REQUIRE(parsed);
        CHECK(*parsed == g);
    }
    CHECK(Stereo::parseGlasses("red/blue") == Stereo::Glasses::RedBlue);
    CHECK(Stereo::parseGlasses("RedCyan") == Stereo::Glasses::RedCyan);
    CHECK_FALSE(Stereo::parseGlasses("red-green"));

    // Dubois' matrices are fitted to cyan filters; under red/blue they would
    // send green to the eye that cannot see it.
    CHECK(Stereo::duboisApplies(Stereo::Glasses::RedCyan));
    CHECK_FALSE(Stereo::duboisApplies(Stereo::Glasses::RedBlue));
}

TEST_CASE("per-eye gain is bounded") {
    const Stereo::EyeBalance unity;
    CHECK(unity.isUnity());
    const auto b = Stereo::clampBalance({0.0f, 99.0f});
    // Zero would be a black eye rather than a dim one, and there is no sense in
    // which that is a balance.
    CHECK(b.left == doctest::Approx(Stereo::MIN_BALANCE));
    CHECK(b.right == doctest::Approx(Stereo::MAX_BALANCE));
    CHECK_FALSE(Stereo::clampBalance({1.0f, 1.4f}).isUnity());
}

TEST_CASE("the green dial is bounded, and is not the strength dial") {
    CHECK(Stereo::clampGreen(0.5f) == doctest::Approx(0.5f));
    CHECK(Stereo::clampGreen(-1.0f) == doctest::Approx(0.0f));
    CHECK(Stereo::clampGreen(2.0f) == doctest::Approx(1.0f));
    CHECK(Stereo::DEFAULT_GREEN == doctest::Approx(1.0f));

    // The distinction the two dials exist on either side of, and the reason both
    // are needed. `strength` desaturates toward luminance — and luminance has
    // *full* green, so it cannot reduce the one channel leaky lenses dispute.
    // Measured on the shipped board under red/blue half-colour: strength 0 left
    // mean green at 106 of 255, where green 0 took it to 0.1.
    //
    // Nothing in C++ enforces that; it is a property of the shader. What is
    // pinned here is that they are separate values, so a later "simplification"
    // that folds one into the other has to come past this comment.
    CHECK(Stereo::clampGreen(0.0f) != Stereo::DEFAULT_STRENGTH);

    // Inert in gray, which is the mode defined by leaving green out — so the
    // command says so rather than letting a user tune a knob that cannot move.
    CHECK_FALSE(Stereo::greenCarriesImage(Stereo::Anaglyph::Gray));
    CHECK(Stereo::greenCarriesImage(Stereo::Anaglyph::HalfColor));
    CHECK(Stereo::greenCarriesImage(Stereo::Anaglyph::Color));
    CHECK(Stereo::greenCarriesImage(Stereo::Anaglyph::Dubois));
}
