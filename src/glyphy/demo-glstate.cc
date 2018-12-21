/*
 * Copyright 2012 Google, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Google Author(s): Behdad Esfahbod, Maysum Panju, Wojciech Baranowski
 */

#include "demo-glstate.h"
#include <memory>

struct demo_glstate_t {
  unsigned int   refcount;

  GLuint program;
  std::shared_ptr<GlyphyAtlas> atlas;

  /* Uniforms */
  double u_debug;
  double u_contrast;
  double u_gamma_adjust;
  double u_outline;
  double u_outline_thickness;
  double u_boldness;
  float u_color[4];
  double u_depth;
};

demo_glstate_t *
demo_glstate_create (void)
{
  TRACE();

  demo_glstate_t *st = reinterpret_cast<demo_glstate_t *>(calloc (1, sizeof (demo_glstate_t)));

  st->refcount = 1;

  st->program = demo_shader_create_program ();
  st->atlas = std::shared_ptr<GlyphyAtlas>(new GlyphyAtlas(2048, 1024, 64, 32));

  st->u_debug = false;
  st->u_contrast = 1.0;
  st->u_gamma_adjust = 1.0;
  st->u_outline = false;
  st->u_outline_thickness = 0.0;
  st->u_boldness = 0.;
  st->u_depth = 0.;
  st->u_color[0] = 0.0f;
  st->u_color[1] = 0.0f;
  st->u_color[2] = 0.0f;
  st->u_color[3] = 1.0f;

  return st;
}

demo_glstate_t *
demo_glstate_reference (demo_glstate_t *st)
{
  if (st) st->refcount++;
  return st;
}

void
demo_glstate_destroy (demo_glstate_t *st)
{
  if (!st || --st->refcount)
    return;

  glDeleteProgram (st->program);

  free (st);
}


static void
set_uniform (GLuint program, const char *name, double *p, double value)
{
  *p = value;
  glUniform1f (glGetUniformLocation (program, name), value);
  LOGI ("Setting %s to %g\n", name + 2, value);
}

#define SET_UNIFORM(name, value) set_uniform (st->program, #name, &st->name, value)

void
demo_glstate_setup (demo_glstate_t *st)
{
  glUseProgram (st->program);

  st->atlas->set_uniforms();

  SET_UNIFORM (u_debug, st->u_debug);
  SET_UNIFORM (u_contrast, st->u_contrast);
  SET_UNIFORM (u_gamma_adjust, st->u_gamma_adjust);
  SET_UNIFORM (u_outline, st->u_outline);
  SET_UNIFORM (u_outline_thickness, st->u_outline_thickness);
  SET_UNIFORM (u_boldness, st->u_boldness);
  glUniform4fv(glGetUniformLocation(st->program, "u_color"), 1, st->u_color);
  SET_UNIFORM (u_depth, st->u_depth);

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void
demo_glstate_fast_setup(demo_glstate_t *st)
{
	glUseProgram(st->program);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUniform4fv(glGetUniformLocation(st->program, "u_color"), 1, st->u_color);
	SET_UNIFORM (u_depth, st->u_depth);
}

std::shared_ptr<GlyphyAtlas>
demo_glstate_get_atlas (demo_glstate_t *st)
{
  return st->atlas;
}

void
demo_glstate_scale_gamma_adjust (demo_glstate_t *st, double factor)
{
  SET_UNIFORM (u_gamma_adjust, clamp (st->u_gamma_adjust * factor, .1, 10.));
}

void
demo_glstate_scale_contrast (demo_glstate_t *st, double factor)
{
  SET_UNIFORM (u_contrast, clamp (st->u_contrast * factor, .1, 10.));
}

void
demo_glstate_toggle_debug (demo_glstate_t *st)
{
  SET_UNIFORM (u_debug, 1 - st->u_debug);
}

void
demo_glstate_set_matrix (demo_glstate_t *st, float mat[16])
{
  glUniformMatrix4fv (glGetUniformLocation (st->program, "u_matViewProjection"), 1, GL_FALSE, mat);
}

void
demo_glstate_toggle_outline (demo_glstate_t *st)
{
  SET_UNIFORM (u_outline, 1 - st->u_outline);
}

void
demo_glstate_scale_outline_thickness (demo_glstate_t *st, double factor)
{
  SET_UNIFORM (u_outline_thickness, clamp (st->u_outline_thickness * factor, .5, 3.));
}

void
demo_glstate_adjust_boldness (demo_glstate_t *st, double adjustment)
{
  SET_UNIFORM (u_boldness, clamp (st->u_boldness + adjustment, -.2, .7));
}

void
demo_glstate_set_color(demo_glstate_t *st, float * color) {
	st->u_color[0] = color[0];
	st->u_color[1] = color[1];
	st->u_color[2] = color[2];
	st->u_color[3] = color[3];
}

void
demo_glstate_set_depth(demo_glstate_t *st, float depth) {
	st->u_depth = depth;
}
