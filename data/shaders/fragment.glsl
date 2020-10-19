#version 330
precision highp float;

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

in vec3 rdb;
flat in vec3 roo;
in float noise;
out vec3 glFragColor;



uniform int NDIM;
uniform vec2 iResolution;
uniform int iStoneCount;
uniform int iBlackCapturedCount;
uniform int iWhiteCapturedCount;
uniform float iTime;
layout(std140) uniform iStoneBlock{
    vec4 iStones[MAXSTONES];
};
uniform mat4 glModelViewMatrix;
uniform vec3 iTranslate;

/* === DO NOT CHANGE ABOVE == */

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
const vec3 c = vec3(1.0, 0.25, 1.0);
const vec4 bnx = vec4(-c.x, -0.2, -c.x, 0.0);
const float mw = 0.85;
const float legh = 0.15;

uniform float gamma;
uniform float contrast;
uniform float fNDIM;
uniform float boardaa;
uniform float boardbb;
uniform float boardcc;
uniform float ww;
uniform float iww;
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

struct Material {
    int id;
    vec3 diffuseAmbientSpecularWeight;
    float specularPower;
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
const int idCupBlack = 15;
const int idCupWhite = 16;
const int idBowlBlackStone = 17;
const int idBowlWhiteStone = 18;

const vec3 bgA = vec3(0.0, 0.0, 0.0);
const vec3 bgB = vec3(0.0, 0.0, 0.0);

const Material mCupBlack = Material(idCupBlack, vec3(0.4, 0.6, 0.15), 16.0, vec3(0.65826, 0.528209, 0.238209), vec3(0.387763, 0.3289191, 0.12761), vec3(0.22005, 0.180002, 0.1244), 1.3);
const Material mCupWhite = Material(idCupWhite, vec3(0.4, 0.6, 0.15), 16.0, vec3(0.45826, 0.428209, 0.238209), vec3(0.287763, 0.289191, 0.12761), vec3(0.12005, 0.120002, 0.085744), 1.3);
const Material mBoard = Material(idBoard, vec3(0.7, 0.3, 0.15), 42.0, vec3(0.93333, 0.713725, 0.38039), vec3(0.53333,0.313725,0.09039), vec3(0.7333,0.613725,0.19039), 1.5);
const Material mTable = Material(idTable, vec3(1.2, 0.15, 0.0), 4.0, vec3(0.566,0.1196,0.0176), vec3(0.766,0.3196,0.2176), vec3(0.666,0.2196,0.1176), 0.0);
const Material mWhite = Material(idWhiteStone, vec3(0.23, 0.63, 0.2), 42.0, vec3(0.94), vec3(0.92,0.97,0.67), vec3(0.92), 0.5);
const Material mBlack = Material(idBlackStone, vec3(0.23, 0.83, 0.1), 8.0, vec3(0.08), vec3(0.04), vec3(0.10), 0.5);
const Material mRed = Material(idLastBlackStone, vec3(0.3, 0.7, 0.25), 4.0, vec3(0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0), 0.0);
const Material mBack = Material(idBack, vec3(0.0, 1.0, 0.0), 1.0, bgA, bgB, bgA, 0.0);
const Material mGrid  = Material(idGrid, vec3(1.5, 0.4, 0.15), 42.0, vec3(0.0),vec3(0.0), vec3(0.0), 0.0);

uniform vec3 cc[2];
const int maxCaptured = 32;
uniform vec3 ddc[6 * maxCaptured];

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
    float dd0 = length((dt + sqt)*R*rd2);///*rd2  dot(rd,X0-ro);
    return vec2(-dd0, distance(ro, X0 + dd));
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


void updateResult(inout Intersection result[2], Intersection ret) {
    if (ret.t.x <= result[0].t.x) {
        result[1] = result[0];
        result[0] = ret;
    }
    else if (ret.t.x <= result[1].t.x) {
        result[1] = ret;
    }
}

void castRay(in vec3 ro, in vec3 rd, out Intersection result[2]) {

    Intersection ret;
    ret.m = idBack;
    ret.d = -farClip;
    ret.n = nBoard;
    ret.t = vec2(farClip);
    ret.p = vec3(-farClip);
    ret.dummy = vec4(0.0);
    ret.isBoard = false;
    result[0] = result[1] = ret;

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
            bool gridx = ip.x > -c.x + mw*ww - 0.5*ww && ip.x < c.x - mw*ww + 0.5*ww && ip.z > -c.z + mw*ww - dw && ip.z < c.z - mw*ww + dw;
            bool gridz = ip.x > -c.x + mw*ww - dw && ip.x < c.x - mw*ww + dw && ip.z > -c.z + mw*ww - 0.5*ww && ip.z < c.z - mw*ww + 0.5*ww;
            float r = boardbb*distance(ro, ip);
            if (ip0.y > 0.0) {
                isBoardTop = true;
                bvec2 nearEnough = lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(dw + dw + r));
                bvec2 farEnough = bvec2(true, true);// greaterThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(0.33*ww));
                if (any(nearEnough) && any(farEnough)) {
                    vec3 dir = vec3(dw, -dw, 0.0);
                    if (nearEnough.x && gridx) {
                        vec3 ccx = vec3(ww*round(iww*ip.x), ip.y, ip.z);
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
                        vec3 ccz = vec3(ip.x, ip.y, ww*round(iww*ip.z));
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
                float rr = 0.1*ww;//8.0*dw;
                nearEnough = lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(rr + rr + r));
                if (any(nearEnough)) {
                    vec3 ppos;
                    float mindist = distance(ip.xz, vec2(0.0));
                    vec3 fpos = vec3(0.0);
                    if (NDIM == 19) {
                        ppos = vec3(6.0, 0.0, 6.0);
                        for (int i = -1; i <= 1; i++) {
                            for (int j = -1; j <= 1; j++) {
                                vec3 pos = ww*vec3(i, 0, j)*ppos.zyz;
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
                                vec3 pos = ww*vec3(i, 0, j)*ppos.zyz;
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
        ret.t = vec2(tb, tb);
        ret.m = idBoard;
        ret.d = -farClip;
        ret.d2 = -farClip;
        ret.p = ip;
        ret.isBoard = isBoardTop;
        bool edge = false;
        if (dist0 > -boardaa) {
            float tc = 0.0, tc2 = 0.0;
            IntersectBox(ro, q2 - ro, bnx.xyz, -bnx.xwz, tc, tc2);
            if (tc2 - tc > 0.001) {
                n0 = normalize(n0);
                vec3 cdist = abs(abs(ip0) - abs(bn0));
                vec2 ab = clamp(vec2(abs(dist0), min(cdist.z, min(cdist.x, cdist.y))) / boardcc, 0.0, 1.0);
                n0 = normalize(mix(normalize(ip0), n0, ab.y));
                n0 = normalize(mix(n0, n1, ab.x));
                ret.n = n0;
                ret.d = -farClip;
                ret.d2 = -farClip;
                ret.p = q2;
                edge = true;
                updateResult(result, ret);
            }
        }
        ret.n = n1;
        ret.p = ip;
        bool upit = false;
        ret.d = dist0;
        dist0 = max(dist0, -dd*boardaa);
        ret.d2 = edge ? -farClip : dist0;
        ret.m = idBoard;
        updateResult(result, ret);
        ret.isBoard = false;
    }
    float t1, t2;
    if (IntersectBox(ro, rd, minBound, maxBound, t1, t2)) {
        vec4 b12 = ro.xzxz + vec4(t1, t1, t2, t2)*rd.xzxz;
        vec2 noise = vec2(ww*0.2);
        vec4 bmnx = vec4(min(b12.xy, b12.zw) - noise, max(b12.xy, b12.zw) + noise);
        vec4 xz12 = floor(iww * bmnx + vec4(0.5*fNDIM));
        ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
        for (int i = mnx.y; i <= mnx.w; i++){
            for (int j = mnx.x; j <= mnx.z; j++){
                vec4 stone0 = iStones[NDIM*i + j];
                int mm0 = idBack;
                float m0 = stone0.z;
                if (m0 >= cidBlackStone) {
                    vec2 xz = ww*stone0.xy;
                    vec3 dd = vec3(xz.x, 0.5*h, xz.y);
                    float tt2;
                    vec4 ret0;
                    if (intersectionRayStone(ro, rd, dd, result[1].t.x, ret0, tt2)) {
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
                        ret.t = vec2(ret0.w, tt2);
                        ret.m = mm0;
                        ret.d = -farClip;
                        ret.n = ret0.xyz;
                        ret.p = ip;
                    }
                }
                else if (isBoardTop && (m0 == cidWhiteArea || m0 == cidBlackArea)) {
                    vec3 dd = vec3(vec2(j, i) - vec2(0.5*fNDIM - 0.5), 0.0)*ww;
                    vec2 w25 = vec2(0.25*ww, dd.z);
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

                if (mm0 > idBack) {
                    //updateResult(result, ret);
                    if (ret.t.x <= result[0].t.x) {
                        result[1] = result[0];
                        result[0] = ret;
                    }
                    else if (ret.t.x <= result[1].t.x) {
                        result[1] = ret;
                    }
                }
            }
        }
    }
    int isInCup = 0;

    for (int i = 0; i < 2; i++) {
        vec2 tc = vec2(dot(-ro - vec3(-0.1), nBoard), dot(bnx.xyz - vec3(2.0) - ro, nBoard)) / dot(rd, nBoard);
        vec2 ts = intersectionRaySphereR(ro, rd, cc[i], bowlRadius2*bowlRadius2);
        //vec2 tsx = intersectionRaySphereR(ro, rd, cc[i], 0.149);
        float firstx = tc.x;
        tc = vec2(min(tc.x, tc.y), max(tc.x, tc.y));
        bool between = ro.y < -0.3 && ro.y > bnx.y - legh;
        if (ts.x != noIntersection2.x && max(tc.x, ts.x) <= min(tc.y, ts.y)) {
            float  rett = max(tc.x, ts.x);
            vec3 cc2 = cc[i];
            cc2.y = 0.1;
            vec2 ts2 = intersectionRaySphereR(ro, rd, cc2, bowlRadius*bowlRadius);
            ret.t = vec2(!between && ts2.x < rett && ts2.y > rett ? ts2.y : rett, ts.y);
            ret.m = i == 0 ? idCupBlack : idCupWhite; //|ts2.x < rett && ts2.y > rett ? 
            ret.p = ro + ret.t.x*rd;
            vec3 nn = (ts2.x > rett || ts2.y < rett) && tc.x > ts.x ? nBoard : ts2.x < rett && ts2.y > rett ? -normalize(ret.p - cc2) : normalize(ret.p - cc[i]);
            float d1 = distanceRaySphere(ro, rd, cc[i], bowlRadius2);
            vec3 q22, q23;
            float d2 = distanceRayCircle(ro, rd, ro + tc.x*rd, vec2(farClip), cc2, bowlRadius, q22).y;
            float ln = distance(cc[i], cc2);
            float d3 = distanceRayCircle(ro, rd, ro + tc.x*rd, vec2(farClip), cc2, sqrt(bowlRadius2*bowlRadius2 - ln*ln), q23).y;
            float md = d2;
            bool solid = true;
            float find = -d3;
            ret.n = nn;
            ret.d = -farClip;
            float dot1 = dot(ret.p, rd);
            float dot2 = dot(cc[i], rd);

            ret.d = -farClip;
            bool exter = d2 < -d3;
            ret.d = max(d2 > -boardaa ? d2 : -farClip, d3 < boardaa ? -d3 : -farClip);
	    ret.dummy.xz = cc[i].xz;
            if (ts2.x != noIntersection2.x)
                isInCup = ret.p.x < 0.0 ? 1 : 2;
            if (d1 < 0.0 && ret.d < 0.0 && ro.y > -0.3) {
                ret.d = max(d1, ret.d);
                ret.t = tc;//vec2(exter ? max(tc.x, ts.x) : min(max(ts2.x < farClip ? ts2.y: -farClip, tc.x), ts.y));
                ret.p = ro + ret.t.x*rd;
                ret.n = nBoard; //normalize(mix(nBoard, nn, clamp(abs(d2)/boardaa,0.0,1.0))); //;//exter ? normalize(ret.p - cc[i]) : normalize(cc2 - ret.p);
                updateResult(result, ret);
                ret.d = farClip;
                if (!exter) {
                    ret.d = -farClip;
                    ret.t = vec2(ts2.x < farClip && ts2.y > tc.x ? ts2.y : tc.x + 0.001);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(cc2 - ret.p);
                }
                else if (dot1 - dot2 <= 0.0 && d1 < -0.5*boardaa) {
                    ret.d = -farClip;
                    ret.t = vec2(tc.x + 0.001);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(ret.p - cc[i]);
                }
            }
            else {
                if (tc.x < ts.x && ts.x < farClip && ts.x > 0.0) {
                    ret.d = d1;
                    ret.t = vec2(ts.x);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(ret.p - cc[i]);
                }
                else if (ts2.y > tc.x && ts2.x < farClip && ts2.y > -0.3) {
                    ret.d = -farClip;
                    ret.t = vec2(ts2.y);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(cc2 - ret.p);
                }
            }
            if (ret.d < farClip){
                updateResult(result, ret);
            }
        }
    }

    int si = 0;
    int ei = 0;
    if (isInCup == 1) { si = 0; ei = min(maxCaptured, iBlackCapturedCount); }
    else if (isInCup == 2){ si = maxCaptured; ei = maxCaptured + min(maxCaptured, iWhiteCapturedCount); }
    for (int i = si; i < ei; ++i) {
        vec3 ddc = ddc[i];
        int mm0;
        vec4 ret0;
        float tt2;
        if (intersectionRayStone(ro, rd, ddc, result[1].t.x, ret0, tt2)) {
            vec3 ip = ro + rd*ret0.w;
            int mm0 = i < maxCaptured ? idBowlBlackStone : idBowlWhiteStone;
            ret.dummy = vec4(ddc, 0.0);
            ret.t = vec2(ret0.w, tt2);
            ret.m = mm0;
            ret.d = -farClip;
            ret.n = ret0.xyz;
            ret.p = ip;
            ret.uid = i;

            if (mm0 > idBack) {
                //        updateResult(result, ret);
                if (ret.t.x <= result[0].t.x) {
                    result[1] = result[0];
                    result[0] = ret;
                }
                else if (ret.t.x <= result[1].t.x) {
                    result[1] = ret;
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
        updateResult(result, ret);
    }
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
    //finalizeStoneIntersection(ro, rd, result, 0);
    vec3 dd = result[0].dummy.xyz;
    bvec3 isStone = bvec3(result[0].m == idBlackStone || result[0].m == idWhiteStone || result[0].m == idBowlBlackStone || result[0].m == idBowlWhiteStone,
        result[0].m == idCapturedBlackStone || result[0].m == idCapturedWhiteStone,
        result[0].m == idLastBlackStone || result[0].m == idLastWhiteStone);
    vec3 dist0a;
    Intersection r2 = result[0];
    if (any(isStone)) {
        dist0a = distanceRayStoneSphere(ro, rd, dd - dn, stoneRadius);
        vec3 dist0b = distanceRayStoneSphere(ro, rd, dd + dn, stoneRadius);
        dist0a = dist0a.y > dist0b.y ? dist0a : dist0b;
        dist0b.yz = distanceRayStone(ro, rd, dd);
        result[0].d = mix(dist0a.y / dist0a.z, dist0b.y / dist0b.z, step(0.0, dist0a.x));
    }
    if (any(isStone.yz)) {
        int idStoneInside = result[0].m;
        int idStone = idStoneInside;
        if(idStone == idCapturedBlackStone){
            idStone = idBlackStone;
            idStoneInside = idWhiteStone;
        }
        else if(idStone == idCapturedWhiteStone){
            idStone = idWhiteStone;
            idStoneInside = idBlackStone;
        }
        else if(idStone == idLastBlackStone){
            idStone = idBlackStone;
            idStoneInside = idLastBlackStone;
        }
        else if(idStone == idLastWhiteStone){
            idStone = idWhiteStone;
            idStoneInside = idLastWhiteStone;
        }
        result[0].m = idStoneInside;

        vec3 cc = dd;
        cc.y -= dn.y - sqrt(stoneRadius2 - 0.25*r1*r1);
        vec3 ip0 = ro + rd*dot(cc - ro, nBoard) / dot(rd, nBoard);
        vec3 q2;

        bool isNotArea = length(result[0].p.xz - cc.xz) > 0.5*r1;
        if (isNotArea) {
            vec2 rr = distanceRayCircle(ro, rd, ip0, result[0].t, cc, 0.5*r1, q2);
            r2.d = abs(rr.y);
            if (rr.x <0.0 && r2.d < boardaa){
                r2.d = max(-r2.d, result[0].d);
                r2.m = idStone;
                updateResult(result, r2);
            }
            else{
                result[0].m = idStone;
            }
        }
    }
    //finalizeStoneIntersection(ro, rd, result, 1);
}

vec2 softshadow(in vec3 pos, in vec3 nor, const vec3 lig, const float ldia, int m, bool ao, int uid){
    vec2 ret = vec2(1.0);
    bool isBoard = m == idBoard || m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4 || m == idGrid;
    if (isBoard && pos.y > h) return ret;
    const float eps = -0.00001;
    float ldia2 = ldia*ldia;
    if (pos.y < -eps) {
        //nn = normalize(pos-cc);
        vec3 ldir = -pos;
        float res = dot(lig - pos, nBoard) / dot(ldir, nBoard);
        vec3 ip = pos + res*ldir;
        //float d = length(ip - lig);
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
        //if(isBoard && m!= idBoard)
        ret.x *= mx;
    }
    if (m == idTable) {
        vec3 ddpos = (pos.x < 0.0 ? cc[0] : cc[1]) - pos;
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
    if ((m == idCupWhite || m == idCupBlack) || ((m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4 || m == idBoard) && pos.y < -0.00001)) {
        vec3 u = normalize(cross(nor, nBoard));
        vec3 v = cross(nor, u);
        vec3 ip = pos + v*dot(lpos - pos, -nBoard) / dot(v, -nBoard);
        float d = length(lpos - ip);
        float phi = 2.0*acos(d / ldia);
        float A = d < ldia ? 0.5*ldia2*(phi - sin(phi)) / (PI*ldia2) : 0.0;
        //ret.x *= dot(lpos - pos, nor) > 0.0 ? 1.0 - A : A;
        int i = m - idLeg1;
        vec2 xz = m == idCupBlack || m == idCupWhite ? (pos.x < 0.0 ? cc[0].xz : cc[1].xz) : m == idBoard ? vec2(0.0) : vec2(1 - 2 * (i & 1), 1 - 2 * ((i >> 1) & 1))*(bnx.xx + vec2(legh));
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
            vec4 xz12 = floor(iww * bmnx + 0.5*vec4(vec2(fNDIM - 1.0), vec2(fNDIM + 1.0)));
            ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM - 1.0));
            for (int i = mnx.y; i <= mnx.w; i++){
                for (int j = mnx.x; j <= mnx.z; j++){
                    int kk = NDIM*i + j;
                    float mm0 = iStones[kk].z;
                    if (mm0 >= cidBlackStone) {
                        vec2 xz = ww*iStones[kk].xy;
                        if (distance(pos.xz, xz) <= r1 + 0.001) {
                            float ldia = 1000.0;
                            float phi = PI;
                            if (isBoard) {
                                ret.y *= min(1.0, distance(pos.xz, xz) / r1);
                            }
                            else {
                                vec3 u = normalize(cross(nor, nBoard));
                                vec3 v = cross(u, nor);
                                float acs = acos(dot(v, normalize(vec3(pos.x, 0.000, pos.z) - vec3(xz.x, 0.0, xz.y))));
                                //ret.y *= min(1.0, acs / PI);
                            }
                        }
                        if (isBoard && pos.y > -0.001){
                            vec3 ddpos = vec3(ww*iStones[kk].xy, h).xzy - pos;
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

        if ((m == idCupBlack || m == idCupWhite || (m == idBowlBlackStone || m == idBowlWhiteStone)) && abs(pos.x) > 1.0) {
            int i = pos.x < 0.0 ? 0 : 1;
            int si = 0; int ei = 0;
            if (i == 0) { si = 0; ei = min(maxCaptured, iBlackCapturedCount); }
            else if (i == 1){ si = maxCaptured; ei = maxCaptured + min(maxCaptured, iWhiteCapturedCount); }
            for (int j = si; j < ei; ++j) {
                if ((m == idCupBlack || m == idCupWhite || uid != j) && pos.y < ddc[j].y - 0.01*h){
                    vec3 ddpos = ddc[j] - pos;
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
                else if ((m == idBowlBlackStone || m == idBowlWhiteStone) && (pos.y < ddc[j].y - 0.01*h || uid == j)) {
                    vec3 u = normalize(cross(nor, nBoard));
                    vec3 v = cross(nor, u);
                    ret.y *= min(1.0, 1.0 - abs(acos(dot(v.xz, normalize(pos.xz - ddc[j].xz)))) / PI);
                }
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
        ret = mBoard;
    }
    else if (m == idCupWhite) {
        ret = mBoard;
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

Material getMaterialColor(in Intersection ip, out vec3 mcol, in vec3 rd, in vec3 ro, out vec3 nn) {
    Material mat = getMaterial(ip.m);
    Material m0 = mat;//;at = mBoard;// getMaterial(idBoard);//ip.m
    bool noisy = false;
    float mult = 1.0;
    vec3 scoord;
    float sfreq;
    float mixcoef, smult1, smult2, smult3, sadd1, mixmult;
    float fpow = 1.0;
    mixmult = 1.0;
    smult1 = 0.0;
    smult2 = 0.0;
    smult3 = 1.0;
    mcol = m0.clrA;
    vec3 mcolb = m0.clrB;
    vec3 mcolc = m0.clrC;
    vec3 scrd2 = 64.0*(ip.p-ip.dummy.xyz);
    vec3 scrd = ip.p.xyz - ip.dummy.xyz;
    float degrade = (1.0 + floor(length(ro) / 3.0));
    vec2 xz;
    xz = scrd.xz;
    if (mat.id == mBoard.id || mat.id == mCupBlack.id || mat.id == mCupWhite.id) {
        scoord = 16.0*vec3(xz.x,scrd.y,xz.y) + vec3(0.0, 0.25, 0.0);
        scoord.x = 2.0*length(scoord.xy);
        scoord.z = 2.0*scoord.y;
        scoord.y = 2.0*length(scoord.xy);
        noisy = true;
        scrd = scoord;
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
		scrd2 *= 3.0;
		vec2 xz = mat2(cosa, sina, -sina, cosa)*scrd2.xz;
		scrd2 = xz.xyy;
		scrd2.y = scrd.y;
		scrd2.z = 1.0;
	}
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
            mcol = mix(mTable.clrA, mTable.clrC, max(c05b, c05a));
        }
        else if (!third){
            mcol = mix(mTable.clrB, mTable.clrC, max(c05b, c05a));
        }
        else {
            mcol = mTable.clrC;
            alpha = 0.0;
        }
            float cosa = cos(alpha);
            float sina = sin(alpha);
            xz = mat2(cosa, sina, -sina, cosa)*ip.p.xz;
            scoord = 16.0*vec3(xz.x, ip.p.y, xz.y) + vec3(0.0, 0.25, 0.0);
        scrd = 13.3*scoord;
        noisy = true;
        const float al = 0.15;
        float mixt = exp(-0.35*max(0.0, length(ip.p)));
        mcolb = mixt*(mcol + (al)*(vec3(1.0) - mcol));
        mcolc = mixt*((1.0 - al)*mcol);
        mcol *= mixt;
    }
    float rnd = 0.0;
    float rnd2 = 0.0;
    vec3 grad = vec3(0.0, 1.0, 0.0);
    vec3 grad2 = vec3(0.0, 1.0, 0.0);
    //if (noisy) {
        rnd2 = snoise(scrd2, grad2);
        rnd = snoise(scrd, grad);
    //}
    if (mat.id == mBoard.id || mat.id == mCupBlack.id || mat.id == mCupWhite.id) {
        float w1 = 3.0*length(scoord.yx - 0.5*vec2(1.57 + 3.1415*rnd));
        float w2 = 0.1*(scoord.x + scoord.z);
        smult1 = mix(rnd, 1.0, 0.05)*(clamp(0.25*(sin(grad.x)), 0.0, 1.0));
        smult2 = mix(rnd, 1.0, 0.05)*(clamp(0.25*(sin(1.5*grad.y)), 0.0, 1.0));

        smult3 = 1.0 - smult2;
    }
    else if (mat.id == mBlack.id || mat.id == mGrid.id || mat.id == mWhite.id || mat.id == mTable.id) {
        smult1 = clamp(abs(rnd),0.0,1.0);
        smult2 = clamp(abs(rnd2),0.0,1.0);
        smult3 = 0.5;//clamp(abs(grad.z),0.0,1.0);
        mixmult = 1.0;
    }
    mcol = mix(mix(mcol, mcolb, smult2), mix(mcol, mcolc, 1.0 - smult2), smult1);
    nn = normalize(mix(ip.n, grad2, 0.01));
    return mat;
}

vec3 shading(in vec3 ro, in vec3 rd, in Intersection ip, const Material mat, vec3 col){
    vec3 ret;
    if (mat.id == mBack.id) {
        ret = mat.clrA;
    }
    else {
        //vec3 smult0 = clamp(abs(col - mat.clrB) / abs(mat.clrA.x - mat.clrB), 0.0, 1.0);
        //float smult = 0.333 * dot(smult0, vec3(1.0));
        vec3 nn = ip.n;
        vec3 ref = reflect(rd, nn);
        vec3 lig = normalize(lpos - ip.p);
        vec3 lig2 = normalize(lpos2 - ip.p);
        vec3 lig3 = normalize(lpos3 - ip.p);
        vec2 shadow = pow(softshadow(ip.p, ip.n, lpos, ldia, ip.m, false, ip.uid), vec2(1.0, 0.25));//0.6
        //shadow += 0.4*pow(softshadow(ip.p, ip.n, lpos2, ldia, ip.m, false, ip.uid), vec2(1.0,0.25));

        float nny = 0.5 + 0.5*nn.y;
        float adsy = dot(vec3(0.6,0.3,0.3), clamp(vec3(dot(nn, lig), dot(nn, lig2), dot(nn, lig3)),0.0,1.0));
        vec4 pws = clamp(vec4(dot(ref, lig), dot(ref, lig2), dot(ref, lig3), dot(ref, lig3)), 0.0, 1.0);
        vec3 cupsab = ip.m == idBowlBlackStone || ip.m == idBowlWhiteStone ? vec3(0.125,0.9,0.25) : vec3(0.125,1.0,0.5);
        vec3 pwr = pow(pws.xyz, mat.specularPower*cupsab);
        vec3 score  = mat.diffuseAmbientSpecularWeight * vec3(adsy * shadow.x, shadow.y,shadow.y*(0.25*pwr.x + pwr.y + pwr.z));
        ret = (score.x + score.y)*col + score.z;
    }
    ret = pow(ret, gamma*exp(contrast*(vec3(0.5) - ret)));
    return ret;
}

vec3 render(in vec3 ro, in vec3 rd, in vec3 bg)
{
    vec3 col0, col1, col2;
    col0 = col1 = col2 = vec3(0.0);

    Intersection ret[2];

    castRay(ro, rd, ret);

    float alpha1 = smoothstep(boardaa, 0.0, -ret[0].d);
    float alpha2 = smoothstep(boardaa, 0.0, -ret[1].d);
    vec3 col, mcol;
    Material mat; 
    vec3 nn;
    mat = getMaterialColor(ret[0], mcol, rd, ro, nn);
    ret[0].n = nn;
    float w = alpha1;
    float wcol = (1.0-w);
    col = shading(ro, rd, ret[0], mat, mcol);
    if (ret[0].m == idBoard) {
        float alpha3 = smoothstep(boardaa, 0.0, -ret[0].d2);
        if(alpha3 > 0.0) {
            col = mix(col, shading(ro, rd, ret[0], mGrid, mGrid.clrA), alpha3);
        }
    }
    col *= (1.0-w);
    gl_FragDepth = (ret[0].m == mBlack.id || ret[0].m == mWhite.id) ? 0.5 : (ret[0].p.y < -0.001 ? 0.25 : 0.75);//; distance(ro, ret[0].p) / 100.0;
    if (alpha1 > 0.0) {
        //ret[0] = mix(ret[0].n, ret[1].n, alpha1)
        mat = getMaterialColor(ret[1], mcol, rd, ro, nn);
        ret[1].n = nn;
        //mcol = 0.5*(mat.clrA+mat.clrB);

        col1 = shading(ro, rd, ret[1], mat, mcol);
        if (ret[1].m == idBoard) {
            float alpha3 = smoothstep(boardaa, 0.0, -ret[1].d2);
            if(alpha3 > 0.0) {
                vec3 col3 = shading(ro, rd, ret[1], mGrid, mGrid.clrA);
                col1 = mix(col1, col3, alpha3);
            }
            col += alpha1*col1;
            wcol += alpha1;
        }
        else {
            col += alpha1*(1.0-alpha2)*col1;
            wcol += alpha1*(1.0-alpha2);
        }
    }
    return col/wcol;
}


void main(void)
{
    glFragColor =  render(roo, normalize(rdb), bgA);//
}
