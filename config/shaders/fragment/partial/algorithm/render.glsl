vec3 render(in vec3 ro, in vec3 rd) {
    SortedLinkedList ret;
    init(ret);

    float dist = dot(-ro, nBoard)/length(ro);
    if(dist > 0.0) {
        ro.y = -ro.y;
        rd.y = -rd.y;
    }
    rScene(ro, rd, ret);

    gl_FragDepth = (ret.ip0.oid == idBlackStone || ret.ip0.oid == idWhiteStone) ? 0.5 : (dist > 0.0 ? 0.25 : 0.75);

    initMaterials();
    return shade(ro, rd, ret);
}
