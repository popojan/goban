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
            dd.y -= dn.y - sqrt(stoneRadius2 - 0.25*r1*r1);
            finalizeStone(ro, rd, dd, rip, rip.isInCup > 0 ? ddc[uid].w : iStones[uid].w, marked);
            insert(ret, rip);
        }
    }
}