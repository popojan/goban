#include object/plane.glsl
#include object/sphere.glsl
#include object/board.glsl
#include object/stones.glsl

void rScene(vec3 ro, vec3 rd, inout SortedLinkedList ret) {
    c = vec3(1.0, 0.25, wwy/wwx);
    bnx = vec4(-c.x, -0.15, -c.z, 0.0);

    rStones(ro, rd, ret);
    rBoard(ro, rd, ret);
    rPlaneY(ro, rd, vec3(0.0, bnx.y-legh, 0.0), nBoard, ret, idTable);
}

vec2 sScene(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec2 shadow1 = sStones(pos, lig, ldia2, ipp);
    vec2 shadow2 = sBoard(pos, lig, ldia2, ipp);
    return shadow1 * shadow2;
}