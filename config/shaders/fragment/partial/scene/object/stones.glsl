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
                else if (m0 == cidWhiteArea || m0 == cidBlackArea || m0 == cidAnnotation) {
                    vec3 dd = vec3(vec2(j, i) - vec2(0.5*fNDIM - 0.5), 0.0)*vec3(wwx, wwy, 0.0);
                    // Annotation patches are larger (0.4) to cover grid cross; territory patches are 0.25
                    float patchSize = (m0 == cidAnnotation) ? 0.4 : 0.25;
                    vec2 w25 = vec2(patchSize*wwx, dd.z);
                    vec3 minb = dd.xzy - w25.xyx;
                    vec3 maxb = dd.xzy + w25.xyx;
                    IP ipp;
                    float tb = dot(-ro+vec3(0.0, 0.0, 0.0), nBoard) / dot(rd, nBoard);
                    ipp.t = vec3(tb, tb, -0.0);
                    vec3 ip = ro + tb*rd;
                    float r = boardbb*distance(ro, ip);
                    if (all(lessThanEqual(abs(ip.xz - dd.xy), w25.xx+r))) {
                        if(try_insert(ret, ipp)) {
                            // Annotation uses board material; territory uses colored areas
                            if (m0 == cidAnnotation) {
                                mm0 = ivec2(idBoard);
                            } else {
                                mm0 = ivec2(stone0.z == cidBlackArea ? idBlackArea : idWhiteArea);
                            }
                            ipp.pid = mm0.x;
                            ipp.oid = mm0.x;
                            // For annotation (idBoard), use vec2(0.0) to match board shading exactly
                            // Territory areas use vec2(1.0) for their colored appearance
                            ipp.a = (m0 == cidAnnotation) ? vec2(0.0) : vec2(1.0);
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
