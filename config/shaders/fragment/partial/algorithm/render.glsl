// What render() last classified the pixel as, in the same units gl_FragDepth
// takes: 0.25 the board seen from below, 0.5 a stone, 0.75 everything else.
// The overlay's own passes sit between them — 0.4 for a label on a stone, 0.6
// for one on the board — which is what hides an annotation behind a stone.
//
// Reported rather than written here, because the *stereo* shader calls render()
// twice into one depth buffer: whichever eye ran last used to win outright, so
// wherever the two eyes disagreed about a stone the label showed straight
// through it. The caller decides; see partial/stereo/on.glsl.
float sceneDepth;

vec3 render(in vec3 ro, in vec3 rd) {
    SortedLinkedList ret;
    init(ret);

    float dist = dot(-ro, nBoard)/length(ro);
    if(dist > 0.0) {
        ro.y = -ro.y;
        rd.y = -rd.y;
    }
    rScene(ro, rd, ret);

    sceneDepth = (ret.ip0.oid == idBlackStone || ret.ip0.oid == idWhiteStone) ? 0.5 : (dist > 0.0 ? 0.25 : 0.75);

    initMaterials();
    return shade(ro, rd, ret);
}
