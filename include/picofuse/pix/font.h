/**
 * @file font.h
 * @brief Bitmap font types.
 * @defgroup Font Font
 * @ingroup Pixel
 *
 * A font is a single block of memory (typically flash-resident, produced by
 * an offline font-generation tool) laid out as a @ref pix_font_t header
 * immediately followed by @ref pix_font_t.glyph_count pointers to
 * @ref pix_glyph_t entries. Glyphs must be sorted ascending by
 * @ref pix_glyph_t.codepoint so @ref pix_font_glyph can binary search them.
 * There is no load/free step: a correctly-formatted block of memory can be
 * used directly as a `const pix_font_t *`.
 */
#pragma once
#include "types.h"
#include <stddef.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

/**
 * @brief A single glyph's bitmap and metrics within a pix_font_t.
 * @ingroup Font
 */
typedef struct {
  uint32_t codepoint;  ///< Unicode codepoint this glyph represents.
  int16_t advance;     ///< Horizontal distance to advance the cursor after
                        ///< drawing this glyph, in pixels.
  pix_point_t offset;  ///< Offset from the cursor origin to the bitmap's
                        ///< top-left corner, in pixels.
  pix_bitmap_t bitmap; ///< Glyph pixel data. @ref pix_bitmap_t.fmt is
                        ///< typically PIX_FMT_MONO for bitmap fonts, but is
                        ///< not required to be.
} pix_glyph_t;

/**
 * @brief A bitmap font: a header followed by an array of glyph pointers.
 * @ingroup Font
 * @details See the file-level documentation for the memory layout this
 * type expects.
 */
typedef struct {
  uint16_t line_height; ///< Recommended vertical distance between
                         ///< successive baselines, in pixels.
  int16_t ascent;  ///< Distance from the baseline to the top of most
                    ///< glyphs, in pixels.
  int16_t descent; ///< Distance from the baseline to the bottom of most
                    ///< glyphs (positive means below the baseline), in
                    ///< pixels.
  uint32_t glyph_count; ///< Number of entries in @ref glyphs.
  const pix_glyph_t *glyphs[]; ///< Pointers to @ref glyph_count glyphs,
                                ///< sorted ascending by codepoint.
} pix_font_t;

///////////////////////////////////////////////////////////////////////////////
// METHODS

/**
 * @brief Look up the glyph for a codepoint.
 * @ingroup Font
 * @param font Font to search.
 * @param codepoint Unicode codepoint to find.
 * @return Pointer to the matching glyph, or NULL if @ref font is NULL or
 * has no glyph for @ref codepoint.
 */
static inline const pix_glyph_t *pix_font_glyph(const pix_font_t *font,
                                                 uint32_t codepoint) {
  if (font == NULL) {
    return NULL;
  }

  uint32_t lo = 0u;
  uint32_t hi = font->glyph_count;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2u;
    const pix_glyph_t *glyph = font->glyphs[mid];
    if (glyph->codepoint == codepoint) {
      return glyph;
    }
    if (glyph->codepoint < codepoint) {
      lo = mid + 1u;
    } else {
      hi = mid;
    }
  }

  return NULL;
}
