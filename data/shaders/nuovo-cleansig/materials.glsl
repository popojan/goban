const vec3 bgA = vec3(0.0, 0.0, 0.0);
const vec3 bgB = vec3(0.0, 0.0, 0.0);

const Material mCup = Material(
    idCupBlack,
    vec3(0.7, 0.15, 0.25),
    vec3(32.0),
    vec3(0.293, 0.171, 0.038),
    vec3(0.053, 0.013, 0.029),
    vec3(0.173, 0.161, 0.089),
    1.5
);

const Material mBoard = Material(
    idBoard,
    vec3(0.7, 0.15, 0.05),
    vec3(42.0),
    vec3(0.933, 0.814, 0.380),
    vec3(0.533, 0.414, 0.090),
    vec3(0.733, 0.614, 0.190),
    1.5
);

const Material mTable = Material(
    idTable,
    vec3(1.0, 0.15, 0.0),
    vec3(4.0),
    vec3(0.566, 0.0196, 0.0176),
    vec3(0.766, 0.1196, 0.1176),
    vec3(0.666, 0.0196, 0.0176),
    0.0
);

const Material mWhite = Material(
    idWhiteStone,
    vec3(0.53, 0.53, 0.2),
    vec3(32.0, 41.0, 2.0),
    vec3(0.96),
    vec3(0.96,0.91,0.97),
    vec3(0.93),
    0.5
);

const Material mBlack = Material(
    idBlackStone,
    vec3(0.53, 0.53, 0.15),
    vec3(28.0),
    vec3(0.08),
    vec3(0.04),
    vec3(0.10),
    0.5
);

const Material mRed = Material(
    idLastBlackStone,
    vec3(0.3, 0.15, 0.25),
    vec3(4.0),
    vec3(0.5, 0.0, 0.0),
    vec3(0.5, 0.0, 0.0),
    vec3(0.5, 0.0, 0.0),
    0.0
);

const Material mBack = Material(
    idBack,
    vec3(0.0, 1.0, 0.0),
    vec3(1.0),
    bgA,
    bgB,
    bgA,
    0.0
);

const Material mDummy = Material(
    idBoard,
    vec3(0.5, 0.5, 0.0),
    vec3(1.0),
    vec3(0.5),
    vec3(0.5),
    vec3(0.5),
    0.0
);

const Material mGrid  = Material(
    idGrid,
    vec3(1.5, 0.15, 0.15),
    vec3(42.0),
    vec3(0.0),
    vec3(0.0),
    vec3(0.0),
    0.0
);