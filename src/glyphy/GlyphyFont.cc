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

#ifndef M_PI
	#define M_PI 3.14159265358979323846
	#define M_SQRT2 1.41421356237309504880
#endif

#define MIN_FONT_SIZE 16
#define TOLERANCE (1./2048)

#include "GlyphyFont.h"

#include <glyphy-freetype.h>

#include <vector>

GlyphyFont::GlyphyFont(FT_Face face, std::shared_ptr<GlyphyAtlas> atlas):
    face(face),
    atlas(atlas),
    num_glyphs(0),
    sum_error(0.),
    sum_endpoints(0),
    sum_fetch(0.),
    sum_bytes(0)
{
  glyph_cache = new glyph_cache_t ();
  acc = glyphy_arc_accumulator_create ();
}

GlyphyFont::~GlyphyFont()
{
  glyphy_arc_accumulator_destroy (acc);
  delete glyph_cache;
}


FT_Face GlyphyFont::get_face ()
{
  return face;
}

std::shared_ptr<GlyphyAtlas>
GlyphyFont::get_atlas ()
{
  return atlas;
}

static glyphy_bool_t
accumulate_endpoint (glyphy_arc_endpoint_t  *endpoint,
		     std::vector<glyphy_arc_endpoint_t> *endpoints)
{
  endpoints->push_back (*endpoint);
  return true;
}

void
GlyphyFont::encode_ft_glyph (
		 unsigned int      glyph_index,
		 double            tolerance_per_em,
		 glyphy_rgba_t    *buffer,
		 unsigned int      buffer_len,
		 unsigned int     *output_len,
		 unsigned int     *nominal_width,
		 unsigned int     *nominal_height,
		 glyphy_extents_t *extents,
		 double           *advance)
{
/* Used for testing only */
#define SCALE  (1. * (1 << 0))

  if (FT_Err_Ok != FT_Load_Glyph (face,
				  glyph_index,
				  FT_LOAD_NO_BITMAP |
				  FT_LOAD_NO_HINTING |
				  FT_LOAD_NO_AUTOHINT |
				  FT_LOAD_NO_SCALE |
				  FT_LOAD_LINEAR_DESIGN |
				  FT_LOAD_IGNORE_TRANSFORM))
    spdlog::error("Failed loading FreeType glyph");

  if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
    spdlog::error("FreeType loaded glyph format is not outline");

  unsigned int upem = face->units_per_EM;
  double tolerance = upem * tolerance_per_em; /* in font design units */
  double faraway = double (upem) / (MIN_FONT_SIZE * M_SQRT2);
  std::vector<glyphy_arc_endpoint_t> endpoints;

  glyphy_arc_accumulator_reset (acc);
  glyphy_arc_accumulator_set_tolerance (acc, tolerance);
  glyphy_arc_accumulator_set_callback (acc,
				       (glyphy_arc_endpoint_accumulator_callback_t) accumulate_endpoint,
				       &endpoints);

  if (FT_Err_Ok != glyphy_freetype(outline_decompose) (&face->glyph->outline, acc))
    spdlog::error("Failed converting glyph outline to arcs");

  assert (glyphy_arc_accumulator_get_error (acc) <= tolerance);

  if (endpoints.size ())
  {
#if 0
    /* Technically speaking, we want the following code,
     * however, crappy fonts have crappy flags.  So we just
     * fixup unconditionally... */
    if (face->glyph->outline.flags & FT_OUTLINE_EVEN_ODD_FILL)
      glyphy_outline_winding_from_even_odd (&endpoints[0], endpoints.size (), false);
    else if (face->glyph->outline.flags & FT_OUTLINE_REVERSE_FILL)
      glyphy_outline_reverse (&endpoints[0], endpoints.size ());
#else
    glyphy_outline_winding_from_even_odd (&endpoints[0], endpoints.size (), false);
#endif
  }

  if (SCALE != 1.)
    for (unsigned int i = 0; i < endpoints.size (); i++)
    {
      endpoints[i].p.x /= SCALE;
      endpoints[i].p.y /= SCALE;
    }

  double avg_fetch_achieved;
  if (!glyphy_arc_list_encode_blob (endpoints.size () ? &endpoints[0] : NULL, endpoints.size (),
				    buffer,
				    buffer_len,
				    faraway / SCALE,
				    4, /* UNUSED */
				    &avg_fetch_achieved,
				    output_len,
				    nominal_width,
				    nominal_height,
				    extents))
    spdlog::error("Failed encoding arcs");

  glyphy_extents_scale (extents, 1. / upem, 1. / upem);
  glyphy_extents_scale (extents, SCALE, SCALE);

  *advance = face->glyph->metrics.horiAdvance / (double) upem;

  spdlog::debug("gid{0:3d}: endpoints{1:3d}; err{2:3g}; tex fetch{3:4.1f}; mem{4:4.1f}kb",
	  glyph_index,
	  (unsigned int) glyphy_arc_accumulator_get_num_endpoints (acc),
	  round (100 * glyphy_arc_accumulator_get_error (acc) / tolerance),
	  avg_fetch_achieved,
	  (*output_len * sizeof (glyphy_rgba_t)) / 1024.);

  num_glyphs++;
  sum_error += glyphy_arc_accumulator_get_error (acc) / tolerance;
  sum_endpoints += glyphy_arc_accumulator_get_num_endpoints (acc);
  sum_fetch += avg_fetch_achieved;
  sum_bytes += (*output_len * sizeof (glyphy_rgba_t));
}

void GlyphyFont::upload_glyph (
			 unsigned int glyph_index,
			 glyph_info_t *glyph_info)
{
  const int BUF_SIZE = 4096 * 16;
  glyphy_rgba_t buffer[BUF_SIZE];
  unsigned int output_len;

  encode_ft_glyph (
		   glyph_index,
		   TOLERANCE,
		   buffer, BUF_SIZE,
		   &output_len,
		   &glyph_info->nominal_w,
		   &glyph_info->nominal_h,
		   &glyph_info->extents,
		   &glyph_info->advance);

  glyph_info->is_empty = glyphy_extents_is_empty (&glyph_info->extents);
  if (!glyph_info->is_empty)
    atlas->alloc(buffer, output_len,
		      &glyph_info->atlas_x, &glyph_info->atlas_y);
}

void GlyphyFont::lookup_glyph (
			unsigned int  glyph_index,
			glyph_info_t *glyph_info)
{
  if (glyph_cache->find (glyph_index) == glyph_cache->end ()) {
    upload_glyph (glyph_index, glyph_info);
    (*glyph_cache)[glyph_index] = *glyph_info;
  } else
    *glyph_info = (*glyph_cache)[glyph_index];
}

void
GlyphyFont::print_stats ()
{
  spdlog::debug("{0:3d} glyphs; avg num endpoints {1:6.2f}; avg error {2:5.1f};"
                 "avg tex fetch {3:5.2f}; avg {4:5.2f}kb per glyph",
	num_glyphs,
	(double) sum_endpoints / num_glyphs,
	100. * sum_error / num_glyphs,
	sum_fetch / num_glyphs,
	sum_bytes / 1024. / num_glyphs);
}
