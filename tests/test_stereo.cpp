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
