vec3 render(in vec3 ro, in vec3 rd) {
    SortedLinkedList ret;
    init(ret);
    rScene(ro, rd, ret);
    initMaterials();
    return shade(ro, rd, ret);
}
