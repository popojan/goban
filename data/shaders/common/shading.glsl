float rand2(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec3 shade(in vec3 ro, in vec3 rd, in SortedLinkedList ret) {
    float tb = dot(-ro, nBoard) / dot(rd, nBoard);
    vec3 bg = vec3(exp(-0.35*max(0.0,length(ro + rd * tb))));
    vec3 col = vec3(0.0);
    vec3 grad0;
    if(ret.size > 0) {
        float wcol = 1.0;
        int k = ret.idx[0];
        float rest = 1.0;
        float alpha = 1.0;
        vec3 lposs = lpos2;
        for (int i = N - 1; i >= 0; --i) {
            if(ret.size <= i) continue;
            int j = ret.idx[i];
            float d = ret.ip[j].d;
            alpha = smoothstep(-boardaa, 0.0, d);
            vec3 ipp = ro + ret.ip[j].t.x * rd;
            vec3 nn = ret.ip[j].n;
            vec3 ref = reflect(rd, nn);
            vec3 lig2 = normalize(lposs - ipp);
            vec3 grad;
            int midxo = matidx[ret.ip[j].oid];
            int midxp = matidx[ret.ip[j].pid];
            float fog = ret.ip[j].fog;
            vec3 mcola1 = fog * materials[midxo].col[0];
            vec3 mcolb1 = fog * materials[midxo].col[1];
            vec3 mcolc1 = fog * materials[midxp].col[1];
            vec4 uvw = ret.ip[j].uvw;
            float rnd = 0.5;
            if(ret.ip[j].oid != idTable) {
                uvw *= fNDIM/19.0 * 0.3*vec4(23.5, 23.5, 11.3, 1.0);
                float ns = 0.08*sin(0.4+1.8*uvw.z);
                rnd = snoise(uvw.xyz, grad);
                uvw = vec4(length(uvw.xy+0.13*rnd), length(uvw.xy+0.13 * rnd), 1.0, 1.0);
                uvw = 13.0*uvw;
                rnd = snoise(uvw.xyz, grad);
            }
            vec4 texo = uvw * sqrt(19.0 / fNDIM);
            vec4 texp = vec4(0.0);
            vec2 a = ret.ip[j].a;

            rnd =  mix(smoothstep(-2.0, 2.0, rnd/max(1.0, 0.5*distance(ipp, ro))), 0.0, 0.0);

            vec3 c = mix(mix(mix(mcola1, mcolb1, rnd), bg, a.y), mcolc1, a.x);
            vec3 diffuse = (clamp(dot(nn, lig2), 0.0, 1.0)) * c;
            vec4 das = materials[midxo].das;
            float ambientLevel = das.y;
            vec3 ambient = ambientLevel * c;

            float num = 0.0;
            float den = 0.0;
            int pos = 0;
            int neg = 0;
            vec2 shadow = sScene(ipp, lposs, 10.5, ret.ip[j]);

            float power = pow(clamp(dot(ref, lig2), 0.0, 1.0), das.w);
            vec3 specular= vec3(0.5);

            diffuse = clamp(das.x * (1.0-das.y)*shadow.x*diffuse + ambient
                + das.z*power*specular*shadow.x, 0.0,1.0);

            col  = mix(diffuse, col, alpha);

        }
    }
    return pow(col, gamma*exp(contrast*(vec3(0.5) - col)));
}
