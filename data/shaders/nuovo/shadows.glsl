vec2 softshadow(vec3 ro, vec3 rd, inout IP ipp, vec3 lig, float ldia) {
    float t1, t2;
    vec3 pos = ro + ipp.t.x * rd;
    vec3 ld = lig - pos;
    vec2 ret = vec2(1.0);
    vec3 nor = ipp.n;
    float ldia2 = ldia*ldia;
    if (pos.y < -0.001) {
        //nn = normalize(pos-cc);
        vec3 ldir = -pos;
        float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
        vec3 ip = pos + res*ldir;
        //float d = length(ip - lig);
        vec2 rr = vec2(abs(bnx.x)*length(ip - pos) / length(pos));
        vec2 ll = vec2(ldia);
        vec4 xz = vec4(min(ip.xz + rr, lpos.xz + ll), max(ip.xz - rr, lpos.xz - ll));
        vec2 xz1 = max(vec2(0.0), xz.xy - xz.zw);

        vec3 dd = vec3(0.0, bnx.y, 0.0);
        ldir = dd - pos;
        res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
        ip = pos + res*ldir;
        rr = vec2(abs(bnx.x)*length(ip - pos) / length(dd - pos));
        xz = vec4(min(ip.xz + rr, lpos.xz + ll), max(ip.xz - rr, lpos.xz - ll));
        vec2 xz2 = max(vec2(0.0), xz.xy - xz.zw);
        float mx = mix(1.0, 1.0 - 0.5*((xz1.x*xz1.y) + (xz2.x*xz2.y)) / (4.0*ldia2), clamp(-pos.y / boardaa, 0.0, 1.0));
        //if(isBoard && m!= idBoard)
        ret.x *= mx;
    }
    if (ipp.oid == idTable) {
        for(int j = 0; j <= 2; j += 2) {
            vec3 cci = (pos.x < 0.0 ? cc[j+0] : cc[j+1]);
            float yy = bnx.y - legh + bowlRadius2 - 0.5* (bowlRadius2 - bowlRadius);
            cci.y = yy + cci.y * bowlRadius;
            vec3 ddpos = cci - pos;
            float ln = length(ddpos);
            vec3 ldir = ddpos;
            float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
            vec3 ip = pos + res*ldir;
            vec2 rR = vec2(bowlRadius2*length(ip - pos) / ln, ldia);
            vec2 rR2 = rR*rR;
            float A = PI*min(rR2.x, rR2.y);
            float d = length(ip - lig);
            vec4 rmR = vec4(rR + rR.yx, rR - rR.yx);
            if (d > abs(rmR.z) && d < rmR.x) {
                vec2 rrRR = rR2.xy - rR2.yx;
                vec2 d12 = 0.5*(vec2(d) + rrRR / d);
                float dt = (-d + rmR.x)*(d + rmR.z)*(d + rmR.w)*(d + rmR.x);
                A = dot(rR2, acos(d12 / rR)) - 0.5*sqrt(dt);
            }
            else if (d > rmR.x) {
                A = 0.0;
            }
            ret.x *= 1.0 - A / (PI*ldia2);
        }
    }
    if ((ipp.oid == idCupWhite ||ipp.oid == idCupBlack || ipp.oid == idLidWhite || ipp.oid == idLidBlack)
    || ((ipp.oid == idLeg1 || ipp.oid == idLeg2 || ipp.oid == idLeg3 || ipp.oid == idLeg4 || ipp.oid == idBoard)
        && pos.y < -0.00001)) {
        vec3 u = normalize(cross(nor, nBoard));
        vec3 v = cross(nor, u);
        vec3 ip = pos + v*dot(lpos - pos, -nBoard) / dot(v, -nBoard);
        float d = length(lpos - ip);
        float phi = 2.0*acos(d / ldia);
        float A = d < ldia ? 0.5*ldia2*(phi - sin(phi)) / (PI*ldia2) : 0.0;
        //ret.x *= dot(lpos - pos, nor) > 0.0 ? 1.0 - A : A;
        int i = ipp.oid - idLeg1;
        vec3 cci;
        if(ipp.isInCup > 0) {
            cci = cc[ipp.isInCup - 1];
        }
        vec2 xz = (ipp.oid == idCupBlack || ipp.oid== idCupWhite || ipp.oid == idLidWhite
        || ipp.oid == idLidBlack) ? cci.xz : ipp.oid == idBoard ?
            vec2(0.0) : vec2(1 - 2 * (i & 1), 1 - 2 * ((i >> 1) & 1))*(bnx.xx + vec2(legh));
        phi = PI - abs(acos(dot(v.xz, normalize(pos.xz - xz))));
        if (distance(pos.xz + nor.xz, xz) > distance(pos.xz - nor.xz, xz) || ipp.oid == idBoard) {
            ret.y *= phi / PI;
            //ret.x *= phi/PI;
        }
    }
    if (ipp.oid != idTable) {
        float t1, t2;
        vec3 ld = lig - pos;
        if (IntersectBox(pos, ld, minBound, maxBound, t1, t2)) {
            vec4 b12 = pos.xzxz + vec4(t1, t1, t2, t2)*ld.xzxz;
            vec4 bmnx = vec4(min(b12.xy, b12.zw), max(b12.xy, b12.zw));
            vec4 xz12 = floor(iww * bmnx + 0.5*vec4(vec2(fNDIM - 1.0), vec2(fNDIM + 1.0)));
            ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
            for (int i = mnx.y; i <= mnx.w; i++){
                for (int j = mnx.x; j <= mnx.z; j++){
                    int kk = NDIM*i + j;
                    float mm0 = iStones[kk].z;
                    if (mm0 >= cidBlackStone) {
                        vec2 xz = ww*iStones[kk].xy;
                        if (distance(pos.xz, xz) <= r1 + 0.001) {
                            float ldia = 1000.0;
                            float phi = PI;
                            if (ipp.oid == idBoard) {
                                ret.y *= min(1.0, distance(pos.xz, xz) / r1);
                            }
                            else {
                                vec3 u = normalize(cross(nor, nBoard));
                                vec3 v = cross(u, nor);
                                vec3 diff = vec3(pos.x, 0.000, pos.z) - vec3(xz.x, 0.0, xz.y);
                                if(length(diff) > 0.01) {
                                    float acs = acos(dot(v, normalize(diff)));
                                    //ret.y *= min(1.0, acs / PI);
                                    ret.y *= mix(1.0,min(1.0, acs / PI),2.0*(length(diff)-0.01)/ww);
                                }
                            }
                        }
                        if (ipp.oid == idBoard && pos.y > -0.001){
                            vec3 ddpos = vec3(ww*iStones[kk].xy, h).xzy - pos;
                            float ln = length(ddpos);
                            vec3 ldir = ddpos;
                            float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
                            vec3 ip = pos + res*ldir;
                            vec2 rR = vec2(r1*length(ip - pos) / ln, ldia);
                            vec2 rR2 = rR*rR;
                            float A = PI*min(rR2.x, rR2.y);
                            float d = length(ip - lig);
                            vec4 rmR = vec4(rR + rR.yx, rR - rR.yx);
                            if (d > abs(rmR.z) && d < rmR.x) {
                                vec2 rrRR = rR2.xy - rR2.yx;
                                vec2 d12 = 0.5*(vec2(d) + rrRR / d);
                                float dt = (-d + rmR.x)*(d + rmR.z)*(d + rmR.w)*(d + rmR.x);
                                A = dot(rR2, acos(d12 / rR)) - 0.5*sqrt(dt);
                            }
                            else if (d > rmR.x) {
                                A = 0.0;
                            }
                            ret.x *= 1.0 - A / (PI*ldia2);
                        }
                    }
                }
            }
        }

        if ((ipp.oid == idCupBlack || ipp.oid == idCupWhite || ipp.oid == idLidWhite || ipp.oid == idLidBlack
        || ipp.oid == idBowlBlackStone || ipp.oid == idBowlWhiteStone) && abs(pos.x) > 1.0) {
            int isInCup = -1;
            if(ipp.isInCup > 0) {
                isInCup = ipp.isInCup;
            }
            int si = 0; int ei = 0;
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
            for (int j = si; j < ei; ++j) {
                vec4 ddcj = ddc[j];
                ddcj.xz += cc[isInCup-1].xz;
                ddcj.y += bnx.y - legh + bowlRadius2 - bowlRadius - 0.5*(bowlRadius2 - bowlRadius);
                float cuty = bnx.y - legh - 0.5*(bowlRadius2 - bowlRadius) + bowlRadius2 + cc[isInCup-1].y * bowlRadius;
                bool isBowl = (ipp.oid == idCupBlack || ipp.oid == idCupWhite || ipp.oid == idLidWhite || ipp.oid == idLidBlack);
                bool isStone = (ipp.oid == idBowlBlackStone || ipp.oid == idBowlWhiteStone);
                if ((isBowl || isStone)  && ipp.uid != j) {
                    vec3 ddpos = ddcj.xyz - pos;
                    float ln = length(ddpos);
                    vec3 ldir = ddpos;
                    float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
                    vec3 ip = pos + res*ldir;
                    vec2 rR = vec2(r1*length(ip - pos) / ln, ldia);
                    vec2 rR2 = rR*rR;
                    float A = PI*min(rR2.x, rR2.y);
                    float d = length(ip - lig);
                    vec4 rmR = vec4(rR + rR.yx, rR - rR.yx);
                    if (d > abs(rmR.z) && d < rmR.x) {
                        vec2 rrRR = rR2.xy - rR2.yx;
                        vec2 d12 = 0.5*(vec2(d) + rrRR / d);
                        float dt = (-d + rmR.x)*(d + rmR.z)*(d + rmR.w)*(d + rmR.x);
                        A = dot(rR2, acos(d12 / rR)) - 0.5*sqrt(dt);
                    }
                    else if (d > rmR.x) {
                        A = 0.0;
                    }
                    float trig = 1.0;
                    if(isStone) {
                        trig = smoothstep(pos.y-0.02*h, ddcj.y-0.01*h, ddcj.y);
                    } else {
                        trig = smoothstep(cuty-0.02*h, cuty-0.01*h, pos.y);
                    }
                    ret.x *= (1.0 - (1.0 - trig) * A / (PI*ldia2));
                }
                //else if ((m == idBowlBlackStone || m == idBowlWhiteStone) && (pos.y < ddcj.y + 0.01*h || uid == j)) {
                //    vec3 u = normalize(cross(nor, nBoard));
                //    vec3 v = cross(nor, u);
                //    ret.y *= min(1.0, 1.0 - abs(acos(dot(v.xz, normalize(pos.xz - ddcj.xz)))) / PI);
                //}
            }
        }
    }
    return ret;
    return ret;
}
/*
vec2 softshadow(in vec3 pos, in vec3 nor, const vec3 lig, const float ldia, int m, bool ao, int uid, int pid) {
vec2 ret = vec2(1.0);
bool isBoard = m == idBoard || m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4 || m == idGrid;
if (isBoard && pos.y > h) return ret;
const float eps = -0.00001;
float ldia2 = ldia*ldia;
if (pos.y < -eps) {
    //nn = normalize(pos-cc);
    vec3 ldir = -pos;
    float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
    vec3 ip = pos + res*ldir;
    //float d = length(ip - lig);
    vec2 rr = vec2(abs(bnx.x)*length(ip - pos) / length(pos));
    vec2 ll = vec2(ldia);
    vec4 xz = vec4(min(ip.xz + rr, lpos.xz + ll), max(ip.xz - rr, lpos.xz - ll));
    vec2 xz1 = max(vec2(0.0), xz.xy - xz.zw);

    vec3 dd = vec3(0.0, bnx.y, 0.0);
    ldir = dd - pos;
    res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
    ip = pos + res*ldir;
    rr = vec2(abs(bnx.x)*length(ip - pos) / length(dd - pos));
    xz = vec4(min(ip.xz + rr, lpos.xz + ll), max(ip.xz - rr, lpos.xz - ll));
    vec2 xz2 = max(vec2(0.0), xz.xy - xz.zw);
    float mx = mix(1.0, 1.0 - 0.5*((xz1.x*xz1.y) + (xz2.x*xz2.y)) / (4.0*ldia2), clamp(-pos.y / boardaa, 0.0, 1.0));
    //if(isBoard && m!= idBoard)
    ret.x *= mx;
}
if (m == idTable) {
    for(int j = 0; j <= 2; j += 2) {
        vec3 cci = (pos.x < 0.0 ? cc[j+0] : cc[j+1]);
        float yy = bnx.y - legh + bowlRadius2 - 0.5* (bowlRadius2 - bowlRadius);
        cci.y = yy + cci.y * bowlRadius;
        vec3 ddpos = cci - pos;
        float ln = length(ddpos);
        vec3 ldir = ddpos;
        float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
        vec3 ip = pos + res*ldir;
        vec2 rR = vec2(bowlRadius2*length(ip - pos) / ln, ldia);
        vec2 rR2 = rR*rR;
        float A = PI*min(rR2.x, rR2.y);
        float d = length(ip - lig);
        vec4 rmR = vec4(rR + rR.yx, rR - rR.yx);
        if (d > abs(rmR.z) && d < rmR.x) {
            vec2 rrRR = rR2.xy - rR2.yx;
            vec2 d12 = 0.5*(vec2(d) + rrRR / d);
            float dt = (-d + rmR.x)*(d + rmR.z)*(d + rmR.w)*(d + rmR.x);
            A = dot(rR2, acos(d12 / rR)) - 0.5*sqrt(dt);
        }
        else if (d > rmR.x) {
            A = 0.0;
        }
        ret.x *= 1.0 - A / (PI*ldia2);
    }
}
if ((m == idCupWhite || m == idCupBlack || m == idLidWhite || m == idLidBlack) || ((m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4 || m == idBoard) && pos.y < -0.00001)) {
    vec3 u = normalize(cross(nor, nBoard));
    vec3 v = cross(nor, u);
    vec3 ip = pos + v*dot(lpos - pos, -nBoard) / dot(v, -nBoard);
    float d = length(lpos - ip);
    float phi = 2.0*acos(d / ldia);
    float A = d < ldia ? 0.5*ldia2*(phi - sin(phi)) / (PI*ldia2) : 0.0;
    //ret.x *= dot(lpos - pos, nor) > 0.0 ? 1.0 - A : A;
    int i = m - idLeg1;
    vec3 cci;
    if(pid > 0) {
        cci = cc[pid - 1];
    }
    vec2 xz = (m == idCupBlack || m == idCupWhite || m == idLidWhite || m == idLidBlack) ? cci.xz : m == idBoard ? vec2(0.0) : vec2(1 - 2 * (i & 1), 1 - 2 * ((i >> 1) & 1))*(bnx.xx + vec2(legh));
    phi = PI - abs(acos(dot(v.xz, normalize(pos.xz - xz))));
    if (distance(pos.xz + nor.xz, xz) > distance(pos.xz - nor.xz, xz) || m == idBoard) {
        ret.y *= phi / PI;
        //ret.x *= phi/PI;
    }
}
if (m != idTable) {
    float t1, t2;
    vec3 ld = lig - pos;
    if (IntersectBox(pos, ld, minBound, maxBound, t1, t2)) {
        vec4 b12 = pos.xzxz + vec4(t1, t1, t2, t2)*ld.xzxz;
        vec4 bmnx = vec4(min(b12.xy, b12.zw), max(b12.xy, b12.zw));
        vec4 xz12 = floor(iww * bmnx + 0.5*vec4(vec2(fNDIM - 1.0), vec2(fNDIM + 1.0)));
        ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
        for (int i = mnx.y; i <= mnx.w; i++){
            for (int j = mnx.x; j <= mnx.z; j++){
                int kk = NDIM*i + j;
                float mm0 = iStones[kk].z;
                if (mm0 >= cidBlackStone) {
                    vec2 xz = ww*iStones[kk].xy;
                    if (distance(pos.xz, xz) <= r1 + 0.001) {
                        float ldia = 1000.0;
                        float phi = PI;
                        if (isBoard) {
                            ret.y *= min(1.0, distance(pos.xz, xz) / r1);
                        }
                        else {
                            vec3 u = normalize(cross(nor, nBoard));
                            vec3 v = cross(u, nor);
            vec3 diff = vec3(pos.x, 0.000, pos.z) - vec3(xz.x, 0.0, xz.y);
            if(length(diff) > 0.01) {
                               float acs = acos(dot(v, normalize(diff)));
                                   //ret.y *= min(1.0, acs / PI);
                   ret.y *= mix(1.0,min(1.0, acs / PI),2.0*(length(diff)-0.01)/ww);
            }
                        }
                    }
                    if (isBoard && pos.y > -0.001){
                        vec3 ddpos = vec3(ww*iStones[kk].xy, h).xzy - pos;
                        float ln = length(ddpos);
                        vec3 ldir = ddpos;
                        float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
                        vec3 ip = pos + res*ldir;
                        vec2 rR = vec2(r1*length(ip - pos) / ln, ldia);
                        vec2 rR2 = rR*rR;
                        float A = PI*min(rR2.x, rR2.y);
                        float d = length(ip - lig);
                        vec4 rmR = vec4(rR + rR.yx, rR - rR.yx);
                        if (d > abs(rmR.z) && d < rmR.x) {
                            vec2 rrRR = rR2.xy - rR2.yx;
                            vec2 d12 = 0.5*(vec2(d) + rrRR / d);
                            float dt = (-d + rmR.x)*(d + rmR.z)*(d + rmR.w)*(d + rmR.x);
                            A = dot(rR2, acos(d12 / rR)) - 0.5*sqrt(dt);
                        }
                        else if (d > rmR.x) {
                            A = 0.0;
                        }
                        ret.x *= 1.0 - A / (PI*ldia2);
                    }
                }
            }
        }
    }

    if ((m == idCupBlack || m == idCupWhite || m == idLidWhite || m == idLidBlack || m == idBowlBlackStone || m == idBowlWhiteStone) && abs(pos.x) > 1.0) {
        int isInCup = -1;
        if(pid > 0) {
            isInCup = pid;
        }
        int si = 0; int ei = 0;
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
        for (int j = si; j < ei; ++j) {
            vec4 ddcj = ddc[j];
            ddcj.xz += cc[isInCup-1].xz;
            ddcj.y += bnx.y - legh + bowlRadius2 - bowlRadius - 0.5*(bowlRadius2 - bowlRadius);
            float cuty = bnx.y - legh - 0.5*(bowlRadius2 - bowlRadius) + bowlRadius2 + cc[isInCup-1].y * bowlRadius;
            bool isBowl = (m == idCupBlack || m == idCupWhite || m == idLidWhite || m == idLidBlack);
            bool isStone = (m == idBowlBlackStone || m == idBowlWhiteStone);
            if ((isBowl || isStone)  && uid != j) {
                vec3 ddpos = ddcj.xyz - pos;
                float ln = length(ddpos);
                vec3 ldir = ddpos;
                float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
                vec3 ip = pos + res*ldir;
                vec2 rR = vec2(r1*length(ip - pos) / ln, ldia);
                vec2 rR2 = rR*rR;
                float A = PI*min(rR2.x, rR2.y);
                float d = length(ip - lig);
                vec4 rmR = vec4(rR + rR.yx, rR - rR.yx);
                if (d > abs(rmR.z) && d < rmR.x) {
                    vec2 rrRR = rR2.xy - rR2.yx;
                    vec2 d12 = 0.5*(vec2(d) + rrRR / d);
                    float dt = (-d + rmR.x)*(d + rmR.z)*(d + rmR.w)*(d + rmR.x);
                    A = dot(rR2, acos(d12 / rR)) - 0.5*sqrt(dt);
                }
                else if (d > rmR.x) {
                    A = 0.0;
                }
                float trig = 1.0;
                if(isStone) {
                    trig = smoothstep(pos.y-0.02*h, ddcj.y-0.01*h, ddcj.y);
                } else {
                    trig = smoothstep(cuty-0.02*h, cuty-0.01*h, pos.y);
                }
                ret.x *= (1.0 - (1.0 - trig) * A / (PI*ldia2));
            }
            //else if ((m == idBowlBlackStone || m == idBowlWhiteStone) && (pos.y < ddcj.y + 0.01*h || uid == j)) {
            //    vec3 u = normalize(cross(nor, nBoard));
            //    vec3 v = cross(nor, u);
            //    ret.y *= min(1.0, 1.0 - abs(acos(dot(v.xz, normalize(pos.xz - ddcj.xz)))) / PI);
            //}
        }
    }
}
return ret;
    return vec2(1.0);
}
*/
