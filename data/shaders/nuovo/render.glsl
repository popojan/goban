vec3 render(in vec3 ro, in vec3 rd) {
    SortedLinkedList ret;
    init(ret);
    rScene(ro, rd, ret, false);
    initMaterials();
    return shade(ro, rd, ret);
}

void main(void)
{
    glFragColor =  render(roo, normalize(rdb));
}
