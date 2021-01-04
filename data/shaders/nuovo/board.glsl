void rBoard(in vec3 ro, in vec3 rd, inout Intersection result[2], inout Intersection ret) {
    float tb, tb2;
    vec3 q2;
    bool isBoard = IntersectBox(ro, rd, bnx.xyz, -bnx.xwz, tb, tb2);
    bool isBoardTop = false;
    if (isBoard) {
        vec3 ip = ro + tb*rd;
        vec3 n0, n1;
        vec3 cc = vec3(0.0, 0.5*bnx.y, 0.0);
        vec3 ip0 = ip - cc;
        vec3 bn0 = bnx.xyz - cc;
        vec3 dif = (abs(bn0) - abs(ip0)) / abs(bn0);
        float dist0 = distanceRaySquare(ro, rd, ip, bnx.xyz, -bnx.xwz, q2);
        float dd = farClip;
        bool extremeTilt = false;

        if (all(lessThan(dif.xx, dif.yz))) {
            n1 = vec3(sign(ip0.x), 0.0, 0.0);
            n0 = vec3(0.0, ip0.y, ip0.z);
        }
        else if (all(lessThan(dif.yy, dif.xz))){

            n1 = vec3(0.0, sign(ip0.y), 0.0);
            n0 = vec3(ip0.x, 0.0, ip0.z);
            //const float dw = 0.00075;
            bool gridx = ip.x > -c.x + mw*ww - 0.5*ww && ip.x < c.x - mw*ww + 0.5*ww && ip.z > -c.z + mw*ww - dw && ip.z < c.z - mw*ww + dw;
            bool gridz = ip.x > -c.x + mw*ww - dw && ip.x < c.x - mw*ww + dw && ip.z > -c.z + mw*ww - 0.5*ww && ip.z < c.z - mw*ww + 0.5*ww;
            float r = boardbb*distance(ro, ip);
            if (ip0.y > 0.0) {
                isBoardTop = true;
                bvec2 nearEnough = lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(dw + dw + r));
                bvec2 farEnough = bvec2(true, true);// greaterThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(0.33*ww));
                if (any(nearEnough) && any(farEnough)) {
                    vec3 dir = vec3(dw, -dw, 0.0);
                    if (nearEnough.x && gridx) {
                        vec3 ccx = vec3(ww*round(iww*ip.x), ip.y, ip.z);
                        mat3 ps = mat3(ccx + dir.yzx, ccx + dir.yzy, ccx + dir.yzz + dir.yzz);
                        mat3 cs = point32plane(ps, ip, rd);
                        float a1 = circleHalfPlaneIntersectionArea(ip, r, cs);
                        ps = mat3(ccx + dir.xzx, ccx + dir.xzy, ccx + dir.xzz + dir.xzz);
                        cs = point32plane(ps, ip, rd);
                        float a2 = circleHalfPlaneIntersectionArea(ip, r, cs);
                        dd = a1 + a2;
                        //n1 = normalize(vec3(-0.01*sign(ip.x - ccx.x)*(1.0-abs(ip.x - (ccx.x + sign(ip.x - ccx.x)*dw))/dw), n1.y, n1.z));//mix(0.5*abs(ccx.x -ip.x)/dw, 0.0, 0.5*abs(ccx.x -ip.x)/dw)));
                    }
                    if (nearEnough.y && gridz) {
                        vec3 ccz = vec3(ip.x, ip.y, ww*round(iww*ip.z));
                        mat3 ps = mat3(ccz + dir.xzy, ccz + dir.yzy, ccz + dir.zzy + dir.zzy);
                        mat3 cs = point32plane(ps, ip, rd);
                        float a1 = circleHalfPlaneIntersectionArea(ip, r, cs);
                        ps = mat3(ccz + dir.xzx, ccz + dir.yzx, ccz + dir.zzx + dir.zzx);
                        cs = point32plane(ps, ip, rd);
                        float a2 = circleHalfPlaneIntersectionArea(ip, r, cs);
                        dd = min(a1 + a2, dd);
                        //n1 = normalize(vec3(n1.x,n1.y,-0.01*sign(ip.z - ccz.z)*(1.0-abs(ip.z - (ccz.z + sign(ip.z - ccz.z)*dw))/dw)));//mix(0.5*abs(ccz.z -ip.z)/dw, 0.0, 0.5*abs(ccz.z -ip.z)/dw)));
                    }
                }
                float rr = 0.1*ww;//8.0*dw;
                nearEnough = lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(rr + rr + r));
                if (any(nearEnough)) {
                    vec3 ppos;
                    float mindist = distance(ip.xz, vec2(0.0));
                    vec3 fpos = vec3(0.0);
                    if (NDIM == 19) {
                        ppos = vec3(6.0, 0.0, 6.0);
                        for (int i = -1; i <= 1; i++) {
                            for (int j = -1; j <= 1; j++) {
                                vec3 pos = ww*vec3(i, 0, j)*ppos.zyz;
                                float dst = distance(ip.xz, pos.xz);
                                if (dst < mindist) {
                                    mindist = dst;
                                    fpos = pos;
                                }
                            }
                        }
                    }
                    else if (NDIM == 13){
                        ppos = vec3(3.0, 0.0, 3.0);
                        for (int i = -1; i <= 1; i += 2) {
                            for (int j = -1; j <= 1; j += 2) {
                                vec3 pos = ww*vec3(i, 0, j)*ppos.zyz;
                                float dst = distance(ip.xz, pos.xz);
                                if (dst < mindist) {
                                    mindist = dst;
                                    fpos = pos;
                                }
                            }
                        }
                    }
                    if (mindist < rr + rr) {
                        vec3 q2;
                        float dd0 = -distanceRayCircle(ro, rd, ip, vec2(farClip), fpos, rr, q2).y;
                        if (dd0 < 0.0) {
                            dd = min(dd, 1.0 + dd0 / boardaa);
                        }
                    }
                }
            }
        }
        else {
            n1 = vec3(0.0, 0.0, sign(ip0.z));
            n0 = vec3(ip0.x, ip0.y, 0.0);
        }
        ret.t = vec2(tb, tb);
        ret.m = idBoard;
        ret.d = -farClip;
        ret.d2 = -farClip;
        ret.p = ip;
        ret.isBoard = isBoardTop;
        bool edge = false;
        if (dist0 > -boardaa) {
            float tc = 0.0, tc2 = 0.0;
            IntersectBox(ro, q2 - ro, bnx.xyz, -bnx.xwz, tc, tc2);
            if (tc2 - tc > 0.001) {
                n0 = normalize(n0);
                vec3 cdist = abs(abs(ip0) - abs(bn0));
                vec2 ab = clamp(vec2(abs(dist0), min(cdist.z, min(cdist.x, cdist.y))) / boardcc, 0.0, 1.0);
                n0 = normalize(mix(normalize(ip0), n0, ab.y));
                n0 = normalize(mix(n0, n1, ab.x));
                ret.n = n0;
                ret.d = -farClip;
                ret.d2 = -farClip;
                ret.p = q2;
                edge = true;
                updateResult(result, ret);
            }
        }
        ret.n = n1;
        ret.p = ip;
        bool upit = false;
        ret.d = dist0;
        dist0 = max(dist0, -dd*boardaa);
        ret.d2 = edge ? -farClip : dist0;
        ret.m = idBoard;
        updateResult(result, ret);
        ret.isBoard = false;
    }
}
