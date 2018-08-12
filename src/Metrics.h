//
// Created by jan on 2.6.17.
//

#ifndef GOBAN_METRICS_H
#define GOBAN_METRICS_H


struct Metrics {
    float fNDIM;
    float w;
    float h;
    float d;
    float y;
    float b;
    float px;
    float br2;
    float dbr;
    float stoneSphereRadius;
    float stoneRadius;
    float innerBowlRadius;
    float bowlsCenters[6];
    float squareSize;
    float stoneHeight;

    static const int maxc = 32;
    float tmpc[6 * maxc];
    float resolution[2];
    float translate[3];
    void calc(unsigned);
};

#endif //GOBAN_METRICS_H
