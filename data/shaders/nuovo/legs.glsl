/*void rLegs(in vec3 ro, in vec3 rd, inout Intersection result[2], inout Intersection ret) {
    for (int i = 0; i < 4; i++){
        const float r = legh;
        vec3 cc = vec3(1 - 2 * (i & 1), 1, 1 - 2 * ((i >> 1) & 1))*(bnx.xyx + vec3(r, -0.5*r, r));
        vec2 ts = intersectionRaySphereR(ro, rd, cc, r*r);
        if (ts.x > 0.0 && ts.x < farClip) {
            ret.d = distanceRaySphere(ro, rd, cc, r);
            vec3 ip = ro + ts.x*rd;
            cc.y = bnx.y - legh;
            ret.t = ts;
            ret.m = idLeg1 + i;
            ret.p = ip;
            ret.n = normalize(ip - cc);
            updateResult(result, ret);
        }
    }
}*/

int rLegs(in vec3 ro, in vec3 rd, inout SortedLinkedList ret, bool shadow) {
    for (int i = 0; i < 4; i++){
        const float r = legh;
        vec3 cc = vec3(1 - 2 * (i & 1), 1, 1 - 2 * ((i >> 1) & 1))*(bnx.xyx + vec3(1.05*r, -0.5*r, 1.05*r));
        vec3 delta = vec3(0.0,0.0*0.45*r, 0.0);
        vec2 tt = intersectionRaySphereR(ro, rd, cc+delta, r*r);
        //vec2 ts2 = intersectionRaySphereR(ro, rd, cc-delta, r*r);
        //vec2 tt = ts;
        vec3 rcc = cc;//ts.x < ts2.x ? cc +delta: cc-delta;
        //tt.x = min(ts.x, ts2.x);
        //tt.y = max(ts.y, ts2.y);
        if (tt.x > 0.0 && tt.x < farClip) {
            IP ipp;
            ipp.t = vec3(tt, 0.0);
            int j = insert(ret, ipp);
            if(j < N) {
                ret.ip[j].d = distanceRaySphere(ro, rd, rcc, r);
                cc.y = bnx.y - legh;
                vec3 ip = ro + tt.x * rd;
                ret.ip[j].n = normalize(ip - cc);
                ret.ip[j].oid = idLeg1+i;
                ret.ip[j].uid = idLeg1+i;
                ret.ip[j].pid = idBoard;
                ret.ip[j].a = vec2(0.0);
                vec4 uvw = vec4(5.0*ip, 1.0);
                uvw = vec4(2.0*uvw.x/ww + ww*uvw.z, uvw.y, uvw.z, 1.0);
                ret.ip[j].uvw = uvw;
            }
            return j;
        }
    }
    return N;
}