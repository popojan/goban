uniform mat4 u_matViewProjection;

attribute vec4 a_glyph_vertex;
// Per-glyph colour. Flat across the quad, so it interpolates to a constant.
attribute vec4 a_glyph_color;

varying vec4 v_glyph;
varying vec4 v_color;

vec4
glyph_vertex_transcode (vec2 v)
{
  ivec2 g = ivec2 (v);
  ivec2 corner = ivec2 (mod (v, 2.));
  g /= 2;
  ivec2 nominal_size = ivec2 (mod (vec2(g), 64.));
  return vec4 (corner * nominal_size, g * 4);
}

void
main()
{
  gl_Position = u_matViewProjection * vec4 (a_glyph_vertex.xy, 0, 1);
  v_glyph = glyph_vertex_transcode (a_glyph_vertex.zw);
  v_color = a_glyph_color;
}
