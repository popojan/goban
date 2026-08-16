in vec3 rdbl;
in vec3 rdbr;
flat in vec3 rool;
flat in vec3 roor;

void main(void)
{
    vec3 cl = render(rool, normalize(rdbl));
    float dl = sceneDepth;
    vec3 cr = render(roor, normalize(rdbr));
    float dr = sceneDepth;
    // One depth buffer, two eyes. Take the nearer classification: where either
    // eye sees a stone the pixel counts as a stone, so an annotation behind it
    // is hidden in both. Keeping only the second call's value — which is what
    // writing gl_FragDepth inside render() did — let the best-move letter shine
    // through a stone across the whole band where the eyes disagree, which for
    // a stone-sized object is most of it.
    gl_FragDepth = min(dl, dr);
    float gl = (cl.r+cl.g+cl.b)/3.0;
    float gr = (cr.r+cr.g+cr.b)/3.0;
    glFragColor = vec3(gl, 0.1, gr);
}
