void rStones(in vec3 ro, in vec3 rd, inout Intersection result[2], inout Intersection ret) {
    float t1, t2;
    if (IntersectBox(ro, rd, minBound, maxBound, t1, t2)) {
        vec4 b12 = ro.xzxz + vec4(t1, t1, t2, t2)*rd.xzxz;
        vec2 noise = vec2(ww*0.2);
        vec4 bmnx = vec4(min(b12.xy, b12.zw) - noise, max(b12.xy, b12.zw) + noise);
        vec4 xz12 = floor(iww * bmnx + vec4(0.5*fNDIM));
        ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
        for (int i = mnx.y; i <= mnx.w; i++){
            for (int j = mnx.x; j <= mnx.z; j++){
                vec4 stone0 = iStones[NDIM*i + j];
                int mm0 = idBack;
                float m0 = stone0.z;
                if (m0 >= cidBlackStone) {
                    vec2 xz = ww*stone0.xy;
                    vec3 dd = vec3(xz.x, 0.5*h, xz.y);
                    float tt2;
                    vec4 ret0;
                    if (intersectionRayStone(ro, rd, dd, result[1].t.x, ret0, tt2)) {
                        //vec3 w25 = vec3(0.25*w, h*vec2(-0.5, 0.6));
                        //vec3 minb = dd - w25.xyx;;
                        //vec3 maxb = dd + w25.xzx;
                        vec3 ip = ro + rd*ret0.w;
                        bool captured = m0 == cidCapturedBlackStone || m0 == cidCapturedWhiteStone;
                        bool last = m0 == cidLastBlackStone || m0 == cidLastWhiteStone;
                        if ((captured || last)) {
                            mm0 = captured ? (m0 == cidCapturedWhiteStone ? idCapturedWhiteStone : idCapturedBlackStone) :
                                (m0 == cidLastWhiteStone ? idLastWhiteStone : idLastBlackStone);
                        }
                        /*else if(captured || last) {
                        mm0 = (m0 == cidCapturedWhiteStone ||m0 == cidLastWhiteStone) ? idWhiteStone : idBlackStone;
                        }*/
                        else {
                            mm0 = m0 == cidBlackStone ? idBlackStone : idWhiteStone;
                        }
                        ret.dummy = vec4(dd, stone0.w);
                        ret.t = vec2(ret0.w, tt2);
                        ret.m = mm0;
                        ret.d = -farClip;
                        ret.n = ret0.xyz;
                        ret.p = ip;
                    }
                }
                else if (/*isBoardTop && */(m0 == cidWhiteArea || m0 == cidBlackArea)) {
                    vec3 dd = vec3(vec2(j, i) - vec2(0.5*fNDIM - 0.5), 0.0)*ww;
                    vec2 w25 = vec2(0.25*ww, dd.z);
                    vec3 minb = dd.xzy - w25.xyx;
                    vec3 maxb = dd.xzy + w25.xyx;
                    vec3 ip = ret.p;//ro + rd*tb;
                    if (all(equal(clamp(ip.xz, minb.xz, maxb.xz), ip.xz))) {
                        mm0 = stone0.z == cidBlackArea ? idBlackArea : idWhiteArea;
                        ret.m = mm0;
                        //ret.t = vec2(tb);
                        vec3 q2;
                        ret.d = distanceRaySquare(ro, rd, ip, minb, maxb, q2);
                        ret.n = nBoard;
                        //ret.p = ip;
                        ret.dummy.w = stone0.w;
                    }
                }

                if (mm0 > idBack) {
                    //updateResult(result, ret);
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
}

void finalizeStoneIntersection(in vec3 ro, in vec3 rd, inout Intersection result[2]) {
    vec3 dd = result[0].dummy.xyz;
    bvec3 isStone = bvec3(result[0].m == idBlackStone || result[0].m == idWhiteStone || result[0].m == idBowlBlackStone || result[0].m == idBowlWhiteStone,
        result[0].m == idCapturedBlackStone || result[0].m == idCapturedWhiteStone,
        result[0].m == idLastBlackStone || result[0].m == idLastWhiteStone);
    vec3 dist0a;
    Intersection r2 = result[0];
    if (any(isStone)) {
        dist0a = distanceRayStoneSphere(ro, rd, dd - dn, stoneRadius);
        vec3 dist0b = distanceRayStoneSphere(ro, rd, dd + dn, stoneRadius);
        dist0a = dist0a.y > dist0b.y ? dist0a : dist0b;
        dist0b.yz = distanceRayStone(ro, rd, dd);
        result[0].d = mix(dist0a.y / dist0a.z, dist0b.y / dist0b.z, step(0.0, dist0a.x));
    }
    if (any(isStone.yz)) {
        int idStoneInside = result[0].m;
        int idStone = idStoneInside;
        if(idStone == idCapturedBlackStone){
            idStone = idBlackStone;
            idStoneInside = idWhiteStone;
        }
        else if(idStone == idCapturedWhiteStone){
            idStone = idWhiteStone;
            idStoneInside = idBlackStone;
        }
        else if(idStone == idLastBlackStone){
            idStone = idBlackStone;
            idStoneInside = idLastBlackStone;
        }
        else if(idStone == idLastWhiteStone){
            idStone = idWhiteStone;
            idStoneInside = idLastWhiteStone;
        }
        result[0].m = idStoneInside;

        vec3 cc = dd;
        cc.y -= dn.y - sqrt(stoneRadius2 - 0.25*r1*r1);
        vec3 ip0 = ro + rd*dot(cc - ro, nBoard) / dot(rd, nBoard);
        vec3 q2;

        bool isNotArea = length(result[0].p.xz - cc.xz) > 0.5*r1;
        if (isNotArea) {
            vec2 rr = distanceRayCircle(ro, rd, ip0, result[0].t, cc, 0.5*r1, q2);
            r2.d = abs(rr.y);
            if (rr.x <0.0 && r2.d < boardaa){
                r2.d = max(-r2.d, result[0].d);
                r2.m = idStone;
                updateResult(result, r2);
            }
            else{
                result[0].m = idStone;
            }
        }
    }
}
