// Tunable scene features (ADR-0017). Declared in the `shader_params` block of
// the configuration, where the key is the uniform name, and offered only where
// the shader entry declares the `bowls` capability they require. A `uniform`
// rather than a `#define` because a #define needs a relink — 2019 ms cold,
// measured in ADR-0013 — which is the cost a live toggle exists to avoid.
//
// Which is which matters, and the geometry disagrees with one of its own names.
// cc[0] and cc[1] are the **lids**, and `bowl_stones.glsl` fills them from
// iBlackCapturedCount / iWhiteCapturedCount — so the lids are where the
// *prisoners* sit, which is what PrisonerMode::Auto is asking about. cc[2] and
// cc[3] are the bowls, holding the reservoir. (The `oid` assigned in
// bowls.glsl calls 0 and 1 `idCup*` and 2 and 3 `idLid*`, which is backwards
// against both Metrics::calc()'s comments and the contents. Left alone here —
// it is a material name, not a position — but do not take it as the authority.)
uniform bool showBowls;
uniform bool showLids;

#include object/plane.glsl
#include object/sphere.glsl
#include object/board.glsl
#include object/legs.glsl
#include object/stone_3d.glsl
#include object/stones.glsl
#include object/bowls.glsl
#include object/bowl_stones.glsl

void rScene(vec3 ro, vec3 rd, inout SortedLinkedList ret) {

    c = vec3(1.0, 0.25, wwy/wwx);
    bnx = vec4(-c.x, -0.2, -c.z, 0.0);

    rBowls(ro, rd, ret);
    rBowlStones(ro, rd, ret);
    rBoard(ro, rd, ret);
    rStones(ro, rd, ret);
    rLegs(ro, rd, ret);
    rPlaneYTextured(ro, rd, vec3(0.0, bnx.y-legh, 0.0), nBoard, ret, idTable);
}

vec2 sScene(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec2 shadow1 = sBowlStones(pos, lig, ldia2, ipp);
    vec2 shadow2 = sStones(pos, lig, ldia2, ipp);
    vec2 shadow3 = sBowls(pos, lig, ldia2, ipp);
    vec2 shadow4 = sBoard(pos, lig, ldia2, ipp);
    vec2 shadow5 = sLegs(pos, lig, ldia2, ipp);
    return shadow1 * shadow2 * shadow3 * shadow4 * shadow5;
}
