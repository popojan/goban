/// Tunable shader parameters — ADR-0017.
///
/// `resolveShaderParams()` is pure over `nlohmann::json` for the reason
/// `resolveQualityPalette()` is: its caller lives in the OpenGL target, so
/// without that separation the capability rule — the whole point of the
/// decision — could only be exercised by launching the renderer.

#include <doctest/doctest.h>

#include "ShaderParams.h"

using nlohmann::json;

namespace {

/// The shipped declarations, in the shape `config/base.json` carries.
json declarations() {
    return json{
        {"showBowls", {{"type", "bool"}, {"default", true},
                       {"label", "Bowls"}, {"group", "Scene"}, {"requires", "bowls"}}},
        {"showLids",  {{"type", "bool"}, {"default", true},
                       {"label", "Lids and prisoners"}, {"group", "Scene"},
                       {"requires", "bowls"}}},
    };
}

json redCarpet()   { return json{{"name", "Red Carpet"}, {"bowls", 1}, {"height", 0.85}}; }
json minimalThin() { return json{{"name", "Minimal Thin"}, {"height", 0.85}}; }

} // namespace

TEST_CASE("a shader declaring the capability is offered the parameters") {
    const auto params = resolveShaderParams(declarations(), redCarpet(), json());

    REQUIRE(params.size() == 2);
    CHECK(findShaderParam(params, "showBowls") != nullptr);
    CHECK(findShaderParam(params, "showLids") != nullptr);

    // Metadata survives the round trip — the label is what a generated control
    // shows, and it is localisable precisely because it comes from here.
    CHECK(findShaderParam(params, "showLids")->label == "Lids and prisoners");
    CHECK(findShaderParam(params, "showLids")->group == "Scene");
}

TEST_CASE("a shader without the capability is offered nothing, silently") {
    // Four of the six shipped shaders have no vessels in the scene. That is not
    // a misconfiguration, so it must not warn and must not produce a toggle that
    // would do nothing.
    const auto params = resolveShaderParams(declarations(), minimalThin(), json());
    CHECK(params.empty());
}

TEST_CASE("the capability accepts JSON's two ways of writing a flag") {
    // base.json writes `"bowls": 1`; a hand-edited config may well write `true`.
    json boolCap{{"name", "x"}, {"bowls", true}};
    CHECK(resolveShaderParams(declarations(), boolCap, json()).size() == 2);

    json zeroCap{{"name", "x"}, {"bowls", 0}};
    CHECK(resolveShaderParams(declarations(), zeroCap, json()).empty());

    json falseCap{{"name", "x"}, {"bowls", false}};
    CHECK(resolveShaderParams(declarations(), falseCap, json()).empty());
}

TEST_CASE("a saved value overrides the default, and only where it was saved") {
    const json saved{{"showLids", false}};
    const auto params = resolveShaderParams(declarations(), redCarpet(), saved);

    CHECK(findShaderParam(params, "showLids")->value == false);
    CHECK(findShaderParam(params, "showLids")->defaultValue == true);
    // Untouched, so it still tracks the shipped default — which is what lets a
    // later change to that default reach everyone who never expressed a view.
    CHECK(findShaderParam(params, "showBowls")->value == true);
}

TEST_CASE("a malformed saved value falls back to the default rather than to false") {
    // Reading a bad value as `false` would silently switch a feature off, which
    // is indistinguishable from the user having asked for that.
    const json saved{{"showLids", "no"}};
    const auto params = resolveShaderParams(declarations(), redCarpet(), saved);
    CHECK(findShaderParam(params, "showLids")->value == true);
}

TEST_CASE("an unsupported type is dropped, not guessed at") {
    // M1 is booleans. A float declaration must not be silently coerced — and it
    // must not stop the booleans beside it resolving, the per-stop degradation
    // rule ADR-0011 established for the palette.
    json decls = declarations();
    decls["ambient"] = {{"type", "float"}, {"default", 0.3}, {"label", "Ambient"}};

    const auto params = resolveShaderParams(decls, redCarpet(), json());
    CHECK(params.size() == 2);
    CHECK(findShaderParam(params, "ambient") == nullptr);
    CHECK(findShaderParam(params, "showBowls") != nullptr);
}

TEST_CASE("a declaration with no requires is offered to every shader") {
    const json decls{
        {"showGrid", {{"type", "bool"}, {"default", true}, {"label", "Grid"}}}};

    CHECK(resolveShaderParams(decls, redCarpet(), json()).size() == 1);
    CHECK(resolveShaderParams(decls, minimalThin(), json()).size() == 1);
}

TEST_CASE("an absent or malformed block resolves to nothing rather than throwing") {
    CHECK(resolveShaderParams(json(), redCarpet(), json()).empty());
    CHECK(resolveShaderParams(json::array(), redCarpet(), json()).empty());
    CHECK(resolveShaderParams(json::object(), redCarpet(), json()).empty());

    // A non-object declaration is skipped; its neighbours still resolve.
    json decls = declarations();
    decls["junk"] = 42;
    CHECK(resolveShaderParams(decls, redCarpet(), json()).size() == 2);
}

TEST_CASE("an unoffered parameter reads as on, never as off") {
    // The fallback that keeps a shader with no declarations behaving exactly as
    // it did before this mechanism existed. `drawsPrisonerPile()` depends on it:
    // reading absent as `false` would hide the pile on every shader that never
    // heard of showLids.
    const auto none = resolveShaderParams(declarations(), minimalThin(), json());
    CHECK(shaderParamValue(none, "showLids", true) == true);

    const auto some = resolveShaderParams(declarations(), redCarpet(),
                                          json{{"showLids", false}});
    CHECK(shaderParamValue(some, "showLids", true) == false);
    CHECK(shaderParamValue(some, "nosuchparam", true) == true);
}
