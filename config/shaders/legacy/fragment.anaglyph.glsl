#version 330 core

/* === DO NOT CHANGE BELOW === */
const int MAXSTONES = 19 * 19;
const float cidBlackArea = 2.0;
const float cidWhiteArea = 3.0;
const float cidCapturedBlackStone = 5.5;
const float cidLastBlackStone = 5.75;
const float cidBlackStone = 5.0;
const float cidWhiteStone = 6.0;
const float cidCapturedWhiteStone = 6.5;
const float cidLastWhiteStone = 6.75;

in vec3 rdbl;
in vec3 rdbr;
flat in vec3 rool;
flat in vec3 roor;
in float noise;
out vec3 glFragColor;

uniform int NDIM;
uniform vec2 iResolution;
uniform int iStoneCount;
uniform int iBlackCapturedCount;
uniform int iWhiteCapturedCount;
uniform int iBlackReservoirCount;
uniform int iWhiteReservoirCount;
uniform float iTime;
layout(std140) uniform iStoneBlock{
    vec4 iStones[MAXSTONES];
};
uniform mat4 glModelViewMatrix;
uniform vec3 iTranslate;

/* === DO NOT CHANGE ABOVE == */


uniform float gamma;
uniform float contrast;
uniform float fNDIM;
uniform float boardaa;
uniform float boardbb;
uniform float boardcc;
uniform float wwx;
uniform float wwy;
uniform float w;
uniform float h;
uniform float stoneRadius;
uniform float d;
uniform float stoneRadius2;
uniform vec3 dn;
uniform float b;
uniform float y;
uniform float px;
uniform float pxs;
uniform float r1;
uniform float r2;
uniform float r123r123;
uniform vec3 rrr;
uniform float r1r1ir2ir2;
uniform vec3 maxBound;
uniform float dw;
//const float dw = 0.00075;
uniform float iscale;
uniform float bowlRadius;
uniform float bowlRadius2;

const float PI = 3.1415926535;
const float farClip = 10000.0;
const vec2 noIntersection2 = vec2(farClip, farClip);
const vec4 noIntersection4 = vec4(farClip);
const vec3 lpos = vec3(-2.0, 12.0, -2.0);
const vec3 lpos2 = vec3(-4.0, 6.0, -2.0);
const vec3 lpos3 = vec3(-12.0, 6.0, 8.0);
const vec3 lpos4 = vec3(1.0, 2.0, 4.0);
const vec3 lposA = vec3(6.0, -1.0, 6.0);
const vec3 lposB = vec3(-6.0, -1.0, 6.0);
const vec3 lposC = vec3(-6.0, -1.0, -6.0);
const vec3 lposD = vec3(6.0, -1.0, -6.0);
const float ldia = 3.5;
const vec3 nBoard = vec3(0.0, 1.0, 0.0);
const vec3 minBound = vec3(-1.2, -0.02, -1.2);
const vec3 p = vec3(0.0, -0.25, 0.0);
vec3 c;
vec4 bnx;
const float mw = 0.85;
const float legh = 0.15;

struct Material {
    int id;
    vec3 diffuseAmbientSpecularWeight;
    vec3 specularPower;
    vec3 clrA;
    vec3 clrB;
    vec3 clrC;
    float refl;
};

struct Intersection {
    vec2 t;
    vec3 p;
    vec3 n;
    int m;
    float d;
    float d2;
    vec4 dummy;
    bool isBoard;
    int uid;
    int pid;
};
//const float gamma = 1.0;
const int idBack = -1;
const int idTable = 0;
const int idBoard = 1;
const int idBlackStone = 2;
const int idWhiteStone = 3;
const int idBlackArea = 4;
const int idWhiteArea = 5;
const int idLeg1 = 6;
const int idLeg2 = 7;
const int idLeg3 = 8;
const int idLeg4 = 9;
const int idGrid = 10;
const int idCapturedBlackStone = 11;
const int idCapturedWhiteStone = 12;
const int idLastBlackStone = 13;
const int idLastWhiteStone = 14;
const int idLidBlack = 15;
const int idCupBlack = 16;
const int idLidWhite = 17;
const int idCupWhite = 18;
const int idBowlBlackStone = 19;
const int idBowlWhiteStone = 20;

const vec3 bgA = vec3(0.5);
const vec3 bgB = vec3(0.5);

const Material mCup = Material(idCupBlack, vec3(0.7, 0.15, 0.25), vec3(32.0), vec3(0.183333, 0.113725, 0.028039), vec3(0.73333,0.4137,0.1829039), vec3(0.07333,0.0613725,0.089039), 1.5);
const Material mBoard = Material(idBoard, vec3(0.7, 0.15, 0.05), vec3(42.0), vec3(0.93333, 0.813725, 0.38039), vec3(0.53333,0.413725,0.09039), vec3(0.7333,0.613725,0.19039), 1.5);
const Material mTable = Material(idTable, vec3(1.0, 0.15, 0.0), vec3(4.0), vec3(0.566,0.0196,0.0176), vec3(0.766,0.1196,0.1176), vec3(0.666,0.0196,0.0176), 0.0);
const Material mWhite = Material(idWhiteStone, vec3(0.53, 0.53, 0.14), vec3(32.0, 41.0, 8.0), vec3(0.98), vec3(0.98,0.95,0.97), vec3(0.96), 0.5);
const Material mBlack = Material(idBlackStone, vec3(1.0, 0.25, 0.15), vec3(28.0), vec3(0.08), vec3(0.04), vec3(0.10), 0.5);
const Material mRed = Material(idLastBlackStone, vec3(0.3, 0.15, 0.25), vec3(4.0), vec3(0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0), 0.0);
const Material mBack = Material(idBack, vec3(0.0, 1.0, 0.0), vec3(1.0), bgA, bgB, bgA, 0.0);
const Material mGrid  = Material(idGrid, vec3(1.5, 0.15, 0.15), vec3(42.0), vec3(0.0),vec3(0.0), vec3(0.0), 0.0);

uniform vec3 cc[4];
const int maxCaptured = 91;
uniform vec4 ddc[2 * maxCaptured];

bool IntersectBox(in vec3 ro, in vec3 rd, in vec3 minimum, in vec3 maximum, out float start, out float final) {
    vec3 ird = 1.0 / rd;
    vec3 OMIN = (minimum - ro) * ird;
    vec3 OMAX = (maximum - ro) * ird;
    vec3 MAX = max(OMAX, OMIN);
    vec3 MIN = min(OMAX, OMIN);
    final = min(MAX.x, min(MAX.y, MAX.z));
    start = max(max(MIN.x, 0.0), max(MIN.y, MIN.z));
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
    return -distance(q1, q2) / distance(ro, q1);
}

