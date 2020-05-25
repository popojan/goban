#version 300 es
precision highp float;

layout(location = 0) in vec4 vertex;
out vec3 rdbl;
out vec3 rdbr;
out vec3 rdb1;
out vec3 rdb2;
out vec3 rdb3;
out vec3 rdb4;
flat out vec3 rool;
flat out vec3 roor;

uniform vec2 iResolution;
uniform float iTime;
uniform mat4 glModelViewMatrix;
uniform vec3 iTranslate;
uniform float iAnimT;

void main() {
	gl_Position = vertex; //gl_Vertex;
    vec3 eoff = vec3(0.01,0.0,0.0);
    vec2 doff = vec2(-0.25,0.0);

    vec3 ta = vec3(0.0,0.0,0.0);
    vec3 ro = vec3(0.0,0.0,-3.0-100.0*max(0.0,iAnimT-iTime));
    vec3 up = vec3(0.0,1.0,0.0);

    float a = 3.0*3.1415926*max(0.0, iAnimT-iTime);
    vec4 col = vec4(cos(a),0.0,sin(a),0.0);
    mat4 m = mat4(col.x,0.0,col.z,0.0,0.0,1.0,0.0,0.0,-col.z,0.0,col.x,0.0,0.0,0.0,0.0,1.0)*glModelViewMatrix;

    vec3 tt = (mat4(col.x,0.0,col.z,0.0,0.0,1.0,0.0,0.0,-col.z,0.0,col.x,0.0,0.0,0.0,0.0,1.0)*vec4(iTranslate,1.0)).xyz;
    vec3 roo = (m*vec4(ro, 1.0)).xyz + tt;
    ta = ta + tt;
    up = normalize((m*vec4(up, 1.0)).xyz);

    vec3 cw = normalize(ta - roo);
    vec3 cu = normalize(cross(up,cw));
    vec3 cv = cross(cw, cu);

    vec2 ratio = vec2(iResolution.x/iResolution.y, 1.0);

    vec2 q0l = (vertex.xy + (vec2(0.5,0.5)-doff)/iResolution) * ratio;
    vec2 q0r = (vertex.xy + (vec2(0.5,0.5)+doff)/iResolution) * ratio;

	rdbl = normalize(q0l.x*cu + q0l.y*cv + 3.0*cw);
	rdbr = normalize(q0r.x*cu + q0r.y*cv + 3.0*cw);
    rool = roo - (m*vec4(eoff,1.0)).xyz;
    roor = roo + (m*vec4(eoff,1.0)).xyz;
    vec2 q0 = q0l;
	rdb1 = normalize(q0.x*cu + q0.y*cv + 3.0*cw);
	q0 = (vertex.xy + vec2(-0.5, 0.5) / iResolution) * ratio;
	rdb2 = normalize(q0.x*cu + q0.y*cv + 3.0*cw);
	q0 = (vertex.xy + vec2(-0.5, -0.5) / iResolution) * ratio;
	rdb3 = normalize(q0.x*cu + q0.y*cv + 3.0*cw);
	q0 = (vertex.xy + vec2(0.5, -0.5) / iResolution) * ratio;
	rdb4 = normalize(q0.x*cu + q0.y*cv + 3.0*cw);
}
