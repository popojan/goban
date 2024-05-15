vec3 render(in vec3 ro, in vec3 rd) {
    SortedLinkedList ret;
    init(ret);
    rScene(ro, rd, ret, false);
    initMaterials();
    return shade(ro, rd, ret);
}

void main(void) {
    c = vec3(1.0, 0.25, wwy/wwx);
    bnx = vec4(-c.x, -0.2, -c.z, 0.0);
    glFragColor =  render(roo, normalize(rdb));
}
