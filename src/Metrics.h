#ifndef GOBAN_METRICS_H
#define GOBAN_METRICS_H

class Metrics {

public:
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

public:
    void calc(unsigned);

};

#endif
