#version 300 es
precision highp float;

/* === DO NOT CHANGE BELOW === */
const int MAXSTONES = 19*19;
const float cidBlackArea          = 2.0;
const float cidWhiteArea          = 3.0;
const float cidCapturedBlackStone = 5.5;
const float cidLastBlackStone     = 5.75;
const float cidBlackStone         = 5.0;
const float cidWhiteStone         = 6.0;
const float cidCapturedWhiteStone = 6.5;
const float cidLastWhiteStone     = 6.75;

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
layout(std140) uniform iStoneBlock {
    vec4 iStones[MAXSTONES];
};
uniform mat4 glModelViewMatrix;
uniform vec3 iTranslate;

/* === DO NOT CHANGE ABOVE == */

const float PI = 3.1415926535;
const float farClip = 10000.0;
const vec2 noIntersection2 = vec2(farClip,farClip);
const vec4 noIntersection4 = vec4(farClip);
const vec3 lpos = vec3(-2.0,12.0,-2.0);
const vec3 lpos2 = vec3(-4.0,12.0,-12.0);
const vec3 lpos3 = vec3(12.0,6.0,-8.0);
const float ldia = 3.5;
const vec3 nBoard = vec3(0.0,1.0,0.0);
const vec3 minBound = vec3(-1.2,-0.02,-1.2);
const vec3 p = vec3(0.0,-0.25,0.0);
const vec3 c = vec3(1.0, 0.25, 1.0);
const vec4 bnx = vec4(-c.x,-0.1,-c.x,0.0);
const float mw = 0.85;
const float legh = 0.01;

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
    float diffuseWeight;
    float specularWeight;
    float ambientWeight;
    float bacWeight;
    float fresnelWeight;
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
    vec3 nn;
    int m;
    float d;
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

const vec3 bgB = vec3(0.2,0.2,0.2);
const vec3 bgA = vec3(0.0,0.0,0.0);

const Material mCupBlack = Material(idCupBlack, 0.4, 0.15, 0.6, 0.0, 0.0, 64.0, vec3(0.65826, 0.528209, 0.238209), vec3(0.387763, 0.3289191, 0.12761), vec3(0.22005, 0.180002, 0.1244), 1.3);
const Material mCupWhite = Material(idCupWhite, 0.4, 0.15, 0.6, 0.0, 0.0, 64.0,vec3(0.45826, 0.428209, 0.238209), vec3(0.287763, 0.289191, 0.12761), vec3(0.12005, 0.120002, 0.085744), 1.3);
const Material mBoard = Material(idBoard, 0.6, 0.15, 0.6, 0.0, 0.0, 128.0, vec3(0.55, 0.55, 0.3), vec3(0.5,0.5,0.3), vec3(0.45,0.45, 0.3), 1.5);
const Material mTable = Material(idTable, 1.2, 0.1, 0.05, 0.0, 0.0, 16.0, vec3(0.45), vec3(0.45), vec3(0.45), 0.0);//vec3(0.8,0.2,0.2), vec3(0.7,0.1,0.1), vec3(0.6,0.2,0.2));
const Material mWhite = Material(idWhiteStone, 0.23, 0.43, 0.63, 0.3, 0.4, 128.0, vec3(1.0), vec3(0.99), vec3(1.0), 0.5);
const Material mBlack = Material(idBlackStone, 0.23, 0.43, 0.63, 0.3, 0.4, 128.0, vec3(0.08), vec3(0.04), vec3(0.0), 0.5);
const Material mRed = Material(idLastBlackStone, 0.3, 0.25, 0.7, 0.3, 0.4, 16.0, vec3(0.5,0.0,0.0), vec3(0.5,0.0,0.0), vec3(0.5,0.0,0.0), 0.0);
const Material mBack  = Material(idBack, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, bgA, bgB, bgA, 0.0);
const Material mGrid  = Material(idGrid, 1.5, 0.15, 0.4, 0.4, 0.4, 128.0, vec3(0.0),vec3(0.0), vec3(0.0), 0.0);

uniform vec3 cc[2];
//float ddcy = 0.5*h-bowlRadius;
const int maxCaptured = 32;
uniform vec3 ddc[6*maxCaptured];

bool IntersectBox ( in vec3 ro, in vec3 rd, in vec3 minimum, in vec3 maximum, out float start, out float final) {
    vec3 ird = 1.0 / rd;
    vec3 OMIN = ( minimum - ro ) * ird;
    vec3 OMAX = ( maximum - ro ) * ird;
    vec3 MAX = max ( OMAX, OMIN );
    vec3 MIN = min ( OMAX, OMIN );
    final = min ( MAX.x, min ( MAX.y, MAX.z ) );
    start = max ( max ( MIN.x, 0.0 ), max ( MIN.y, MIN.z ) );
    return final > start;
}

