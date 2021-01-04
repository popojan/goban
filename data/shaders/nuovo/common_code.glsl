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

void updateResult(inout Intersection result[2], Intersection ret) {
    if (ret.t.x <= result[0].t.x) {
        result[1] = result[0];
        result[0] = ret;
    }
    else if (ret.t.x <= result[1].t.x) {
        result[1] = ret;
    }
}
