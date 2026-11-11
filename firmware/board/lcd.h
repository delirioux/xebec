/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - display driver (ST7789V, 240 x 240, RGB565).
 */
#ifndef XB_LCD_DRIVER_H
#define XB_LCD_DRIVER_H

#include <stdint.h>

#define XB_LCD_W 240u
#define XB_LCD_H 240u

/* Pack 8-bit R, G, B into RGB565. */
#define XB_RGB(r, g, b) \
    ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

/* RGB565 swapped into the byte order the panel takes (high byte first). */
#define XB_LCD_WIRE(rgb565) ((uint16_t)(((rgb565) >> 8) | ((rgb565) << 8)))

void xb_lcd_init(void);
void xb_lcd_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t rgb565);

/* Copy a w x h block of pixels, already in wire order (XB_LCD_WIRE), to
 * the screen at (x, y). The block must lie inside the screen and stay in
 * RAM until the call returns. */
void xb_lcd_blit(unsigned x, unsigned y, unsigned w, unsigned h, const uint16_t *wire);

#endif