float IntersectBox ( in vec3 ro, in vec3 rd, in vec3 minimum, in vec3 maximum ) {
    vec3 ird = 1.0 / rd;
    vec3 OMIN = ( minimum - ro ) * ird;
    vec3 OMAX = ( maximum - ro ) * ird;
    vec3 MAX = max ( OMAX, OMIN );
    return min ( MAX.x, min ( MAX.y, MAX.z ) );
}

vec2 intersectionRaySphere(in vec3 ro, in vec3 rd, in vec3 center/*float radius2*/){
    vec3 vdif = ro - center;
    float dt = dot(rd, vdif);
    float x = dt*dt - dot(vdif, vdif) + stoneRadius2;
    vec2 ret;
    if(x >= 0.0) {
       float sqt = sqrt(x);
       ret = vec2(-dt-sqt, -dt+sqt);
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
    if(x >= 0.0) {
       float sqt = sqrt(x);
       ret = vec2(-dt-sqt, -dt+sqt);
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
    if(x >= 0.0) {
        float sqt = sqrt(x);
        ret = vec2(-dt-sqt, -dt+sqt)/den;
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
    if(x >= 0.0) {
        float sqt = sqrt(x);
        tt.xy = vec2(-dt-sqt, -dt+sqt)/den;
        float r2dr1 = r2/r1;
        ret = vec4(normalize((ro + tt.x*rd-center)*vec3(r2dr1, r1/r2, r2dr1)), tt.x);
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
    vec3 dt = n*(p - mat3(o,o,o));
    return p - mat3(dt.x*n, dt.y*n, dt.z*n);
}

vec3 point2line(vec3 p, vec3 o, vec3 u){
    vec3 v = p - o;
    return o + dot(u, v) * u / dot(v,v);
}
float circleHalfPlaneIntersectionArea(vec3 c, float r, mat3 cs) {
    vec3 p = cs[0];
    vec3 v = cs[1] - cs[0];
    vec3 n = cross(cross(v, c - p), v);
    bool isInside = sign(dot(c-p, n)) == sign(dot(cs[2]-p, n));
    float h = r - length(cross(p - c, p + v - c))/length(v);
    float ret;
    if(h <= 0.0) {
        ret = isInside ? 1.0 : 0.0;
    }
    else {
        float A = acos((r-h)/r)/PI - (r-h)*sqrt(2.0*h*r - h*h)/(PI*r*r);
        ret = isInside ? 1.0 - A : A;
    }
    return ret;
}

vec3 distanceRayStoneSphere(in vec3 ro, in vec3 rd, in vec3 dd, float radius2){
		vec3 p = ro + dot(dd - ro, rd) * rd;
		float dist = length(p - dd) - radius2;
		vec3 q = dd + radius2*normalize(p - dd);
		float d = length(p.xz - dd.xz);
    return vec3(d - abs(p.y - dd.y)*px/abs(radius2 - 0.5*h + 0.5*b), dist, distance(p, ro));
}

float distanceRaySphere(in vec3 ro, in vec3 rd, in vec3 dd, float radius2){
    vec3 u = cross(dd - ro, dd - ro - rd);
    vec3 v = normalize(cross(rd, u));
    float dist = length(u)/length(rd)-radius2;
    vec3 x0 = dd + length(u)*v;
    return dist/distance(x0, ro);
}

float distanceLineLine(vec3 ro, vec3 rd, vec3 p0, vec3 v, out float s, out vec3 q2) {
    vec3 p21 = p0 - ro;
    vec3 m = cross(v, rd);
    float m2 = dot(m, m);
    vec3 rr = cross(p21, m/m2);
    s = dot(rr, v);
    vec3 q1 = ro + s * rd;
    q2 = p0 + dot(rr, rd) * v;
    return -distance(q1, q2)/distance(ro, q1);
}

vec2 distanceRayCircle(in vec3 ro, in vec3 rd, in vec3 ip, in vec2 t0, in vec3 dd, float r, out vec3 q2){
    vec3 dif = vec3(ip.x, dd.y, ip.z) - dd;
    vec3 x0 = dd + r*normalize(dif);
    float s = 0.0;
    return vec2(abs(t0.x - s) - abs(t0.y - s), sign(length(dif)-r) * distanceLineLine(ro, rd, x0, normalize(vec3(dd.z-ip.z, 0.0, ip.x-dd.x)), s, q2));
}

vec2 distanceRayStone(in vec3 ro, in vec3 rd, in vec3 dd){
    vec3 R = vec3(r1,r2,r1);
    vec3 A = ro - dd;
    vec3 B = A + rd;
    vec3 v = A - B;
    vec3 rr = R.yzx * R.zxy;
    float den = dot(v*v, rr*rr);
    vec3 RR = R*R;
    vec3 BB = B*B;
    vec3 xyz0 = RR*vec3(
       dot(RR.yz, A.xx*BB.zy + A.zy*(B.xx*(A.zy-B.zy)-A.xx*B.zy)),
       dot(RR.zx, A.yy*BB.xz + A.xz*(B.yy*(A.xz-B.xz)-A.yy*B.xz)),
       dot(RR.xy, A.zz*BB.yx + A.yx*(B.zz*(A.yx-B.yx)-A.zz*B.yx))
    );
    vec3 X0 = xyz0/den;
    vec3 ro2 = X0/R;
    vec3 rd2 = -normalize(ro2/(R*R));
    float dt = dot(rd2, ro2);
    float sqt = sqrt(max(0.0, dt*dt - dot(ro2, ro2) + 1.0));
    float dd0 = length((dt+sqt)*R*rd2);///*rd2  dot(rd,X0-ro);
    return vec2(-dd0, distance(ro, X0+dd));
}

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

bool intersectionRayStone(in vec3 ro, in vec3 rd, in vec3 dd, float mint, out vec4 res, out float t2){
    vec2 dpre = intersectionRaySphereR(ro, rd, dd, r1*r1);
    bool rval = false;
    if(dpre.x < mint && dpre.y > 0.0) { /*dd.y -  0.5*h*/
        vec2 d2 = intersectionRaySphere(ro, rd, dd - dn);
        float d2x = d2.x < 0.0 ? d2.y : d2.x;
        if(d2x < mint && d2x > 0.0) {
            vec2 d1 = intersectionRaySphere(ro, rd, dd + dn);
            float p1 = max(d1.x, d2.x);
            float p2 = min(d1.y, d2.y);
            if(p1 >= 0.0 && p1 < p2 && p1 < mint) {
                vec2 d3 = intersectionRayCylinder(ro, rd, dd, px*px);
                vec2 d4;
                vec4 tt = intersectionRayEllipsoid(ro, rd, dd, d4);
                vec3 ip = ro+ p1*rd;
                vec3 rcc = dd + (d1.x < d2.x ? -dn : dn);
                vec3 n = normalize(ip - rcc);
                if(p1 > d3.x && p1 < d3.y && d3.x < farClip) {
                  res = vec4(n ,p1);
                  t2 = p2;
                  rval = true;
                }
                else if(tt.x < farClip) {
                    n.y = mix(n.y, tt.y, smoothstep(px,r1,length(ip.xz-dd.xz)));
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

void updateResult(inout Intersection result[2], Intersection ret) {
    if(ret.t.x <= result[0].t.x) {
        result[1] = result[0];
        result[0] = ret;
    }
    else if(ret.t.x <= result[1].t.x) {
        result[1] = ret;
    }
}

void finalizeStoneIntersection(in vec3 ro, in vec3 rd, inout Intersection ret[2], const int i) {
    vec3 dd = ret[i].dummy.xyz;
    bvec3 isStone = bvec3(ret[i].m == idBlackStone || ret[i].m == idWhiteStone || ret[i].m == idBowlBlackStone || ret[i].m == idBowlWhiteStone,
    	ret[i].m == idCapturedBlackStone || ret[i].m == idCapturedWhiteStone,
    	ret[i].m == idLastBlackStone || ret[i].m == idLastWhiteStone);
    vec3 dist0a;
    Intersection r2 = ret[i];
	if(any(isStone)) {
        dist0a = distanceRayStoneSphere(ro, rd, dd -dn, stoneRadius);
        vec3 dist0b = distanceRayStoneSphere(ro, rd, dd +dn, stoneRadius);
        dist0a = dist0a.y > dist0b.y ? dist0a : dist0b;
        dist0b.yz = distanceRayStone(ro, rd, dd);
        ret[i].d = mix(dist0a.y/dist0a.z, dist0b.y/dist0b.z, step(0.0, dist0a.x));
    }
    if(any(isStone.yz)) {
        vec3 cc = dd;
        cc.y -= dn.y - sqrt(stoneRadius2-0.25*r1*r1);
        vec3 ip0 = ro + rd*dot(cc - ro, nBoard)/dot(rd, nBoard);
        vec3 q2;
		int idStone = (ret[i].m == idLastBlackStone || ret[i].m == idCapturedBlackStone) ? idBlackStone : idWhiteStone;
		bool isNotArea = length(ret[i].p.xz - cc.xz) > 0.5*r1;
		if(isNotArea) {
			vec2 rr = distanceRayCircle(ro, rd, ip0, ret[i].t, cc, 0.5*r1, q2);
			r2.d = abs(rr.y);
		 	if(rr.x <0.0 && r2.d < boardaa){
				r2.d = max(-r2.d, ret[i].d);
				r2.m = idStone;
				updateResult(ret, r2);
			}else{
				ret[i].m = idStone;
			}
	    }
	}
}
vec2 softshadow( in vec3 pos, in vec3 nor, const vec3 lig, const float ldia, int m, bool ao, int uid ){
    vec2 ret = vec2(1.0);
    bool isBoard = m == idBoard || m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4 || m == idGrid;
    if(isBoard && pos.y > h) return ret;
    const float eps = -0.00001;
    float ldia2 = ldia*ldia;
    if (pos.y < -eps) {
        //nn = normalize(pos-cc);
        vec3 ldir = -pos;
        float res = dot(lig-pos,nBoard)/dot(ldir, nBoard);
        vec3 ip = pos+res*ldir;
        //float d = length(ip - lig);
        vec2 rr = vec2(abs(bnx.x)*length(ip - pos)/length(pos));
        vec2 ll = vec2(ldia);
        vec4 xz = vec4(min(ip.xz + rr, lpos.xz + ll), max(ip.xz - rr, lpos.xz - ll));
        vec2 xz1 = max(vec2(0.0), xz.xy - xz.zw);

        vec3 dd = vec3(0.0, bnx.y, 0.0);
        ldir = dd - pos;
        res = dot(lig - pos, nBoard)/dot(ldir, nBoard);
        ip = pos + res*ldir;
        rr = vec2(abs(bnx.x)*length(ip -pos)/length(dd -pos));
        xz = vec4(min(ip.xz + rr, lpos.xz + ll), max(ip.xz - rr, lpos.xz - ll));
        vec2 xz2 = max(vec2(0.0), xz.xy - xz.zw);
        float mx = mix(1.0, 1.0 - 0.5*((xz1.x*xz1.y)+(xz2.x*xz2.y))/(4.0*ldia2), clamp(-pos.y/boardaa, 0.0, 1.0));
        //if(isBoard && m!= idBoard)
        ret.x *= mx;
     }
     if(m != idTable) {
        float t1, t2;
        vec3 rd = lig - pos;
        if(IntersectBox(pos, rd, minBound, maxBound, t1, t2)) {
            vec4 b12 = pos.xzxz + vec4(t1, t1, t2, t2)*rd.xzxz;
            vec4 bmnx = vec4(min(b12.xy, b12.zw), max(b12.xy, b12.zw));
            vec4 xz12 = floor(iww * bmnx + 0.5*vec4(vec2(fNDIM-1.0), vec2(fNDIM+1.0)));
            ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM-1.0));
            for(int i = mnx.y; i <= mnx.w; i++){
                for(int j = mnx.x; j <= mnx.z; j++){
                    int kk = NDIM*i + j;
                    float mm0 = iStones[kk].z;
                    if(mm0 >= cidBlackStone) {
                        vec2 xz = ww*iStones[kk].xy;
                        if(distance(pos.xz, xz)<=r1+0.001) {
                            float ldia = 1000.0; 
                            float phi = PI;
                            if(isBoard) {
                                ret.y *= min(1.0,distance(pos.xz, xz)/r1);
                            }
                            else {
                                vec3 u = normalize(cross(nor, nBoard));
                                vec3 v = cross(u, nor);
                                float acs = acos(dot(v,normalize(vec3(pos.x, 0.0,pos.z)-vec3(xz.x,0.0,xz.y))));
                                ret.y *= min(1.0, acs/PI);
                            }
                        }
                        if (isBoard && pos.y > -0.001){ 
                            vec3 ddpos = vec3(ww*iStones[kk].xy, h).xzy - pos;
                            float ln = length(ddpos);
                            vec3 ldir = ddpos;
                            float res = dot(lig - pos, nBoard)/dot(ldir, nBoard);
                            vec3 ip = pos + res*ldir;
                            vec2 rR = vec2(r1*length(ip-pos)/ln, ldia);
                            vec2 rR2 = rR*rR;
                            float A = PI*min(rR2.x, rR2.y);
                            float d = length(ip - lig);
                            vec4 rmR = vec4(rR + rR.yx, rR - rR.yx);
                            if(d > abs(rmR.z) && d < rmR.x) {
                                vec2 rrRR = rR2.xy - rR2.yx;
                                vec2 d12 = 0.5*(vec2(d) + rrRR/d);
                                float dt = (-d + rmR.x)*(d + rmR.z)*(d + rmR.w)*(d + rmR.x);
                                A = dot(rR2, acos(d12/rR)) - 0.5*sqrt(dt);
                            }
                            else if(d > rmR.x) {
                                A = 0.0;
                            }
                            ret.x *= 1.0 - A/(PI*ldia2);
                        }
                   }
                }
            }
         }
    }
    return ret;
}


void castRay(in vec3 ro, in vec3 rd, out Intersection result[2]) {

    Intersection ret;
    ret.m = idBack;
    ret.d= -farClip;
    ret.n = nBoard;
    ret.t = vec2(farClip);
    ret.p = vec3(-farClip);
    ret.isBoard = false;
    result[0] = result[1] = ret;

    float tb, tb2;
    vec3 q2;
    bool isBoard = IntersectBox(ro, rd, bnx.xyz, -bnx.xwz, tb, tb2);
    bool isBoardTop = false;
    if(isBoard) {
        vec3 ip = ro  + tb*rd;
        vec3 n0, n1;
        vec3 cc = vec3(0.0,0.5*bnx.y,0.0);
        vec3 ip0 = ip - cc;
        vec3 bn0 = bnx.xyz - cc;
        vec3 dif = (abs(bn0) - abs(ip0))/abs(bn0);
        float dist0 = distanceRaySquare(ro, rd, ip, bnx.xyz, -bnx.xwz, q2);
        float dd = farClip;
        bool extremeTilt = false;

        if(all(lessThan(dif.xx, dif.yz))) {
           n1 = vec3(sign(ip0.x), 0.0, 0.0);
           n0 = vec3(0.0, ip0.y, ip0.z);
        }
        else if(all(lessThan(dif.yy, dif.xz))){

           n1 = vec3(0.0, sign(ip0.y), 0.0);
           n0 = vec3(ip0.x, 0.0, ip0.z);
           //const float dw = 0.00075;
           bool gridx = ip.x > -c.x + mw*ww - 0.5*ww && ip.x < c.x - mw*ww + 0.5*ww && ip.z > -c.z + mw*ww - dw && ip.z < c.z - mw*ww + dw;
           bool gridz = ip.x > -c.x + mw*ww - dw && ip.x < c.x - mw*ww + dw && ip.z > -c.z + mw*ww - 0.5*ww && ip.z < c.z - mw*ww + 0.5*ww;
           float r = boardbb*distance(ro, ip);
           if(ip0.y > 0.0) {
    						isBoardTop = true;
                bvec2 nearEnough =  lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(dw + dw + r));
                if(any(nearEnough)) {
                    vec3 dir = vec3(dw, -dw, 0.0);
                    if(nearEnough.x && gridx) {
                        vec3 ccx = vec3(ww*round(iww*ip.x), ip.y, ip.z);
                        mat3 ps = mat3(ccx + dir.yzx, ccx + dir.yzy, ccx + dir.yzz + dir.yzz);
                        mat3 cs = point32plane(ps, ip, rd);
                        float a1 = circleHalfPlaneIntersectionArea(ip, r, cs);
                        ps = mat3(ccx + dir.xzx, ccx + dir.xzy, ccx + dir.xzz + dir.xzz);
                        cs = point32plane(ps, ip, rd);
                        float a2 = circleHalfPlaneIntersectionArea(ip, r, cs);
                        dd = a1+a2;
                        //n1 = normalize(vec3(-0.01*sign(ip.x - ccx.x)*(1.0-abs(ip.x - (ccx.x + sign(ip.x - ccx.x)*dw))/dw), n1.y, n1.z));//mix(0.5*abs(ccx.x -ip.x)/dw, 0.0, 0.5*abs(ccx.x -ip.x)/dw)));
                    }
                    if(nearEnough.y && gridz) {
                        vec3 ccz = vec3(ip.x, ip.y, ww*round(iww*ip.z));
                        mat3 ps = mat3(ccz + dir.xzy, ccz + dir.yzy, ccz + dir.zzy + dir.zzy);
                        mat3 cs = point32plane(ps, ip, rd);
                        float a1 = circleHalfPlaneIntersectionArea(ip, r, cs);
                        ps = mat3(ccz + dir.xzx, ccz + dir.yzx, ccz + dir.zzx + dir.zzx);
                        cs = point32plane(ps, ip, rd);
                        float a2 = circleHalfPlaneIntersectionArea(ip, r, cs);
                        dd = min(a1+a2, dd);
                        //n1 = normalize(vec3(n1.x,n1.y,-0.01*sign(ip.z - ccz.z)*(1.0-abs(ip.z - (ccz.z + sign(ip.z - ccz.z)*dw))/dw)));//mix(0.5*abs(ccz.z -ip.z)/dw, 0.0, 0.5*abs(ccz.z -ip.z)/dw)));
                   }
               }
               float rr = 0.1*ww;//8.0*dw;
               nearEnough =  lessThan(abs(ip.xz - ww*round(ip.xz*iww)), vec2(rr + rr + r));
               if(any(nearEnough)) {
                   vec3 ppos;
                   float mindist = distance(ip.xz, vec2(0.0));
                   vec3 fpos =  vec3(0.0);
                   if(NDIM == 19) {
                      ppos = vec3(6.0, 0.0, 6.0);
                      for(int i = -1; i <= 1; i++) {
                        for(int j = -1; j <= 1; j++) {
                            vec3 pos = ww*vec3(i, 0, j)*ppos.zyz;
                            float dst = distance(ip.xz, pos.xz);
                            if(dst < mindist) {
                                mindist = dst;
                                fpos = pos;
                            }
                        }
                     }
                   }
                   else if (NDIM == 13){
                   		ppos = vec3(3.0, 0.0, 3.0);
                      for(int i = -1; i <= 1; i+=2) {
                        for(int j = -1; j <= 1; j+=2) {
                            vec3 pos = ww*vec3(i, 0, j)*ppos.zyz;
                            float dst = distance(ip.xz, pos.xz);
                            if(dst < mindist) {
                                mindist = dst;
                                fpos = pos;
                            }
                        }
                     }
                  }
                   if(mindist < rr + rr) {
                        vec3 q2;
                        float dd0 = -distanceRayCircle(ro, rd, ip, vec2(farClip), fpos, rr, q2).y;
                        if(dd0 < 0.0) {
                            dd = min(dd, 1.0+dd0/boardaa);
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
        ret.p = ip;
        ret.isBoard = isBoardTop;

        if(dist0 > -boardaa) {
	        float tc, tc2;
            IntersectBox(ro, q2 - ro, bnx.xyz, -bnx.xwz, tc, tc2);
            if(tc2 - tc > 0.001) {
	            n0 = normalize(n0);
	            vec3 cdist = abs(abs(ip0) - abs(bn0));
	        	vec2 ab = clamp(vec2(abs(dist0), min(cdist.z, min(cdist.x, cdist.y)))/boardcc, 0.0, 1.0);
	            n0 = normalize(mix(normalize(ip0), n0, ab.y));
	            n0 = normalize(mix(n0, n1, ab.x)); 
	            ret.n = n0;
                ret.d = -farClip;
                ret.p = q2;
                updateResult(result, ret);
            }
        }
        ret.n = n1;

        if(dd < 1.0 && dist0 < -dd*boardaa) {
            ret.d = dist0;
            ret.m = idGrid;
            ret.p = ip;
            updateResult(result, ret);
        }
	    if(dd > 0.0) {
	        ret.m = idBoard;
	       	dist0 = max(dist0, -dd*boardaa);
	        ret.p = ip;
	        ret.d = dist0;
        	updateResult(result, ret);
        }
        ret.isBoard = false;
    }
    float t1, t2;
    if(IntersectBox(ro, rd, minBound, maxBound, t1, t2)) {
        vec4 b12 = ro.xzxz + vec4(t1, t1, t2, t2)*rd.xzxz;
        vec2 noise = vec2(ww*0.2);
        vec4 bmnx = vec4(min(b12.xy, b12.zw)-noise, max(b12.xy, b12.zw)+noise);
        vec4 xz12 = floor(iww * bmnx + vec4(0.5*fNDIM));
        ivec4 mnx = ivec4(clamp(xz12, 0.0, fNDIM-1.0));
        for(int i = mnx.y; i <= mnx.w; i++){
            for(int j = mnx.x; j <= mnx.z; j++){
                vec4 stone0 = iStones[NDIM*i + j];
                int mm0 = idBack;
                float m0 = stone0.z;
                if(m0 >= cidBlackStone) {
                    vec2 xz = ww*stone0.xy;
                    vec3 dd = vec3(xz.x,0.5*h,xz.y);
                    float tt2;
                    vec4 ret0;
                    if(intersectionRayStone(ro, rd, dd, result[1].t.x, ret0, tt2)) {
                        //vec3 w25 = vec3(0.25*w, h*vec2(-0.5, 0.6));
                        //vec3 minb = dd - w25.xyx;;
                        //vec3 maxb = dd + w25.xzx;
                        vec3 ip = ro + rd*ret0.w;
                        bool captured = m0 == cidCapturedBlackStone || m0 == cidCapturedWhiteStone;
                        bool last = m0 == cidLastBlackStone || m0 == cidLastWhiteStone;
                        if((captured || last)) {
                            mm0 = captured ? (m0 == cidCapturedWhiteStone ? idCapturedWhiteStone : idCapturedBlackStone) :
                            (m0 == cidLastWhiteStone ? idLastWhiteStone : idLastBlackStone) ;
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
                    vec3 dd = vec3(vec2(j, i)-vec2(0.5*fNDIM - 0.5),0.0)*ww;
                    vec2 w25 = vec2(0.25*ww, dd.z);
                    vec3 minb = dd.xzy - w25.xyx;
                    vec3 maxb = dd.xzy + w25.xyx;
                    vec3 ip = ro + rd*tb;
                    if(all(equal(clamp(ip.xz, minb.xz, maxb.xz), ip.xz))) {
                        mm0 = stone0.z == cidBlackArea ? idBlackArea : idWhiteArea;
                        ret.m = mm0;
                        ret.t = vec2(tb);
                        ret.d = distanceRaySquare(ro, rd, ip, minb, maxb, q2);
                        ret.n = nBoard;
                        ret.p = ip;
                        ret.dummy.w = stone0.w;
                    }
                }

                if(mm0 > idBack) {
                    if(ret.t.x <= result[0].t.x) {
                        result[1] = result[0];
                        result[0] = ret;
                    }
                    else if(ret.t.x <= result[1].t.x) {
                        result[1] = ret;
                    }
                }
            }
        }
    }
    float t = dot(vec3(0.0,bnx.y-legh,0.0)-ro,nBoard)/dot(rd,nBoard);
    bool aboveTable = ro.y >= bnx.y-legh;
    if(t > 0.0 && t < farClip && aboveTable) {
        ret.m = idTable;
        ret.t = vec2(t);
        ret.p = ro + t*rd;
        ret.n = nBoard;
        ret.d = -farClip;
        updateResult(result, ret);
    }
    finalizeStoneIntersection(ro, rd, result, 1);
    finalizeStoneIntersection(ro, rd, result, 0);
    //result[2] = result[1];
}


Material getMaterial(int m) {
    Material ret;
    if(m == idTable) {
        ret = mTable;
    }
    else if(m == idBoard || m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4) {
        ret = mBoard;
    }
    else if(m == idCupBlack) {
        ret = mBoard;
    }
    else if(m == idCupWhite) {
        ret = mBoard;
    }
    else if(m == idWhiteStone || m == idWhiteArea || m == idCapturedBlackStone || m == idBowlWhiteStone) {
        ret = mWhite;
    }
    else if(m == idBlackStone || m == idBlackArea || m == idCapturedWhiteStone || m == idBowlBlackStone) {
        ret = mBlack;
    }
    else if(m == idLastBlackStone || m == idLastWhiteStone) {
    		ret = mRed;
    }
    else if(m == idGrid) {
        ret = mGrid;
    }
    else {
        ret = mBack;
    }
    return ret;
}

Material getMaterialColor(in Intersection ip, out vec3 mcol, in vec3 rd, in vec3 ro) {
    Material mat = getMaterial(ip.m);
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
    mcol = mat.clrA;
    vec3 mcolb = mat.clrB;
    vec3 mcolc = mat.clrC;
    vec3 scrd;
    float degrade = (1.0+floor(length(ro)/3.0));
		    //float alpha = iTime;
        //float cosa = cos(alpha);
        //float sina = sin(alpha);
        //vec2 xz = mat2(cosa,sina, -sina, cosa)*ip.p.xz;

    if(mat.id == mBoard.id || mat.id == mCupBlack.id || mat.id == mCupWhite.id) {
            scoord = 16.0*(ip.p+vec3(0.0,0.25,0.0));
            //scoord.z *= 0.24;
            //scoord.y += 1.2;
            scoord.x *= 6.14;
            scoord.y *= 5.14;
        noisy = false;
        scrd = 0.5*scoord;
    }
    else if (mat.id == mGrid.id) {
        scoord = 350.0*ip.p;
        sfreq = 1.5;
        noisy = false;
        scrd = scoord;
    }
    else if(mat.id == mWhite.id || mat.id == mBlack.id) {
        float alpha = ip.dummy.w;
        float cosa = cos(alpha);
        float sina = sin(alpha);
        vec2 xz = mat2(cosa,sina, -sina, cosa)*ip.p.xz;
        scoord = xz.xyy;
        scoord.y = 1.0;
        sfreq = 1.0*(1.0 + 0.1*sin(11.0+xz.x));
        sadd1 = 5.0 + sin(131.0+15.0*xz.x);
        noisy = false;
        scrd = 1117.0*scoord;
    }
    else if (mat.id == mTable.id) {
    	mcol = mat.clrC;
    }
    float rnd = 0.0;
    if(mat.id == mBoard.id || mat.id == mCupBlack.id || mat.id == mCupWhite.id) {
        float w1 = 3.0*length(scoord.yx-0.5*vec2(1.57+3.1415*rnd));
        float w2 = 0.1*(scoord.x + scoord.z);
        smult1 = mix(rnd,1.0,0.05)*(clamp(0.25*(sin(w1)),0.0,1.0));
        smult2 = mix(rnd,1.0,0.05)*(clamp(0.25*(sin(1.5*w1)),0.0,1.0));
        smult3 = 1.0-smult2;
    }
    else if (mat.id == mBlack.id || mat.id == mGrid.id) {
            smult1 = rnd;
            smult2 = 0.5;
            smult3 = 1.0 - smult2;
            mixmult = 1.0;
    }
    else if(mat.id == mWhite.id) { 
       //scoord.x = length(scoord.xy);
       smult1 = abs(sin(11.0+131.0*scoord.x));
       smult2 = abs(sin(1.3-101.0*scoord.x)+sin(1.3-121.0*scoord.x));
       smult3 = 1.0 - smult2;
    }
    else {
       smult3 = 1.0 - smult2;
    }
    smult1 = mix(0.5,smult1, exp(-0.05*length(ip.p-ro)));
    smult2 = mix(0.5,smult2, exp(-0.05*length(ip.p-ro)));
    mcol = mix(mix(mcol, mcolb, smult2), mix(mcol, mcolc,1.0-smult2), smult1);
    return mat;
}
vec3 shading(in vec3 ro, in Intersection ip, const Material mat, vec3 col){
    vec3 ret;
    if(mat.id == mBack.id) {
    	ret = mat.clrA;
    }
    else {
	    vec3 rd = normalize(ip.p - ro);
	    vec3 smult0 = vec3(0.01);
	    smult0 = clamp(abs(col - mat.clrB)/abs(mat.clrA.x - mat.clrB),0.0,1.0);
	    float smult = (smult0.x + smult0.y + smult0.z)/3.0;
	    vec3 nn = ip.n;//ip.isBoard ? ip.nn : ip.n;// mat.id != mWhite.id && mat.id != mBlack.id ? ip.nn : ip.n;//normalize(mix(ip.n, ip.nn, 0.15));
	    vec3 ref = reflect(rd, nn);
	    //max4x3 ligs = mat4x3(lig,lig2,lig3,lig);
	    //mat4x4 ress = ligs*ligs;
 		    float alpha = 0.0;//2.1*iTime;
        float cosa = cos(alpha);
        float sina = sin(alpha);
        mat2 mm = mat2(cosa,sina, -sina, cosa);

			vec3 lpos =vec3(mm * lpos.xz, lpos.y).xzy;
			vec3 lpos2 =vec3(mm * lpos2.xz, lpos2.y).xzy;
			vec3 lpos3 =vec3(mm * lpos3.xz, lpos3.y).xzy;
	    vec3 lig = normalize(lpos - ip.p);
	    vec3 lig2 = normalize(lpos2 - ip.p);
	    vec3 lig3 = normalize(lpos3 - ip.p);
	    vec2 shadow = 0.75*softshadow( ip.p, ip.n, lpos, ldia, ip.m, false, ip.uid);
	    shadow += 0.25*softshadow( ip.p, ip.n, lpos2, ldia, ip.m, false, ip.uid);
	    
		float nny = 0.5+0.5*nn.y;
	    vec2 ads = 
		0.6*clamp(vec2(nny, dot(nn, lig)), 0.0,1.0) +
  		0.3*clamp(vec2(nny, dot(nn, lig2)),0.0,1.0) +
  		0.3*clamp(vec2(nny, dot(nn, lig3)),0.0,1.0);

            vec4 pws = clamp(vec4(dot(ref, lig), dot(ref, lig2), dot(ref, lig3), dot(ref, lig3)), 0.0,1.0);
	    float pw = exp(mat.refl*(-0.5-smult)); 
			float cupsa = ip.m == idBowlBlackStone || ip.m == idBowlWhiteStone ? 0.9 : 1.0;
			float cupsb = ip.m == idBowlBlackStone || ip.m == idBowlWhiteStone ? 0.5 : 1.0;
	    ret = dot(
            vec2(mat.diffuseWeight, mat.ambientWeight),
            vec2(ads.y * shadow.x,sqrt(shadow.y))
        )*col + cupsb*shadow.x*mat.specularWeight*(0.25*pow(pws.x, 0.125*mat.specularPower)+pow(pws.y, cupsb*pw*mat.specularPower)
		+pow(pws.z, cupsb*0.5*pw*mat.specularPower));
	    if(ip.m == idTable) {
	       ret = mix(mBack.clrA, ret, exp(-0.35*max(0.0,length(ip.p))));
	   }
	  }
	  ret = pow(ret, gamma*exp(contrast*(vec3(0.5)-ret)));
    return ret;
}
vec3 render(in vec3 ro, in vec3 rd, in vec3 bg)
{
    vec3 col0, col1, col2;
    col0 = col1 = col2 = vec3(0.0);

    Intersection ret[2];

		castRay(ro, rd, ret);

    float alpha1 = smoothstep(boardaa, 0.0, -ret[0].d);
    vec3 col, mcol;
		Material mat;
		mat = getMaterialColor(ret[0], mcol, rd, ro);
        col0 = shading(ro, ret[0], mat, mcol);
	    if(alpha1 > 0.05) {
            mat = getMaterialColor(ret[1],mcol,rd, ro);
            col1 = shading(ro, ret[1], mat, mcol);
            float alpha2 = smoothstep(boardaa, 0.0, -ret[1].d);
	        if(ret[1].m == idBoard && alpha2 > 0.0) {
	            col2 = shading(ro, ret[1], mGrid, mGrid.clrA);
	            col = mix(col0, mix(col1, col2, alpha2), alpha1);
	        }
	        else {
	            alpha2 = 0.0;
	            col = mix(col0, col1, alpha1);
	        }
	    }
	    else {
	    		alpha1 = 0.0;
	        col = col0;
	    }
    return col;
}

void main( void )
{
	glFragColor  = render(roo, normalize(rdb), bgA);
}
