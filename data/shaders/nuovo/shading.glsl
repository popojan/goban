vec3 shade(in vec3 ro, in vec3 rd, in SortedLinkedList ret) {
    float tb = dot(-ro, nBoard) / dot(rd, nBoard);
    vec3 bg = vec3(exp(-0.35*max(0.0,length(ro + rd * tb))));
    vec3 col = vec3(0.0);
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

            vec4 texo = uvw * sqrt(19.0 / fNDIM);
            vec4 texp = vec4(0.0);
            vec2 a = ret.ip[j].a;
            float rnd = snoise(texo.xyz, grad);
            //rnd = snoise(texo.xyz + 0.03*grad, grad);

            rnd =  mix(smoothstep(-2.0, 2.0, rnd/max(1.0, 0.5*distance(ipp, ro))), 0.0, 0.0);//(1.0 + 0.5/(1.0+ exp(-rnd)));

            vec3 c = mix(mix(mix(mcola1, mcolb1, rnd), bg, a.y), mcolc1, a.x);
            vec3 diffuse = (clamp(dot(nn, lig2), 0.0, 1.0)/*+clamp(dot(nn, lig3), 0.0, 1.0)*/) * c;
            vec4 das = materials[midxo].das;
            float ambientLevel = das.y;
            vec3 ambient = ambientLevel * c;

            float num = 0.0;
            float den = 0.0;
            int pos = 0;
            int neg = 0;
            float shadow = sScene(ipp, lposs, 10.5, ret.ip[j]).x;
            //clamp(softshadow(ro, rd, ret.ip[j], lpos2, 3.5).x, 0.0, 1.0);
            float power = pow(clamp(dot(ref, lig2), 0.0, 1.0), das.w);
            vec3 specular= vec3(0.5);
            /*SortedLinkedList x;
            init(x);
            //x.ip[x.idx[0]].gid = ret.ip[ret.idx[0]].gid;
            vec3 ldir = normalize(lposs-ipp);

            rScene(ipp + 0.001*ldir, ldir, x, true);
            bool valid = false;
            float d2 = 1.0;

            vec3 iqq = ipp + 0.001*ldir + x.ip[x.idx[0]].t.x * ldir;
            for(int k = 0; k < N; ++k) {
                if(x.size > k) {
                    d2 *= smoothstep(-0.5,0.5, x.ip[x.idx[k]].d);//min(d0, x.ip[x.idx[k]].d);
                }
            }
            float shadow = d2;
            vec3 specular= vec3(0.5);
            float power = pow(clamp(dot(ref, lig2), 0.0, 1.0), 64.0);
            //if(ipp.y < 0.001) power = 0.0;
            //if(iqq.y > ipp.y) shadow = 1.0;*/
            diffuse = clamp(das.x * (1.0-das.y)*shadow*diffuse + ambient + das.z*power*specular*shadow, 0.0,1.0);

            col  = mix(diffuse, col, alpha);
            //col = vec3(-d/boardaa/3.0);
        }
    }
    return pow(col, gamma*exp(contrast*(vec3(0.5) - col)));
}
