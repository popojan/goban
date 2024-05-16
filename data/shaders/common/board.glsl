float dBox(in vec3 ro, in vec3 rd, in vec3 ip, in vec3 minimum, in vec3 maximum, out vec3 n0) {
    float r = boardaa*distance(ro, ip);
    mat4x3 rect;

    minimum -= r;
    maximum += r;

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

void rBoard(in vec3 ro, in vec3 rd, inout SortedLinkedList ret, bool shadow) {
    if(shadow) return;
    float tb, tb2;
    vec3 q2;
    bool isBoard = IntersectBox(ro, rd, bnx.xyz, -bnx.xwz, tb, tb2);

    if (isBoard) {
        IP ipp;
        vec3 ip = ro + tb * rd;
        ipp.t = vec3(tb, tb2, 0.0);

        float xx = abs(ip.x) - 1.0;
        float zz = abs(ip.z) - 1.0;

        vec4 uvw = vec4(ip.xyz, 1.0);
        if(ipp.t.x > 0.0) {
            if(try_insert(ret, ipp)) {
                vec3 n0, n1;
                vec3 cc = vec3(0.0, 0.5*bnx.y, 0.0);
                vec3 ip0 = ip - cc;
                vec3 bn0 = bnx.xyz - cc;
                vec3 dif = (abs(bn0) - abs(ip0)) / abs(bn0);
                float dd = farClip;
                bool extremeTilt = false;

                if (all(lessThan(dif.xx, dif.yz))) {
                    n1 = vec3(1.0, 0.0, 0.0);
                    n0 = vec3(0.0, ip0.y, ip0.z);
                }
                else if (all(lessThan(dif.yy, dif.xz))){

                    n1 = vec3(0.0, 1.0, 0.0);
                    n0 = vec3(ip0.x, 0.0, ip0.z);
                    bool gridx = ip.x > -c.x + mw*wwx - 0.5*wwx && ip.x < c.x - mw*wwx + 0.5*wwx && ip.z > -c.z + mw*wwy - dw && ip.z < c.z - mw*wwy + dw;
                    bool gridz = ip.x > -c.x + mw*wwx - dw && ip.x < c.x - mw*wwx + dw && ip.z > -c.z + mw*wwy - 0.5*wwy && ip.z < c.z - mw*wwy + 0.5*wwy;
                    float r = boardbb*distance(ro, ip);

                    if (ip0.y > -0.1) {
                        const bvec2 nearEnough = bvec2(true);
                        //lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(dw + dw + r));
                        const bvec2 farEnough = bvec2(true);
                        //greaterThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(0.33*ww));
                        if (any(nearEnough) && any(farEnough)) {
                            vec3 dir = vec3(dw, -dw, 0.0);
                            if (nearEnough.x && gridx) {
                                vec3 ccx = vec3(wwx*round(ip.x/wwx), ip.y, ip.z);
                                mat3 ps = mat3(ccx + dir.yzx, ccx + dir.yzy, ccx + dir.yzz + dir.yzz);
                                mat3 cs = point32plane(ps, ip, rd);
                                float a1 = circleHalfPlaneIntersectionArea(ip, r, cs);
                                ps = mat3(ccx + dir.xzx, ccx + dir.xzy, ccx + dir.xzz + dir.xzz);
                                cs = point32plane(ps, ip, rd);
                                float a2 = circleHalfPlaneIntersectionArea(ip, r, cs);
                                dd = a1 + a2;
                            }
                            if (nearEnough.y && gridz) {
                                vec3 ccz = vec3(ip.x, ip.y, wwy*round(ip.z/wwy));
                                mat3 ps = mat3(ccz + dir.xzy, ccz + dir.yzy, ccz + dir.zzy + dir.zzy);
                                mat3 cs = point32plane(ps, ip, rd);
                                float a1 = circleHalfPlaneIntersectionArea(ip, r, cs);
                                ps = mat3(ccz + dir.xzx, ccz + dir.yzx, ccz + dir.zzx + dir.zzx);
                                cs = point32plane(ps, ip, rd);
                                float a2 = circleHalfPlaneIntersectionArea(ip, r, cs);
                                dd = min(a1 + a2, dd);
                            }
                        }
                        float rr = 0.1*wwx;
                        //const bvec2 nearEnough = bvec2(true);
                        //lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(rr + rr + r));
                        if (any(nearEnough)) {
                            vec3 ppos;
                            float mindist = distance(ip.xz, vec2(0.0));
                            vec3 fpos = vec3(0.0);
                            if (NDIM == 19) {
                                ppos = vec3(6.0, 0.0, 6.0);
                                for (int i = -1; i <= 1; i++) {
                                    for (int j = -1; j <= 1; j++) {
                                        vec3 pos =vec3(wwx, 0.0, wwy)*vec3(i, 0, j)*ppos.zyz;
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
                                        vec3 pos = vec3(wwx, 0.0, wwy)*vec3(i, 0, j)*ppos.zyz;
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
                    n1 = vec3(0.0, 0.0, 1.0);
                    n0 = vec3(ip0.x, ip0.y, 0.0);
                }
                float dist0 = -dd*boardaa;

                bool edge = false;
                float tc = 0.0, tc2 = 0.0;

                vec3 ip2 = ro + tb2*rd;
                float xxx = dBox(ro, rd, ip, bnx.xyz, -bnx.xwz, n0);

                ipp.a = vec2(clamp(1.0-dd, 0.0, 1.0),1.0-xxx);

                ipp.d = -xxx*boardaa;
                ipp.pid = all( lessThan(abs(ip.xz), vec2(0.95, wwy/wwx * 0.95) )) ? idBlackStone : idBoard;
                ipp.oid = idBoard;
                ipp.n = n0;
                ipp.uvw = uvw;
                ipp.fog = 1.0;
                insert(ret, ipp);
            }
        }
    }
}

vec2 sBoard(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec2 ret = vec2(1.0);
    vec3 nor = ipp.n;

    vec3 ldir = -pos;
    float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
    vec3 ip = pos + res*ldir;

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

    ret.x *= mx;

    return ret;
}