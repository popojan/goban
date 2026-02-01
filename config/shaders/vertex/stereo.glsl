#version 330 core

layout(location = 0) in vec4 vertex;
out vec3 rdbl;
out vec3 rdbr;
flat out vec3 rool;
flat out vec3 roor;

uniform vec2 iResolution;
uniform float iTime;
uniform mat4 glModelViewMatrix;
uniform vec2 cameraPan;
uniform float cameraDistance;
uniform float iAnimT;

uniform float eof;
uniform float dof;

const float focalLength = 3.0;

void main() {
    gl_Position = vertex; //gl_Vertex;
    vec4 eoff = vec4(eof,0.0,0.0, 0.0);

    float introZoom = 100.0*max(0.0,iAnimT-iTime);
    vec3 up = vec3(0.0,1.0,0.0);

    float a = 3.0*3.1415926*max(0.0, iAnimT-iTime);
    vec4 col = vec4(cos(a),0.0,sin(a),0.0);
    mat4 m = mat4(col.x,0.0,col.z,0.0,0.0,1.0,0.0,0.0,-col.z,0.0,col.x,0.0,0.0,0.0,0.0,1.0)*glModelViewMatrix;
    mat4 introM = mat4(col.x,0.0,col.z,0.0,0.0,1.0,0.0,0.0,-col.z,0.0,col.x,0.0,0.0,0.0,0.0,1.0);

    vec3 ta = (introM * vec4(cameraPan.x, 0.0, cameraPan.y, 1.0)).xyz;
    vec3 viewDir = normalize((m * vec4(0.0, 0.0, 1.0, 0.0)).xyz);
    vec3 center = ta - (cameraDistance + introZoom) * viewDir;

    up = normalize((m*vec4(up, 1.0)).xyz);

    vec3 cw = normalize(ta - center);
    vec3 cu = normalize(cross(up, cw));
    vec3 cv = cross(cw, cu);

    // Scale stereo base with distance
    float scaleFactor = (cameraDistance + introZoom) / focalLength;
    vec3 scaledEye = scaleFactor * eoff.xyz;

    // Left eye at negative X, right eye at positive X in camera space
    rool = center - (m * vec4(scaledEye, 0.0)).xyz;
    roor = center + (m * vec4(scaledEye, 0.0)).xyz;

    vec2 ratio = vec2(iResolution.x/iResolution.y, 1.0);
    vec2 q0 = (vertex.xy + (vec2(0.5,0.5))/iResolution) * ratio;

    // Parallel cameras with horizontal image shift (HIT) for convergence control
    rdbl = normalize((q0.x + dof)*cu + q0.y*cv + focalLength*cw);
    rdbr = normalize((q0.x - dof)*cu + q0.y*cv + focalLength*cw);
}
