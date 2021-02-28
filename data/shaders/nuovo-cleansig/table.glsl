void rTable(in vec3 ro, in vec3 rd, inout Intersection result[2], inout Intersection ret) {
    float t = dot(vec3(0.0, bnx.y - legh, 0.0) - ro, nBoard) / dot(rd, nBoard);
    bool aboveTable = ro.y >= bnx.y - legh;
    if (t > 0.0 && t < farClip && aboveTable) {
        ret.m = idTable;
        ret.t = vec2(t);
        ret.p = ro + t*rd;
        ret.n = nBoard;
        ret.d = -farClip;
        updateResult(result, ret);
    }
}

