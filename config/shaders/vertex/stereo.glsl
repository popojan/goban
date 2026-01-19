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

    // Compute effective camera distance along z-axis (independent of rotation)
    // ro.z is negative (camera behind board), iTranslate.z positive = zoom in
    float camDist = -ro.z - iTranslate.z;

    // Scale stereo base to keep on-screen deviation bounded
    // - At refDist or further: use full eof (user's tuned value)
    // - Closer than refDist: reduce eof proportionally to maintain ~1/30 deviation
    float refDist = 3.0;
    float scaleFactor = min(1.0, camDist / refDist);
    vec4 scaledEof = eoff * scaleFactor;

    // Left eye at negative X, right eye at positive X
    // (In OpenGL coords: X right, Y up, Z toward viewer)
    rool = (m*vec4(ro-scaledEof.xyz, 1.0)).xyz+tt;  // Left eye: -X offset
    roor = (m*vec4(ro+scaledEof.xyz, 1.0)).xyz+tt;  // Right eye: +X offset

    up = normalize((m*vec4(up, 1.0)).xyz);

    // Parallel cameras: both eyes look in the SAME direction
    // Stereo effect comes from horizontal offset only, not from toe-in
    // This is the correct approach - toe-in causes eye strain and distortion
    vec3 roo = 0.5*(rool+roor);  // Center position
    vec3 cw = normalize(tt - roo);  // Look at translated target (handles pan/zoom)
    vec3 cu = normalize(cross(up, cw));
    vec3 cv = cross(cw, cu);

    vec2 ratio = vec2(iResolution.x/iResolution.y, 1.0);
    vec2 q0 = (vertex.xy + (vec2(0.5,0.5))/iResolution) * ratio;

    // Parallel cameras with horizontal image shift (HIT) for convergence control
    // dof controls where zero parallax occurs (convergence plane)
    // Positive dof = convergence closer, negative = convergence further
    // At dof=0, convergence is at infinity (pure parallel)
    rdbl = normalize((q0.x + dof)*cu + q0.y*cv + 3.0*cw);  // Left eye: point more right (converge)
    rdbr = normalize((q0.x - dof)*cu + q0.y*cv + 3.0*cw);  // Right eye: point more left (converge)
}
