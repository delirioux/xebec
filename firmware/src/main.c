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
#include "update_usb.h"
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
/* About 250 us at 360 MHz without touching any bus: the loop runs from the instruction cache. */
static void cpu_wait(void)
{
    uint32_t i;

    for (i = 0; i < 40000u; i++)
        __asm__ volatile("nop");
}
#define DIAG_Y 3u            /* above the title, clear of every check line */

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

static void diag_line(unsigned y, const char *const parts[3], const uint32_t vals[3])
{
    char line[40], num[12];
    unsigned i, k, n = 0;

    for (i = 0; i < 3u; i++) {
        for (k = 0; parts[i][k]; k++)
            line[n++] = parts[i][k];
        num_text(num, vals[i]);
        for (k = 0; num[k]; k++)
            line[n++] = num[k];
    }
    line[n] = 0;
    xb_lcd_fill(LABEL_X, y, XB_LCD_W - 2u * LABEL_X, 14u, XB_COLOUR_SEA);
    xb_text_draw(LABEL_X, y, line, 2, XB_COLOUR_TEXT, XB_COLOUR_SEA);
}

/* Top: transfers in, identity questions, transfers out. Bottom, over READY: SIE timeouts,
 * controller starts, bus resets from the host. */
static void diag_draw(void)
{
    static const char *const top[3] = { "RX ", " ID ", " TX " };
    static const char *const bottom[3] = { "TO ", " ST ", " RS " };
    uint32_t a[3], b[3];

    a[0] = xb_usb_rx_count;
    a[1] = xb_sysex_identities;
    a[2] = xb_usb_tx_count;
    b[0] = xb_usb_sie_timeouts;
    b[1] = xb_usb_starts;
    b[2] = xb_usb_bus_resets;
    diag_line(DIAG_Y, top, a);
    diag_line(READY_Y, bottom, b);
}

static uint32_t diag_sum(void)
{
    return xb_usb_rx_count + xb_sysex_identities + xb_usb_tx_count + xb_usb_sie_timeouts +
           xb_usb_starts + xb_usb_bus_resets;
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

static void usb_idle(void)
{
    cpu_wait();
    xb_usb_poll();
    xb_sysex_service();
}

/* The installer sent UPGRADE: check the package with it (update_usb.h), then
 * stage the loader and restart as update_mode() does. A refused package
 * leaves everything as it was; the screen says why and Xebec runs on. */
static void usb_update(void)
{
    char code[12];
    int rc;
    uint32_t i;

    xb_sysex_upgrade = 0;
    xb_lcd_fill(0, CHECK_Y0, XB_LCD_W, XB_LCD_H - CHECK_Y0, XB_COLOUR_SEA);
    check_line = 0;
    check("UPDATE USB", 1);
    rc = xb_update_usb_check(usb_idle);
    if (rc) {
        unsigned k;

        /* "1/43": the error, then how many read requests were made (where it stopped) */
        num_text(code, (uint32_t)-rc);
        for (k = 0; code[k]; k++)
            ;
        code[k++] = '/';
        num_text(code + k, xb_update_usb_reads);
        check("PACKAGE", 0);
        check_value("REFUSED", code, 1);
        line_value(check_line - 1u, code, XB_COLOUR_FAIL);
        return;
    }
    check("PACKAGE", 1);
    rc = xb_update_prepare();
    check("LOADER", rc == XB_FLASH_OK);
    if (rc != XB_FLASH_OK) {
        centred(READY_Y, "UPDATE FAILED");
        for (;;)
            usb_idle();
    }
    centred(READY_Y, "RESTART");
    for (i = 0; i < 1200u; i++)            /* about 300 ms, USB still served */
        usb_idle();
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
        int shown = 0, seen = 0, updated = 0;
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

            /* Between polls the CPU waits on itself, not on a peripheral register: spinning on
             * the SIE (0.0.7-0.0.9) or on TIMER4 (0.0.10) kept the peripheral bus busy, and the
             * host saw babble (EOVERFLOW) on Xebec's replies and reset the port. */
            usb_idle();
            if (xb_sysex_upgrade) {
                usb_update();                  /* returns only when the package was refused */
                updated = 1;
            }
            now = xb_usb_configured();
            if (now != shown && !updated) {
                shown = now;
                line_value(usb_line, now ? "OK" : "--", now ? XB_COLOUR_OK : XB_COLOUR_TEXT);
            }
            if (now)
                seen = 1;                      /* counters only once a host has configured us */
            if (seen && diag != diag_sum()) {  /* every counter only grows */
                diag = diag_sum();
                diag_draw();
            }
            if (!xb_usb_up() && xb_ticks() - tried > 24000000u) {   /* no cable: try once a second */
                xb_usb_start();
                tried = xb_ticks();
            }
        }
    }
}
