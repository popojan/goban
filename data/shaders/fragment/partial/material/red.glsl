const vec3 lpos = vec3(2.0, 12.0, 6.0);
const vec3 lpos2 = vec3(-4.0, 6.0, -2.0);
const float ldia = 3.5;
const float ldia2 = 10.5;

const float mw = 0.85;
const float legh = 0.15;

const float ambientLevel = 0.3;
const vec4 das = vec4(1.0, ambientLevel, 1.0, 64.0);
const vec4 das0 = vec4(1.0, 0.5*ambientLevel, 0.01, 4.0);
const vec4 das1 = vec4(1.0, 0.2, 0.1, 4.0);

const MAT materials[8] = MAT[8](
    MAT(das, mat3(vec3(0.5), vec3(0.5), vec3(0.5)), texSolidColor),
    MAT(das0, mat3(vec3(0.566,0.0196,0.1176), vec3(0.566,0.0696,0.0676), vec3(0.766,0.1196,0.1176)), texCarpet),
    MAT(das1, mat3(vec3(0.05), vec3(0.15), vec3(0.1)), texWood),
    MAT(das, mat3(vec3(0.92), vec3(0.95), vec3(0.99)), texWood),
    MAT(das1, mat3(vec3(0.895,0.79,0.45), vec3(0.52,0.52,0.19), vec3(0.05)), texWood),
    MAT(das0, mat3(vec3(0.766,0.1196,0.1176),vec3(0.966,0.1196,0.1176),vec3(0.766,0.1196,0.1176)), texCarpet),
    MAT(das0, mat3(vec3(0.566,0.0196,0.0176), vec3(0.466,0.0196,0.0176), vec3(0.666,0.0196,0.0176)), texCarpet),
    MAT(das1, mat3(vec3(0.053333,0.0133725,0.029039), vec3(0.293333, 0.1713725, 0.038039), vec3(0.17333,0.1613725,0.089039)), texWood)
);

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