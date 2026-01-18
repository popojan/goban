const int texSolidColor = -1;
const int texWood = 1;
const int texShell = 2;
const int texSlate = 3;
const int texCarpet = 4;

vec4 getTexture(vec4 uvw, int algo, float scale, out float rnd, out vec3 grad) {
    switch(algo) {
        case texWood:
            uvw *= fNDIM/19.0 * 0.3*vec4(23.5, 23.5, 11.3, 1.0);
            break;
        case texShell:
            uvw *= fNDIM/19.0 * 0.3*vec4(13.5, 13.5, 1.0, 1.0);
            break;
        case texCarpet:
            rnd = 0.5;
            break;
        case texSlate:
            uvw *= 77.0;
            break;
    }
    switch(algo) {
        case texWood:
        case texShell:
            rnd = snoise(uvw.xyz, grad);
            break;
    }
    switch(algo) {
        case texWood:
            uvw = 7.0 * vec4(vec2(length(uvw.xy+0.13*rnd)), 0.13 *uvw.z + 0.1*(rnd-0.5), 1.0);
            break;
        case texShell:
            uvw = 7.0 * vec4(vec2(length(uvw.xy+0.23*rnd*rnd)), 0.13 *uvw.z + 0.05*(rnd-0.5), 1.0);
            break;
    }
    switch(algo) {
        case texWood:
        case texShell:
            rnd = snoise(uvw.xyz, grad);
            break;
    }
    return uvw;
}

vec3 getAlteredNormal(vec4 uvw, vec3 nn, int algo, float scale, vec3 grad) {
    switch(algo) {
        case texWood:
            scale *= 0.75;
        case texCarpet:
        case texSlate:
            vec3 grad;
            float rnd = snoise(uvw.xyz, grad);
            vec3 n = normalize(mix(nn, normalize(grad.xyz), scale));
            return n;
    }
    return nn;
}
