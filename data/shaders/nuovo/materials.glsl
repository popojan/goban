const vec3 bgA = vec3(0.0, 0.0, 0.0);
const vec3 bgB = vec3(0.0, 0.0, 0.0);

const Material mCup = Material(idCupBlack, vec3(0.7, 0.15, 0.25), vec3(32.0), vec3(0.293333, 0.1713725, 0.038039), vec3(0.053333,0.0133725,0.029039), vec3(0.17333,0.1613725,0.089039), 1.5);
const Material mBoard = Material(idBoard, vec3(0.7, 0.15, 0.05), vec3(42.0), vec3(0.93333, 0.813725, 0.38039), vec3(0.53333,0.413725,0.09039), vec3(0.7333,0.613725,0.19039), 1.5);
const Material mTable = Material(idTable, vec3(1.0, 0.15, 0.0), vec3(4.0), vec3(0.566,0.0196,0.0176), vec3(0.766,0.1196,0.1176), vec3(0.666,0.0196,0.0176), 0.0);
const Material mWhite = Material(idWhiteStone, vec3(0.53, 0.53, 0.2), vec3(32.0, 41.0, 2.0), vec3(0.96), vec3(0.96,0.91,0.97), vec3(0.93), 0.5);
const Material mBlack = Material(idBlackStone, vec3(0.53, 0.53, 0.15), vec3(28.0), vec3(0.08), vec3(0.04), vec3(0.10), 0.5);
const Material mRed = Material(idLastBlackStone, vec3(0.3, 0.15, 0.25), vec3(4.0), vec3(0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0), 0.0);
const Material mBack = Material(idBack, vec3(0.0, 1.0, 0.0), vec3(1.0), bgA, bgB, bgA, 0.0);
const Material mGrid  = Material(idGrid, vec3(1.5, 0.15, 0.15), vec3(42.0), vec3(0.0),vec3(0.0), vec3(0.0), 0.0);

Material getMaterial(int m) {
    Material ret;
    if (m == idTable) {
        ret = mTable;
    }
    else if (m == idBoard || m == idLeg1 || m == idLeg2 || m == idLeg3 || m == idLeg4) {
        ret = mBoard;
    }
    else if (m == idCupBlack) {
        ret = mCup;
    }
    else if (m == idCupWhite) {
        ret = mCup;
    }
    else if (m == idLidBlack) {
        ret = mCup;
    }
    else if (m == idLidWhite) {
        ret = mCup;
    }
    else if (m == idWhiteStone || m == idWhiteArea || m == idCapturedWhiteStone || m == idBowlWhiteStone) {
        ret = mWhite;
    }
    else if (m == idBlackStone || m == idBlackArea || m == idCapturedBlackStone || m == idBowlBlackStone) {
        ret = mBlack;
    }
    else if (m == idLastBlackStone || m == idLastWhiteStone) {
        ret = mRed;
    }
    else if (m == idGrid) {
        ret = mGrid;
    }
    else {
        ret = mBack;
    }
    return ret;
}

