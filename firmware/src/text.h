/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 *
 * Xebec - text on the display, in the 5 x 7 font scaled up.
 */
#ifndef XB_TEXT_H
#define XB_TEXT_H

#include <stdint.h>

#define XB_TEXT_MAX_SCALE 4u

/* Width in pixels of `str` at `scale`: one pixel of space after each glyph. */
unsigned xb_text_width(const char *str, unsigned scale);

/* Draw `str` with its top-left corner at (x, y), each font pixel a
 * scale x scale square (1..XB_TEXT_MAX_SCALE), in `fg` over `bg`. Text
 * running past the right edge is cut off. */
void xb_text_draw(unsigned x, unsigned y, const char *str, unsigned scale,
                  uint16_t fg, uint16_t bg);

#endif
