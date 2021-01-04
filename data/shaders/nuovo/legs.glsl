void rLegs(in vec3 ro, in vec3 rd, inout Intersection result[2], inout Intersection ret) {
    for (int i = 0; i < 4; i++){
        const float r = legh;
        vec3 cc = vec3(1 - 2 * (i & 1), 1, 1 - 2 * ((i >> 1) & 1))*(bnx.xyx + vec3(r, -0.5*r, r));
        vec2 ts = intersectionRaySphereR(ro, rd, cc, r*r);
        if (ts.x > 0.0 && ts.x < farClip) {
            ret.d = distanceRaySphere(ro, rd, cc, r);
            vec3 ip = ro + ts.x*rd;
            cc.y = bnx.y - legh;
            ret.t = ts;
            ret.m = idLeg1 + i;
            ret.p = ip;
            ret.n = normalize(ip - cc);
            updateResult(result, ret);
        }
    }
}
