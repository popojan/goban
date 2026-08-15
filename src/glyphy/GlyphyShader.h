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
 * Google Author(s): Behdad Esfahbod
 */

#ifndef DEMO_SHADERS_H
#define DEMO_SHADERS_H

#include <glyphy.h>
#include "GlyphyFont.h"


struct glyph_vertex_t {
  /* Position */
  GLfloat x;
  GLfloat y;
  /* Glyph info */
  GLfloat g16hi;
  GLfloat g16lo;
  /* Colour, per glyph rather than per draw call.
   *
   * Upstream glyphy carries colour in the `u_color` uniform, which makes a
   * buffer single-coloured and forces one buffer and one draw call per colour.
   * That is why GobanOverlay has a light layer and a dark layer at the same
   * height: they are one layer, split only so that text on a black stone and
   * text on a white stone can differ. Colour is orthogonal to everything the
   * SDF machinery does — it computes an alpha and multiplies — so it belongs on
   * the vertex. `u_color` survives as a global tint. */
  GLfloat r;
  GLfloat g;
  GLfloat b;
  GLfloat a;
};

/// `color` is four floats, RGBA, written into all four corners of the glyph.
void
demo_shader_add_glyph_vertices (const glyphy_point_t        &p,
				double                       font_size,
				glyph_info_t                *gi,
				std::vector<glyph_vertex_t> *vertices,
				glyphy_extents_t            *extents,
				bool extents_only,
				const GLfloat               *color);


GLuint
demo_shader_create_program ();


#endif /* DEMO_SHADERS_H */
