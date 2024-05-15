#version 300 es
precision highp float;

layout(location = 0) in vec4 vertex;
out vec3 rdb;
flat out vec3 roo;

uniform vec2 iResolution;
uniform float iTime;
uniform mat4 glModelViewMatrix;
uniform vec3 iTranslate;
uniform float iAnimT;

void main() {
	gl_Position = vertex; //gl_Vertex;

    vec3 ta = vec3(0.0,0.0,0.0);
    vec3 ro = vec3(0.0,0.0,-3.0-100.0*max(0.0,iAnimT-iTime));
    vec3 up = vec3(0.0,1.0,0.0);

    float a = 3.0*3.1415926*max(0.0, iAnimT-iTime);
    vec4 col = vec4(cos(a),0.0,sin(a),0.0);
    mat4 m = mat4(col.x,0.0,col.z,0.0,0.0,1.0,0.0,0.0,-col.z,0.0,col.x,0.0,0.0,0.0,0.0,1.0)*glModelViewMatrix;

    vec3 tt = (mat4(col.x,0.0,col.z,0.0,0.0,1.0,0.0,0.0,-col.z,0.0,col.x,0.0,0.0,0.0,0.0,1.0)*vec4(iTranslate,1.0)).xyz;
    roo = (m*vec4(ro, 1.0)).xyz + tt;
    ta = ta + tt;
    up = normalize((m*vec4(up, 1.0)).xyz);

    vec3 cw = normalize(ta - roo);
    vec3 cu = normalize(cross(up,cw));
    vec3 cv = cross(cw, cu);

    vec2 ratio = vec2(iResolution.x/iResolution.y, 1.0);
    vec2 q0 = (vertex.xy + vec2(0.5,0.5)/iResolution) * ratio;
	rdb = normalize(q0.x*cu + q0.y*cv + 3.0*cw);
}
