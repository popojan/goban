#ifndef GOBAN_METRICS_H
#define GOBAN_METRICS_H

class Metrics {
// cpp >> gsls parameters
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
    float bowlsCenters[12];
    float squareSizeX;
    float squareSizeY;

    static const int maxc = 91;
    float tmpc[8 * maxc];
    float translate[3];

public:
    void calc(unsigned);

};

#endif
