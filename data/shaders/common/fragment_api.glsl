#version 300 es
precision highp float;

/* === DO NOT CHANGE BELOW === */

const int MAXSTONES = 19 * 19;
const float cidBlackArea = 2.0;
const float cidWhiteArea = 3.0;
const float cidCapturedBlackStone = 5.5;
const float cidLastBlackStone = 5.75;
const float cidBlackStone = 5.0;
const float cidWhiteStone = 6.0;
const float cidCapturedWhiteStone = 6.5;
const float cidLastWhiteStone = 6.75;

in vec3 rdb;
flat in vec3 roo;
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
uniform float ww;
uniform float iww;
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
uniform vec4 cc[4];
const int maxCaptured = 91;
uniform vec4 ddc[2 * maxCaptured];

/* === DO NOT CHANGE ABOVE == */
