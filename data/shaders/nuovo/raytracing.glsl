void castRay(in vec3 ro, in vec3 rd, out Intersection result[2]) {

    Intersection ret;
    ret.m = idBack;
    ret.d = -farClip;
    ret.n = nBoard;
    ret.t = vec2(farClip);
    ret.p = vec3(-farClip);
    ret.dummy = vec4(0.0);
    ret.isBoard = false;
    ret.pid = 0;
    ret.uid = 0;
    result[0] = result[1] = ret;

    rBoard(ro, rd, result, ret);
    rStones(ro, rd, result, ret);
    rBowls(ro, rd, result, ret);
    rBowlStones(ro, rd, result, ret);
    rTable(ro, rd, result, ret);
    rLegs(ro, rd, result, ret);
    finalizeStoneIntersection(ro, rd, result);
}