vec2 distanceRayCircle(in vec3 ro, in vec3 rd, in vec3 ip, in vec2 t0, in vec3 dd, float r, out vec3 q2){
    vec3 dif = vec3(ip.x, dd.y, ip.z) - dd;
    vec3 x0 = dd + r*normalize(dif);
    float s = 0.0;
    return vec2(abs(t0.x - s) - abs(t0.y - s), sign(length(dif) - r)
    * distanceLineLine(ro, rd, x0, normalize(vec3(dd.z - ip.z, 0.0, ip.x - dd.x)), s, q2));
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

float distanceRaySquare(in vec3 ro, in vec3 rd, in vec3 ip, in vec3 bmin, in vec3 bmax, out vec3 q2) {
    vec3 c = 0.5*(bmin + bmax);
    vec3 r = 0.5*abs(bmax - bmin);
    vec3 aip = abs(abs(ip - c) - r);
    if (r.y == 0.0) aip.y = 1.0;
    vec3 p[4];
    float mmin = min(aip.x, min(aip.y, aip.z));
    if (r.y > 0.0 && mmin == aip.z) {
        p[0] = vec3(c.x - r.x, c.y - r.y, ip.z);
        p[1] = vec3(c.x - r.x, c.y + r.y, ip.z);
        p[2] = vec3(c.x + r.x, c.y + r.y, ip.z);
        p[3] = vec3(c.x + r.x, c.y - r.y, ip.z);
    }
    else if (r.y == 0.0 || mmin == aip.y) {
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
    for (int i = 0; i < 4; i++){
        vec3 cc = cross(rd, p[i] - lastp);
        float dist0 = length(dot(ro - lastp, cc)) / length(cc);
        if (dist0 < dist) {
            p0 = lastp;
            v = p[i] - lastp;
            dist = dist0;
        }
        lastp = p[i];
    }
    vec3 p21 = p0 - ro;
    vec3 m = cross(v, rd);
    float m2 = dot(m, m);
    vec3 rr = cross(p21, m / m2);
    float s = dot(rr, v);
    float t = dot(rr, rd);
    vec3 q1 = ro + s * rd;
    q2 = p0 + t * v;
    ret = -distance(q1, q2) / distance(ro, q1);
    return ret;
}

bool intersectionRayStone(in vec3 ro, in vec3 rd, in vec3 dd, out vec4 res, out float t2){
    vec2 dpre = intersectionRaySphereR(ro, rd, dd, r1*r1);
    bool rval = false;
    if (dpre.x < farClip && dpre.y > 0.0) { /*dd.y -  0.5*h*/
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
                vec3 n = normalize(ip - rcc);
                if (p1 >= d3.x && p1 < d3.y && d3.x < farClip) {
                    res = vec4(n, p1);
                    t2 = p1;
                    rval = true;
                }
                else if (tt.x < farClip) {
                    n.y = mix(n.y, tt.y, smoothstep(px, r1, length(ip.xz - dd.xz)));
                    res = vec4(normalize(n), d4.x);
                    t2 = d4.x;
                    rval = true;
                }
            }
        }
    }
    return rval;
}

// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash(uint x) {
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}
uint hash(uvec2 v) { return hash(v.x ^ hash(v.y)); }
uint hash(uvec3 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z)); }
uint hash(uvec4 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w)); }


// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat(m);       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

vec3 mod289(vec3 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
    return mod289(((x*34.0)+1.0)*x);
}