Material getMaterialColor(in Intersection ip, out vec4 mcol, in vec3 rd, in vec3 ro, out vec3 nn) {
    Material mat = getMaterial(ip.m);
    Material m0 = mat;//;at = mBoard;// getMaterial(idBoard);//ip.m
    bool noisy = false;
    float mult = 1.0;
    vec3 scoord;
    float sfreq;
    float mixcoef, smult1, smult2, smult3, sadd1, mixmult, mixnorm;
    float fpow = 1.0;
    mixmult = 0.0;
    mixnorm = mixmult;
    smult1 = 0.0;
    smult2 = 0.0;
    smult3 = 1.0;
    mcol.xyz = m0.clrA;
    vec3 mcolb = m0.clrB;
    vec3 mcolc = m0.clrC;
    vec3 flr = ip.dummy.xyz;
    vec3 scrd2 = 64.0*(ip.p.xyz-flr);
    vec3 scrd = ip.p.xyz - flr;
    float degrade = (1.0 + floor(length(ro) / 3.0));
    vec2 xz;
    xz = scrd.xz;
    if (mat.id == mBoard.id || mat.id == mCup.id) {
        scoord = 16.0*vec3(xz.x,scrd.y,xz.y) + vec3(0.0, 0.25, 0.0);
        scoord.x = 2.0*length(scoord.xy);
        scoord.z = 2.0*scoord.y;
        scoord.y = 2.0*length(scoord.xy);
        scoord.xyz = 0.1*vec3(length(scoord.yz));
        noisy = true;
        scrd = scoord;
	      mixmult = 1.0;
        mixnorm = 0.01;
    }
    else if (mat.id == mGrid.id) {
        scoord = 350.0*scrd;
        sfreq = 1.5;
        noisy = true;
        scrd = scoord;
    }
    else if (mat.id == mWhite.id || mat.id == mBlack.id) {
        float alpha = ip.dummy.w;
        float cosa = cos(alpha);
        float sina = sin(alpha);
        //scoord = reflect(rd, ip.n);
        //scoord = 0.5*(1.0 + sin(vec3(3.0,3.0,3.0)*scoord));
        sfreq = 1.0*(1.0 + 0.1*sin(11.0 + xz.x));
        sadd1 = 5.0 + sin(131.0 + 15.0*xz.x);
        noisy = true;
	if(mat.id == mBlack.id) {
		scrd2 *= 5.0;
	}
        else {
		//scrd2 *= 3.0;
		vec2 xz = mat2(cosa, sina, -sina, cosa)*scrd2.xz;
		scrd2 = xz.xyy;
		scrd2.y = scrd.y;
		scrd2.z = 1.0;
	}
	mixmult = 0.015;
  mixnorm = 0.015;
    }
    if (m0.id == mTable.id) {
        vec3 color;// = mix(mTable.clrA,mTable.clrB,density);
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
        bool third = fb < 1.5 || fb > 3.0 || c05a > 0.4 || c05b  > 0.45;// ? 0.5 : 0.0;
        float jord = fb;
        bool ab = (iord == -2.0) || (iord == -1.0 && mod(jord, 1.5) < 1.0) || (iord == 3.0 && mod(jord, 1.5) >= 1.0)
            || (iord == 0.0 && mod(jord + 0.5, 1.5) < 1.0) || ((iord == 4.0 || iord == -4.0) && mod(jord + 0.5, 1.5) >= 1.0)
            || (iord == -1.0 && mod(jord, 1.5) < 1.0) || (iord == 3.0 && mod(jord, 1.5) >= 1.0)
            || (iord == 1.0 && mod(jord, 1.5) >= 0.5) || (iord == -3.0 && mod(jord, 1.5) < 0.5);
        c05a = smoothstep(0.395, 0.4, c05a);
        c05b = smoothstep(0.44, 0.45, c05b);
        float w1 = 0.0;
        float alpha = 3.1415926/4.0;
        if (!third && ab) //;(a &&!b) || (!a && b))
        {
            mcol.xyz = mix(mTable.clrA, mTable.clrC, max(c05b, c05a));
        }
        else if (!third){
            mcol.xyz = mix(mTable.clrB, mTable.clrC, max(c05b, c05a));
        }
        else {
            mcol.xyz = mTable.clrC;
            alpha = 0.0;
        }
            float cosa = cos(alpha);
            float sina = sin(alpha);
            xz = mat2(cosa, sina, -sina, cosa)*ip.p.xz;
        scoord = ip.p;//16.0*vec3(xz.x, ip.p.y, xz.y) + vec3(0.0, 0.25, 0.0);
        scrd = 66.1*scoord;
        scrd2 = 16.1*scoord;
        noisy = true;
        const float al = 0.15;
        float mixt = exp(-0.35*max(0.0, length(ip.p)));
        mcolb = mixt*(mcol.xyz + (al)*(vec3(1.0) - mcol.xyz));
        mcolc = mixt*((1.0 - al)*mcol.xyz);
        mcol.xyz *= mixt;
        mixmult = 0.1;
        mixnorm = 0.0;
    }
    float rnd = 0.0;
    float rnd2 = 0.0;
    vec3 grad = vec3(0.0, 1.0, 0.0);
    vec3 grad2 = vec3(0.0, 1.0, 0.0);
    //if (noisy) {
        rnd = snoise(scrd, grad);
        rnd2 = snoise(scrd2+mixmult*grad, grad2);
    //}
    if (mat.id == mBoard.id || mat.id == mCup.id) {
        float w1 = 3.0*length(scoord.yx - 0.5*vec2(1.57 + 3.1415*rnd));
        float w2 = 0.1*(scoord.x + scoord.z);
        smult1 = mix(clamp(abs(rnd),0.0,1.0), 1.0, 0.01)*(clamp(0.25*(sin(grad.x)), 0.0, 1.0));
        smult2 = mix(1.0 - clamp(abs(rnd),0.0,1.0), 1.0, 0.01)*(clamp(0.25*(sin(1.5*grad.y)), 0.0, 1.0));

        smult3 = 1.0 - smult2;
    }
    else if (mat.id == mBlack.id || mat.id == mGrid.id || mat.id == mWhite.id || mat.id == mTable.id) {
        smult1 = clamp(abs(rnd),0.0,1.0);
        smult2 = clamp(abs(rnd2),0.0,1.0);
        smult3 = 0.5;//clamp(abs(grad.z),0.0,1.0);
        mixmult = 0.0;
    }
    mcol.xyz = mix(mix(mcol.xyz, mcolb, smult2), mix(mcol.xyz, mcolc, 1.0 - smult2), smult1);
    mcol.w   = mix(mix(mat.specularPower.x, mat.specularPower.y, smult2), mix(mat.specularPower.x, mat.specularPower.z, 1.0 - smult2), smult1);
    nn = normalize(mix(ip.n, grad2, mixnorm));
    //nn = ip.n;
    return mat;
}
