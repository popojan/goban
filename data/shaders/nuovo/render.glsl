vec3 render(in vec3 ro, in vec3 rd, in vec3 bg)
{
    vec3 col0, col1, col2;
    col0 = col1 = col2 = vec3(0.0);

    Intersection ret[2];

    castRay(ro, rd, ret);

    float alpha1 = smoothstep(boardaa, 0.0, -ret[0].d);
    float alpha2 = smoothstep(boardaa, 0.0, -ret[1].d);
    vec3 col;
    vec4 mcol;
    Material mat; 
    vec3 nn;
    mat = getMaterialColor(ret[0], mcol, rd, ro, nn);
    ret[0].n = nn;
    float w = alpha1;
    float wcol = (1.0-w);
    col = shading(ro, rd, ret[0], mat, mcol);
    if (ret[0].m == idBoard) {
        float alpha3 = smoothstep(boardaa, 0.0, -ret[0].d2);
        if(alpha3 > 0.0) {
            col = mix(col, shading(ro, rd, ret[0], mGrid, vec4(mGrid.clrA, mGrid.specularPower.x)), alpha3);
        }
    }
    col *= (1.0-w);
    gl_FragDepth = (ret[0].m == mBlack.id || ret[0].m == mWhite.id) ? 0.5 : (ret[0].p.y < -0.001 ? 0.25 : 0.75);//; distance(ro, ret[0].p) / 100.0;
    if (alpha1 > 0.0) {
        //ret[0] = mix(ret[0].n, ret[1].n, alpha1)
        mat = getMaterialColor(ret[1], mcol, rd, ro, nn);
        ret[1].n = nn;
        //mcol = 0.5*(mat.clrA+mat.clrB);

        col1 = shading(ro, rd, ret[1], mat, mcol);
        if (ret[1].m == idBoard) {
            float alpha3 = smoothstep(boardaa, 0.0, -ret[1].d2);
            if(alpha3 > 0.0) {
                vec3 col3 = shading(ro, rd, ret[1], mGrid, vec4(mGrid.clrA, mGrid.specularPower.x));
                col1 = mix(col1, col3, alpha3);
            }
            col += alpha1*col1;
            wcol += alpha1;
        }
        else {
            col += alpha1*(1.0-alpha2)*col1;
            wcol += alpha1*(1.0-alpha2);
        }
    }
    return col/wcol;
}

void main(void)
{
    glFragColor =  render(roo, normalize(rdb), bgA);
}
