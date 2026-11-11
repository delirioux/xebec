/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - firmware for the M-VAVE FM-1, built from scratch.
 *
 * Milestone 1, the bare boot: bring up the display and paint it one
 * colour. Nothing else runs yet.
 */
#include "../board/lcd.h"
#include "version.h"

/* Xebec's sea teal. */
#define XB_COLOUR_SEA XB_RGB(0x00, 0x80, 0x80)

void xb_main(void)
{
    xb_lcd_init();
    xb_lcd_fill(0, 0, XB_LCD_W, XB_LCD_H, XB_COLOUR_SEA);

    for (;;)
        ;
}
