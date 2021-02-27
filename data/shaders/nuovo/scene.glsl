void rScene(vec3 ro, vec3 rd, inout SortedLinkedList ret, bool shadow) {
    rStones(ro, rd, ret, shadow);
    rBowlStones(ro, rd, ret, shadow);
    if(!shadow) {
        rBowls(ro, rd, ret, shadow);
        rBoard(ro, rd, ret, shadow);
        rLegs(ro, rd, ret, shadow);
        rPlaneY(ro, rd, vec3(0.0, bnx.y-legh, 0.0), nBoard, ret, idTable, shadow);
    }
}

vec2 sScene(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec2 shadow = vec2(1.0);
    shadow *= sBowlStones(pos, lig, 10.5, ipp);
    shadow *= sStones(pos, lig, 10.5, ipp);
    shadow *= sBowls(pos, lig, 10.5, ipp);
    shadow *= sBoard(pos, lig, 10.5, ipp);
    return shadow;
}