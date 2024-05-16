void rPlaneY(in vec3 ro, in vec3 rd, in vec3 p0, in vec3 n, inout SortedLinkedList ret, int oid, bool shadow) {

    if(shadow) return;
    float denom = dot(rd, n);
    if (abs(denom) > 1e-6) {
        float t = dot(p0 - ro, n) / denom;
        if (t >= 0.0) {
            IP ip;
            ip.t = vec3(t, t, 1.0);

            if (try_insert(ret, ip)) {
                ip.n = n;
                ip.d = -1.0;
                ip.oid = idTable;
                ip.pid = idTable;
                ip.a = vec2(1.0);
                ip.uid = idTable;

                vec3 ipp = ro + rd*ip.t.x;
                vec2 xz = ipp.xz;
                vec3 color;
                const float DT = 2.0*3.1415926 / 8.0;
                float fa = atan(xz.y, xz.x);
                float iord = floor((fa + 0.5*DT) / DT);
                float far = iord*DT;
                float fb = cos(abs(fa - far))*length(xz);
                fa = mod(fa, DT);

                bool a = fa < DT;
                bool b = fb < 0.5;
                float c05a = abs(0.5*DT - mod(fa + 0.5*DT, DT)) / DT;
                float c05b = abs(0.25 - mod(fb, 0.5)) / 0.5;
                bool third = fb < 1.5
                    || fb > 3.0
                    || c05a > 0.4
                    || c05b  > 0.45;
                float jord = fb;
                bool ab = (iord == -2.0)
                    || (iord == -1.0 && mod(jord, 1.5) < 1.0)
                    || (iord == 3.0 && mod(jord, 1.5) >= 1.0)
                    || (iord == 0.0 && mod(jord + 0.5, 1.5) < 1.0)
                    || ((iord == 4.0 || iord == -4.0) && mod(jord + 0.5, 1.5) >= 1.0)
                    || (iord == -1.0 && mod(jord, 1.5) < 1.0)
                    || (iord == 3.0 && mod(jord, 1.5) >= 1.0)
                    || (iord == 1.0 && mod(jord, 1.5) >= 0.5)
                    || (iord == -3.0 && mod(jord, 1.5) < 0.5);
                c05a = smoothstep(0.395, 0.4, c05a);
                c05b = smoothstep(0.44, 0.45, c05b);
                float w1 = 0.0;
                float alpha = 3.1415926/4.0;
                float al = 0.0;
                if (!third && ab) {
                    ip.oid = idYin;
                    al = max(c05b, c05a);
                }
                else if (!third){
                    ip.oid = idYang;
                    al = max(c05b, c05a);
                }
                else {
                    alpha = 0.0;
                }
                ip.uvw = 73.0*ipp.xyzx;

                float cosa = cos(alpha);
                float sina = sin(alpha);
                xz = mat2(cosa, sina, -sina, cosa)*ipp.xz;
                vec3 scoord = ipp;

                ip.a = vec2(al, 0.0);
                ip.fog = exp(-0.35*max(0.0,length(ipp)));
                insert(ret, ip);
            }
        }
    }
}