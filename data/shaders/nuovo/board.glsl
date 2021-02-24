float dBox(in vec3 ro, in vec3 rd, in vec3 ip, in vec3 minimum, in vec3 maximum, out vec3 n0) {
    float r = boardaa*distance(ro, ip);
    mat4x3 rect;

    float dx = min(abs(ip.x - minimum.x), abs(ip.x - maximum.x));
    float dy = min(abs(ip.y - minimum.y), abs(ip.y - maximum.y));
    float dz = min(abs(ip.z - minimum.z), abs(ip.z - maximum.z));
    minimum += r;
    maximum -= r;
    if(dx < dy && dx < dz) {
        rect = mat4x3(
        vec3(ip.x, maximum.y, minimum.z),
        vec3(ip.x, minimum.y, minimum.z),
        vec3(ip.x, minimum.y, maximum.z),
        vec3(ip.x, maximum.y, maximum.z)
        );
        n0 = vec3(sign(ip.x),0.0,0.0);
    } else if(dz < dy && dz < dx) {
        rect = mat4x3(
        vec3(minimum.x, maximum.y, ip.z),
        vec3(minimum.x, minimum.y,  ip.z),
        vec3(maximum.x, minimum.y,  ip.z),
        vec3(maximum.x, maximum.y,  ip.z)
        );
        n0 = vec3(0.0,0.0, sign(ip.z));
    } else {
        rect = mat4x3(
        vec3(minimum.x, ip.y, minimum.z),
        vec3(minimum.x, ip.y, maximum.z),
        vec3(maximum.x, ip.y, maximum.z),
        vec3(maximum.x, ip.y, minimum.z)
        );
        n0 = vec3(0.0,sign(ip.y-0.5 * (maximum.y+minimum.y)),0.0);
    }
    vec3 ccc = 0.25*(rect[0] + rect[1]+rect[2] + rect[3]);
    mat3 ps = mat3(rect[0], rect[1], ccc);
    mat3 cs = point32plane(ps, ip, rd);
    float xxx = circleHalfPlaneIntersectionArea(ip, r, cs);
    ps = mat3(rect[1], rect[2], ccc);
    cs = point32plane(ps, ip, rd);
    xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));
    ps = mat3(rect[2], rect[3], ccc);
    cs = point32plane(ps, ip, rd);
    xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));
    ps = mat3(rect[3], rect[0], ccc);
    cs = point32plane(ps, ip, rd);
    xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));
    return xxx;
}

