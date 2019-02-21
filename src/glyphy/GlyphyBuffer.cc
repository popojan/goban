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

#include "GlyphyBuffer.h"
#include <memory>

#undef max

GlyphyBuffer::GlyphyBuffer() {
    vertices = new std::vector<glyph_vertex_t>;
    glGenBuffers(1, &buf_name);

    clear();
    console = spdlog::get("console");
}

GlyphyBuffer::~GlyphyBuffer()
{
    glDeleteBuffers(1, &buf_name);
    delete vertices;
}


void GlyphyBuffer::clear()
{
    vertices->clear ();
    glyphy_extents_clear(&ink_extents);
    glyphy_extents_clear(&logical_extents);
    dirty = true;
}

void GlyphyBuffer::extents(
    glyphy_extents_t *ink_extents,
    glyphy_extents_t *logical_extents)
{
    if (ink_extents)
        *ink_extents = this->ink_extents;
    if (logical_extents)
        *logical_extents = this->logical_extents;
}

void GlyphyBuffer::move_to (
    const glyphy_point_t *p)
{
    cursor = *p;
}

void GlyphyBuffer::current_point (
    glyphy_point_t *p)
{
    *p = cursor;
}

void GlyphyBuffer::add_text (
    const char *utf8,
    std::shared_ptr<GlyphyFont> font,
    double font_size)
{
    FT_Face face = font->get_face ();
    glyphy_point_t top_left = cursor;
    //buffer->cursor.y += font_size /* * font->ascent */;

    glyphy_point_t origin(cursor);

    unsigned int unicode;

    double max_y = 0.0;
    for (int extents_only = 1; extents_only >= 0; --extents_only) {
        for (const unsigned char *p = (const unsigned char *)utf8; *p; p++) {
            if (*p < 128) {
                unicode = *p;
            }
            else {
                unsigned int j;
                if (*p < 0xE0) {
                    unicode = *p & ~0xE0;
                    j = 1;
                }
                else if (*p < 0xF0) {
                    unicode = *p & ~0xF0;
                    j = 2;
                }
                else {
                    unicode = *p & ~0xF8;
                    j = 3;
                    continue;
                }
                p++;
                for (; j && *p; j--, p++)
                    unicode = (unicode << 6) | (*p & ~0xC0);
                p--;
            }

            if (unicode == '\n') {
                cursor.y += font_size;
                cursor.x = top_left.x;
                continue;
            }
            unsigned int glyph_index = FT_Get_Char_Index(face, unicode);
            glyph_info_t gi;
            font->lookup_glyph(glyph_index, &gi);

            /* Update ink extents */
            glyphy_extents_t ink_extents;

            demo_shader_add_glyph_vertices(cursor, font_size, &gi, vertices, &ink_extents, extents_only);
            max_y = std::max(max_y, font_size*(gi.extents.max_y - gi.extents.min_y));
            glyphy_extents_extend(&ink_extents, &ink_extents);
            glyphy_point_t corner;
            corner.x = -1;// buffer->cursor.x;
            corner.y = -1;// buffer->cursor.y - 4.0 / 3.0*font_size;
            glyphy_extents_add(&logical_extents, &corner);
            corner.x = 1;// buffer->cursor.x + font_size * gi.advance;
            corner.y = 1;// buffer->cursor.y + 2.0 / 3.0*font_size;
            glyphy_extents_add(&logical_extents, &corner);
            cursor.x += font_size * gi.advance;
        }
        cursor.x = origin.x + (origin.x - cursor.x) / 2;
        cursor.y = origin.y + (origin.y - cursor.y + max_y) / 2;
    }
    dirty = true;
}

void GlyphyBuffer::draw ()
{
    GLint program;
    glGetIntegerv (GL_CURRENT_PROGRAM, &program);
    GLuint a_glyph_vertex_loc = glGetAttribLocation (program, "a_glyph_vertex");
    glBindBuffer (GL_ARRAY_BUFFER, buf_name);
    if (dirty) {
        glBufferData (GL_ARRAY_BUFFER,  sizeof (glyph_vertex_t) * vertices->size (),
                      (const char *) &(*vertices)[0], GL_STATIC_DRAW);
        dirty = false;
    }
    glEnableVertexAttribArray (a_glyph_vertex_loc);
    glVertexAttribPointer (a_glyph_vertex_loc, 4, GL_FLOAT, GL_FALSE, sizeof (glyph_vertex_t), 0);
    glDrawArrays (GL_TRIANGLES, 0, vertices->size ());
    glDisableVertexAttribArray (a_glyph_vertex_loc);
}
