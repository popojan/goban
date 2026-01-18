bool IntersectBox(in vec3 ro, in vec3 rd, in vec3 minimum, in vec3 maximum, out float start, out float final) {
    vec3 ird = 1.0 / rd;
    vec3 OMIN = (minimum - ro) * ird;
    vec3 OMAX = (maximum - ro) * ird;
    vec3 MAX = max(OMAX, OMIN);
    vec3 MIN = min(OMAX, OMIN);
    final = min(MAX.x, min(MAX.y, MAX.z));
    start = max(MIN.x, max(MIN.y, MIN.z));
    return final > start;
}

float IntersectBox(in vec3 ro, in vec3 rd, in vec3 minimum, in vec3 maximum) {
    vec3 ird = 1.0 / rd;
    vec3 OMIN = (minimum - ro) * ird;
    vec3 OMAX = (maximum - ro) * ird;
    vec3 MAX = max(OMAX, OMIN);
    return min(MAX.x, min(MAX.y, MAX.z));
}

vec2 intersectionRaySphere(in vec3 ro, in vec3 rd, in vec3 center/*float radius2*/){
    vec3 vdif = ro - center;
    float dt = dot(rd, vdif);
    float x = dt*dt - dot(vdif, vdif) + stoneRadius2;
    vec2 ret;
    if (x >= 0.0) {
        float sqt = sqrt(x);
        ret = vec2(-dt - sqt, -dt + sqt);
    }
    else {
        ret = noIntersection2;
    }
    return ret;
}
vec2 intersectionRaySphereR(in vec3 ro, in vec3 rd, in vec3 center, const float radius2){
    vec3 vdif = ro - center;
    float dt = dot(rd, vdif);
    float x = dt*dt - dot(vdif, vdif) + radius2;
    vec2 ret;
    if (x >= 0.0) {
        float sqt = sqrt(x);
        ret = vec2(-dt - sqt, -dt + sqt);
    }
    else {
        ret = noIntersection2;
    }
    return ret;
}

vec2 intersectionRayCylinder(in vec3 ro, in vec3 rd, in vec3 center, const float radius2){
    vec2 rot = ro.xz - center.xz;
    float dt = dot(rd.xz, rot);
    float den = dot(rd.xz, rd.xz);
    float x = dt*dt - (dot(rot, rot) - radius2)*den;
    vec2 ret;
    if (x >= 0.0) {
        float sqt = sqrt(x);
        ret = vec2(-dt - sqt, -dt + sqt) / den;
    }
    else {
        ret = noIntersection2;
    }
    return ret;
}


vec4 intersectionRayEllipsoid(in vec3 ro, in vec3 rd, in vec3 center, out vec2 tt/*, float r1, float r2, float r3*/){
    vec3 vdif = ro - center;
    vec3 rdrrr = 2.0*rd*rrr;
    float dt = dot(vdif, rdrrr);
    float den = dot(rd, rdrrr);
    float x = dt*dt - 2.0*den*(dot(vdif*vdif, rrr) - r123r123);
    vec4 ret;
    if (x >= 0.0) {
        float sqt = sqrt(x);
        tt.xy = vec2(-dt - sqt, -dt + sqt) / den;
        float r2dr1 = r2 / r1;
        ret = vec4(normalize((ro + tt.x*rd - center)*vec3(r2dr1, r1 / r2, r2dr1)), tt.x);
    }
    else {
        ret = noIntersection4;
    }
    return ret;
}

bool intersectionRayStone(in vec3 ro, in vec3 rd, in vec3 dd, float mint, out vec4 res, out float t2){
    vec2 dpre = intersectionRaySphereR(ro, rd, dd, r1*r1);
    bool rval = false;
    if (dpre.x < mint && dpre.y > 0.0) { /*dd.y -  0.5*h*/
        vec2 d2 = intersectionRaySphere(ro, rd, dd - dn);
        float d2x = d2.x < 0.0 ? d2.y : d2.x;
        if (d2x < mint && d2x > 0.0) {
            vec2 d1 = intersectionRaySphere(ro, rd, dd + dn);
            float p1 = max(d1.x, d2.x);
            float p2 = min(d1.y, d2.y);
            if (p1 >= 0.0 && p1 <= p2 && p1 < mint) {
                vec2 d3 = intersectionRayCylinder(ro, rd, dd, px*px);
                vec2 d4;
                vec4 tt = intersectionRayEllipsoid(ro, rd, dd, d4);
                vec3 ip = ro + p1*rd;
                vec3 rcc = dd + (d1.x < d2.x ? -dn : dn);
                vec3 n = normalize(ip - rcc);
                if (p1 >= d3.x && p1 < d3.y && d3.x < farClip) {
                    res = vec4(n, p1);
                    t2 = p2;
                    rval = true;
                }
                else if (tt.x < farClip) {
                    n.y = mix(n.y, tt.y, smoothstep(px, r1, length(ip.xz - dd.xz)));
                    res = vec4(normalize(n), d4.x);
                    t2 = d4.y;
                    rval = true;
                }
                else {
                    rval = false;
                }
            }
        }
    }
    return rval;
}
