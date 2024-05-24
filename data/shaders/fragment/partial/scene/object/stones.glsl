void finalizeStone(vec3 ro, vec3 rd, vec3 dd, inout IP ipp, float rot, bool marked) {
    mat4 rmat = mat4(
        cos(rot),0.0,sin(rot),0.0,
        0.0,1.0,0.0,0.0,
        -sin(rot),0.0,cos(rot),0.0,
        0.0,0.0,0.0,1.0
    );
    vec3 ip = (ro + ipp.t.x * rd);
    vec3 add = 4.0*(ip.xyz - vec3(13.0+dd.x,dd.y-13.0, dd.z));
    ipp.uvw = rmat * vec4(add, 1.0);

    if(marked) {
        vec3 cc = dd;
        cc.y -= dn.y - sqrt(stoneRadius2 - 0.25*r1*r1);
        vec3 ip0 = ro + rd*dot(cc - ro, nBoard) / dot(rd, nBoard);
        vec3 q2;

        ipp.a = vec2(0.0);
        bool isNotArea = length(ip.xz - cc.xz) > 0.5*r1 || ip.y < dd.y;
        if (isNotArea) {
            vec2 rr = distanceRayCircle(ro, rd, ip0, ipp.t.xy, cc, 0.5*r1, q2);
            float d = abs(rr.y);
            if (sign(ro.y - dd.y) * rr.x <= 0.0 && d < boardaa){
                ipp.a = vec2(max(d/boardaa, ipp.d));
            }
            else {
                ipp.a = vec2(0.0);
                ipp.oid = ipp.pid;
            }
        }
    }
}

void rStoneY(
    in vec3 ro, in vec3 rd, vec3 dd,
    inout SortedLinkedList ret, inout IP rip, ivec2 oid, bool marked,
    int uid
) {
    vec2 dpre = intersectionRaySphereR(ro, rd, dd, r1*r1);
    bool rval = false;
    vec3 n;
    if (dpre.x < farClip && dpre.y > 0.0) {
        vec2 d2 = intersectionRaySphere(ro, rd, dd - dn);
        float d2x = d2.x < 0.0 ? d2.y : d2.x;
        if (d2x < farClip && d2x > 0.0) {
            vec2 d1 = intersectionRaySphere(ro, rd, dd + dn);
            float p1 = max(d1.x, d2.x);
            float p2 = min(d1.y, d2.y);
            if (p1 >= 0.0 && p1 <= p2 && p1 > 0.0) {
                vec2 d3 = intersectionRayCylinder(ro, rd, dd, px*px);
                vec2 d4;
                vec4 tt = intersectionRayEllipsoid(ro, rd, dd, d4);
                vec3 ip = ro + p1*rd;
                vec3 rcc = dd + (d1.x < d2.x ? -dn : dn);
                n = normalize(ip - rcc);
                if (p1 >= d3.x && p1 < d3.y && d3.x < farClip) {
                    rval = true;
                    rip.t = vec3(p1, p2, -0.0);
                }
                else if (tt.x < farClip) {
                    n.y = mix(n.y, tt.y, smoothstep(px, r1, length(ip.xz - dd.xz)));
                    rval = true;
                    rip.t = vec3(d4,-0.0);
                }
            }
        }
    }
    if(rval && rip.t.x > 0.0){
        if(!(rval && rip.t.x > 0.0)) {
            rip.t = vec3(distance(ro, dd), farClip, -2.0);
        }
        if(try_insert(ret, rip)) {
            vec3 dist0a = distanceRayStoneSphere(ro, rd, dd - dn, stoneRadius);
            vec3 dist0b = distanceRayStoneSphere(ro, rd, dd + dn, stoneRadius);
            dist0a = dist0a.y > dist0b.y ? dist0a : dist0b;
            dist0b.yz = distanceRayStone(ro, rd, dd);
            rip.d = mix(dist0a.y / dist0a.z, dist0b.y / dist0b.z, step(0.0, dist0a.x));
            rip.oid = oid.x;
            rip.pid = oid.y;
            rip.a = vec2(0.0);
            rip.n = normalize(n);
            rip.uid = uid;
            rip.fog = 1.0;
            finalizeStone(ro, rd, dd, rip, iStones[uid].w, marked);
            insert(ret, rip);
        }
    }
}

