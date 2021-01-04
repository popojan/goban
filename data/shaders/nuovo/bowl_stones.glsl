void rBowlStones(in vec3 ro, in vec3 rd, inout Intersection result[2], inout Intersection ret) {
    int isInCup = ret.pid;
    int si = 0;
    int ei = 0;
    vec3 cci;
    cci = cc[isInCup - 1];
    if (isInCup == 2 || isInCup ==4) {
        si = 0;
        if(isInCup == 2)
            ei = min(maxCaptured, iWhiteCapturedCount);
        else
            ei = min(maxCaptured, iBlackReservoirCount);
    }
    else if (isInCup == 1 || isInCup == 3){
        si = maxCaptured;
        if(isInCup == 1)
            ei = maxCaptured + min(maxCaptured, iBlackCapturedCount);
        else
            ei = maxCaptured + min(maxCaptured, iWhiteReservoirCount);
    }
    for (int i = si; i < ei; ++i) {
        vec4 ddc = ddc[i];
        ddc.xz += cci.xz;
        ddc.y += bnx.y - legh + bowlRadius2 - bowlRadius - 0.5 * (bowlRadius2 - bowlRadius);
        int mm0;
        vec4 ret0;
        float tt2;
        if (intersectionRayStone(ro, rd, ddc.xyz, result[1].t.x, ret0, tt2)) {
            vec3 ip = ro + rd*ret0.w;
            int mm0 = (i < maxCaptured != (isInCup < 3)) ? idBowlBlackStone : idBowlWhiteStone;
            ret.dummy = ddc;
            ret.t = vec2(ret0.w, tt2);
            ret.m = mm0;
            ret.d = -farClip;
            ret.n = ret0.xyz;
            ret.p = ip;
            ret.uid = i+ (isInCup - 1)*maxCaptured;
            ret.pid = isInCup;

            if (mm0 > idBack) {
                //        updateResult(result, ret);
                if (ret.t.x <= result[0].t.x) {
                    result[1] = result[0];
                    result[0] = ret;
                }
                else if (ret.t.x <= result[1].t.x) {
                    result[1] = ret;
                }
            }
        }
    }
}
