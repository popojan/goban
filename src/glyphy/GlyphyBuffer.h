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

#ifndef DEMO_BUFFER_H
#define DEMO_BUFFER_H

#include <glyphy.h>
#include <memory>
#include <spdlog/spdlog.h>

#include "GlyphyFont.h"
#include "GlyphyShader.h"


/// Where the point passed to `move_to()` sits relative to the text.
enum class TextAlign { Center, Left, Right };

class GlyphyBuffer {
public:
	GlyphyBuffer();
    GlyphyBuffer(const GlyphyBuffer& b);

	~GlyphyBuffer();


	void clear ();

	void extents(
			glyphy_extents_t *ink_extents,
			glyphy_extents_t *logical_extents);

	void move_to (const glyphy_point_t *p);

	void current_point (glyphy_point_t *p);

	/// `color` is four floats, RGBA, applied to every glyph in this call. Text
	/// added in separate calls may differ, so one buffer can hold many colours
	/// — which is the whole reason it is here rather than in `u_color`.
	void add_text (
			const char *utf8,
			std::shared_ptr<GlyphyFont> font,
			double font_size,
			const GLfloat *color,
			TextAlign align = TextAlign::Center
	);

	void draw ();

private:
	glyphy_point_t cursor;
	std::vector<glyph_vertex_t> *vertices;
	glyphy_extents_t ink_extents;
	glyphy_extents_t logical_extents;
	bool dirty;
	GLuint buf_name;
};

#endif /* DEMO_BUFFER_H */
