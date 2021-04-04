#include "Metrics.h"
#include <cmath>

void Metrics::calc(unsigned NDIM) {
    fNDIM = static_cast<float>(NDIM);
    float ww = 2.0f/(fNDIM - 1.0f + 2.0f*0.85f);
    w =  1.05f*ww;//1.05
    h = 0.4f*w;
    stoneSphereRadius = 0.25f * (w*w/h + h);
    b = 0.75f*h; //0.45
    d = stoneSphereRadius-0.5f*h;
    y = b / 2.0f + d;
    px = sqrtf(stoneSphereRadius*stoneSphereRadius - y*y);
    float r1 = 0.5f*sqrtf(4.0f*px*px + b*(2.0f*b-h+2.0f*stoneSphereRadius));
    float br = sqrtf(0.12f * 9.0f/ fNDIM );
    br2 = br + sqrtf(0.15f) - sqrtf(0.12f);
    dbr = - sqrtf(0.15f) + br2;
    stoneRadius = r1;
    innerBowlRadius = br;
    bowlsCenters[0] = -1.5f - dbr;
    bowlsCenters[1] = -0.35f;
    bowlsCenters[2] = 1.0f - br2;

    bowlsCenters[3] = 1.5f + dbr;
    bowlsCenters[4] = -0.35f;
    bowlsCenters[5] = -1.0f + br2;

    bowlsCenters[6] = -1.5f - dbr;
    bowlsCenters[7] = 0.35f;
    bowlsCenters[8] = 1.0f - 3.2*br2;

    bowlsCenters[9] = 1.5f + dbr;
    bowlsCenters[10] = 0.35f;
    bowlsCenters[11] = -1.0f + 3.2*br2;

    squareSizeX = ww;
    squareSizeY = 13.0f * ww / 12.0f;
    stoneHeight = 0.5f*h;
}