vec4 taylorInvSqrt(vec4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v, out vec3 gradient)
{
    const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
    const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

    // First corner
    vec3 i  = floor(v + dot(v, C.yyy) );
    vec3 x0 =   v - i + dot(i, C.xxx) ;

    // Other corners
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min( g.xyz, l.zxy );
    vec3 i2 = max( g.xyz, l.zxy );

    //   x0 = x0 - 0.0 + 0.0 * C.xxx;
    //   x1 = x0 - i1  + 1.0 * C.xxx;
    //   x2 = x0 - i2  + 2.0 * C.xxx;
    //   x3 = x0 - 1.0 + 3.0 * C.xxx;
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
    vec3 x3 = x0 - D.yyy;      // -1.0+3.0*C.x = -0.5 = -D.y

    // Permutations
    i = mod289(i);
    vec4 p = permute( permute( permute(
    i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
    + i.y + vec4(0.0, i1.y, i2.y, 1.0 ))
    + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

    // Gradients: 7x7 points over a square, mapped onto an octahedron.
    // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    float n_ = 0.142857142857; // 1.0/7.0
    vec3  ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  //  mod(p,7*7)

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

    vec4 x = x_ *ns.x + ns.yyyy;
    vec4 y = y_ *ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4( x.xy, y.xy );
    vec4 b1 = vec4( x.zw, y.zw );

    //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
    //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
    vec4 s0 = floor(b0)*2.0 + 1.0;
    vec4 s1 = floor(b1)*2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
    vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

    vec3 p0 = vec3(a0.xy,h.x);
    vec3 p1 = vec3(a0.zw,h.y);
    vec3 p2 = vec3(a1.xy,h.z);
    vec3 p3 = vec3(a1.zw,h.w);

    //Normalise gradients
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix final noise value
    vec4 m = max(0.5 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    vec4 m2 = m * m;
    vec4 m4 = m2 * m2;
    vec4 pdotx = vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3));

    // Determine noise gradient
    vec4 temp = m2 * m * pdotx;
    gradient = -8.0 * (temp.x * x0 + temp.y * x1 + temp.z * x2 + temp.w * x3);
    gradient += m4.x * p0 + m4.y * p1 + m4.z * p2 + m4.w * p3;
    gradient *= 105.0;

    return 105.0 * dot(m4, pdotx);
}


void updateResult(inout Intersection result0, inout Intersection result1, inout Intersection result2, in Intersection ret) {
    if (ret.t.x <= result0.t.x) {
        result2 = result1;
        result1 = result0;
        result0 = ret;
    }
    else if (ret.t.x <= result1.t.x) {
        result2 = result1;
        result1 = ret;
    }
    else if (ret.t.x <= result2.t.x) {
        result2 = ret;
    }
}

vec2 csg_union(in vec2 a, in vec2 b) {
    if(b == noIntersection2)
    return a;
    if(a == noIntersection2)
    return b;
    return vec2(min(a.x, b.x), max(a.y, b.y));
}

vec2 csg_intersection(in vec2 a, in vec2 b) {
    if(b == noIntersection2)
    return b;
    if(a == noIntersection2)
    return a;
    return vec2(max(a.x, b.x), min(a.y, b.y));
}

vec2 csg_difference(in vec2 a, in vec2 b) {
    if(b == noIntersection2)
    return a;
    if(a == noIntersection2)
    return a;
    if(b.x > a.x)
    return vec2(a.x, min(b.x, a.y));
    return vec2(max(b.y, a.x), max(b.y, a.y));
}

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

void finalizeStone(in vec3 ro, in vec3 rd, inout Intersection result0, inout Intersection result00, inout Intersection result1, inout Intersection result2) {
    vec3 dd = result0.dummy.xyz;
    bvec3 isStone = bvec3(result0.m == idBlackStone || result0.m == idWhiteStone
    || result0.m == idBowlBlackStone || result0.m == idBowlWhiteStone,
    result0.m == idCapturedBlackStone || result0.m == idCapturedWhiteStone,
    result0.m == idLastBlackStone || result0.m == idLastWhiteStone);
    vec3 dist0a;
    vec3 ip = (ro + result0.t.x * rd);
    /*if (any(isStone)) {
        vec3 dist0a = distanceRayStoneSphere(ro, rd, dd.xyz - dn, stoneRadius);
        vec3 dist0b = distanceRayStoneSphere(ro, rd, dd.xyz + dn, stoneRadius);
        dist0a = dist0a.y > dist0b.y ? dist0a : dist0b;
        dist0b.yz = distanceRayStone(ro, rd, dd.xyz);
        //result0.d = distance(ip.xz, dd.xz) > px ? dist0b.y / dist0b.z : dist0a.y / dist0a.z;
        result0.d = mix(dist0a.y / dist0a.z, dist0b.y / dist0b.z, step(0.0, dist0a.x));
    }*/
    bool up = false;
    Intersection r2 = result0;
    if (any(isStone.yz)) {
        int idStoneInside = result0.m;
        int idStone = idStoneInside;
        if (idStone == idCapturedBlackStone){
            idStone = idBlackStone;
            idStoneInside = idWhiteStone;
        }
        else if (idStone == idCapturedWhiteStone){
            idStone = idWhiteStone;
            idStoneInside = idBlackStone;
        }
        else if (idStone == idLastBlackStone){
            idStone = idBlackStone;
            idStoneInside = idLastBlackStone;
        }
        else if (idStone == idLastWhiteStone){
            idStone = idWhiteStone;
            idStoneInside = idLastWhiteStone;
        }
        result0.m = idStoneInside;

        vec3 cc = dd;
        cc.y -= dn.y - sqrt(stoneRadius2 - 0.25*r1*r1);
        vec3 ip0 = ro + rd*dot(cc - ro, nBoard) / dot(rd, nBoard);
        vec3 q2;

        bool isNotArea = length(result0.p.xz - cc.xz) > 0.5*r1 || ip.y < dd.y;
        if (isNotArea) {
            vec2 rr = distanceRayCircle(ro, rd, ip0, result0.t, cc, 0.5*r1, q2);
            //r2.t += 0.00001;//TODO
            float d = abs(rr.y);
            //r2.t -= 0.0001;
            if (sign(ro.y - dd.y) * rr.x <= 0.0 && d < boardaa){
                r2.d = -min(d, -r2.d);
                r2.m = idStone;
                up = true;
            }
            else{
                result0.m = idStone;
            }
        }
    }
    updateResult(result00, result1, result2, result0);
    if(up) { updateResult(result00, result1, result2, r2); }
}
void castRay(in vec3 ro, in vec3 rd, inout Intersection result0, inout Intersection result1, inout Intersection result2) {

    Intersection ret;
    ret.m = idBack;
    ret.d = -farClip;
    ret.n = nBoard;
    ret.t = vec2(farClip);
    ret.p = vec3(-farClip);
    ret.dummy = vec4(0.0);
    ret.isBoard = false;
    ret.pid = 0;
    ret.uid = 0;
    result0 = result1 = result2 = ret;

    float tb, tb2;
    vec3 q2;
    bool isBoard = IntersectBox(ro, rd, bnx.xyz, -bnx.xwz, tb, tb2);
    bool isBoardTop = false;
    if (isBoard) {
        vec3 ip = ro + tb*rd;
        vec3 n0, n1;
        vec3 cc = vec3(0.0, 0.5*bnx.y, 0.0);
        vec3 ip0 = ip - cc;
        vec3 bn0 = bnx.xyz - cc;
        vec3 dif = (abs(bn0) - abs(ip0)) / abs(bn0);
        float dist0 = distanceRaySquare(ro, rd, ip, bnx.xyz, -bnx.xwz, q2);
        float dd = farClip;
        bool extremeTilt = false;

        if (all(lessThan(dif.xx, dif.yz))) {
            n1 = vec3(sign(ip0.x), 0.0, 0.0);
            n0 = vec3(0.0, ip0.y, ip0.z);
        }
        else if (all(lessThan(dif.yy, dif.xz))){

            n1 = vec3(0.0, sign(ip0.y), 0.0);
            n0 = vec3(ip0.x, 0.0, ip0.z);
            //const float dw = 0.00075;
            bool gridx = ip.x > -c.x + mw*wwx - 0.5*wwx && ip.x < c.x - mw*wwx + 0.5*wwx && ip.z > -c.z + mw*wwy - dw && ip.z < c.z - mw*wwy + dw;
            bool gridz = ip.x > -c.x + mw*wwx - dw && ip.x < c.x - mw*wwx + dw && ip.z > -c.z + mw*wwy - 0.5*wwy && ip.z < c.z - mw*wwy + 0.5*wwy;
            float r = boardbb*distance(ro, ip);
            if (ip0.y > 0.0) {
                isBoardTop = true;
                bvec2 nearEnough = lessThan(abs(ip.xz - vec2(wwx, wwy)*round(ip.xz/vec2(wwx, wwy))), vec2(dw + dw + r));
                bvec2 farEnough = bvec2(true, true);// greaterThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(0.33*ww));
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
                        //n1 = normalize(vec3(-0.01*sign(ip.x - ccx.x)*(1.0-abs(ip.x - (ccx.x + sign(ip.x - ccx.x)*dw))/dw), n1.y, n1.z));//mix(0.5*abs(ccx.x -ip.x)/dw, 0.0, 0.5*abs(ccx.x -ip.x)/dw)));
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
                        //n1 = normalize(vec3(n1.x,n1.y,-0.01*sign(ip.z - ccz.z)*(1.0-abs(ip.z - (ccz.z + sign(ip.z - ccz.z)*dw))/dw)));//mix(0.5*abs(ccz.z -ip.z)/dw, 0.0, 0.5*abs(ccz.z -ip.z)/dw)));
                    }
                }
                float rr = 0.1*wwx;//8.0*dw;
                nearEnough = lessThan(abs(ip.xz - vec2(wwx, wwy)*round(ip.xz/vec2(wwx, wwy))), vec2(rr + rr + r));
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
            n1 = vec3(0.0, 0.0, sign(ip0.z));
            n0 = vec3(ip0.x, ip0.y, 0.0);
        }
        ret.t = vec2(tb,tb);
        ret.m = idBoard;
        ret.d = -farClip;
        ret.d2 = -farClip;
        ret.p = ip;
        ret.isBoard = isBoardTop;
        bool edge = false;
        float xxx = dBox(ro, rd, ip, bnx.xyz, -bnx.xwz, n0);
        if (dist0 > -boardaa) {
            float tc = 0.0, tc2 = 0.0;
            IntersectBox(ro, q2 - ro, bnx.xyz, -bnx.xwz, tc, tc2);
            if (tc2 - tc > 0.001) {
                n0 = normalize(n0);
                vec3 cdist = abs(abs(ip0) - abs(bn0));
                vec2 ab = clamp(vec2(abs(dist0), min(cdist.z, min(cdist.x, cdist.y))) / boardcc, 0.0, 1.0);
                //n0 = normalize(mix(normalize(ip0), n0, ab.y));
                //n0 = normalize(mix(n0, n1, ab.x));
                ret.n = n0;
                ret.d = -farClip;
                ret.d2 = -farClip;
                ret.p = q2;
                edge = true;
                //updateResult(result0, result1, result2, ret);
            }
        }
        ret.n = n0;
        ret.p = ip;
        bool upit = false;
        ret.m = dist0 > -dd*boardaa  ? idBoard : idGrid;
        ret.d = -xxx*boardaa;
        updateResult(result0, result1, result2, ret);
        ret.d = max(dist0, -dd*boardaa);
        //dist0 = dist0;
        ret.m = idBoard;
        //ret.t.x -= 0.0001;//TODO
        updateResult(result0, result1, result2, ret);
        ret.isBoard = false;
        ret.d = -farClip;
    }
    float t1, t2;
    if (IntersectBox(ro, rd, minBound, maxBound, t1, t2)) {
        vec4 b12 = ro.xzxz + vec4(t1, t1, t2, t2)*rd.xzxz;
        vec2 noise = vec2(wwx*0.2);
        vec4 bmnx = vec4(min(b12.xy, b12.zw) - noise, max(b12.xy, b12.zw) + noise);
        vec4 xz12 = floor(bmnx/vec4(wwx, wwy, wwx, wwy) + vec4(0.5*fNDIM));
        ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
        for (int i = mnx.y; i <= mnx.w; i++){
            for (int j = mnx.x; j <= mnx.z; j++){
                vec4 stone0 = iStones[NDIM*i + j];
                int mm0 = idBack;
                float m0 = stone0.z;
                if (m0 >= cidBlackStone) {
                    vec2 xz = vec2(wwx, wwy)*stone0.xy;
                    vec3 dd = vec3(xz.x, 0.5*h, xz.y);
                    float tt2;
                    vec4 ret0;
                    if (intersectionRayStone(ro, rd, dd, ret0, tt2)) {
                        //vec3 w25 = vec3(0.25*w, h*vec2(-0.5, 0.6));
                        //vec3 minb = dd - w25.xyx;;
                        //vec3 maxb = dd + w25.xzx;
                        vec3 ip = ro + rd*ret0.w;
                        bool captured = m0 == cidCapturedBlackStone || m0 == cidCapturedWhiteStone;
                        bool last = m0 == cidLastBlackStone || m0 == cidLastWhiteStone;
                        if ((captured || last)) {
                            mm0 = captured ? (m0 == cidCapturedWhiteStone ? idCapturedWhiteStone : idCapturedBlackStone) :
                            (m0 == cidLastWhiteStone ? idLastWhiteStone : idLastBlackStone);
                        }
                        /*else if(captured || last) {
                        mm0 = (m0 == cidCapturedWhiteStone ||m0 == cidLastWhiteStone) ? idWhiteStone : idBlackStone;
                        }*/
                        else {
                            mm0 = m0 == cidBlackStone ? idBlackStone : idWhiteStone;
                        }
                        ret.dummy = vec4(dd, stone0.w);
                        ret.t = vec2(tt2);
                        ret.m = mm0;
                        ret.d = -farClip;
                        ret.n = ret0.xyz;
                        ret.p = ip;
                        //TODO
                        vec3 dist0a = distanceRayStoneSphere(ro, rd, dd.xyz - dn, stoneRadius);
                        vec3 dist0b = distanceRayStoneSphere(ro, rd, dd.xyz + dn, stoneRadius);
                        dist0a = dist0a.y > dist0b.y ? dist0a : dist0b;
                        dist0b.yz = distanceRayStone(ro, rd, dd.xyz);
                        ret.d = mix(dist0a.y / dist0a.z, dist0b.y / dist0b.z, step(0.0, dist0a.x));

                    }
                }
                else if (isBoardTop && (m0 == cidWhiteArea || m0 == cidBlackArea)) {
                    vec3 dd = vec3(vec2(j, i) - vec2(0.5*fNDIM - 0.5), 0.0)*vec3(wwx, wwy, 0.0);
                    vec2 w25 = vec2(0.25*wwx, dd.z);
                    vec3 minb = dd.xzy - w25.xyx;
                    vec3 maxb = dd.xzy + w25.xyx;
                    vec3 ip = ro + rd*tb;
                    if (all(equal(clamp(ip.xz, minb.xz, maxb.xz), ip.xz))) {
                        mm0 = stone0.z == cidBlackArea ? idBlackArea : idWhiteArea;
                        ret.m = mm0;
                        ret.t = vec2(tb);
                        ret.d = distanceRaySquare(ro, rd, ip, minb, maxb, q2);
                        ret.n = nBoard;
                        ret.p = ip;
                        ret.dummy.w = stone0.w;
                    }
                }

                /*if (mm0 > idBack) {
                    //updateResult(result, ret);
                    if (ret.t.x <= result0.t.x) {
                        result2 = result1;
                        result1 = result0;
                        result0 = ret;
                    }
                    else if (ret.t.x <= result1.t.x) {
                        result2 = result1;
                        result1 = ret;
                    }
                    else if (ret.t.x <= result2.t.x) {
                        result2 = ret;
                    }
                }*/
                if(mm0 > idBack) {
                    finalizeStone(ro, rd, ret, result0, result1, result2);
                }
            }
        }
    }
    int isInCup = 0;

    for (int i = 0; i < 4; i++) {
        vec3 cc1 = cc[i];
        cc1.y = bnx.y - legh + bowlRadius2 - 0.5* (bowlRadius2 - bowlRadius);
        vec3 cc2 = cc1;
        vec2 ts = intersectionRaySphereR(ro, rd, cc1, bowlRadius2*bowlRadius2);
        float cuty = cc2.y + cc[i].y * bowlRadius;
        vec2 tc = vec2(dot(-ro + vec3(cuty), nBoard), dot(bnx.xyz - vec3(10.0) - ro, nBoard)) / dot(rd, nBoard);
        bool between = ro.y < cuty && ro.y > bnx.y - legh;
        float ipy = ro.y + ts.x * rd.y;
        if (((ro.y > cuty && max(tc.x, ts.x) <= min(tc.y, ts.y)) || (ro.y <= cuty && ipy< cuty)) && ts.x != noIntersection2.x){
            float firstx = tc.x;
            tc = vec2(min(tc.x, tc.y), max(tc.x, tc.y));
            vec2 ts2 = intersectionRaySphereR(ro, rd, cc2, bowlRadius*bowlRadius);
            ret.t = csg_difference(csg_intersection(tc, ts), ts2);
            isInCup = i+1;
            //if (ret.t.x == ts2.y && ts2.y != noIntersection2.x) {
            //}
            //float rett = max(tc.x, ts.x);
            //ret.t = vec2(!between && ts2.x < rett && ts2.y > rett ? ts2.y : rett, ts.y);
            ret.m = i < 2 ? (i == 0 ? idLidBlack : idLidWhite) : (i == 2 ? idCupBlack: idCupWhite); //|ts2.x < rett && ts2.y > rett ?
            ret.p = ro + ret.t.x*rd;
            float ln = cc2.y - cuty;
            float r1 = sqrt(bowlRadius*bowlRadius - ln*ln);
            float r2 = sqrt(bowlRadius2*bowlRadius2 - ln*ln);
            float alpha = distance(ret.p.xz, cc2.xz) - 0.5*(r2 + r1);
            //vec3 nn = ret.t.x == tc.x ? nBoard : ret.t.x == ts2.y ?
            //  normalize(mix(-normalize(ret.p - cc2), nBoard, 1.0/exp(-150.0*alpha)))
            //  : normalize(mix(normalize(ret.p - cc1), nBoard, 1.0/exp(-150.0*alpha)));//(ts2.x > rett || ts2.y < rett) && tc.x > ts.x ? nBoard : ts2.x < rett && ts2.y > rett ? -normalize(ret.p - cc2) : normalize(ret.p - cc1);
            vec3 nn = ret.t.x == tc.x ? nBoard : ret.t.x == ts2.y ? -normalize(ret.p - cc2) : normalize(ret.p - cc1);
            //(ts2.x > rett || ts2.y < rett) && tc.x > ts.x ? nBoard : ts2.x < rett && ts2.y > rett ? -normalize(ret.p - cc2) : normalize(ret.p - cc1);
            /*vec3 nn = ret.t.x == tc.x ? nBoard : ret.t.x == ts2.y ? -normalize(ret.p - cc2) : normalize(ret.p - cc1);
            if(ret.t.x == tc.x) {
                if(alpha > 0.0) {
                    nn = normalize(mix(-normalize(ret.p - cc2), nn, 1.0/exp(25.0*abs(alpha))));
                } else if ( alpha < 0.0) {
                    nn = normalize(mix(normalize(ret.p - cc1), nn, 1.0/exp(25.0*abs(alpha))));
                }
            }*/
            float d1 = distanceRaySphere(ro, rd, cc1, bowlRadius2);
            vec3 q22, q23;
            vec3 cc3 = cc2;
            cc3.y = cuty;
            float d2 = distanceRayCircle(ro, rd, ro + tc.x*rd, vec2(farClip), cc3, r1, q22).y;
            float d3 = distanceRayCircle(ro, rd, ro + tc.x*rd, vec2(farClip), cc3, r2, q23).y;
            float md = d2;
            bool solid = true;
            float find = -d3;
            ret.n = nn;
            //ret.d = -farClip;//-farClip;
            float dot1 = dot(ret.p, rd);
            float dot2 = dot(cc1, rd);

            ret.d = -farClip;
            bool exter = d2 < -d3;
            ret.d = max(d2 > -boardaa ? d2 : -farClip, d3 < boardaa ? -d3 : -farClip);
            if (d1 < 0.0 && ret.d < 0.0 && ro.y > bnx.y - legh) {
                //ret.d = max(d1, ret.d);
                //ret.t = tc;
                ret.p = ro + ret.t.x*rd;
                ret.n = nBoard;
                updateResult(result0, result1, result2, ret);
                ret.d = farClip;
                if (!exter) {
                    ret.d = -farClip;
                    ret.t = vec2(ts2.x < farClip && ts2.y > tc.x ? ts2.y : tc.x + 0.001);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(cc2 - ret.p);
                    ret.pid = isInCup;
                }
                else if (dot1 - dot2 < 0.0 && d1 < -0.5*boardaa) {
                    ret.d = -farClip;
                    ret.t = vec2(tc.x + 0.001);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(ret.p - cc1);
                }
            }
            else {
                if (tc.x - 0.02 < ts.x && ts.x < farClip && ts.x > 0.0) {
                    ret.d = d1;
                    ret.t = vec2(ts.x - 0.01);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(ret.p - cc1);
                }
                else if (ts2.y > tc.x && ts2.x < farClip && ts2.y > bnx.y - legh) {
                    ret.d = -farClip;
                    ret.t = vec2(ts2.y);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(cc2 - ret.p);
                    ret.pid = isInCup;
                }
            }
            if (ret.d < farClip){
                updateResult(result0, result1, result2, ret);
            }
        }
    }

    int si = 0;
    int ei = 0;
    //vec3 cci;
    for(int i = 0; i < 4; ++i) {
        //int isInCup = i + 1;
        vec3 cc1 = cc[i].xyz;
        //vec3 cc1 = cc[i].xyz;
        cc1.y = bnx.y - legh + bowlRadius2 - 0.5* (bowlRadius2 - bowlRadius);
        vec4 ret0;
        float tt2;
        float t = intersectionRaySphereR(ro, rd, cc1, bowlRadius2*bowlRadius2).x;
        int isInCup = t < farClip ? i +1: 0;
        if(isInCup == 0)
        continue;
        vec3 cci;
        cci = cc[isInCup - 1].xyz;
        if (isInCup == 2 || isInCup ==4) {
            si = 0;
            if (isInCup == 2)
            ei = min(maxCaptured, iWhiteCapturedCount);
            else
            ei = min(maxCaptured, iBlackReservoirCount);
        }
        else if (isInCup == 1 || isInCup == 3){
            si = maxCaptured;
            if (isInCup == 1)
            ei = maxCaptured + min(maxCaptured, iBlackCapturedCount);
            else
            ei = maxCaptured + min(maxCaptured, iWhiteReservoirCount);
        }
        for (int i = si; i < ei; ++i) {
            vec4 ddc = ddc[i];
            ddc.xz += cci.xz;
            ddc.y += bnx.y - legh + 0.5 * (bowlRadius2 - bowlRadius);
            int mm0;
            vec4 ret0;
            float tt2;
            if (intersectionRayStone(ro, rd, ddc.xyz, ret0, tt2)) {
                vec3 ip = ro + rd*ret0.w;
                int mm0 = (i < maxCaptured != (isInCup < 3)) ? idBowlBlackStone : idBowlWhiteStone;
                ret.dummy = ddc;
                ret.t = vec2(ret0.w, tt2);
                ret.m = mm0;
                ret.d = -farClip;
                ret.n = ret0.xyz;
                ret.p = ip;
                ret.uid = i+ (isInCup - 1)*maxCaptured;
                ret.pid = isInCup;

                vec3 dist0a = distanceRayStoneSphere(ro, rd, ddc.xyz - dn, stoneRadius);
                vec3 dist0b = distanceRayStoneSphere(ro, rd, ddc.xyz + dn, stoneRadius);
                dist0a = dist0a.y > dist0b.y ? dist0a : dist0b;
                dist0b.yz = distanceRayStone(ro, rd, ddc.xyz);
                ret.d = mix(dist0a.y / dist0a.z, dist0b.y / dist0b.z, step(0.0, dist0a.x));


                //finalizeStoneIntersection(ro, rd, ddc, ret, 0.0, false);

                if (mm0 > idBack) {
                    //        updateResult(result, ret);
                    if (ret.t.x <= result0.t.x) {
                        result2 = result1;
                        result1 = result0;
                        result0 = ret;
                    }
                    else if (ret.t.x <= result1.t.x) {
                        result2 = result1;
                        result1 = ret;
                    }
                    else if (ret.t.x <= result2.t.x) {
                        result2 = ret;
                    }
                }
            }
        }
    }
    float t = dot(vec3(0.0, bnx.y - legh, 0.0) - ro, nBoard) / dot(rd, nBoard);
    bool aboveTable = ro.y >= bnx.y - legh;
    if (t > 0.0 && t < farClip && aboveTable) {
        ret.m = idTable;
        ret.t = vec2(t);
        ret.p = ro + t*rd;
        ret.n = nBoard;
        ret.d = -farClip;
        updateResult(result0, result1, result2, ret);
    }
    for (int i = 0; i < 4; i++){
        const float r = legh;
        vec3 cc = vec3(1 - 2 * (i & 1), 1, 1 - 2 * ((i >> 1) & 1))*(bnx.xyz + vec3(r, -0.5*r, r));
        vec2 ts = intersectionRaySphereR(ro, rd, cc, r*r);
        if (ts.x > 0.0 && ts.x < farClip) {
            ret.d = distanceRaySphere(ro, rd, cc, r);
            vec3 ip = ro + ts.x*rd;
            cc.y = bnx.y - legh;
            ret.t = ts;
            ret.m = idLeg1 + i;
            ret.p = ip;
            ret.n = normalize(ip - cc);
            updateResult(result0, result1, result2, ret);
        }
    }
}


