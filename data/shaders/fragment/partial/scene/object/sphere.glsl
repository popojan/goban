float dSphere(in vec3 ro, in vec3 rd, in vec3 dd, float radius2){
    vec3 u = cross(dd - ro, dd - ro - rd);
    vec3 v = normalize(cross(rd, u));
    float dist = length(u) / length(rd) - radius2;
    vec3 x0 = dd + length(u)*v;
    return dist / distance(x0, ro);
}

void rSphere(in vec3 ro, in vec3 rd, in vec3 center, const float radius2, inout SortedLinkedList ret, int oid){
    vec3 vdif = ro - center;
    float dt = dot(rd, vdif);
    float x = dt*dt - dot(vdif, vdif) + radius2;
    IP ip;
    if (x >= 0.0) {
        float sqt = sqrt(x);
        vec3 t = vec3(-dt-sqt, -dt+sqt, 0.0);
        if(t.x > 0.0) {
            ip.t = t;
            if(try_insert(ret, ip)) {
                ip.n = normalize(ro + t.x * rd - center);
                ip.d = dSphere(ro, rd, center, sqrt(radius2));
                ip.oid = oid;
                ip.fog = 1.0;
                insert(ret, ip);
            }
        }
    }
}
vec2 sCircle(in vec3 center, float r1, in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec3 nor = ipp.n;
    vec3 rd = lig - pos;
    vec2 ret = vec2(1.0);
    vec2 xz = center.xz;
    float avoid = smoothstep(center.y-0.01, center.y, 0.99*pos.y);

    if(center.y > pos.y) {
        vec3 ddpos = center.xyz - pos;
        float ln = length(ddpos);
        vec3 ldir = ddpos;
        float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
        vec3 ip = pos + res*ldir;
        float ldia = sqrt(ldia2);
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
        ret.x *= clamp(1.0 - A / (PI*ldia2), 0.0, 1.0);
    }
    return mix(ret, vec2(1.0), avoid);
}
