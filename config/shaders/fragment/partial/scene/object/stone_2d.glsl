void rStoneY(
in vec3 ro, in vec3 rd, vec3 dd,
inout SortedLinkedList ret, inout IP rip, ivec2 oid, bool marked,
   int uid
) {
    bool rval = false;
    float t = dot(-ro,nBoard)/dot(rd, nBoard);
    if(t < farClip  && length(ro.xz + t*rd.xz - dd.xz) < r1) {
        rip.n = nBoard;
        rip.oid = oid.x;
        rip.pid = oid.y;
        rip.a = vec2(0.0);
        rip.uid = uid;
        rip.fog = 1.0;
        rip.t = vec3(t, t, 0.0);
        rval = true;
    }
    if(rval && rip.t.x > 0.0){
        if(try_insert(ret, rip)) {
            vec3 q2;
            rip.d = -distanceRayCircle(ro, rd, ro + t*rd, vec2(farClip), vec3(dd.x, 0.0, dd.z), r1, q2).y;
            dd.y = 0.0;
            finalizeStone(ro, rd, dd, rip, iStones[uid].w, marked);
            insert(ret, rip);
        }
    }
}