vec2 softshadow(in vec3 pos, in vec3 nor, const vec3 lig, const float ldia, int m, bool ao, int uid, int pid) {
    vec2 ret = vec2(1.0);
    bool isBoard = m == idBoard || m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4;// || m == idGrid;
    if (isBoard && pos.y > h) return ret;
    const float eps = -0.00001;
    float ldia2 = ldia*ldia;
    if (pos.y < -eps) {
        //nn = normalize(pos-cc);
        vec3 ldir = -pos;
        float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
        vec3 ip = pos + res*ldir;
        //float d = length(ip - lig);
        vec2 rr = vec2(abs(bnx.xz)*length(ip - pos) / length(pos));
        vec2 ll = vec2(ldia);
        vec4 xz = vec4(min(ip.xz + rr, lpos.xz + ll), max(ip.xz - rr, lpos.xz - ll));
        vec2 xz1 = max(vec2(0.0), xz.xy - xz.zw);

        vec3 dd = vec3(0.0, bnx.y, 0.0);
        ldir = dd - pos;
        res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
        ip = pos + res*ldir;
        rr = vec2(abs(bnx.xz)*length(ip - pos) / length(dd - pos));
        xz = vec4(min(ip.xz + rr, lpos.xz + ll), max(ip.xz - rr, lpos.xz - ll));
        vec2 xz2 = max(vec2(0.0), xz.xy - xz.zw);
        float mx = mix(1.0, 1.0 - 0.5*((xz1.x*xz1.y) + (xz2.x*xz2.y)) / (4.0*ldia2), clamp(-pos.y / boardaa, 0.0, 1.0));
        //if(isBoard && m!= idBoard)
        ret.x *= mx;
    }
    if (m == idTable) {
        for(int j = 0; j <= 2; j += 2) {
            vec3 cci = (pos.x < 0.0 ? cc[j+0] : cc[j+1]);
            float yy = bnx.y - legh + bowlRadius2 - 0.5* (bowlRadius2 - bowlRadius);
            cci.y = yy + cci.y * bowlRadius;
            vec3 ddpos = cci - pos;
            float ln = length(ddpos);
            vec3 ldir = ddpos;
            float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
            vec3 ip = pos + res*ldir;
            vec2 rR = vec2(bowlRadius2*length(ip - pos) / ln, ldia);
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
            ret.x *= 1.0 - A / (PI*ldia2);
        }
    }
    if ((m == idCupWhite || m == idCupBlack || m == idLidWhite || m == idLidBlack) || ((m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4 || m == idBoard) && pos.y < -0.00001)) {
        vec3 u = normalize(cross(nor, nBoard));
        vec3 v = cross(nor, u);
        vec3 ip = pos + v*dot(lpos - pos, -nBoard) / dot(v, -nBoard);
        float d = length(lpos - ip);
        float phi = 2.0*acos(d / ldia);
        float A = d < ldia ? 0.5*ldia2*(phi - sin(phi)) / (PI*ldia2) : 0.0;
        //ret.x *= dot(lpos - pos, nor) > 0.0 ? 1.0 - A : A;
        int i = m - idLeg1;
        vec3 cci;
        if(pid > 0) {
            cci = cc[pid - 1];
        }
        vec2 xz = (m == idCupBlack || m == idCupWhite || m == idLidWhite || m == idLidBlack) ? cci.xz : m == idBoard ? vec2(0.0) : vec2(1 - 2 * (i & 1), 1 - 2 * ((i >> 1) & 1))*(bnx.xz + vec2(legh));
        phi = PI - abs(acos(dot(v.xz, normalize(pos.xz - xz))));
        if (distance(pos.xz + nor.xz, xz) > distance(pos.xz - nor.xz, xz) || m == idBoard) {
            ret.y *= phi / PI;
            //ret.x *= phi/PI;
        }
    }
    if (m != idTable) {
        float t1, t2;
        vec3 rd = lig - pos;
        if (IntersectBox(pos, rd, minBound, maxBound, t1, t2)) {
            vec4 b12 = pos.xzxz + vec4(t1, t1, t2, t2)*rd.xzxz;
            vec4 bmnx = vec4(min(b12.xy, b12.zw), max(b12.xy, b12.zw));
            vec4 xz12 = floor(bmnx/vec4(wwx, wwy, wwx, wwy) + 0.5*vec4(vec2(fNDIM - 1.0), vec2(fNDIM + 1.0)));
            ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
            for (int i = mnx.y; i <= mnx.w; i++){
                for (int j = mnx.x; j <= mnx.z; j++){
                    int kk = NDIM*i + j;
                    float mm0 = iStones[kk].z;
                    if (mm0 >= cidBlackStone) {
                        vec2 xz = vec2(wwx, wwy)*iStones[kk].xy;
                        if (distance(pos.xz, xz) <= r1 + 0.001) {
                            float ldia = 1000.0;
                            float phi = PI;
                            if (isBoard) {
                                ret.y *= min(1.0, distance(pos.xz, xz) / r1);
                            }
                            else {
                                vec3 u = normalize(cross(nor, nBoard));
                                vec3 v = cross(u, nor);
                                vec3 diff = vec3(pos.x, 0.000, pos.z) - vec3(xz.x, 0.0, xz.y);
                                if(length(diff) > 0.01) {
                                    float acs = acos(dot(v, normalize(diff)));
                                    //ret.y *= min(1.0, acs / PI);
                                    ret.y *= mix(1.0,min(1.0, acs / PI),2.0*(length(diff)-0.01)/wwx);
                                }
                            }
                        }
                        if (isBoard && pos.y > -0.001){
                            vec3 ddpos = vec3(vec2(wwx, wwy)*iStones[kk].xy, h).xzy - pos;
                            float ln = length(ddpos);
                            vec3 ldir = ddpos;
                            float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
                            vec3 ip = pos + res*ldir;
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
                            ret.x *= 1.0 - A / (PI*ldia2);
                        }
                    }
                }
            }
        }

        if ((m == idCupBlack || m == idCupWhite || m == idLidWhite || m == idLidBlack || m == idBowlBlackStone || m == idBowlWhiteStone) && abs(pos.x) > 1.0) {
            int isInCup = -1;
            if(pid > 0) {
                isInCup = pid;
            }
            int si = 0; int ei = 0;
            if (isInCup == 2 || isInCup ==4) {
                si = 0;
                if(isInCup == 2)
                ei = min(maxCaptured, iWhiteCapturedCount);
                else
                ei = min(maxCaptured, iBlackReservoirCount);
            }
            else if (isInCup == 1 || isInCup == 3){
                si = maxCaptured;
                if(isInCup == 1)
                ei = maxCaptured + min(maxCaptured, iBlackCapturedCount);
                else
                ei = maxCaptured + min(maxCaptured, iWhiteReservoirCount);
            }
            for (int j = si; j < ei; ++j) {
                vec4 ddcj = ddc[j];
                ddcj.xz += cc[isInCup-1].xz;
                ddcj.y += bnx.y - legh + bowlRadius2 - bowlRadius - 0.5 * (bowlRadius2 - bowlRadius);

                float cuty = bnx.y - legh - 0.5*(bowlRadius2 - bowlRadius) + bowlRadius2 + cc[isInCup-1].y * bowlRadius;
                if (((m == idCupBlack || m == idCupWhite || m == idLidWhite || m == idLidBlack) && pos.y < cuty - 0.0001)
                || ((m == idBowlBlackStone || m == idBowlWhiteStone) && uid != j && pos.y < ddcj.y - 0.01*h)){
                    vec3 ddpos = ddcj.xyz - pos;
                    float avoid = smoothstep(ddcj.y-0.01, ddcj.y, 0.99*pos.y);
                    float ln = length(ddpos);
                    vec3 ldir = ddpos;
                    float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
                    vec3 ip = pos + res*ldir;
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
                    ret.x *= mix(1.0 - A / (PI*ldia2), 1.0, avoid);
                }
                /*else if ((m == idBowlBlackStone || m == idBowlWhiteStone) && (pos.y < ddcj.y + 0.01*h || uid == j)) {
                    vec3 u = normalize(cross(nor, nBoard));
                    vec3 v = cross(nor, u);
                    ret.y *= min(1.0, 1.0 - abs(acos(dot(v.xz, normalize(pos.xz - ddcj.xz)))) / PI);
                }*/
            }
        }
    }
    return ret;
}

