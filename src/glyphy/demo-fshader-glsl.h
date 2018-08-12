static const char* demo_fshader_glsl = 
"uniform float u_contrast;\n"
"uniform vec4  u_color;\n"
"uniform float u_depth;\n"
"uniform float u_gamma_adjust;\n"
"uniform float u_outline_thickness;\n"
"uniform bool  u_outline;\n"
"uniform float u_boldness;\n"
"uniform bool  u_debug;\n"
"\n"
"varying vec4 v_glyph;\n"
"\n"
"\n"
"#define SQRT2_2 0.70710678118654757 /* 1 / sqrt(2.) */\n"
"#define SQRT2   1.4142135623730951\n"
"\n"
"struct glyph_info_t {\n"
"  ivec2 nominal_size;\n"
"  ivec2 atlas_pos;\n"
"};\n"
"\n"
"glyph_info_t\n"
"glyph_info_decode (vec4 v)\n"
"{\n"
"  glyph_info_t gi;\n"
"  gi.nominal_size = (ivec2 (mod (v.zw, 256.)) + 2) / 4;\n"
"  gi.atlas_pos = ivec2 (v_glyph.zw) / 256;\n"
"  return gi;\n"
"}\n"
"\n"
"\n"
"float\n"
"antialias (float d)\n"
"{\n"
"  return smoothstep (-.75, +.75, d);\n"
"}\n"
"\n"
"void\n"
"main()\n"
"{\n"
"  vec2 p = v_glyph.xy;\n"
"  glyph_info_t gi = glyph_info_decode (v_glyph);\n"
"\n"
"  /* isotropic antialiasing */\n"
"  vec2 dpdx = dFdx (p);\n"
"  vec2 dpdy = dFdy (p);\n"
"  float m = length (vec2 (length (dpdx), length (dpdy))) * SQRT2_2;\n"
"\n"
"  vec4 color = u_color;\n"
"\n"
"  float gsdist = glyphy_sdf (p, gi.nominal_size GLYPHY_DEMO_EXTRA_ARGS);\n"
"  float sdist = gsdist / m * u_contrast;\n"
"\n"
"  if (!u_debug) {\n"
"    sdist -= u_boldness * 10.;\n"
"    if (u_outline)\n"
"      sdist = abs (sdist) - u_outline_thickness * .5;\n"
"    if (sdist > 1.)\n"
"      discard;\n"
"    float alpha = antialias (-sdist);\n"
"    if (u_gamma_adjust != 1.)\n"
"      alpha = pow (alpha, 1./u_gamma_adjust);\n"
"    color = vec4 (color.rgb,color.a * alpha);\n"
"  } else {\n"
"    color = vec4 (0,0,0,0);\n"
"\n"
"    // Color the inside of the glyph a light red\n"
"    color += vec4 (.5,0,0,.5) * smoothstep (1., -1., sdist);\n"
"\n"
"    float udist = abs (sdist);\n"
"    float gudist = abs (gsdist);\n"
"    // Color the outline red\n"
"    color += vec4 (1,0,0,1) * smoothstep (2., 1., udist);\n"
"    // Color the distance field in green\n"
"    if (!glyphy_isinf (udist))\n"
"      color += vec4(0,.4,0,.4 - (abs(gsdist) / max(float(gi.nominal_size.x), float(gi.nominal_size.y))) * 4.);\n"
"\n"
"    float pdist = glyphy_point_dist (p, gi.nominal_size GLYPHY_DEMO_EXTRA_ARGS);\n"
"    // Color points green\n"
"    color = mix (vec4 (0,1,0,.5), color, smoothstep (.05, .06, pdist));\n"
"\n"
"    glyphy_arc_list_t arc_list = glyphy_arc_list (p, gi.nominal_size GLYPHY_DEMO_EXTRA_ARGS);\n"
"    // Color the number of endpoints per cell blue\n"
"    color += vec4 (0,0,1,.1) * float(arc_list.num_endpoints) * 32./255.;\n"
"  }\n"
"\n"
"  gl_FragColor = color;\n"
"  gl_FragDepth = u_depth;\n"
"}\n"
"\n"
;