int rBoard(in vec3 ro, in vec3 rd, inout SortedLinkedList ret, bool shadow) {
    if(shadow) return N;
    float tb, tb2;
    vec3 q2;
    bool isBoard = IntersectBox(ro, rd, bnx.xyz, -bnx.xwz, tb, tb2);
    //bool isBoardTop = false;
    //tb = tb2 = dot(-ro + vec3(0.0,0.0, 0.0), nBoard) / dot(rd, nBoard);


    if (isBoard) {
        IP ipp;
        vec3 ip = ro + tb * rd;
        ipp.t = vec3(tb, tb2, 0.0);

        float xx = abs(ip.x) - 1.0;
        float zz = abs(ip.z) - 1.0;

        //float r = boardbb*distance(ro, ip);
        vec4 uvw = vec4(ip.xyz, 1.0);
        //uvw = vec4(2.0*uvw.x/ww + ww*uvw.z, uvw.y, uvw.z, 1.0);
        if(ipp.t.x > 0.0 /*&& all(lessThanEqual(abs(ip.xz), vec2(1.0) + r))*/) {
            int i = insert(ret, ipp);
            //ipp.t = vec3(tb2, tb2, 0.0);
            int j = insert(ret, ipp);
            if(i < N) {
                vec3 n0, n1;
                vec3 cc = vec3(0.0, 0.5*bnx.y, 0.0);
                vec3 ip0 = ip - cc;
                vec3 bn0 = bnx.xyz - cc;
                vec3 dif = (abs(bn0) - abs(ip0)) / abs(bn0);
                float dd = farClip;
                bool extremeTilt = false;

                /*if (all(lessThan(dif.xx, dif.yz))) {
                    n1 = vec3(1.0, 0.0, 0.0);
                    n0 = vec3(0.0, ip0.y, ip0.z);
                }
                else */if (all(lessThan(dif.yy, dif.xz))){

                    n1 = vec3(0.0, 1.0, 0.0);
                    n0 = vec3(ip0.x, 0.0, ip0.z);
                    //const float dw = 0.00075;
                    bool gridx = ip.x > -c.x + mw*ww - 0.5*ww && ip.x < c.x - mw*ww + 0.5*ww && ip.z > -c.z + mw*ww - dw && ip.z < c.z - mw*ww + dw;
                    bool gridz = ip.x > -c.x + mw*ww - dw && ip.x < c.x - mw*ww + dw && ip.z > -c.z + mw*ww - 0.5*ww && ip.z < c.z - mw*ww + 0.5*ww;
                    float r = boardbb*distance(ro, ip);
                    if (ip0.y > -0.1) {
                        //isBoardTop = true;
                        bvec2 nearEnough = bvec2(true);//lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(dw + dw + r));
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
                        nearEnough = bvec2(true);//lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(rr + rr + r));
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
                /*else {
                    n1 = vec3(0.0, 0.0, 1.0);
                    n0 = vec3(ip0.x, ip0.y, 0.0);
                }*/
                float dist0 = -dd*boardaa;// : tb2 - tb;
                //float xxx = distanceRaySquare(ro, rd, ip, bnx.xyz, -bnx.xwz, q2);
                //ret.ip[i].d = -farClip;
                //ret.d2 = -farClip;
                //ret.isBoard = isBoardTop;
                bool edge = false;
                float tc = 0.0, tc2 = 0.0;
                //IntersectBox(ro, q2 - ro, bnx.xyz, -bnx.xwz, tc, tc2);

                /*
                mat4x3 rect;
                //ret.ip[i].a = min(xxx, -dist0/boardaa);
                if(abs(ip.x - bnx.x) < 0.001 || abs(ip.x + bnx.x) < 0.001) {
                    rect = mat4x3(
                        vec3(ip.x, -bnx.w, bnx.z),
                        vec3(ip.x, bnx.y, bnx.z),
                        vec3(ip.x, bnx.y, -bnx.z),
                        vec3(ip.x, -bnx.w, -bnx.z)
                    );
                    n0 = vec3(sign(ip.x),0.0,0.0);
                } else if(abs(ip.z - bnx.z) < 0.001 || abs(ip.z + bnx.z) < 0.001) {
                    rect = mat4x3(
                        vec3(bnx.x, -bnx.w, ip.z),
                        vec3(bnx.x, bnx.y,  ip.z),
                        vec3(-bnx.x, bnx.y,  ip.z),
                        vec3(-bnx.x, -bnx.w,  ip.z)
                    );
                    n0 = vec3(0.0,0.0, sign(ip.z));
                } else {
                    rect = mat4x3(
                        vec3(bnx.x, ip.y, bnx.z),
                        vec3(bnx.x, ip.y, -bnx.z),
                        vec3(-bnx.x, ip.y, -bnx.z),
                        vec3(-bnx.x, ip.y, bnx.z)
                    );
                    n0 = vec3(0.0,sign(ip.y-0.5 * bnx.y),0.0);
                }
                vec3 ccc = 0.25*(rect[0] + rect[1]+rect[2] + rect[3]);
                mat3 ps = mat3(rect[0], rect[1], ccc);
                mat3 cs = point32plane(ps, ip, rd);
                float xxx = circleHalfPlaneIntersectionArea(ip, r, cs);
                ps = mat3(rect[1], rect[2], ccc);
                cs = point32plane(ps, ip, rd);
                xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));
                ps = mat3(rect[2], rect[3], ccc);
                cs = point32plane(ps, ip, rd);
                xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));
                ps = mat3(rect[3], rect[0], ccc);
                cs = point32plane(ps, ip, rd);
                xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));
                */
                float xxx = dBox(ro, rd, ip, bnx.xyz, -bnx.xwz, n0);

                ret.ip[i].a = vec2(clamp(1.0-dd, 0.0, 1.0),1.0-xxx);



                ret.ip[i].d = -xxx*boardaa;
                ret.ip[i].pid = all( lessThan(abs(ip.xz), vec2(0.95))) ? idBlackStone : idBoard;
                ret.ip[i].oid = idBoard;
                uvw = vec4(23.5, 23.5, 2.3, 1.0)*(uvw + vec4(-0.3, -3.6 * bnx.y, 0.0,0.0));
                float ns = 0.1*sin(0.4+1.8*uvw.z);
                vec3 grad;
                float rnd = snoise(uvw.xyz, grad);
                uvw = vec4(length(uvw.xy+0.13*rnd),  length(uvw.xy+0.13 * rnd), 1.0, 1.0);
                ret.ip[i].uvw = uvw;
                ret.ip[i].n = n0;
                //mix(vec3(0.31, 0.015,0.02), vec3(0.68,0.65,0.15), xxx);//idBoard;
                //if (dist0 > -boardaa) {
                   // if(tc2 - tc > 0.001) {
                        //n0 = normalize(n0);
                        //vec3 cdist = abs(abs(ip0) - abs(bn0));
                        //vec2 ab = clamp(vec2(abs(dist0), min(cdist.z, min(cdist.x, cdist.y))) / boardcc, 0.0, 1.0);
                        //n0 = normalize(mix(normalize(ip0), n0, ab.y));
                        //n0 = normalize(mix(n0, n1, ab.x));
                        //ret.ip[i].d2 = -farClip;
                        //ret.p = q2;
                        edge = true;
                   //}
                    //updateResult(result, ret);
                //} else {
                    //ret.p = ip;
                    //bool upit = false;
                    /*if(dist0 < -dd*boardaa) {
                        int j = insert(ret, ipp);
                        if(j < N) {
                            ret.ip[j].m = vec3(0.05, 0.05, 0.05);//idBoard;
                            ret.ip[j].d = -farClip;
                        }
                    }*/
                //}
                /*if(dist0 < -dd*boardaa) {
                    int j = insert(ret, ipp);
                    if(j < N) {
                        ret.ip[j].m = vec3(0.05, 0.05, 0.05);//idBoard;
                        ret.ip[j].d = -farClip;
                    }
                }*/
                //float dist01 = max(dist0, -dd*boardaa);
                //ret.ip[i].n = n1;
                //ret.ip[i].d = edge ? -farClip : dist01;
                /*if(ret.ip[i].d > -boardaa) {
                    int j = insert(ret, ipp);
                    if (j < N) {
                        ret.ip[j].m = vec3(0.05, 0.05, 0.05);//idBoard;
                        ret.ip[j].d = -farClip;
                    }
                }*/
                //ret.ip[i].d = dist0;
                //ret.m = idBoard;
                //updateResult(result, ret);
                //ret.isBoard = false;
                if(j < N) {
                    ret.ip[j].a = vec2(1.0, 1.0);
                    float xxx = dBox(ro, rd, ro + tb2*rd, bnx.xyz, -bnx.xwz, n1);
                    ret.ip[j].d = -xxx*boardaa;
                    ret.ip[j].pid = idBoard;
                    ret.ip[j].oid = idBoard;
                    ret.ip[j].uvw = uvw;
                    ret.ip[j].n = n0;
                }
            }
            return i;
        }
    }
        /*vec3 ip = ro + tb*rd;
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
    }*/
    return N;
}
/*void rBoard(in vec3 ro, in vec3 rd, inout Intersection result[2], inout Intersection ret) {
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
}                      if (dst < mindist) {
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
}*/
vec2 sBoard(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec2 ret = vec2(1.0);
    vec3 nor = ipp.n;
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
    return ret;
}