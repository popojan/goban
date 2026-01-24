in vec3 rdbl;
in vec3 rdbr;
flat in vec3 rool;
flat in vec3 roor;

void main(void)
{
    vec3 cl = render(rool, normalize(rdbl));
    vec3 cr = render(roor, normalize(rdbr));
    float gl = (cl.r+cl.g+cl.b)/3.0;
    float gr = (cr.r+cr.g+cr.b)/3.0;
    glFragColor = vec3(gl, 0.1, gr);
}
