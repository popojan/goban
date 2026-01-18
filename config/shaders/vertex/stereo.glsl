#version 330 core

layout(location = 0) in vec4 vertex;
out vec3 rdbl;
out vec3 rdbr;
flat out vec3 rool;
flat out vec3 roor;

uniform vec2 iResolution;
uniform float iTime;
uniform mat4 glModelViewMatrix;
uniform vec3 iTranslate;
uniform float iAnimT;

uniform float eof;
uniform float dof;

void main() {
    gl_Position = vertex; //gl_Vertex;
    vec4 eoff = vec4(eof,0.0,0.0, 0.0);
    vec4 doff = vec4(dof,0.0, 0.0, 0.0);

    vec3 ro = vec3(0.0,0.0,-3.0-100.0*max(0.0,iAnimT-iTime));
    vec3 up = vec3(0.0,1.0,0.0);

    float a = 3.0*3.1415926*max(0.0, iAnimT-iTime);
    vec4 col = vec4(cos(a),0.0,sin(a),0.0);
    mat4 m = mat4(col.x,0.0,col.z,0.0,0.0,1.0,0.0,0.0,-col.z,0.0,col.x,0.0,0.0,0.0,0.0,1.0)*glModelViewMatrix;

    vec3 tt = (mat4(col.x,0.0,col.z,0.0,0.0,1.0,0.0,0.0,-col.z,0.0,col.x,0.0,0.0,0.0,0.0,1.0)*vec4(iTranslate, 0.0)).xyz;

    rool = (m*vec4(ro, 1.0)).xyz + tt;
    rool += (m * (eoff / length(rool))).xyz;
    roor = (m*vec4(ro, 1.0)).xyz + tt;
    roor -= (m * (eoff / length(rool))).xyz;

    up = normalize((m*vec4(up, 1.0)).xyz);

    vec3 roo = 0.5*(rool+roor);

    vec3 tal = vec3(-dof / length(roo),0.0,0.0);
    vec3 tar = vec3( dof / length(roo),0.0,0.0);

    vec3 cwl = normalize(tt + tal - rool);
    vec3 cul = normalize(cross(up,cwl));
    vec3 cvl = cross(cwl, cul);
    vec3 cwr = normalize(tt + tar - roor);
    vec3 cur = normalize(cross(up,cwr));
    vec3 cvr = cross(cwr, cur);

    vec2 ratio = vec2(iResolution.x/iResolution.y, 1.0);

    vec2 q0 = (vertex.xy + (vec2(0.5,0.5))/iResolution) * ratio;

    rdbl = normalize((q0.x*cul + q0.y*cvl + 3.0*cwl));
    rdbr = normalize((q0.x*cur + q0.y*cvr + 3.0*cwr));
}
