in vec3 rdbl;
in vec3 rdbr;
flat in vec3 rool;
flat in vec3 roor;

void main(void)
{
    vec3 cl = render(rool, normalize(rdbl));
    vec3 cr = render(roor, normalize(rdbr));
    //glFragColor = vec3(0.9707,0.0,0.0)*cl.rgb + vec3(0.0,0.7709,0.1989)*cr.rgb;
    glFragColor = vec3(cl.r, cr.g, cr.b);
}