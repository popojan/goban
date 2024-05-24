vec3 point2plane(vec3 p, vec3 o, vec3 n){
    return p - dot(p - o, n)*n;
}
mat3 point32plane(mat3 p, vec3 o, vec3 n){
    vec3 dt = n*(p - mat3(o, o, o));
    return p - mat3(dt.x*n, dt.y*n, dt.z*n);
}

vec3 point2line(vec3 p, vec3 o, vec3 u){
    vec3 v = p - o;
    return o + dot(u, v) * u / dot(v, v);
}
float circleHalfPlaneIntersectionArea(vec3 c, float r, mat3 cs) {
    vec3 p = cs[0];
    vec3 v = cs[1] - cs[0];
    vec3 n = cross(cross(v, c - p), v);
    bool isInside = sign(dot(c - p, n)) == sign(dot(cs[2] - p, n));
    float h = r - length(cross(p - c, p + v - c)) / length(v);
    float ret;
    if (h <= 0.0) {
        ret = isInside ? 1.0 : 0.0;
    }
    else {
        float A = acos((r - h) / r) / PI - (r - h)*sqrt(2.0*h*r - h*h) / (PI*r*r);
        ret = isInside ? 1.0 - A : A;
    }
    return ret;
}

vec3 distanceRayStoneSphere(in vec3 ro, in vec3 rd, in vec3 dd, float radius2){
    vec3 p = ro + dot(dd - ro, rd) * rd;
    float dist = length(p - dd) - radius2;
    vec3 q = dd + radius2*normalize(p - dd);
    float d = length(p.xz - dd.xz);
    return vec3(d - abs(p.y - dd.y)*px / abs(radius2 - 0.5*h + 0.5*b), dist, distance(p, ro));
}

float distanceRaySphere(in vec3 ro, in vec3 rd, in vec3 dd, float radius2){
    vec3 u = cross(dd - ro, dd - ro - rd);
    vec3 v = normalize(cross(rd, u));
    float dist = length(u) / length(rd) - radius2;
    vec3 x0 = dd + length(u)*v;
    return dist / distance(x0, ro);
}

float distanceLineLine(vec3 ro, vec3 rd, vec3 p0, vec3 v, out float s, out vec3 q2) {
    vec3 p21 = p0 - ro;
    vec3 m = cross(v, rd);
    float m2 = dot(m, m);
    vec3 rr = cross(p21, m / m2);
    s = dot(rr, v);
    vec3 q1 = ro + s * rd;
    q2 = p0 + dot(rr, rd) * v;
    s = dot(rr, rd);
    return -distance(q1, q2) / distance(ro, q1);
}

vec2 distanceRayCircle(in vec3 ro, in vec3 rd, in vec3 ip, in vec2 t0, in vec3 dd, float r, out vec3 q2){
    vec3 dif = vec3(ip.x, dd.y, ip.z) - dd;
    vec3 x0 = dd + r*normalize(dif);
    float s = 0.0;
    return vec2(abs(t0.x - s) - abs(t0.y - s), sign(length(dif) - r) * distanceLineLine(ro, rd, x0, normalize(vec3(dd.z - ip.z, 0.0, ip.x - dd.x)), s, q2));
}

vec2 distanceRayStone(in vec3 ro, in vec3 rd, in vec3 dd){
    vec3 R = vec3(r1, r2, r1);
    vec3 A = ro - dd;
    vec3 B = A + rd;
    vec3 v = A - B;
    vec3 rr = R.yzx * R.zxy;
    float den = dot(v*v, rr*rr);
    vec3 RR = R*R;
    vec3 BB = B*B;
    vec3 xyz0 = RR*vec3(
        dot(RR.yz, A.xx*BB.zy + A.zy*(B.xx*(A.zy - B.zy) - A.xx*B.zy)),
        dot(RR.zx, A.yy*BB.xz + A.xz*(B.yy*(A.xz - B.xz) - A.yy*B.xz)),
        dot(RR.xy, A.zz*BB.yx + A.yx*(B.zz*(A.yx - B.yx) - A.zz*B.yx))
        );
    vec3 X0 = xyz0 / den;
    vec3 ro2 = X0 / R;
    vec3 rd2 = -normalize(ro2 / (R*R));
    float dt = dot(rd2, ro2);
    float sqt = sqrt(max(0.0, dt*dt - dot(ro2, ro2) + 1.0));
    float dd0 = sign(-dt-sqt)*length((dt + sqt)*R*rd2);///*rd2  dot(rd,X0-ro);
    return vec2(dd0, distance(ro, X0 + dd));
}

