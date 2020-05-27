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

#ifndef DEMO_FONT_H
#define DEMO_FONT_H

#include <glyphy.h>
#include "GlyphyAtlas.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <memory>
#include <map>

#ifdef _WIN32
#define DEFAULT_FONT "Calibri"
#undef near
#undef far
#endif

typedef struct {
  glyphy_extents_t extents;
  double           advance;
  glyphy_bool_t    is_empty; /* has no outline; eg. space; don't draw it */
  unsigned int     nominal_w;
  unsigned int     nominal_h;
  unsigned int     atlas_x;
  unsigned int     atlas_y;
} glyph_info_t;

class GlyphyFont {

public:
	GlyphyFont(FT_Face face, std::shared_ptr<GlyphyAtlas> atlas);
    GlyphyFont(const GlyphyFont& b);

	~GlyphyFont();

	FT_Face get_face();

	std::shared_ptr<GlyphyAtlas> get_atlas();

	void lookup_glyph(unsigned int glyph_index, glyph_info_t *glyph_info);

	void upload_glyph(unsigned int glyph_index, glyph_info_t *glyph_info);

    void encode_ft_glyph(unsigned int      glyph_index,
                         double            tolerance_per_em,
                         glyphy_rgba_t    *buffer,
                         unsigned int      buffer_len,
                         unsigned int     *output_len,
                         unsigned int     *nominal_width,
                         unsigned int     *nominal_height,
                         glyphy_extents_t *extents,
                         double           *advance);

	void print_stats();

private:
	typedef std::map<unsigned int, glyph_info_t> glyph_cache_t;

	FT_Face face;
	glyph_cache_t *glyph_cache;
	std::shared_ptr<GlyphyAtlas> atlas;
	glyphy_arc_accumulator_t *acc;

	unsigned int num_glyphs;
	double       sum_error;
	unsigned int sum_endpoints;
	double       sum_fetch;
	unsigned int sum_bytes;
};

#endif /* DEMO_FONT_H */
