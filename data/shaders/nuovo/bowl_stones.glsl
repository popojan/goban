void rBowlStones(in vec3 ro, in vec3 rd, inout SortedLinkedList ret, bool shadow) {

    int si = 0;
    int ei = 0;

    for (int i = 0; i < 4; i++) {
        vec3 cc1 = cc[i];
        cc1.y = bnx.y - legh + bowlRadius2 - 0.5* (bowlRadius2 - bowlRadius);
        vec4 ret0;
        float tt2;
        float t = intersectionRaySphereR(ro, rd, cc1, bowlRadius2*bowlRadius2).x;
        int isInCup = t < farClip ? i +1: 0;
        if(isInCup == 0)
            continue;
        vec3 cci;
        cci = cc[isInCup - 1];
        ivec2 mm0;
        if (isInCup == 2 || isInCup ==4) {
            si = 0;
            if (isInCup == 2) {
                ei = min(maxCaptured, iWhiteCapturedCount);
                mm0 = ivec2(idBowlBlackStone);
            }
            else {
                ei = min(maxCaptured, iBlackReservoirCount);
                mm0 = ivec2(idBowlBlackStone);
            }
        }
        else if (isInCup == 1 || isInCup == 3){
            si = maxCaptured;
            if (isInCup == 1) {
                ei = maxCaptured + min(maxCaptured, iBlackCapturedCount);
                mm0 = ivec2(idBowlWhiteStone);
            }
            else {
                ei = maxCaptured + min(maxCaptured, iWhiteReservoirCount);
                mm0 = ivec2(idBowlWhiteStone);
            }
        }
        for (int i = si; i < ei; ++i) {
            vec4 ddcj = ddc[i];
            ddcj.xz += cci.xz;
            ddcj.y += bnx.y - legh + bowlRadius2 - bowlRadius - 0.5 * (bowlRadius2 - bowlRadius);
            vec4 ret0;
            float tt2;
            int j = rStoneY(ro, rd, ddcj.xyz, ret, mm0, false, shadow, i);
            if(!shadow && j < N) {
                finalizeStone(ro, rd, ddcj.xyz, ret.ip[j], 0.0, false);
                ret.ip[j].isInCup = isInCup;
            }
        }
    }
}

vec2 sBowlStones(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    int si = 0;
    int ei = 0;
    vec2 ret = vec2(1.0);

    int isInCup = ipp.isInCup;
    vec3 cci;
    cci = cc[isInCup - 1];
    vec3 m;
    if (isInCup == 2 || isInCup ==4) {
        si = 0;
        if (isInCup == 2) {
            ei = min(maxCaptured, iWhiteCapturedCount);
            m = vec3(0.05);
        }
        else {
            ei = min(maxCaptured, iBlackReservoirCount);
            m = vec3(0.05);
        }
    }
    else if (isInCup == 1 || isInCup == 3){
        si = maxCaptured;
        if (isInCup == 1) {
            ei = maxCaptured + min(maxCaptured, iBlackCapturedCount);
            m = vec3(0.95);
        }
        else {
            ei = maxCaptured + min(maxCaptured, iWhiteReservoirCount);
            m = vec3(0.95);
        }
    }
    for (int i = si; i < ei; ++i) {
        vec4 ddcj = ddc[i];
        ddcj.xz += cci.xz;
        ddcj.y += bnx.y - legh + bowlRadius2 - bowlRadius - 0.5 * (bowlRadius2 - bowlRadius);
        int mm0;
        vec4 ret0;
        float tt2;
        ret *= sCircle(ddcj.xyz, r1, pos, lig, ldia2, ipp);
    }

    return ret;
}