/*
float distanceRaySquare(in vec3 ro, in vec3 rd, in vec3 ip, in vec3 bmin, in vec3 bmax, out vec3 q2) {
    float t; vec3 tt;
    vec3 a = vec3(bmin.x, bmax.y, bmin.z);
    vec3 b = vec3(bmin.x, bmin.y, bmin.z);
    vec3 c = vec3(bmin.x, bmin.y, bmax.z);
    vec3 d = vec3(bmin.x, bmax.y, bmax.z);

    vec3 e = vec3(bmax.x, bmax.y, bmin.z);
    vec3 f = vec3(bmax.x, bmin.y, bmin.z);
    vec3 g = vec3(bmax.x, bmin.y, bmax.z);
    vec3 h = vec3(bmax.x, bmax.y, bmax.z);

    float ret = farClip;
    float d1 = distanceLineLine(ro, rd, a, b-a, t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, b, (c-b), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, c, (d-c), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, d, (a-d), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    float ret2 = ret;

    ret = farClip;
    d1 = distanceLineLine(ro, rd, e, (f-e), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, f, (g-f), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, g, (h-g), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, h, (e-h), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    ret2 = max(-ret2, -ret);

    ret = farClip;
    d1 = distanceLineLine(ro, rd, a, (e-a), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, b, (f-b), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, c, (g-c), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);
    d1 = distanceLineLine(ro, rd, d, (h-d), t, tt);
    if(t >= 0.0 &&  t <= 1.0 ) ret = min(ret, -d1);

    return ret2;
}*/

float distanceRaySquare(in vec3 ro, in vec3 rd, in vec3 ip, in vec3 bmin, in vec3 bmax, out vec3 q2) {
    vec3 c = 0.5*(bmin + bmax);
    vec3 r = 0.5*abs(bmax - bmin);
    vec3 aip = abs(abs(ip - c)-r);
    if(r.y == 0.0) aip.y = 1.0;
    vec3 p[4];
    float mmin = min(aip.x, min(aip.y, aip.z));
    if(r.y > 0.0 && mmin == aip.z) {
        p[0] = vec3(c.x - r.x, c.y - r.y, ip.z);
        p[1] = vec3(c.x - r.x, c.y + r.y, ip.z);
        p[2] = vec3(c.x + r.x, c.y + r.y, ip.z);
        p[3] = vec3(c.x + r.x, c.y - r.y, ip.z);
    }
    else if(r.y == 0.0 || mmin == aip.y) {
        p[0] = vec3(c.x - r.x, ip.y, c.z - r.z);
        p[1] = vec3(c.x - r.x, ip.y, c.z + r.z);
        p[2] = vec3(c.x + r.x, ip.y, c.z + r.z);
        p[3] = vec3(c.x + r.x, ip.y, c.z - r.z);
    }
    else {
        p[0] = vec3(ip.x, c.y - r.y, c.z - r.z);
        p[1] = vec3(ip.x, c.y - r.y, c.z + r.z);
        p[2] = vec3(ip.x, c.y + r.y, c.z + r.z);
        p[3] = vec3(ip.x, c.y + r.y, c.z - r.z);
    }

    float dist = farClip;
    vec3 lastp = p[3];
    vec3 p0, v;
    float ret;
    for(int i = 0; i < 4; i++){
        vec3 cc = cross(rd, p[i] - lastp);
        float dist0 = length(dot(ro-lastp,cc))/length(cc);
        if(dist0 < dist) {
            p0 = lastp;
            v = p[i] - lastp;
            dist = dist0;
        }
        lastp = p[i];
    }
    vec3 p21 = p0 - ro;
    vec3 m = cross(v, rd);
    float m2 = dot(m, m);
    vec3 rr = cross(p21, m/m2);
    float s = dot(rr, v);
    float t = dot(rr, rd);
    vec3 q1 = ro + s * rd;
    q2 = p0 + t * v;
    ret = -distance(q1, q2)/distance(ro, q1);
    return ret;
}
