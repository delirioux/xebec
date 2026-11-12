/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 *
 * Xebec - firmware for the M-VAVE FM-1, built from scratch.
 *
 * The boot test: bring up the display, show who we are, then run the
 * boot checks one by one, each on its own line. If Xebec hangs, the last
 * line on screen shows where.
 *
 * OCT- and OCT+ held at power-on: update mode instead. Xebec stages
 * M-VAVE's OTA loader and restarts into it (update.h).
 */
#include "../board/lcd.h"
#include "../board/flash.h"
#include "../board/keys.h"
#include "../board/clock.h"
#include "../board/usb.h"
#include "sysex.h"
#include "update.h"
#include "delay.h"
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
#define CHECK_Y0    100u
#define CHECK_STEP   20u
#define READY_Y     222u
#define LABEL_X      12u
#define RESULT_X    204u

static unsigned check_line;

/* Read-only: the chip answers through the RAM driver with a known JEDEC ID. */
static int flash_check(void)
{
    uint32_t id;

    return xb_flash_id(&id) == XB_FLASH_OK && XB_FLASH_KNOWN(id);
}

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

/* Diagnostics at the top: USB transfers in, identity questions, transfers out. */
#define DIAG_Y 6u            /* above the title, clear of every check line */

static void num_text(char *out, uint32_t v)
{
    char tmp[10];
    unsigned n = 0, i = 0;

    do {
        uint32_t q = 0;

        while (v >= 10u) {           /* v / 10 without a divide, fine for small counts */
            v -= 10u;
            q++;
        }
        tmp[n++] = (char)('0' + v);
        v = q;
    } while (v && n < sizeof tmp);
    while (n)
        out[i++] = tmp[--n];
    out[i] = 0;
}

static void diag_draw(void)
{
    char line[40], num[12];
    const char *parts[3] = { "RX ", "  ID ", "  TX " };
    uint32_t vals[3];
    unsigned i, k, n = 0;

    vals[0] = xb_usb_rx_count;
    vals[1] = xb_sysex_identities;
    vals[2] = xb_usb_tx_count;
    for (i = 0; i < 3u; i++) {
        for (k = 0; parts[i][k]; k++)
            line[n++] = parts[i][k];
        num_text(num, vals[i]);
        for (k = 0; num[k]; k++)
            line[n++] = num[k];
    }
    line[n] = 0;
    xb_lcd_fill(LABEL_X, DIAG_Y, XB_LCD_W - 2u * LABEL_X, 8u, XB_COLOUR_SEA);
    xb_text_draw(LABEL_X, DIAG_Y, line, 1, XB_COLOUR_TEXT, XB_COLOUR_SEA);
}

/* A line's value, right-aligned where OK ends, over whatever was there. */
static void line_value(unsigned line, const char *value, uint16_t colour)
{
    unsigned y = CHECK_Y0 + line * CHECK_STEP;
    unsigned right = RESULT_X + xb_text_width("OK", 2);

    xb_lcd_fill(RESULT_X - 60u, y, right - (RESULT_X - 60u), 14u, XB_COLOUR_SEA);
    xb_text_draw(right - xb_text_width(value, 2), y, value, 2, colour, XB_COLOUR_SEA);
}

/* A check line with a value instead of OK. */
static void check_value(const char *label, const char *value, int ok)
{
    xb_text_draw(LABEL_X, CHECK_Y0 + check_line * CHECK_STEP, label, 2, XB_COLOUR_TEXT, XB_COLOUR_SEA);
    if (ok)
        line_value(check_line, value, XB_COLOUR_OK);
    else
        line_value(check_line, "FAIL", XB_COLOUR_FAIL);
    check_line++;
}

/* "240MHZ": up to four digits, without a divide. */
static void mhz_text(char *out, uint32_t mhz)
{
    static const uint32_t pow10[] = { 1000u, 100u, 10u, 1u };
    unsigned i, n = 0;

    for (i = 0; i < 4u; i++) {
        char digit = '0';

        while (mhz >= pow10[i]) {
            mhz -= pow10[i];
            digit++;
        }
        if (n || digit != '0' || i == 3u)
            out[n++] = digit;
    }
    out[n++] = 'M';
    out[n++] = 'H';
    out[n++] = 'Z';
    out[n] = 0;
}

static void centred(unsigned y, const char *text)
{
    unsigned w = xb_text_width(text, 2);

    xb_text_draw((XB_LCD_W - w) / 2u, y, text, 2, XB_COLOUR_TEXT, XB_COLOUR_SEA);
}

/* OCT- + OCT+ held: stage the loader, write the record, restart into it.
 * On a failure nothing is left behind; the screen says so and Xebec stops
 * there (power off and on to start normally). */
static void update_mode(void)
{
    int rc;

    check("UPDATE KEYS", 1);
    rc = xb_update_prepare();
    check("LOADER", rc == XB_FLASH_OK);
    if (rc != XB_FLASH_OK) {
        centred(READY_Y, "UPDATE FAILED");
        for (;;)
            ;
    }
    centred(READY_Y, "RESTART");
    xb_delay_ms(300);
    xb_update_restart();
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

    xb_keys_init();
    if (xb_keys_update_held())
        update_mode();

    check("RAM 64K", xb_ramtest());
    {
        uint32_t mhz = xb_cpu_mhz();
        char text[8];

        mhz_text(text, mhz);
        check_value("CPU", text, mhz >= 24u && mhz <= 1000u);
    }
    {
        int flash = flash_check(), rc = 0;
        unsigned usb_line;
        int shown = 0;
        uint32_t tried, diag = 0xFFFFFFFFu;

        check("FLASH", flash);
        /* USB: "--" until the computer has configured the FM-1. */
        usb_line = check_line;
        check_value("USB", "--", 1);
        line_value(usb_line, "--", XB_COLOUR_TEXT);
        /* Only shown when an earlier update left something to clear. */
        if (flash)
            rc = xb_update_cleanup();
        if (rc)
            check("CLEANUP", rc > 0);

        centred(READY_Y, "READY");

        xb_usb_start();
        tried = xb_ticks();
        for (;;) {
            int now;

            xb_usb_poll();
            xb_sysex_service();
            now = xb_usb_configured();
            if (now != shown) {
                shown = now;
                line_value(usb_line, now ? "OK" : "--", now ? XB_COLOUR_OK : XB_COLOUR_TEXT);
            }
            if (now && diag != xb_usb_rx_count + 1000u * xb_sysex_identities + 1000000u * xb_usb_tx_count) {
                diag = xb_usb_rx_count + 1000u * xb_sysex_identities + 1000000u * xb_usb_tx_count;
                diag_draw();
            }
            if (!xb_usb_up() && xb_ticks() - tried > 24000000u) {   /* no cable: try once a second */
                xb_usb_start();
                tried = xb_ticks();
            }
        }
    }
}
