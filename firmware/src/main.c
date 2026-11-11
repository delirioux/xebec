/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 *
 * Xebec - firmware for the M-VAVE FM-1, built from scratch.
 *
 * The boot test: bring up the display, show who we are, then run the
 * boot checks one by one, each on its own line. If Xebec hangs, the last
 * line on screen shows where.
 */
#include "../board/lcd.h"
#include "text.h"
#include "ramtest.h"
#include "version.h"

#ifndef XB_BUILD
#define XB_BUILD "LOCAL"
#endif

/* Colours. The boot screen check in CI (tools/check_boot_screen.py)
 * knows these and the layout below. */
#define XB_COLOUR_SEA   XB_RGB(0x00, 0x80, 0x80)   /* background */
#define XB_COLOUR_TEXT  XB_RGB(0xFF, 0xFF, 0xFF)
#define XB_COLOUR_RULE  XB_RGB(0x80, 0xC0, 0xC0)
#define XB_COLOUR_OK    XB_RGB(0x40, 0xFF, 0x40)
#define XB_COLOUR_FAIL  XB_RGB(0xFF, 0x40, 0x40)

/* Layout: title, version line, a rule, then the check lines. */
#define TITLE_Y      20u
#define VERSION_Y    64u
#define RULE_Y       90u
#define CHECK_Y0    104u
#define CHECK_STEP   24u
#define LABEL_X      12u
#define RESULT_X    204u

static unsigned check_line;

static void check(const char *label, int ok)
{
    unsigned y = CHECK_Y0 + check_line * CHECK_STEP;

    xb_text_draw(LABEL_X, y, label, 2, XB_COLOUR_TEXT, XB_COLOUR_SEA);
    if (ok)
        xb_text_draw(RESULT_X, y, "OK", 2, XB_COLOUR_OK, XB_COLOUR_SEA);
    else
        xb_text_draw(RESULT_X - 24u, y, "FAIL", 2, XB_COLOUR_FAIL, XB_COLOUR_SEA);
    check_line++;
}

void xb_main(void)
{
    unsigned w;

    xb_lcd_init();
    xb_lcd_fill(0, 0, XB_LCD_W, XB_LCD_H, XB_COLOUR_SEA);

    w = xb_text_width("XEBEC", 4);
    xb_text_draw((XB_LCD_W - w) / 2u, TITLE_Y, "XEBEC", 4, XB_COLOUR_TEXT, XB_COLOUR_SEA);

    w = xb_text_width("V" XB_VERSION "  " XB_BUILD, 2);
    xb_text_draw((XB_LCD_W - w) / 2u, VERSION_Y, "V" XB_VERSION "  " XB_BUILD, 2,
                 XB_COLOUR_TEXT, XB_COLOUR_SEA);

    xb_lcd_fill(LABEL_X, RULE_Y, XB_LCD_W - 2u * LABEL_X, 2, XB_COLOUR_RULE);

    /* Reaching this line means start-up, the C runtime and the display work. */
    check("DISPLAY", 1);
    check("RAM 64K", xb_ramtest());
    check("CORE 0", 1);

    w = xb_text_width("READY", 2);
    xb_text_draw((XB_LCD_W - w) / 2u, 208u, "READY", 2, XB_COLOUR_TEXT, XB_COLOUR_SEA);

    for (;;)
        ;
}
