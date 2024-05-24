const float PI = 3.1415926535;
const float farClip = 10000.0;
const vec2 noIntersection2 = vec2(farClip, farClip);
const vec4 noIntersection4 = vec4(farClip);
const vec3 nBoard = vec3(0.0, 1.0, 0.0);
const vec3 minBound = vec3(-1.2, -0.02, -1.2);
const vec3 p = vec3(0.0, -0.25, 0.0);
vec3 c;
vec4 bnx;

const int idNone = -2;
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
const int idYin = 21;
const int idYang = 22;
const int idLastObject = 23;

int matidx[idLastObject];

struct IP {
    vec3 t; //parameter; distance along the ray, z == z-score
    float d; //signed distance from the object surface
    vec3 n; //normal
    int oid; //object id
    int pid; //part id
    int gid; //group id
    vec2 a; // mixture of oid + pid
    vec4 uvw;
    int isInCup;
    int uid;
    float fog;
};

struct MAT {
    vec4 das; //diffuse ambient specular weights
    mat3 col;
    int tid;
};

const int N = 3;
struct SortedLinkedList {
    IP ip0;
    IP ip1;
    IP ip2;
};

void init(inout SortedLinkedList x) {
    x.ip0.oid = idNone;
    x.ip1.oid = idNone;
    x.ip2.oid = idNone;
    x.ip0.t = vec3(farClip);
    x.ip1.t = vec3(farClip);
    x.ip2.t = vec3(farClip);
}

bool try_insert(inout SortedLinkedList ret, in IP ip) {
    return ip.t.z < ret.ip2.t.z || ip.t.x < ret.ip2.t.x;
}

void insert(inout SortedLinkedList ret, in IP ip) {
    if (ip.t.z < ret.ip0.t.z || ip.t.x <= ret.ip0.t.x) {
        ret.ip2 = ret.ip1;
        ret.ip1 = ret.ip0;
        ret.ip0 = ip;
    } else if (ip.t.z < ret.ip1.t.z || ip.t.x <= ret.ip1.t.x) {
        ret.ip2 = ret.ip1;
        ret.ip1 = ip;
    } else if (ip.t.z < ret.ip2.t.z || ip.t.x <= ret.ip2.t.x) {
        ret.ip2 = ip;
    }
}