Material getMaterial(int m) {
    Material ret;
    if (m == idTable) {
        ret = mTable;
    }
    else if (m == idBoard || m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4) {
        ret = mBoard;
    }
    else if (m == idCupBlack) {
        ret = mCup;
    }
    else if (m == idCupWhite) {
        ret = mCup;
    }
    else if (m == idLidBlack) {
        ret = mCup;
    }
    else if (m == idLidWhite) {
        ret = mCup;
    }
    else if (m == idWhiteStone || m == idWhiteArea || m == idCapturedWhiteStone || m == idBowlWhiteStone) {
        ret = mWhite;
    }
    else if (m == idBlackStone || m == idBlackArea || m == idCapturedBlackStone || m == idBowlBlackStone) {
        ret = mBlack;
    }
    else if (m == idLastBlackStone || m == idLastWhiteStone) {
        ret = mRed;
    }
    else if (m == idGrid) {
        ret = mGrid;
    }
    else {
        ret = mBack;
    }
    return ret;
}

Material getMaterialColor(in Intersection ip, out vec4 mcol, in vec3 rd, in vec3 ro, out vec3 nn) {
    Material mat = getMaterial(ip.m);
    Material m0 = mat;//;at = mBoard;// getMaterial(idBoard);//ip.m
    bool noisy = false;
    float mult = 1.0;
    vec3 scoord;
    float sfreq;
    float mixcoef, smult1, smult2, smult3, sadd1, mixmult, mixnorm;
    float fpow = 1.0;
    mixmult = 0.0;
    mixnorm = mixmult;
    smult1 = 0.0;
    smult2 = 0.0;
    smult3 = 1.0;
    mcol.xyz = m0.clrA;
    vec3 mcolb = m0.clrB;
    vec3 mcolc = m0.clrC;
    vec3 flr = ip.dummy.xyz;
    vec3 scrd2 = 64.0*(ip.p.xyz-flr);
    vec3 scrd = ip.p.xyz - flr;
    vec2 xz;
    xz = scrd.xz;
    if (mat.id == mBoard.id || mat.id == mCup.id) {
        scoord = 16.0*vec3(xz.x,scrd.y,xz.y) + vec3(0.0, 0.25, 0.0);
        scoord.x = 2.0*length(scoord.xy);
        scoord.z = 2.0*scoord.y;
        scoord.y = 2.0*length(scoord.xy);
        scoord.xyz = 0.1*vec3(length(scoord.yz));
        noisy = true;
        scrd = scoord;
        mixmult = 1.0;
        mixnorm = 0.01;
    }
    else if (mat.id == mGrid.id) {
        scoord = 350.0*scrd;
        sfreq = 1.5;
        noisy = true;
        scrd = scoord;
    }
    else if (mat.id == mWhite.id || mat.id == mBlack.id) {
        float alpha = ip.dummy.w;
        float cosa = cos(alpha);
        float sina = sin(alpha);
        //scoord = reflect(rd, ip.n);
        //scoord = 0.5*(1.0 + sin(vec3(3.0,3.0,3.0)*scoord));
        sfreq = 1.0*(1.0 + 0.1*sin(11.0 + xz.x));
        sadd1 = 5.0 + sin(131.0 + 15.0*xz.x);
        noisy = true;
        if(mat.id == mBlack.id) {
            scrd2 *= 5.0;
        }
        else {
            //scrd2 *= 3.0;
            vec2 xz = mat2(cosa, sina, -sina, cosa)*scrd2.xz;
            scrd2 = xz.xyy;
            scrd2.y = scrd.y;
            scrd2.z = 1.0;
        }
        mixmult = 0.015;
        mixnorm = 0.015;
    }
    if (m0.id == mTable.id) {
        vec3 color;// = mix(mTable.clrA,mTable.clrB,density);
        const float DT = 2.0*3.1415926 / 8.0;
        float fa = atan(xz.y, xz.x);
        float iord = floor((fa + 0.5*DT) / DT);
        float far = iord*DT;
        float fb = cos(abs(fa - far))*length(xz);
        fa = mod(fa, DT);

        bool a = fa < DT;
        bool b = fb < 0.5;
        float c05a = abs(0.5*DT - mod(fa + 0.5*DT, DT)) / DT;
        float c05b = abs(0.25 - mod(fb, 0.5)) / 0.5;
        bool third = fb < 1.5 || fb > 3.0 || c05a > 0.4 || c05b  > 0.45;// ? 0.5 : 0.0;
        float jord = fb;
        bool ab = (iord == -2.0) || (iord == -1.0 && mod(jord, 1.5) < 1.0) || (iord == 3.0 && mod(jord, 1.5) >= 1.0)
        || (iord == 0.0 && mod(jord + 0.5, 1.5) < 1.0) || ((iord == 4.0 || iord == -4.0) && mod(jord + 0.5, 1.5) >= 1.0)
        || (iord == -1.0 && mod(jord, 1.5) < 1.0) || (iord == 3.0 && mod(jord, 1.5) >= 1.0)
        || (iord == 1.0 && mod(jord, 1.5) >= 0.5) || (iord == -3.0 && mod(jord, 1.5) < 0.5);
        c05a = smoothstep(0.395, 0.4, c05a);
        c05b = smoothstep(0.44, 0.45, c05b);
        float w1 = 0.0;
        float alpha = 3.1415926/4.0;
        if (!third && ab) //;(a &&!b) || (!a && b))
        {
            mcol.xyz = mix(mTable.clrA, mTable.clrC, max(c05b, c05a));
        }
        else if (!third){
            mcol.xyz = mix(mTable.clrB, mTable.clrC, max(c05b, c05a));
        }
        else {
            mcol.xyz = mTable.clrC;
            alpha = 0.0;
        }
        float cosa = cos(alpha);
        float sina = sin(alpha);
        xz = mat2(cosa, sina, -sina, cosa)*ip.p.xz;
        scoord = ip.p;//16.0*vec3(xz.x, ip.p.y, xz.y) + vec3(0.0, 0.25, 0.0);
        scrd = 66.1*scoord;
        scrd2 = 16.1*scoord;
        noisy = true;
        const float al = 0.15;
        float mixt = exp(-0.35*max(0.0, length(ip.p)));
        mcolb = mixt*(mcol.xyz + (al)*(vec3(1.0) - mcol.xyz));
        mcolc = mixt*((1.0 - al)*mcol.xyz);
        mcol.xyz *= mixt;
        mixmult = 0.1;
        mixnorm = 0.0;
    }
    float rnd = 0.0;
    float rnd2 = 0.0;
    vec3 grad = vec3(0.0, 1.0, 0.0);
    vec3 grad2 = vec3(0.0, 1.0, 0.0);
    //if (noisy) {
    if(mat.id == mBoard.id || mat.id == mCup.id) {
        scrd2 = ip.p.xyz * fNDIM/19.0 *vec3(23.0, 23.5, 1.3);
        if(mat.id == mCup.id) {
            scrd2.z *= 21.0;
        }
    }
    if(mat.id == mTable.id) {
        scrd2 *=13.0;
        scrd += mixmult *grad;
    }
    rnd2 = snoise(scrd2, grad);
    if(mat.id == mBoard.id || mat.id == mCup.id) {
        scrd = 2.2*vec3(length(scrd2.xy+0.03*grad.xz), length(scrd2.xy+0.03 * grad.zx), 1.0);
        if(mat.id == mCup.id) {
            float alpha = grad.y;
            scrd.xz = 0.33*vec2(
            -cos(alpha) * scrd.x + sin(alpha)*scrd.z,
            sin(alpha) * scrd.x + cos(alpha)*scrd.z
            );
        }
    }
    rnd = snoise(scrd, grad2);
    if (mat.id == mBoard.id || mat.id == mCup.id) {
        float w1 = 3.0*length(scoord.yx - 0.5*vec2(1.57 + 3.1415*rnd));
        float w2 = 0.1*(scoord.x + scoord.z);
        smult1 = mix(clamp(abs(rnd),0.0,1.0), 1.0, 0.01)*(clamp(0.25*(sin(grad.x)), 0.0, 1.0));
        smult2 = mix(1.0 - clamp(abs(rnd),0.0,1.0), 1.0, 0.01)*(clamp(0.25*(sin(1.5*grad.y)), 0.0, 1.0));

        smult3 = 1.0 - smult2;
    }
    else if (mat.id == mBlack.id || mat.id == mGrid.id || mat.id == mWhite.id || mat.id == mTable.id) {
        smult1 = clamp(abs(rnd),0.0,1.0);
        smult2 = clamp(abs(rnd2),0.0,1.0);
        smult3 = 0.5;//clamp(abs(grad.z),0.0,1.0);
        mixmult = 0.0;
    }
    mcol.xyz = mix(mix(mcol.xyz, mcolb, smult2), mix(mcol.xyz, mcolc, 1.0 - smult2), smult1);
    mcol.w   = mix(mix(mat.specularPower.x, mat.specularPower.y, smult2), mix(mat.specularPower.x, mat.specularPower.z, 1.0 - smult2), smult1);
    nn = normalize(mix(ip.n, grad2, mixnorm));
    //nn = ip.n;
    return mat;
}

