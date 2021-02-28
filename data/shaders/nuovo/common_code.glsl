const float PI = 3.1415926535;
const float farClip = 10000.0;
const vec2 noIntersection2 = vec2(farClip, farClip);
const vec4 noIntersection4 = vec4(farClip);
const vec3 lpos = vec3(-2.0, 12.0, -2.0);
const vec3 lpos2 = vec3(-4.0, 6.0, -2.0);
const vec3 lpos3 = vec3(0.0, 6.0, 0.0);
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
const int idYin = 21;
const int idYang = 22;
const int idLastObject = 23;


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

const int texSolidColor = -1;
const int texWood = 1;
const float ambientLevel = 0.3;
const vec4 das = vec4(1.0, ambientLevel, 1.0, 64.0);
const vec4 das0 = vec4(1.0, 0.5*ambientLevel, 0.1, 4.0);
const MAT materials[8] = MAT[8](
    MAT(das, mat3(vec3(0.5), vec3(0.5), vec3(0.5)), texSolidColor),
    MAT(das0, mat3(vec3(0.566,0.0196,0.0176), vec3(0.766,0.1196,0.1176),vec3(0.566,0.0696,0.0676)), texSolidColor),
    MAT(das0, mat3(vec3(0.05), vec3(0.15), vec3(0.1)), texWood),
    MAT(das, mat3(vec3(0.92), vec3(0.95), vec3(0.99)), texWood),
    MAT(das, mat3(vec3(0.88,0.75,0.45), vec3(0.48,0.45,0.15), vec3(0.0)), texWood),
    MAT(das0, mat3(vec3(0.766,0.1196,0.1176),vec3(0.966,0.1196,0.1176),vec3(0.766,0.1196,0.1176)), texSolidColor),
    MAT(das0, mat3(vec3(0.566,0.0196,0.0176), vec3(0.466,0.0196,0.0176), vec3(0.666,0.0196,0.0176)), texSolidColor),
    MAT(das0, mat3(vec3(0.293333, 0.1713725, 0.038039), vec3(0.053333,0.0133725,0.029039), vec3(0.17333,0.1613725,0.089039)), texSolidColor)
);

int matidx[idLastObject];

void initMaterials() {
    for(int i = 0; i < idLastObject; ++i) {
        matidx[i] = 0;
    }
    matidx[idTable] = 1;
    matidx[idBlackStone] = 2;
    matidx[idWhiteStone] = 3;
    matidx[idBowlBlackStone] = 2;
    matidx[idBowlWhiteStone] = 3;
    matidx[idBlackArea] = 2;
    matidx[idWhiteArea] = 3;
    matidx[idCapturedBlackStone] = 2;
    matidx[idCapturedWhiteStone] = 3;
    matidx[idBoard] = 4;
    matidx[idLeg1] = 4;
    matidx[idLeg2] = 4;
    matidx[idLeg3] = 4;
    matidx[idLeg4] = 4;
    matidx[idCupBlack] = 7;
    matidx[idLidBlack] = 7;
    matidx[idCupWhite] = 7;
    matidx[idLidWhite] = 7;
    matidx[idYin] = 6;
    matidx[idYang] = 5;

}

vec4 getTextureCoordinates(vec4 coord, int algo, float scale) {
    vec4 uvw = coord;

    bool noisy = false;
    switch(algo) {
        case texWood:
            uvw = vec4(2.0*uvw.x/ww + ww*uvw.z, uvw.y, uvw.z, 1.0);
            uvw = vec4(length(uvw.xy+0.78*sin(0.4+1.8*uvw.z)), uvw.y,  length(uvw.xy), 1.0);
            noisy = true;
            break;
    }
    //
    if(noisy) {
        return uvw;
    }
    return vec4(0.0,0.0,0.0,0.5);
}

const int UNION = 0;
const int INTERSECTION = 1;
const int DIFFERENCE = 2;

const int N = 3;
struct SortedLinkedList {
    IP ip[N]; // intersections as they come
    int idx[N]; // indices of the sorted intersection
    int size;
};

void init(inout SortedLinkedList x) {
    x.size = 0;
}

int insert(inout SortedLinkedList ret, in IP ip) {
    int i = N;
    int atj = ret.size;
    for (int i = ret.size - 1; i >= 0; --i) {
        int j = ret.idx[i];
        if (ip.t.z < ret.ip[j].t.z || ip.t.x < ret.ip[j].t.x) {
            atj = i;
        } else {
            break;
        }
    }
    if (atj < N) {
        int ati;
        if(ret.size >= N) {
            ati = ret.idx[N - 1];
        } else {
            ati = ret.size;
        }
        ret.ip[ati].t = ip.t;
        ret.ip[ati].isInCup = 0;
        ret.ip[ati].fog = 1.0;
        for(int i = min(ret.size, N-1); i > atj; --i) {
            ret.idx[i] = ret.idx[i-1];
        }
        ret.idx[atj] = ati;
        if(ret.size < N) {
            ret.size += 1;
        }
        return ati;
    }
    return N;
}