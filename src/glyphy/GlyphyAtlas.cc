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

#include "GlyphyAtlas.h"
#include <iostream>


GlyphyAtlas::GlyphyAtlas(unsigned int w, unsigned int h, unsigned int item_w, unsigned int item_h_quantum)
{
  TRACE();

  glGetIntegerv (GL_ACTIVE_TEXTURE, (GLint *) &tex_unit);
  glGenTextures (1, &tex_name);
  tex_w = w;
  tex_h = h;
  this->item_w = item_w;
  item_h_q = item_h_quantum;
  cursor_x = 0;
  cursor_y = 0;

  bind_texture();

  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  gl(TexImage2D) (GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}

GlyphyAtlas::~GlyphyAtlas() {
  glDeleteTextures (1, &tex_name);
}

void GlyphyAtlas::bind_texture ()
{
  glActiveTexture (tex_unit);
  glBindTexture (GL_TEXTURE_2D, tex_name);
}

void GlyphyAtlas::set_uniforms()
{
  GLuint program;
  glGetIntegerv (GL_CURRENT_PROGRAM, (GLint *) &program);

  glUniform4i (glGetUniformLocation (program, "u_atlas_info"),
	       tex_w, tex_h, item_w, item_h_q);
  glUniform1i (glGetUniformLocation (program, "u_atlas_tex"), tex_unit - GL_TEXTURE0);
}

void GlyphyAtlas::alloc (glyphy_rgba_t *data,
		  unsigned int   len,
		  unsigned int  *px,
		  unsigned int  *py)
{
  GLuint w = 0, h = 0, x = 0, y = 0;

  w = item_w;
  h = (len + w - 1) / w;

  if (cursor_y + h > tex_h) {
    /* Go to next column */
    cursor_x += item_w;
    cursor_y = 0;
  }

  if (cursor_x + w <= tex_w &&
      cursor_y + h <= tex_h)
  {
    x = cursor_x;
    y = cursor_y;
    cursor_y += (h + item_h_q - 1) & ~(item_h_q - 1);
  } else
    die ("Ran out of atlas memory");

  bind_texture();

  if (w * h == len)
    gl(TexSubImage2D) (GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
  else {
	  gl(TexSubImage2D) (GL_TEXTURE_2D, 0, x, y, w, h - 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
    // Upload the last row separately
    gl(TexSubImage2D) (GL_TEXTURE_2D, 0, x, y + h - 1, len - (w * (h - 1)), 1, GL_RGBA, GL_UNSIGNED_BYTE,
		       data + w * (h - 1));
  }

  *px = x / item_w;
  *py = y / item_h_q;
}