vec3 shading(in vec3 ro, in vec3 rd, in Intersection ip, const Material mat, vec4 col){
    vec3 ret;
    if (mat.id == mBack.id) {
        ret = mat.clrA;
    }
    else {
        vec3 nn = ip.n;
        vec3 ref = reflect(rd, nn);
        vec3 lig = normalize(lpos - ip.p);
        vec3 lig2 = normalize(lpos2 - ip.p);
        vec3 lig3 = normalize(lpos3 - ip.p);
        vec2 shadow = pow(softshadow(ip.p, ip.n, lpos, ldia, ip.m, false, ip.uid, ip.pid), vec2(1.0, 0.25));//0.6
        //shadow += 0.4*pow(softshadow(ip.p, ip.n, lpos2, ldia, ip.m, false, ip.uid), vec2(1.0,0.25));

        float nny = 0.5 + 0.5*nn.y;
        float adsy = dot(vec3(0.6,0.3,0.3), clamp(vec3(dot(nn, lig), dot(nn, lig2), dot(nn, lig3)),0.0,1.0));
        vec4 pws = clamp(vec4(dot(ref, lig), dot(ref, lig2), dot(ref, lig3), dot(ref, lig3)), 0.0, 1.0);
        vec3 cupsab = vec3(1.0);//ip.m == idBowlBlackStone || ip.m == idBowlWhiteStone ? vec3(0.125,0.9,0.25) : vec3(0.125,1.0,0.5);
        vec3 pwr = pow(pws.xyz, col.w*cupsab);
        vec3 score  = mat.diffuseAmbientSpecularWeight * vec3(adsy * shadow.x, 1.0,shadow.x * shadow.y*(0.25*pwr.x + pwr.y + pwr.z));
        ret = (score.x+score.y)*col.xyz + score.z;
    }
    ret = pow(ret, gamma*exp(contrast*(vec3(0.5) - ret)));
    return ret;
}

