#version 300 es
precision lowp float;

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
const vec4 bnx = vec4(-c.x, -0.6, -c.x, 0.0);
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

const vec3 bgA = vec3(0.0, 0.0, 0.0);
const vec3 bgB = vec3(0.0, 0.0, 0.0);

//*žlutá*//
//const Material mBoard = Material(idBoard, 0.7, 0.05, 0.25, 0.0, 0.0, 32.0, vec3(0.68826, 0.680998, 0.428209), vec3(0.787763, 0.789191, 0.4761), vec3(0.858005, 0.780002, 0.405744), 1.2);
//const Material mBoard = Material(idBoard, 0.5, 0.25, 0.4, 0.0, 0.0, 64.0, vec3(0.75826, 0.728209, 0.438209), vec3(0.787763, 0.789191, 0.4761), vec3(0.758005, 0.780002, 0.405744), 1.3);
//const Material mBoard = Material(idBoard, 0.6, 0.15, 0.4, 0.0, 0.0, 64.0, vec3(0.55826, 0.528209, 0.238209), vec3(0.487763, 0.489191, 0.3761), vec3(0.52005, 0.520002, 0.385744), 1.3);
const Material mCupBlack = Material(idCupBlack, 0.4, 0.15, 0.6, 0.0, 0.0, 64.0, vec3(0.65826, 0.528209, 0.238209), vec3(0.387763, 0.3289191, 0.12761), vec3(0.22005, 0.180002, 0.1244), 1.3);
const Material mCupWhite = Material(idCupWhite, 0.4, 0.15, 0.6, 0.0, 0.0, 64.0, vec3(0.45826, 0.428209, 0.238209), vec3(0.287763, 0.289191, 0.12761), vec3(0.12005, 0.120002, 0.085744), 1.3);
//const Material mBoard = Material(idBoard, 0.6, 0.15, 0.6, 0.0, 0.0, 128.0, vec3(0.55, 0.55, 0.3), vec3(0.5,0.5,0.3), vec3(0.45,0.45, 0.3), 1.5);
////const Material mBoard = Material(idBoard, 0.6, 0.15, 0.6, 0.0, 0.0, 128.0, vec3(0.45826, 0.328209, 0.238209), vec3(0.287763, 0.189191, 0.12761), vec3(0.22005, 0.120002, 0.105744), 1.5);
///const Material mBoard = Material(idBoard, 0.7, 0.15, 0.3, 0.0, 0.0, 128.0, vec3(0.88, 0.78, 0.43), vec3(0.7,0.6,0.3), vec3(0.7,0.65, 0.2), 1.5);
const Material mBoard = Material(idBoard, 0.7, 0.15, 0.3, 0.0, 0.0, 128.0, vec3(0.93333, 0.713725, 0.38039), vec3(0.53333,0.313725,0.09039), vec3(0.7333,0.613725,0.19039), 1.5);
//const Material mBoard = Material(idBoard, 0.5, 0.05, 0.25, 0.0, 0.0, 32.0, vec3(0.48826, 0.328209, 0.228209), vec3(0.587763, 0.589191, 0.4761), vec3(0.658005, 0.580002, 0.405744), 0.5);
//const Material mBoard = Material(idBoard, 0.7, 0.05, 0.25, 0.0, 0.0, 15.0, vec3(0.7,0.7,0.3), vec3(0.65,0.65,0.3), vec3(0.6,0.6,0.3), 0.5);
////const Material mTable = Material(idTable, 1.2, 0.1, 0.05, 0.0, 0.0, 16.0, vec3(0.566, 0.1196, 0.0176), vec3(0.666, 0.3196, 0.2176), vec3(0.666, 0.2196, 0.1176), 0.0);//vec3(0.8,0.2,0.2), vec3(0.7,0.1,0.1), vec3(0.6,0.2,0.2));
const Material mTable = Material(idTable, 1.2, 0.1, 0.05, 0.0, 0.0, 16.0, vec3(0.566,0.1196,0.0176), vec3(0.766,0.3196,0.2176), vec3(0.666,0.2196,0.1176), 0.0);//vec3(0.8,0.2,0.2), vec3(0.7,0.1,0.1), vec3(0.6,0.2,0.2));
const Material mWhite = Material(idWhiteStone, 0.23, 0.2, 0.63, 0.3, 0.4, 128.0, vec3(0.92), vec3(0.94), vec3(1.0), 0.5);
const Material mBlack = Material(idBlackStone, 0.23, 0.6, 0.83, 0.3, 0.4, 128.0, vec3(0.08), vec3(0.04), vec3(0.16), 0.5);
const Material mRed = Material(idLastBlackStone, 0.3, 0.25, 0.7, 0.3, 0.4, 16.0, vec3(0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0), 0.0);
const Material mBack = Material(idBack, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, bgA, bgB, bgA, 0.0);
////const Material mGrid = Material(idGrid, 1.5, 0.15, 0.4, 0.4, 0.4, 128.0, 
////	vec3(0.67826, 0.488209, 0.348209), vec3(0.67826, 0.488209, 0.348209), vec3(0.67826, 0.488209, 0.348209),/* vec3(0.45826, 0.328209, 0.238209), vec3(0.45826, 0.328209, 0.238209), */ 0.0);
const Material mGrid  = Material(idGrid, 1.5, 0.15, 0.4, 0.4, 0.4, 128.0, vec3(0.0),vec3(0.0), vec3(0.0), 0.0);

