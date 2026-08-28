#version 330 core

/* === DO NOT CHANGE BELOW === */

const int MAXSTONES = 19 * 19;
const float cidAnnotation = 1.0;  // Empty point with hidden grid (for overlays)
const float cidBlackArea = 2.0;
const float cidWhiteArea = 3.0;
const float cidCapturedBlackStone = 5.5;
const float cidLastBlackStone = 5.75;
const float cidBlackStone = 5.0;
const float cidWhiteStone = 6.0;
const float cidCapturedWhiteStone = 6.5;
const float cidLastWhiteStone = 6.75;

in float noise;
out vec3 glFragColor;

layout(std140) uniform iStoneBlock{
    vec4 iStones[MAXSTONES];
};

uniform int NDIM;
uniform vec2 iResolution;
uniform int iStoneCount;
uniform int iBlackCapturedCount;
uniform int iWhiteCapturedCount;
uniform int iBlackReservoirCount;
uniform int iWhiteReservoirCount;
uniform float iTime;

uniform float gamma;
uniform float contrast;
uniform float fNDIM;
uniform float boardaa;
uniform float boardbb;
uniform float boardcc;
uniform float wwx;
uniform float wwy;
uniform float w;
uniform float h;
uniform float stoneRadius;
uniform float d;
uniform float stoneRadius2;
uniform vec3 dn;
uniform float b;
uniform float y;
uniform float px;
uniform float pxs;
uniform float r1;
uniform float r2;
uniform float r123r123;
uniform vec3 rrr;
uniform float r1r1ir2ir2;
uniform vec3 maxBound;
uniform float dw;
uniform float iscale;
uniform float bowlRadius;
uniform float bowlRadius2;

uniform mat4 glModelViewMatrix;
uniform vec3 iTranslate;
uniform vec2 cursor;
uniform vec3 cc[4];
const int maxCaptured = 91;
uniform vec4 ddc[2 * maxCaptured];

/* === DO NOT CHANGE ABOVE == */

// The pointer mark drawn on the wood, and how strongly: 0 hides it.
//
// A native mouse pointer is composited by the window system at the screen plane,
// with no disparity at all, so under a stereo shader it can never sit at the
// depth of the point it indicates — fuse the board and you see two pointers.
// A mark ray-traced with the board inherits that eye's disparity and occlusion
// for free, which is the only way a pointer and its target can agree.
uniform float cursorMark;

// Coverage of the pointer mark at a point on the board plane, 0 clear, 1 full
// ink. Four ticks turned a quarter turn from the grid — no arm parallel to a
// line — and gapped at the centre so the intersection it names stays clear.
// A disc would read as a stone and an upright cross as the grid: on this board
// both shapes are already spoken for.
//
// One implementation, because two things draw it. The board draws it on the
// wood, and the *annotation patch* has to draw it as well: that patch is a quad
// of clean board laid over the grid to give a label somewhere legible, and it
// covered the mark exactly as it covers the lines. Moving the ticks outside the
// patch was the obvious answer and is not available — the patch reaches 0.4 of a
// spacing and the neighbouring one starts at 0.6, so the ticks would have to
// live in a corridor 0.2 wide, and with the imprecise-hand offset drifting the
// whole mark they would have to be stubs to stay inside it.
float pointerCoverage(vec3 ro, vec3 ip) {
    if (cursorMark <= 0.0) return 0.0;
    // `cursor` arrives centred on the board in grid units and offset by half a
    // spacing, the C++ side subtracting N/2 where the lines sit at (N-1)/2.
    vec2 cpos = vec2(wwx, wwy) * (cursor + 0.5);
    vec2 dxz = ip.xz - cpos;
    // Into the diagonal frame, where the arms lie along the grid's diagonals.
    vec2 e = vec2(dxz.x + dxz.y, dxz.x - dxz.y) * 0.7071067;
    float hw = 0.045 * wwx;  // arm half-width: lighter than a grid line
    float gap = 0.34 * wwx;  // clear of the point itself
    float out2 = 0.72 * wwx; // and clear of a stone standing on it
    float arm1 = max(abs(e.y) - hw, max(gap - abs(e.x), abs(e.x) - out2));
    float arm2 = max(abs(e.x) - hw, max(gap - abs(e.y), abs(e.y) - out2));
    float sd = min(arm1, arm2);
    // The board's own pixel footprint, so the ticks soften with distance
    // exactly as the grid lines do.
    float aa = max(boardbb * distance(ro, ip), 1e-6);
    return cursorMark * (1.0 - smoothstep(-aa, aa, sd));
}
