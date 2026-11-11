/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 *
 * Xebec - text on the display.
 *
 * A line of text is rendered into one RAM strip (screen width x glyph
 * height) and sent to the panel in one go.
 */
#include "text.h"
#include "font5x7.h"
#include "../board/lcd.h"

#define CELL_W (XB_FONT_W + 1u)   /* glyph plus one column of space */

static uint16_t strip[XB_LCD_W * XB_FONT_H * XB_TEXT_MAX_SCALE];

static const uint8_t *glyph(char c)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    if (c < XB_FONT_FIRST || c > XB_FONT_LAST)
        c = ' ';
    return xb_font5x7[c - XB_FONT_FIRST];
}

static unsigned text_len(const char *str)
{
    unsigned n = 0;

    while (str[n])
        n++;
    return n;
}

unsigned xb_text_width(const char *str, unsigned scale)
{
    unsigned n = text_len(str);

    return n ? (n * CELL_W - 1u) * scale : 0;
}

void xb_text_draw(unsigned x, unsigned y, const char *str, unsigned scale,
                  uint16_t fg, uint16_t bg)
{
    uint16_t wfg = XB_LCD_WIRE(fg), wbg = XB_LCD_WIRE(bg);
    unsigned w, h, px, py;

    if (scale < 1u)
        scale = 1u;
    if (scale > XB_TEXT_MAX_SCALE)
        scale = XB_TEXT_MAX_SCALE;
    if (x >= XB_LCD_W || y >= XB_LCD_H)
        return;

    w = xb_text_width(str, scale);
    if (w == 0)
        return;
    if (w > XB_LCD_W - x)
        w = XB_LCD_W - x;
    h = XB_FONT_H * scale;
    if (h > XB_LCD_H - y)
        h = XB_LCD_H - y;

    /* Walk font rows and columns with counters rather than dividing:
     * no division routines are linked into Xebec. */
    {
        unsigned row = 0, row_rep = 0;

        for (py = 0; py < h; py++) {
            unsigned cell = 0, col = 0, col_rep = 0;
            const uint8_t *g = glyph(str[0]);

            for (px = 0; px < w; px++) {
                uint16_t c = wbg;

                if (col < XB_FONT_W && (g[row] & (0x10u >> col)))
                    c = wfg;
                strip[py * w + px] = c;

                if (++col_rep == scale) {
                    col_rep = 0;
                    if (++col == CELL_W) {
                        col = 0;
                        g = glyph(str[++cell]);
                    }
                }
            }
            if (++row_rep == scale) {
                row_rep = 0;
                row++;
            }
        }
    }

    xb_lcd_blit(x, y, w, h, strip);
}
