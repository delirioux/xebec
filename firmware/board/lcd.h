/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - display driver (ST7789V, 240 x 240, RGB565).
 */
#ifndef XB_LCD_H
#define XB_LCD_H

#include <stdint.h>

#define XB_LCD_W 240u
#define XB_LCD_H 240u

/* Pack 8-bit R, G, B into RGB565. */
#define XB_RGB(r, g, b) \
    ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

void xb_lcd_init(void);
void xb_lcd_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t rgb565);

#endif
