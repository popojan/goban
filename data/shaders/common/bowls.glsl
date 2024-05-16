void rBowls(in vec3 ro, in vec3 rd, inout SortedLinkedList ret, bool shadow) {
    int isInCup = 0;

    for (int i = 0; i < 4; i++) {
        vec3 cc1 = cc[i].xyz;
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
            vec2 rett = csg_difference(csg_intersection(tc, ts), ts2);
            isInCup = 0;

            vec3 retp = ro + rett.x*rd;
            float ln = cc2.y - cuty;
            float r1 = sqrt(bowlRadius*bowlRadius - ln*ln);
            float r2 = sqrt(bowlRadius2*bowlRadius2 - ln*ln);
            float alpha = distance(retp.xz, cc2.xz) - 0.5*(r2 + r1);

            float d1 = distanceRaySphere(ro, rd, cc1, bowlRadius2);
            vec3 q22, q23;
            vec3 cc3 = cc2;
            cc3.y = cuty;
            float d2 = distanceRayCircle(ro, rd, ro + tc.x*rd, vec2(farClip), cc3, r1, q22).y;
            float d3 = distanceRayCircle(ro, rd, ro + tc.x*rd, vec2(farClip), cc3, r2, q23).y;
            float md = d2;
            bool solid = true;
            float find = -d3;

            float dot1 = dot(retp, rd);
            float dot2 = dot(cc1, rd);

            bool exter = d2 < -d3;

            vec3 nn = nBoard;
            float xxx = max(d2 > -boardaa ? d2 : -farClip, d3 < boardaa ? -d3 : -farClip);
            if(rett.x == tc.x) { //top edge
                nn = nBoard;
            } else {
                if(rett.x == ts2.y) { //inside sphere
                    nn = -normalize(retp - cc2);
                    isInCup = i+1;
                    //xxx = -farClip;
                } else { //outside sphere
                    nn = normalize(retp - cc1);
                    //xxx = d1;
                }
            }

            vec4 uvw = vec4(retp, 1.0);

            if(rett.x > 0.0) {
                IP ipp;
                ipp.t = vec3(rett, 0.0);
                if(try_insert(ret, ipp)) {
                    ipp.n = nn;
                    ipp.oid = i < 2 ? (i == 0 ? idCupBlack :idCupWhite) : (i == 2 ? idLidBlack : idLidWhite) ;
                    ipp.pid = idBlackStone;
                    ipp.isInCup = isInCup;
                    ipp.uid = idCupBlack + i;
                    ipp.d = xxx;
                    ipp.a = vec2(0.0);
                    ipp.uvw = uvw;
                    ipp.fog = 1.0;
                    insert(ret, ipp);
                }

                vec3 retn = nBoard;

                float retd = farClip;
                vec2 rett = vec2(0.0);
                vec3 retp;
                if (d1 < 0.0 && xxx < 0.0 && ro.y > bnx.y - legh) {
                    if (!exter) {
                       retd = -farClip;
                       rett = vec2(ts2.x < farClip && ts2.y > tc.x ? ts2.y : tc.x + 0.001);
                       retp = ro + rett.x*rd;
                       retn = normalize(cc2 - retp);
                    }
                    else if (dot1 - dot2 < 0.0 && d1 < -0.5*boardaa) {
                       retd = -farClip;
                       rett = vec2(tc.x + 0.001);
                       retp = ro + rett.x*rd;
                       retn = normalize(retp - cc1);
                    }
                }
                else {
                    if (tc.x < ts.x && ts.x < farClip && ts.x > 0.0) {
                       retd = d1;
                       rett = vec2(ts.x - 0.01);
                       retp = ro + rett.x*rd;
                       retn = normalize(retp - cc1);
                    }
                    else if (ts2.y > tc.x && ts2.x < farClip && ts2.y > bnx.y - legh) {
                       retd = -farClip;
                       rett = vec2(ts2.y);
                       retp = ro + rett.x*rd;
                       retn = normalize(cc2 - retp);
                    }
                }
                if(retd < farClip && rett.x > 0.0) {
                    ipp.t = vec3(rett, 0.0);
                    if (try_insert(ret, ipp)) {
                       ipp.n = retn;
                       ipp.oid = i < 2 ? (i == 0 ? idCupBlack :idCupWhite) : (i == 2 ? idLidBlack : idLidWhite);
                       ipp.pid = idBlackStone;
                       ipp.isInCup = isInCup;
                       ipp.uid = idCupBlack + i;
                       ipp.d = retd;
                       ipp.a = vec2(0.0);
                       ipp.uvw = uvw;
                       insert(ret, ipp);
                    }
                }
            }
        }
    }
}

vec2 sBowls(in vec3 pos, in vec3 lig, float ldia2, in IP ipp) {
    vec2 ret = vec2(1.0);
    for (int j = 0; j <= 2; j += 2) {
        int jj = pos.x < 0.0 ? j+0 : j+1;
        if(ipp.oid != idBowlBlackStone && ipp.oid != idBowlWhiteStone && ipp.uid != idCupBlack +jj) {
            vec3 cci = cc[jj].xyz;
            float yy = bnx.y - legh + bowlRadius2 - 0.5* (bowlRadius2 - bowlRadius);
            cci.y = yy + cci.y * bowlRadius;
            float ln = yy - cci.y;

            float r2 = sqrt(bowlRadius2*bowlRadius2 - ln*ln);
            ret *= sCircle(cci, r2, pos, lig, ldia2, ipp);

            cci.y = cci.y + 0.5 * (bnx.y- legh + cci.y);
            ln = yy - cci.y;
            r2 = sqrt(bowlRadius2*bowlRadius2 - ln*ln);
            ret *= sCircle(cci, r2, pos, lig, ldia2, ipp);

            cci.y = 0.5 * (bnx.y- legh + cci.y);
            ln = yy - cci.y;
            r2 = sqrt(bowlRadius2*bowlRadius2 - ln*ln);
            ret *= sCircle(cci, r2, pos, lig, ldia2, ipp);
        }
    }
    return ret;
}