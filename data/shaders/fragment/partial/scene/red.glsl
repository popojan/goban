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
