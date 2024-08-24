void rLegs(in vec3 ro, in vec3 rd, inout SortedLinkedList ret) {
    for (int i = 0; i < 4; i++){
        const float r = legh;
        vec3 cc = vec3(1 - 2 * (i & 1), 1, 1 - 2 * ((i >> 1) & 1))*(bnx.xyz + vec3(r, -0.5*r, r));
        vec3 delta = vec3(0.0,0.0*0.45*r, 0.0);
        vec2 tt = intersectionRaySphereR(ro, rd, cc+delta, r*r);

        vec3 rcc = cc;

        if (tt.x > 0.0 && tt.x < farClip) {
            IP ipp;
            ipp.t = vec3(tt, 0.0);
            if(try_insert(ret, ipp)) {
                ipp.d = distanceRaySphere(ro, rd, rcc, r);
                cc.y = bnx.y - legh;
                vec3 ip = ro + tt.x * rd;
                ipp.n = normalize(ip - cc);
                ipp.oid = idLeg1+i;
                ipp.uid = idLeg1+i;
                ipp.pid = idBoard;
                ipp.a = vec2(0.0);
                vec4 uvw = vec4(ip, 1.0);
                ipp.uvw = uvw;
                ipp.fog = 1.0;
                insert(ret, ipp);
            }
        }
    }
}


vec2 sLegs(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec2 ret = vec2(1.0);
    if (ipp.oid == idTable) {
        for (int i = 0; i < 4; i++){
            const float r = legh;
            vec3 cc = vec3(1 - 2 * (i & 1), 1, 1 - 2 * ((i >> 1) & 1))*(bnx.xyz + vec3(r, -0.5*r, r));
            float t1, t2;
            vec3 ld = lig - pos;
            ret *= sCircle(cc, r, pos, lig, ldia2, ipp);
        }
    }
    return ret;
}
