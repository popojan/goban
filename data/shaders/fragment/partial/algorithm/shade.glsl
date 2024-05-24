void aamix(in vec3 ro, in vec3 rd, vec3 bg, in IP ip, inout vec3 col) {
    if(ip.oid == idNone) return;
    vec3 lposs = lpos2;
    float d = ip.d;
    vec3 ipp = ro + ip.t.x * rd;
    float alpha = smoothstep(-boardaa, 0.0, d);
    vec3 lig2 = normalize(lposs - ipp);
    int midxo = matidx[ip.oid];
    int midxp = matidx[ip.pid];
    float fog = ip.fog;
    vec3 mcola1 = fog * materials[midxo].col[0];
    vec3 mcolb1 = fog * materials[midxo].col[1];
    vec3 mcolc1 = fog * materials[midxp].col[1];
    float rnd = 0.5;
    vec3 grad;
    vec4 uvw = getTexture(ip.uvw, materials[midxo].tid, 1.0, rnd, grad);

    rnd =  smoothstep(-2.0, 2.0, rnd/max(1.5, 0.5*distance(ipp, ro)));

    vec3 c = mix(mix(mix(mcola1, mcolb1, rnd), bg, ip.a.y), mcolc1, ip.a.x);
    vec3 nn = getAlteredNormal(ip.uvw, ip.n, materials[midxo].tid, 0.2, grad);
    vec3 diffuse = (clamp(dot(nn, lig2), 0.0, 1.0)) * c;
    vec4 das = materials[midxo].das;
    float ambientLevel = das.y;
    vec3 ambient = ambientLevel * c;

    float num = 0.0;
    float den = 0.0;
    int pos = 0;
    int neg = 0;
    vec2 shadow = sScene(ipp, lposs, ldia2, ip);

    vec3 ref = reflect(rd, nn);
    float power = pow(clamp(dot(ref, lig2), 0.0, 1.0), das.w);
    vec3 specular = vec3(0.5);

    diffuse = clamp(das.x * (1.0-das.y)*shadow.x*diffuse + ambient
        + das.z*power*specular*shadow.x, 0.0,1.0);

    col  = mix(diffuse, col, alpha);
}

vec3 shade(in vec3 ro, in vec3 rd, in SortedLinkedList ret) {
    float tb = dot(-ro, nBoard) / dot(rd, nBoard);
    vec3 bg = vec3(exp(-0.35*max(0.0,length(ro + rd * tb))));
    vec3 col = vec3(0.0);
    vec3 grad0;
    aamix(ro, rd, bg, ret.ip2, col);
    aamix(ro, rd, bg, ret.ip1, col);
    aamix(ro, rd, bg, ret.ip0, col);
    return pow(col, gamma*exp(contrast*(vec3(0.5) - col)));
}
