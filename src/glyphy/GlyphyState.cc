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

#include <memory>
#include <glm/glm.hpp>
#include "GlyphyState.h"

GlyphyState::GlyphyState():
    program(demo_shader_create_program()),
    atlas(new GlyphyAtlas(2048, 1024, 64, 32)),
    u_debug(false),
    u_contrast(1.0),
    u_gamma_adjust(1.0),
    u_outline(false),
    u_outline_thickness(0.0),
    u_boldness(0.),
    u_depth(0.),
    u_color{.0f, .0f, .0f, 1.0f}
{
}


GlyphyState::~GlyphyState()
{
  glDeleteProgram (program);
}

void GlyphyState::set_uniform (GLuint program, const char *name, double *p, double value)
{
  *p = value;
  glUniform1f (glGetUniformLocation (program, name), value);
  spdlog::debug("Setting {} to {}", name + 2, value);
}

#define SET_UNIFORM(name, value) set_uniform (program, #name, &name, value)

void GlyphyState::setup ()
{
  glUseProgram (program);

  atlas->set_uniforms();

  SET_UNIFORM (u_debug, u_debug);
  SET_UNIFORM (u_contrast, u_contrast);
  SET_UNIFORM (u_gamma_adjust, u_gamma_adjust);
  SET_UNIFORM (u_outline, u_outline);
  SET_UNIFORM (u_outline_thickness, u_outline_thickness);
  SET_UNIFORM (u_boldness, u_boldness);
  glUniform4fv(glGetUniformLocation(program, "u_color"), 1, u_color);
  SET_UNIFORM (u_depth, u_depth);

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GlyphyState::fast_setup()
{
	glUseProgram(program);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUniform4fv(glGetUniformLocation(program, "u_color"), 1, u_color);
	SET_UNIFORM (u_depth, u_depth);
}

void GlyphyState::scale_gamma_adjust (double factor)
{
  SET_UNIFORM (u_gamma_adjust, glm::clamp(u_gamma_adjust * factor, .1, 10.));
}

void GlyphyState::scale_contrast (double factor)
{
  SET_UNIFORM (u_contrast, glm::clamp (u_contrast * factor, .1, 10.));
}

void GlyphyState::toggle_debug () {
  SET_UNIFORM (u_debug, 1 - u_debug);
}

void GlyphyState::set_matrix (float mat[16])
{
  glUniformMatrix4fv (glGetUniformLocation (program, "u_matViewProjection"), 1, GL_FALSE, mat);
}

void GlyphyState::toggle_outline ()
{
  SET_UNIFORM (u_outline, 1 - u_outline);
}

void GlyphyState::scale_outline_thickness (double factor)
{
  SET_UNIFORM (u_outline_thickness, glm::clamp (u_outline_thickness * factor, .5, 3.));
}

void GlyphyState::adjust_boldness (double adjustment)
{
  SET_UNIFORM (u_boldness, glm::clamp (u_boldness + adjustment, -.2, .7));
}


void GlyphyState::set_color(float * color) {
	u_color[0] = color[0];
	u_color[1] = color[1];
	u_color[2] = color[2];
	u_color[3] = color[3];
}

void GlyphyState::set_depth(float depth) {
	u_depth = depth;
}