void rStones(in vec3 ro, in vec3 rd, inout SortedLinkedList ret) {
    float t1, t2;
    if (IntersectBox(ro, rd, minBound, maxBound, t1, t2)) {
        vec4 b12 = ro.xzxz + vec4(t1, t1, t2, t2)*rd.xzxz;
        vec2 noise = vec2(wwx*0.2);
        vec4 bmnx = vec4(min(b12.xy, b12.zw) - noise, max(b12.xy, b12.zw) + noise);
        vec4 xz12 = floor(bmnx/vec4(wwx, wwy, wwx, wwy) + vec4(0.5*fNDIM));
        ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
        const int off = 0;
        for (int i = mnx.y - off; i <= mnx.w+off; i++){
            for (int j = mnx.x-off; j <= mnx.z+off; j++){
                vec4 stone0 = iStones[NDIM*i + j];
                ivec2 mm0 = ivec2(idBlackStone, idWhiteStone);
                float m0 = stone0.z;
                if (m0 >= cidBlackStone) {
                    vec2 xz = vec2(wwx, wwy)*stone0.xy;
                    vec3 dd = vec3(xz.x, 0.5*h, xz.y);
                    float tt2;
                    vec4 ret0;

                    bool captured = m0 == cidCapturedBlackStone || m0 == cidCapturedWhiteStone;
                    bool last = m0 == cidLastBlackStone || m0 == cidLastWhiteStone;
                    if ((captured || last)) {
                        mm0 = m0 == cidCapturedBlackStone ? mm0.yx : mm0.xy;
                    }
                    else {
                        mm0 = m0 == cidBlackStone ? mm0.xy : mm0.yx;
                    }
                    IP rip;
                    rStoneY(ro, rd, dd, ret, rip, mm0, captured||last, NDIM*i+j);
                }
                else if (m0 == cidWhiteArea || m0 == cidBlackArea) {
                    vec3 dd = vec3(vec2(j, i) - vec2(0.5*fNDIM - 0.5), 0.0)*vec3(wwx, wwy, 0.0);
                    vec2 w25 = vec2(0.25*wwx, dd.z);
                    vec3 minb = dd.xzy - w25.xyx;
                    vec3 maxb = dd.xzy + w25.xyx;
                    IP ipp;
                    float tb = dot(-ro+vec3(0.0, 0.0, 0.0), nBoard) / dot(rd, nBoard);
                    ipp.t = vec3(tb, tb, -0.0);
                    vec3 ip = ro + tb*rd;
                    float r = boardbb*distance(ro, ip);
                    if (all(lessThanEqual(abs(ip.xz - dd.xy), w25.xx+r))) {
                        if(try_insert(ret, ipp)) {
                            mm0 = ivec2(stone0.z == cidBlackArea ? idBlackArea : idWhiteArea);
                            ipp.pid = mm0.x;
                            ipp.oid = mm0.x;
                            ipp.a = vec2(1.0);
                            vec3 bmin = vec3(dd.x - w25.x, 0.0, dd.y - w25.x);
                            vec3 bmax = vec3(dd.x + w25.x, 0.0, dd.y + w25.x);
                            mat3 ps = mat3(
                                vec3(bmax.x,0.0,bmin.z),
                                vec3(bmin.x,0.0,bmin.z),
                                vec3(dd.xy, 0.0).xzy
                            );
                            mat3 cs = point32plane(ps, ip, rd);
                            float xxx = circleHalfPlaneIntersectionArea(ip, r, cs);
                            ps = mat3(
                                vec3(bmin.x,0.0,bmin.z),
                                vec3(bmin.x,0.0, bmax.z),
                                vec3(dd.xy, 0.0).xzy
                            );
                            cs = point32plane(ps, ip, rd);
                            xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));
                            ps = mat3(
                                vec3(bmin.x,0.0,bmax.z),
                                vec3(bmax.x,0.0, bmax.z),
                                vec3(dd.xy, 0.0).xzy
                            );
                            cs = point32plane(ps, ip, rd);
                            xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));
                            ps = mat3(
                                vec3(bmax.x,0.0,bmax.z),
                                vec3(bmax.x,0.0, bmin.z),
                                vec3(dd.xy, 0.0).xzy
                            );
                            cs = point32plane(ps, ip, rd);
                            xxx = min(xxx, circleHalfPlaneIntersectionArea(ip, r, cs));

                            vec3 q2;
                            ipp.d = -xxx*boardaa;
                            ipp.n = nBoard;
                            ipp.uid = NDIM*i + j;
                            ipp.uvw = vec4(ip, 1.0);
                            ipp.fog = 1.0;
                            insert(ret, ipp);
                        }
                    }
                }
            }
        }
    }
}

vec2 sStones(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec2 ret = vec2(1.0);
    if (ipp.oid != idTable) {
        float t1, t2;
        vec3 ld = lig - pos;
        if (IntersectBox(pos, ld, minBound, maxBound, t1, t2)) {
            vec4 b12 = pos.xzxz + vec4(t1, t1, t2, t2)*ld.xzxz;
            vec2 noise = vec2(wwx*0.2);
            vec4 bmnx = vec4(min(b12.xy, b12.zw) - noise, max(b12.xy, b12.zw) + noise);
            vec4 xz12 = floor(bmnx/vec4(wwx, wwy, wwx, wwy) + 0.5*vec4(vec2(fNDIM - 1.0), vec2(fNDIM + 1.0)));
            ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
            for (int i = mnx.y; i <= mnx.w; i++){
                for (int j = mnx.x; j <= mnx.z; j++){
                    //for (int kk  = 0; kk <= 10; kk++){
                    int kk = NDIM*i + j;
                    float mm0 = iStones[kk].z;
                    if (mm0 >= cidBlackStone) {
                        vec2 xz = vec2(wwx, wwy)*iStones[kk].xy;
                        ret *= sCircle(vec3(xz, 0.5*h).xzy, r1, pos, lig, ldia2, ipp);
                    }
                }
           }
        }
    }
    return ret;
}
