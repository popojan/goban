vec3 shading(in vec3 ro, in vec3 rd, in Intersection ip, const Material mat, vec4 col){
    vec3 ret;
    if (mat.id == mBack.id) {
        ret = mat.clrA;
    }
    else {
        vec3 nn = ip.n;
        vec3 ref = reflect(rd, nn);
        vec3 lig = normalize(lpos - ip.p);
        vec3 lig2 = normalize(lpos2 - ip.p);
        vec3 lig3 = normalize(lpos3 - ip.p);
        vec2 shadow = pow(softshadow(ip.p, ip.n, lpos, ldia, ip.m, false, ip.uid, ip.pid), vec2(1.0, 0.25));//0.6

        float adsy = dot(vec3(0.6,0.3,0.3), clamp(vec3(dot(nn, lig), dot(nn, lig2), dot(nn, lig3)),0.0,1.0));
        vec4 pws = clamp(vec4(dot(ref, lig), dot(ref, lig2), dot(ref, lig3), dot(ref, lig3)), 0.0, 1.0);
        vec3 pwr = pow(pws.xyz, vec3(col.w));
        vec3 score  = mat.diffuseAmbientSpecularWeight * vec3(adsy * shadow.x, shadow.y,shadow.y*(0.25*pwr.x + pwr.y + pwr.z));
        ret = (score.x + score.y)*col.xyz + score.z;
    }
    ret = pow(ret, gamma*exp(contrast*(vec3(0.5) - ret)));
    return ret;
}
