in vec3 rdb;
flat in vec3 roo;

void main(void) {
    glFragColor =  render(roo, normalize(rdb));
}