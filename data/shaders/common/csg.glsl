vec2 csg_union(in vec2 a, in vec2 b) {
    if(b == noIntersection2)
        return a;
    if(a == noIntersection2)
        return b;
    return vec2(min(a.x, b.x), max(a.y, b.y));
}

vec2 csg_intersection(in vec2 a, in vec2 b) {
    if(b == noIntersection2)
        return b;
    if(a == noIntersection2)
        return a;
    return vec2(max(a.x, b.x), min(a.y, b.y));
}

vec2 csg_difference(in vec2 a, in vec2 b) {
    if(b == noIntersection2)
        return a;
    if(a == noIntersection2)
        return a;
    if(b.x > a.x)
        return vec2(a.x, min(b.x, a.y));
    return vec2(max(b.y, a.x), max(b.y, a.y));
}
