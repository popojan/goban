const int texSolidColor = -1;
const int texWood = 1;
const int texCarpet = 2;

vec4 getTexture(vec4 uvw, int algo, float scale, out float rnd, out vec3 grad) {
    switch(algo) {
        case texWood:
            uvw *= fNDIM/19.0 * 0.3*vec4(23.5, 23.5, 11.3, 1.0);
            rnd = snoise(uvw.xyz, grad);
            uvw = vec4(vec2(length(uvw.xy+0.13*rnd)), 0.13 *uvw.z + 0.1*(rnd-0.5), 1.0);
            uvw = 13.0*uvw;
            rnd = snoise(uvw.xyz, grad);
            break;
        case texCarpet:
            rnd = 0.5;
            //rnd = snoise(3.0 * uvw.xyz, grad);
            break;
    }
    return uvw;
}

vec3 getAlteredNormal(vec4 uvw, vec3 nn, int algo, float scale, vec3 grad) {
    switch(algo) {
        case texCarpet:
            vec3 grad;
            float rnd = snoise(uvw.xyz, grad);
            vec3 n = normalize(mix(nn, normalize(grad.xyz), scale));
            n.y = abs(n.y);
            return n;
    }
    return nn;
}