const vec3 lpos = vec3(0.0, 8.0, 0.0);
const vec3 lpos2 = vec3(0.0, 6.0, 0.0);
const float ldia = 1.0;
const float ldia2 = 3.5;

const float mw = 0.85;
const float legh = 0.0000001;

const vec3 bgB = vec3(0.2);
const vec3 bgA = vec3(0.0);

const vec4 das = vec4(1.0, 0.63, 1.0, 64.0);
const vec4 das0 = vec4(1.2, 0.15, 0.1, 4.0);
const vec4 das1 = vec4(0.6, 0.63, 0.15, 4.0);

const MAT materials[5] = MAT[5](
    MAT(das, mat3(bgA, bgB, bgA), texSolidColor),
    MAT(das0, mat3(vec3(0.45), vec3(0.45), vec3(0.45)), texSolidColor),
    MAT(das1, mat3(vec3(0.05), vec3(0.15), vec3(0.1)), texSolidColor),
    MAT(das, mat3(vec3(0.92), vec3(0.95), vec3(0.99)), texSolidColor),
    MAT(das1, mat3(vec3(0.8, 0.8, 0.4), vec3(0.8, 0.8, 0.4), vec3(0.8, 0.8, 0.4)), texSolidColor)
);

void initMaterials() {
    for(int i = 0; i < idLastObject; ++i) {
        matidx[i] = 0;
    }
    matidx[idTable] = 1;
    matidx[idBlackStone] = 2;
    matidx[idWhiteStone] = 3;
    matidx[idBlackArea] = 2;
    matidx[idWhiteArea] = 3;
    matidx[idCapturedBlackStone] = 2;
    matidx[idCapturedWhiteStone] = 3;
    matidx[idBoard] = 4;
}