vec3 render(in vec3 ro, in vec3 rd, in vec3 bg)
{
    vec3 col0, col1, col2;
    col0 = col1 = col2 = vec3(0.0);
    col0 = vec3(0.0);
    Intersection ret0;
    Intersection ret1;
    Intersection ret2;

    float dist = dot(vec3(0.0,0.85*h,0.0)-ro, nBoard)/length(ro);
    if(dist > 0.0) {
        ro.y = 0.85*h-(ro.y-0.85*h);
        rd.y = -rd.y;
    }

    castRay(ro, rd, ret0, ret1, ret2);

    vec3 col;
    vec4 mcol;
    Material mat;
    vec3 nn;

    //gl_FragDepth = (ret0.m == mBlack.id || ret0.m == mWhite.id) ? 0.5 : (ret0.p.y < -0.001 ? 0.25 : 0.75);//; distance(ro, ret[0].p) / 100.0;

    float alpha0 = 0.0;
    float alpha1 = 0.0;
    float alpha2 = 0.0;
    if(ret0.t.x < farClip) {
        alpha0 = smoothstep(-boardaa, 0.0, ret0.d);
        //if(alpha0 > 0.0) {
        mat = getMaterialColor(ret0, mcol, rd, ro, nn);
        ret0.n = nn;
        col0 = shading(ro, rd, ret0, mat, mcol);
        //}
    }

    if(ret1.t.x < farClip) {
        alpha1 = smoothstep(-boardaa, 0.0, ret1.d);
        //if(alpha1 > 0.0) {
        mat = getMaterialColor(ret1, mcol, rd, ro, nn);
        ret1.n = nn;
        col1 = shading(ro, rd, ret1, mat, mcol);
        //}
    }

    if(ret2.t.x < farClip) {
        alpha2 = smoothstep(-boardaa, 0.0, ret2.d);
        //if(alpha2 > 0.0) {
        mat = getMaterialColor(ret2, mcol, rd, ro, nn);
        ret2.n = nn;
        col2 = shading(ro, rd, ret2, mat, mcol);
        //}
    }

    if(ret2.t.x < farClip) {
        col2 = vec3(0.0) * alpha2 + col2 * (1.0-alpha2);
        col1 = col2 * alpha1 + col1 * (1.0-alpha1);
        col0 = col1 * alpha0 + col0 * (1.0-alpha0);
    }
    else if(ret1.t.x < farClip) {
        col1 = vec3(0.0) * alpha1 + col1 * (1.0-alpha1);
        col0 = col1 * alpha0 + col0 * (1.0-alpha0);
    } else if (ret0.t.x < farClip) {
        col0 = vec3(0.0) * alpha0 + col0 * (1.0-alpha0);
    }
    return col0;
}


void main(void)
{
    vec3 gamma = vec3(2.3386);
    c = vec3(1.0, 0.25, wwy/wwx);
    bnx = vec4(-c.x, -0.2, -c.z, 0.0);
    vec3 cl = render(rool, normalize(rdbl), bgA);
    vec3 cr = render(roor, normalize(rdbr), bgA);
    glFragColor = vec3(0.9707,0.0,0.0)*cl.rgb + vec3(0.0,0.7709,0.1989)*cr.rgb;
}