uniform vec3 cc[2];
//float ddcy = 0.5*h-bowlRadius;
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
/*float func(in vec3 ro, in vec3 rd, in vec3 cc, float t) {
vec3 a = ro + t*rd - cc;
return (dot(a.xz,a.xz) + 8.0*(1.0 + dot(a.xz, a.xz))*a.y*a.y)/(r1*r1) - 1.0;
}

bool intersectionRayStone(in vec3 ro, in vec3 rd, in vec3 dd, float mint, out vec4 res, out float t2){
float tn1 = length(ro - dd) - r1;
float tn0 = length(ro - dd);
float tn2;
bool found = false;
float e = 0.01;
float y1, y2;
float d;
for(int i = 0; i < 16; i++){
float y1 = func(ro, rd, dd,tn1);
float y2 = func(ro, rd, dd,tn0);
tn2 = tn1 - y1*(tn1-tn0)/(y1 - y2);
d = min(y1,y2);
if(abs(y2) < e && abs(y1) < e && abs(tn1-tn0)<e) {
found = true;
break;
}
tn0 = tn1;
tn1 = tn2;
}
res = noIntersection4;
if (found) {
vec3 ip = ro+rd*tn2;
res = vec4(normalize(vec3(2.0*ip.x+8.0*ip.x*ip.y*ip.y,8.0*(1.0+dot(ip.xz, ip.xz))*ip.y, 2.0*ip.z+8.0*ip.z*ip.y*ip.y)), tn2);
}
return found;
}*/

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
			if (p1 >= 0.0 && p1 < p2 && p1 < mint) {
				vec2 d3 = intersectionRayCylinder(ro, rd, dd, px*px);
				vec2 d4;
				vec4 tt = intersectionRayEllipsoid(ro, rd, dd, d4);
				vec3 ip = ro + p1*rd;
				vec3 rcc = dd + (d1.x < d2.x ? -dn : dn);
				vec3 n = normalize(ip - rcc);
				if (p1 > d3.x && p1 < d3.y && d3.x < farClip) {
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

//
// GLSL textureless classic 3D noise "cnoise",
// with an RSL-style periodic variant "pnoise".
// Author:  Stefan Gustavson (stefan.gustavson@liu.se)
// Version: 2011-10-11
//
// Many thanks to Ian McEwan of Ashima Arts for the
// ideas for permutation and gradient selection.
//
// Copyright (c) 2011 Stefan Gustavson. All rights reserved.
// Distributed under the MIT license. See LICENSE file.
// https://github.com/ashima/webgl-noise
//

// Classic Perlin noise

// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash(uint x) {
	x += (x << 10u);
	x ^= (x >> 6u);
	x += (x << 3u);
	x ^= (x >> 11u);
	x += (x << 15u);
	return x;
}



// Compound versions of the hashing algorithm I whipped together.
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


// Pseudo-random value in half-open range [0:1].
float rand(float x) { return floatConstruct(hash(floatBitsToUint(x))); }
float rand(vec2  v) { return floatConstruct(hash(floatBitsToUint(v))); }
float rand(vec3  v) { return floatConstruct(hash(floatBitsToUint(v))); }
float rand(vec4  v) { return floatConstruct(hash(floatBitsToUint(v))); }

highp float rand_(vec3 co)
{
	const highp vec4 abcd = vec4(12.9898, 78.233, 37.6253, 43758.5453);
	highp float dt = dot(abcd.zyx + co, abcd.xzy);
	//highp float sn= mod(dt,3.14159265358979);
	return fract(sin(dt)*abcd.z);
	//sn = fract(mod(1.0 - 2.0*dt, 1.0)*d);
	//return sn;
}

float cnoise(vec3 P)
{

	vec3 Pi0 = floor(P); // Integer part for indexing
	vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
	//Pi0 = mod289(Pi0);
	Pi0 = Pi0 - floor(Pi0 * (1.0 / 289.0)) * 289.0;
	Pi1 = Pi1 - floor(Pi1 * (1.0 / 289.0)) * 289.0;
	//Pi1 = mod289(Pi1);
	vec3 Pf0 = fract(P); // Fractional part for interpolation
	vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0

	float rnd1 = rand(vec3(Pi0.x, Pi0.y, Pi0.z));
	float rnd2 = rand(vec3(Pi0.x, Pi0.y, Pi1.z));
	float rnd3 = rand(vec3(Pi1.x, Pi0.y, Pi1.z));
	float rnd4 = rand(vec3(Pi1.x, Pi0.y, Pi0.z));
	float rnd5 = rand(vec3(Pi0.x, Pi1.y, Pi0.z));
	float rnd6 = rand(vec3(Pi0.x, Pi1.y, Pi1.z));
	float rnd7 = rand(vec3(Pi1.x, Pi1.y, Pi1.z));
	float rnd8 = rand(vec3(Pi1.x, Pi1.y, Pi0.z));
	return mix(
		mix(mix(rnd1, rnd2, Pf0.z), mix(rnd4, rnd3, Pf0.z), Pf0.x),
		mix(mix(rnd5, rnd6, Pf0.z), mix(rnd8, rnd7, Pf0.z), Pf0.x),
		Pf0.y);
}
float dnoise(vec3 P)
{

	vec3 Pi0 = floor(P); // Integer part for indexing
	vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
	//Pi0 = mod289(Pi0);
	Pi0 = Pi0 - floor(Pi0 * (1.0 / 289.0)) * 289.0;
	Pi1 = Pi1 - floor(Pi1 * (1.0 / 289.0)) * 289.0;
	//Pi1 = mod289(Pi1);
	vec3 Pf0 = fract(P); // Fractional part for interpolation
	vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0

	float rnd1 = rand(vec3(Pi0.x, Pi0.y, Pi0.z));
	float rnd2 = rand(vec3(Pi0.x, Pi0.y, Pi1.z));
	float rnd3 = rnd1; //rand(vec3(Pi1.x, Pi0.y, Pi1.z));
	float rnd4 = rnd2; //rand(vec3(Pi1.x, Pi0.y, Pi0.z));
	float rnd5 = rand(vec3(Pi0.x, Pi1.y, Pi0.z));
	float rnd6 = rand(vec3(Pi0.x, Pi1.y, Pi1.z));
	float rnd7 = rnd5;//rand(vec3(Pi1.x, Pi1.y, Pi1.z));
	float rnd8 = rnd6;//rand(vec3(Pi1.x, Pi1.y, Pi0.z));
	return mix(
		mix(mix(rnd1, rnd2, Pf0.z), mix(rnd4, rnd3, Pf0.z), Pf0.x),
		mix(mix(rnd5, rnd6, Pf0.z), mix(rnd8, rnd7, Pf0.z), Pf0.x),
		Pf0.y);
}

float surface3(vec3 coord, float frequency) {
	float n = 0.0;
	const int N = 3;
	vec3 v;
	v = coord*frequency;
	//    v[1] = 7.0 * v[0];
	//    v[2] = 13.0 * v[0];
	vec3 Pi0;
	//    for(int i = 0; i < N; ++i) {
	return cnoise(v);
	//    }
	//    return n;///3.0;
}

void updateResult(inout Intersection result[2], Intersection ret) {
	/*	float phi = 2.0*3.14*surface3(vec3(10.0)+1117.0*ret.p.xyz,1.0);
	float len = surface3(vec3(10.0)+1117.0*ret.p.xyz,1.0);
	vec3 u = ret.n;
	vec3 v = cross(u, vec3(1.0));
	vec3 w = cross(v, u);
	ret.nn = normalize(mix(normalize(ret.n), normalize((cos(phi)*v - sin(phi)*w)), sqrt(0.05*len)));
	*/
	//result[idx] = ret;
	if (ret.t.x <= result[0].t.x) {
		result[1] = result[0];
		result[0] = ret;
	}
	else if (ret.t.x <= result[1].t.x) {
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
	if (any(isStone)) {
		dist0a = distanceRayStoneSphere(ro, rd, dd - dn, stoneRadius);
		vec3 dist0b = distanceRayStoneSphere(ro, rd, dd + dn, stoneRadius);
		dist0a = dist0a.y > dist0b.y ? dist0a : dist0b;
		dist0b.yz = distanceRayStone(ro, rd, dd);
		ret[i].d = mix(dist0a.y / dist0a.z, dist0b.y / dist0b.z, step(0.0, dist0a.x));
		//ret[i].d = mix(dist0a.y, dist0b.y, step(0.0, dist0a.x))/distance(ro, dd);
		//float dn = surface3(vec3(153.0,52.0,151.0)*ret[i].p.xyz, 1.0);
		//dn *= 0.25*abs(-1.0+2.0/(1.0+exp(-80.0*max(0.0, length(ret[i].p.xz - dd.xz)-0.25*ww))));
		//ret[i].n = normalize(ret[i].n - dn * normalize(ret[i].p.xyz - dd));
	}
	if (any(isStone.yz)) {
		int idStoneInside = ret[i].m;
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
		ret[i].m = idStoneInside;

		vec3 cc = dd;
		cc.y -= dn.y - sqrt(stoneRadius2 - 0.25*r1*r1);
		vec3 ip0 = ro + rd*dot(cc - ro, nBoard) / dot(rd, nBoard);
		vec3 q2;

		//idStone = (ret[i].m == idLastBlackStone || ret[i].m == idCapturedBlackStone) ? idBlackStone : idWhiteStone;
		bool isNotArea = length(ret[i].p.xz - cc.xz) > 0.5*r1;
		if (isNotArea) {
			vec2 rr = distanceRayCircle(ro, rd, ip0, ret[i].t, cc, 0.5*r1, q2);
			r2.d = abs(rr.y);
			if (rr.x <0.0 && r2.d < boardaa){
				r2.d = max(-r2.d, ret[i].d);
				r2.m = idStone;
				updateResult(ret, r2);
			}
			else{
				ret[i].m = idStone;
			}
		}
	}
}


void castRay(in vec3 ro, in vec3 rd, out Intersection result[2]) {

	Intersection ret;
	ret.m = idBack;
	ret.d = -farClip;
	ret.n = nBoard;
	ret.t = vec2(farClip);
	ret.p = vec3(-farClip);
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
		ret.p = ip;
		ret.isBoard = isBoardTop;

		if (dist0 > -boardaa) {
			float tc, tc2;
			IntersectBox(ro, q2 - ro, bnx.xyz, -bnx.xwz, tc, tc2);
			if (tc2 - tc > 0.001) {
				n0 = normalize(n0);
				vec3 cdist = abs(abs(ip0) - abs(bn0));
				vec2 ab = clamp(vec2(abs(dist0), min(cdist.z, min(cdist.x, cdist.y))) / boardcc, 0.0, 1.0);
				n0 = normalize(mix(normalize(ip0), n0, ab.y));
				n0 = normalize(mix(n0, n1, ab.x));
				ret.n = n0;
				ret.d = -farClip;
				ret.p = q2;
				updateResult(result, ret);
			}
		}
		ret.n = n1;
		/*float phi = 2.0*3.14*surface3(vec3(10.0)+1117.0*ret.p.xyz,1.0);
		float len = surface3(vec3(10.0)+1117.0*ret.p.xyz,1.0);
		vec3 u = ret.n;
		vec3 v = cross(u, vec3(1.0));
		vec3 w = cross(v, u);
		ret.nn = normalize(mix(normalize(ret.n), normalize((cos(phi)*v - sin(phi)*w)), sqrt(0.15*len)));
		*/

		if (dd < 1.0 && dist0 < -dd*boardaa) {
			ret.d = dist0;
			ret.m = idGrid;
			ret.p = ip;
			updateResult(result, ret);
		}
		if (dd > 0.0) {
			ret.m = idBoard;
			dist0 = max(dist0, -dd*boardaa);
			ret.p = ip;
			ret.d = dist0;
			updateResult(result, ret);
		}
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
		vec2 tc = vec2(dot(-ro - vec3(0.3), nBoard), dot(bnx.xyz - vec3(2.0) - ro, nBoard)) / dot(rd, nBoard);
		vec2 ts = intersectionRaySphereR(ro, rd, cc[i], bowlRadius2*bowlRadius2);
		//vec2 tsx = intersectionRaySphereR(ro, rd, cc[i], 0.149);
		float firstx = tc.x;
		tc = vec2(min(tc.x, tc.y), max(tc.x, tc.y));
		bool between = ro.y < -0.3 && ro.y > bnx.y - legh;
		if (ts.x != noIntersection2.x && max(tc.x, ts.x) <= min(tc.y, ts.y)) {
			float  rett = max(tc.x, ts.x);
			vec3 cc2 = cc[i];
			cc2.y = -0.3;
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
				//		updateResult(result, ret);
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
		//float alpha = iTime;
		//float cosa = cos(alpha);
		//float sina = sin(alpha);
		//vec2 xz = mat2(cosa,sina, -sina, cosa)*ret.p.xz;
		//vec3 retp = vec3(xz, ret.p.y).xzy;
		vec3 nn = 2.0*vec3(-0.5 + surface3(ret.p, 212.0), 0.0, -0.5 + surface3(ret.p, 213.0));
		ret.n = normalize(mix(nBoard, nn, 0.15*exp(-0.075*length(ret.p - ro))));
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
			//if(aboveTable) ret.d = max(dist0, distanceRayCircle(ro, rd, ip, ts, cc, rc).y);
			ret.t = ts;
			ret.m = idLeg1 + i;
			ret.p = ip;
			ret.n = normalize(ip - cc);
			updateResult(result, ret);
		}
	}
	finalizeStoneIntersection(ro, rd, result, 0);
	finalizeStoneIntersection(ro, rd, result, 1);
	
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
	if ((pos.y < -0.00001 && (m == idCupWhite || m == idCupBlack || m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4 || m == idBoard))) {
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
								float acs = acos(dot(v, normalize(vec3(pos.x, 0.0, pos.z) - vec3(xz.x, 0.0, xz.y))));
								ret.y *= min(1.0, acs / PI);
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

Material getMaterialColor(in Intersection ip, out vec3 mcol, in vec3 rd, in vec3 ro) {
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
	vec3 scrd;
	float degrade = (1.0 + floor(length(ro) / 3.0));
	//float alpha = iTime;
	//float cosa = cos(alpha);
	//float sina = sin(alpha);
	//vec2 xz = mat2(cosa,sina, -sina, cosa)*ip.p.xz;
	vec2 xz;
	xz = ip.p.xz;
	if (mat.id == mBoard.id || mat.id == mCupBlack.id || mat.id == mCupWhite.id) {
		scoord = 16.0*vec3(xz.x,ip.p.y,xz.y) + vec3(0.0, 0.25, 0.0);
		//scoord.z *= 0.24;
		//scoord.y += 1.2;
		scoord.x *= 6.14;
		scoord.y *= 5.14;
		noisy = true;
		scrd = 0.5*scoord;
	}
	else if (mat.id == mGrid.id) {
		scoord = 350.0*ip.p;
		sfreq = 1.5;
		noisy = true;
		scrd = scoord;
	}
	else if (mat.id == mWhite.id || mat.id == mBlack.id) {
		float alpha = ip.dummy.w;
		float cosa = cos(alpha);
		float sina = sin(alpha);
		vec2 xz = mat2(cosa, sina, -sina, cosa)*ip.p.xz;
		scoord = xz.xyy;
		scoord.y = 1.0;
        scoord = reflect(rd, ip.n);
        scoord = 0.5*(1.0 + sin(vec3(3.0,3.0,3.0)*scoord));
		sfreq = 1.0*(1.0 + 0.1*sin(11.0 + xz.x));
		sadd1 = 5.0 + sin(131.0 + 15.0*xz.x);
		noisy = true;
		scrd = scoord;
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
		//scoord.z *= 0.24;
		//scoord.y += 1.2;
		scoord.x *= 6.14;
		scoord.y *= 5.14;
		//noisy = true;
		scrd = 0.5*scoord;
		noisy = false;
	}
	float rnd = 0.0;
	//if (noisy) {
		rnd = surface3(scrd, 1.0);
	//}
	if (mat.id == mBoard.id || mat.id == mCupBlack.id || mat.id == mCupWhite.id) {
		float w1 = 3.0*length(scoord.yx - 0.5*vec2(1.57 + 3.1415*rnd));
		float w2 = 0.1*(scoord.x + scoord.z);
		smult1 = mix(rnd, 1.0, 0.05)*(clamp(0.25*(sin(w1)), 0.0, 1.0));
		smult2 = mix(rnd, 1.0, 0.05)*(clamp(0.25*(sin(1.5*w1)), 0.0, 1.0));
		smult3 = 1.0 - smult2;
	}
	else if (mat.id == mBlack.id || mat.id == mGrid.id || mat.id == mWhite.id) {
		smult1 = rnd;
		smult2 = 0.2;//5;
		smult3 = 1.0 - smult2;
		mixmult = 1.0;
	}/*
	else if (mat.id == mWhite.id) {
		//scoord.x = length(scoord.xy);
		smult1 = rnd;
		//smult1 = abs(sin(11.0 + 131.0*scoord.x));
		smult2 = 0.2;//abs(sin(1.3 - 101.0*scoord.x) + sin(1.3 - 121.0*scoord.x));
		smult3 = 1.0 - smult2;
	}
	else {
		smult3 = 1.0 - smult2;
	}*/
	//smult1 = mix(0.5, smult1, exp(-0.05*length(ip.p - ro)));
	//smult2 = mix(0.5, smult2, exp(-0.05*length(ip.p - ro)));
	mcol = mix(mix(mcol, mcolb, smult2), mix(mcol, mcolc, 1.0 - smult2), smult1);
	return mat;
}

vec3 shading(in vec3 ro, in Intersection ip, const Material mat, vec3 col){
	vec3 ret;
	if (mat.id == mBack.id) {
		ret = mat.clrA;
	}
	else {
				vec3 rd = normalize(ip.p - ro);
		vec3 smult0 = vec3(0.01);
		smult0 = clamp(abs(col - mat.clrB) / abs(mat.clrA.x - mat.clrB), 0.0, 1.0);
		float smult = (smult0.x + smult0.y + smult0.z) / 3.0;
		vec3 nn = ip.n;//ip.isBoard ? ip.nn : ip.n;// mat.id != mWhite.id && mat.id != mBlack.id ? ip.nn : ip.n;//normalize(mix(ip.n, ip.nn, 0.15));
		vec3 ref = reflect(rd, nn);
		//max4x3 ligs = mat4x3(lig,lig2,lig3,lig);
		//mat4x4 ress = ligs*ligs;
		float alpha = 0.0; //2.1*iTime;
		float cosa = cos(alpha);
		float sina = sin(alpha);
		mat2 mm = mat2(cosa, sina, -sina, cosa);

		vec3 lpos = vec3(mm * lpos.xz, lpos.y).xzy;
		vec3 lpos2 = vec3(mm * lpos2.xz, lpos2.y).xzy;
		vec3 lpos3 = vec3(mm * lpos3.xz, lpos3.y).xzy;
		vec3 lig = normalize(lpos - ip.p);
		vec3 lig2 = normalize(lpos2 - ip.p);
		vec3 lig3 = normalize(lpos3 - ip.p);
		//vec3 lig4 = normalize(lpos4 - ip.p);
		//vec3 ligA = normalize(lposA - ip.p);
		//vec3 ligB = normalize(lposB - ip.p);
		//vec3 ligC = normalize(lposC - ip.p);
		//vec3 ligD = normalize(lposD - ip.p);
		vec2 shadow = 0.6*pow(softshadow(ip.p, ip.n, lpos, ldia, ip.m, false, ip.uid), vec2(1.0, 0.25));
		shadow += 0.4*pow(softshadow(ip.p, ip.n, lpos2, ldia, ip.m, false, ip.uid), vec2(1.0,0.25));

		float nny = 0.5 + 0.5*nn.y;
		vec2 ads =
			0.6*clamp(vec2(nny, dot(nn, lig)), 0.0, 1.0) +
			0.3*clamp(vec2(nny, dot(nn, lig2)), 0.0, 1.0) +
			0.3*clamp(vec2(nny, dot(nn, lig3)), 0.0, 1.0);
		//0.2*clamp(vec2(nny, dot(nn, lig4)),0.0,1.0);

		vec4 pws = clamp(vec4(dot(ref, lig), dot(ref, lig2), dot(ref, lig3), dot(ref, lig3)), 0.0, 1.0);
		//ads += 0.15*clamp(vec3(0.5+0.5*ip.n.y, dot(ip.n, ligA), 0.0), 0.0,1.0);
		//ads += 0.15*clamp(vec3(0.5+0.5*ip.n.y, dot(ip.n, ligB), 0.0), 0.0,1.0);
		//ads += 0.15*clamp(vec3(0.5+0.5*ip.n.y, dot(ip.n, ligC), 0.0), 0.0,1.0);
		//ads += 0.15*clamp(vec3(0.5+0.5*ip.n.y, dot(ip.n, ligD), 0.0), 0.0,1.0);
		float pw = exp(mat.refl*(-0.5 - smult));
		float cupsa = ip.m == idBowlBlackStone || ip.m == idBowlWhiteStone ? 0.9 : 1.0;
		float cupsb = ip.m == idBowlBlackStone || ip.m == idBowlWhiteStone ? 0.5 : 1.0;
		ret = dot(
			vec2(mat.diffuseWeight, mat.ambientWeight),
			vec2(ads.y * shadow.x, shadow.y)
			)*col + vec3(1.0)*mat.specularWeight*(0.25*pow(pws.x, 0.125*mat.specularPower) + pow(pws.y, cupsb*pw*mat.specularPower)
			+ pow(pws.z, cupsb*0.5*pw*mat.specularPower));
		//		+pow(pws.w, pw*mat.specularPower));
		if (ip.m == idTable) {
			ret = mix(mBack.clrA, ret, exp(-0.35*max(0.0, length(ip.p))));
		}
	}
	ret = pow(ret, gamma*exp(contrast*(vec3(0.5) - ret)));
	return ret;
}

// fur ball
// (c) simon green 2013
// @simesgreen
// v1.1

const float uvScale = 1.0;
const float colorUvScale = 0.1;
const float furDepth = 0.055;
const int furLayers = 32;
const float rayStep = furDepth / float(furLayers);
const float furThreshold = 0.85;
const float shininess = 50.0;

vec3 rotateX(vec3 p, float a)
{
	float sa = sin(a);
	float ca = cos(a);
	return vec3(p.x, ca*p.y - sa*p.z, sa*p.y + ca*p.z);
}

vec3 rotateY(vec3 p, float a)
{
	float sa = sin(a);
	float ca = cos(a);
	return vec3(ca*p.x + sa*p.z, p.y, -sa*p.x + ca*p.z);
}

vec2 cartesianToSpherical(vec3 p)
{
	float r = length(p);

	float t = (r - (1.0 - furDepth)) / furDepth;
	p = rotateX(p.zyx, -cos(1.5)*t*t*0.4).zyx;	// curl

	p /= r;
	vec2 uv = vec2(atan(p.y, p.x), acos(p.z));

	//uv.x += cos(iGlobalTime*1.5)*t*t*0.4;	// curl
	//uv.y += sin(iGlobalTime*1.7)*t*t*0.2;
	uv.y -= t*t*0.1;	// curl down
	return uv;
}

// returns fur density at given position
float furDensity(vec3 pos, out vec3 Pi0)
{
	float r = abs(bnx.y - legh - pos.y);//length(pos);
	float t = 1.0 - r / furDepth;

	Pi0 = vec3(250.0*pos.x, 0.0, 250.0*pos.z);
	float tex = cnoise(Pi0);

	float density = smoothstep(furThreshold - 0.5, furThreshold + 0.5, tex);
	return density;
}

vec3 furShade(vec3 pos, vec3 ro, float density, inout vec3 col7, vec3 Pi0)
{
	float r = abs(bnx.y - legh - pos.y);
	float t = 1.0 - r / furDepth;
	t = clamp(t*t, 0.0, 1.0);
	return t*col7;
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
	/*if(ret[0].m == idTable) {
	vec4 c = vec4(0.0);
	mat = getMaterialColor(ret[0], mcol, rd, ro);

	vec3 pos = ret[0].p;
	vec3 col8 = shading(ro, ret[0], mTable, mcol);;
	for(int i=0; i<furLayers; i++) {
	vec3 Pi0;
	vec4 sampleCol;
	sampleCol.a = furDensity(pos, Pi0);
	if (sampleCol.a > 0.0) {
	sampleCol.rgb = furShade(pos, ro, sampleCol.a, col8, Pi0);
	// pre-multiply alpha
	sampleCol.rgb *= sampleCol.a;
	if (c.a > 0.95) break;
	c = c + sampleCol*(1.0 - c.a);
	}
	float t = dot(vec3(0.0,bnx.y-legh-float(i+1)*rayStep,0.0)-ro,nBoard)/dot(rd,nBoard);
	pos = ro + t*rd;
	}
	col = mix(c.rgb, col8, 0.75);

	//col = col8;
	alpha1 = 0.0;
	}
	else {*/
	mat = getMaterialColor(ret[0], mcol, rd, ro);
	col0 = shading(ro, ret[0], mat, mcol);
	/*if(ret[0].m != idCup && (ret[0].isBoard || ret[0].m != idBoard)) {
	vec3 rd = reflect(rd, ret[0].n);
	ro = ret[0].p + 0.0001*ret[0].n;
	Intersection ret[2];
	castRay(ro, rd, ret);
	Material mat = getMaterialColor(ret[0], mcol, rd, ro);
	col0 += (vec3(1.0) - col0)*0.25*shading(ro, ret[0], mat, mcol);
	}*/
	
	gl_FragDepth = (ret[0].m == mBlack.id || ret[0].m == mWhite.id) ? 0.5 : (ret[0].p.y < -0.001 ? 0.25 : 0.75);//; distance(ro, ret[0].p) / 100.0;
	if (alpha1 > 0.05) {
		//ret[0] = mix(ret[0].n, ret[1].n, alpha1)
		mat = getMaterialColor(ret[1], mcol, rd, ro);
		//mcol = 0.5*(mat.clrA+mat.clrB);

		col1 = shading(ro, ret[1], mat, mcol);
		float alpha2 = smoothstep(boardaa, 0.0, -ret[1].d);
		if (ret[1].m == idBoard && alpha2 > 0.0) {
			col2 = shading(ro, ret[1], mBoard, mBoard.clrA);
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
	//}
	
	return col;
}


void main(void)
{
	glFragColor =  render(roo, normalize(rdb), bgA);//
}
