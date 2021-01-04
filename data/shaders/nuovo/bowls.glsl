void rBowls(in vec3 ro, in vec3 rd, inout Intersection result[2], inout Intersection ret) {
    int isInCup = 0;

    for (int i = 0; i < 4; i++) {
        vec3 cc1 = cc[i];
        cc1.y = bnx.y - legh + bowlRadius2 - 0.5* (bowlRadius2 - bowlRadius);
        vec3 cc2 = cc1;
        vec2 ts = intersectionRaySphereR(ro, rd, cc1, bowlRadius2*bowlRadius2);
        float cuty = cc2.y + cc[i].y * bowlRadius;
        vec2 tc = vec2(dot(-ro + vec3(cuty), nBoard), dot(bnx.xyz - vec3(10.0) - ro, nBoard)) / dot(rd, nBoard);
        bool between = ro.y < cuty && ro.y > bnx.y - legh;
        float ipy = ro.y + ts.x * rd.y;
        if (((ro.y > cuty && max(tc.x, ts.x) <= min(tc.y, ts.y)) || (ro.y <= cuty && ipy< cuty)) && ts.x != noIntersection2.x){
            float firstx = tc.x;
            tc = vec2(min(tc.x, tc.y), max(tc.x, tc.y));
            vec2 ts2 = intersectionRaySphereR(ro, rd, cc2, bowlRadius*bowlRadius);
            ret.t = csg_difference(csg_intersection(tc, ts), ts2);
            isInCup = i+1;
            //if (ret.t.x == ts2.y && ts2.y != noIntersection2.x) {
            //}
            //float rett = max(tc.x, ts.x);
            //ret.t = vec2(!between && ts2.x < rett && ts2.y > rett ? ts2.y : rett, ts.y);
            ret.m = i < 2 ? (i == 0 ? idLidBlack : idLidWhite) : (i == 2 ? idCupBlack: idCupWhite); //|ts2.x < rett && ts2.y > rett ? 
            ret.p = ro + ret.t.x*rd;
            float ln = cc2.y - cuty;
            float r1 = sqrt(bowlRadius*bowlRadius - ln*ln);
            float r2 = sqrt(bowlRadius2*bowlRadius2 - ln*ln);
            float alpha = distance(ret.p.xz, cc2.xz) - 0.5*(r2 + r1);
            //vec3 nn = ret.t.x == tc.x ? nBoard : ret.t.x == ts2.y ? 
            //  normalize(mix(-normalize(ret.p - cc2), nBoard, 1.0/exp(-150.0*alpha)))
            //  : normalize(mix(normalize(ret.p - cc1), nBoard, 1.0/exp(-150.0*alpha)));//(ts2.x > rett || ts2.y < rett) && tc.x > ts.x ? nBoard : ts2.x < rett && ts2.y > rett ? -normalize(ret.p - cc2) : normalize(ret.p - cc1);
            vec3 nn = ret.t.x == tc.x ? nBoard : ret.t.x == ts2.y ? -normalize(ret.p - cc2) : normalize(ret.p - cc1);
            //(ts2.x > rett || ts2.y < rett) && tc.x > ts.x ? nBoard : ts2.x < rett && ts2.y > rett ? -normalize(ret.p - cc2) : normalize(ret.p - cc1);
            /*vec3 nn = ret.t.x == tc.x ? nBoard : ret.t.x == ts2.y ? -normalize(ret.p - cc2) : normalize(ret.p - cc1);
            if(ret.t.x == tc.x) {
                if(alpha > 0.0) {
                    nn = normalize(mix(-normalize(ret.p - cc2), nn, 1.0/exp(25.0*abs(alpha))));
                } else if ( alpha < 0.0) {
                    nn = normalize(mix(normalize(ret.p - cc1), nn, 1.0/exp(25.0*abs(alpha))));
                }
            }*/
            float d1 = distanceRaySphere(ro, rd, cc1, bowlRadius2);
            vec3 q22, q23;
            vec3 cc3 = cc2;
            cc3.y = cuty;
            float d2 = distanceRayCircle(ro, rd, ro + tc.x*rd, vec2(farClip), cc3, r1, q22).y;
            float d3 = distanceRayCircle(ro, rd, ro + tc.x*rd, vec2(farClip), cc3, r2, q23).y;
            float md = d2;
            bool solid = true;
            float find = -d3;
            ret.n = nn;
            //ret.d = -farClip;//-farClip;
            float dot1 = dot(ret.p, rd);
            float dot2 = dot(cc1, rd);

            ret.d = -farClip;
            bool exter = d2 < -d3;
            ret.d = max(d2 > -boardaa ? d2 : -farClip, d3 < boardaa ? -d3 : -farClip);
            if (d1 < 0.0 && ret.d < 0.0 && ro.y > bnx.y - legh) {
                //ret.d = max(d1, ret.d);
                //ret.t = tc;
                ret.p = ro + ret.t.x*rd;
                ret.n = nBoard;
                updateResult(result, ret);
                ret.d = farClip;
                if (!exter) {
                    ret.d = -farClip;
                    ret.t = vec2(ts2.x < farClip && ts2.y > tc.x ? ts2.y : tc.x + 0.001);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(cc2 - ret.p);
                    ret.pid = isInCup;
                }
                else if (dot1 - dot2 < 0.0 && d1 < -0.5*boardaa) {
                    ret.d = -farClip;
                    ret.t = vec2(tc.x + 0.001);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(ret.p - cc1);
                }
            }
            else {
                if (tc.x - 0.02 < ts.x && ts.x < farClip && ts.x > 0.0) {
                    ret.d = d1;
                    ret.t = vec2(ts.x - 0.01);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(ret.p - cc1);
                }
                else if (ts2.y > tc.x && ts2.x < farClip && ts2.y > bnx.y - legh) {
                    ret.d = -farClip;
                    ret.t = vec2(ts2.y);
                    ret.p = ro + ret.t.x*rd;
                    ret.n = normalize(cc2 - ret.p);
                    ret.pid = isInCup;
                }
            }
            if (ret.d < farClip){
                updateResult(result, ret);
            }
        }
    }
}
