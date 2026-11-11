/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 *
 * Xebec - flash driver test image, for fm1-emu only (never for an FM-1:
 * it erases and writes the first sector of the write window).
 *
 * Built next to the firmware from the same driver code, with this file in
 * place of main.c. Runs every step once, records the outcome in
 * xb_flash_test and parks in xb_flash_test_park(). tools/check_flash_test.py
 * reads the record and the emulator's flash command log and its trace.
 */
#include "../board/flash.h"

#define STEPS 13u

struct xb_flash_test_result {
    uint32_t magic;        /* 'XBFT' when the test ran to its end */
    uint32_t done;
    uint32_t passed;       /* bit n-1 set: step n passed */
    uint32_t failed;       /* bit n-1 set: step n failed */
    uint32_t first_fail;   /* number of the first failed step, 0 if none */
    uint32_t ram_calls;    /* calls made into the driver's RAM code */
    uint32_t id;           /* JEDEC ID read */
    uint32_t safe_lo;      /* start of the write window */
};

struct xb_flash_test_result xb_flash_test;

static uint8_t pattern[512];
static uint8_t readback[512];

/* Constant data: lives in flash, so the driver must refuse it as a source. */
static const uint8_t in_flash[16] = {
    0x58, 0x45, 0x42, 0x45, 0x43, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
};

static void step(uint32_t n, int ok)
{
    if (ok) {
        xb_flash_test.passed |= 1u << (n - 1u);
    } else {
        xb_flash_test.failed |= 1u << (n - 1u);
        if (!xb_flash_test.first_fail)
            xb_flash_test.first_fail = n;
    }
}

static int all(const uint8_t *p, uint32_t n, uint8_t v)
{
    uint32_t i;

    for (i = 0; i < n; i++)
        if (p[i] != v)
            return 0;
    return 1;
}

void __attribute__((noinline)) xb_flash_test_park(void)
{
    for (;;)
        ;
}

void xb_main(void)
{
    uint32_t lo = xb_flash_safe_lo();
    uint32_t id = 0, i;
    int ok;

    xb_flash_test.safe_lo = lo;

    /* 1. The chip answers with a known ID (fm1-emu models the Puya part). */
    step(1, xb_flash_id(&id) == XB_FLASH_OK && id == 0x856014u);
    xb_flash_test.id = id;

    /* 2-7. Everything outside the rules is refused before the chip is touched. */
    step(2, xb_flash_erase_4k(0) == XB_FLASH_ERANGE);                  /* boot loader */
    step(3, xb_flash_erase_4k(lo - XB_FLASH_SECTOR) == XB_FLASH_ERANGE); /* just below */
    step(4, xb_flash_erase_4k(XB_FLASH_SAFE_HI) == XB_FLASH_ERANGE);   /* just above */
    step(5, xb_flash_erase_4k(lo + 0x10u) == XB_FLASH_ERANGE);         /* misaligned */
    for (i = 0; i < 32u; i++)
        pattern[i] = (uint8_t)i;
    step(6, xb_flash_write(XB_FLASH_SAFE_HI - 0x10u, pattern, 32u) == XB_FLASH_ERANGE);
    step(7, xb_flash_write(lo, in_flash, sizeof in_flash) == XB_FLASH_EBUF);

    /* 8. Erase the first sector of the window, read back blank. */
    step(8, xb_flash_erase_4k(lo) == XB_FLASH_OK);

    /* 9. 300 bytes from +0x80: across a page boundary, read back by the driver. */
    for (i = 0; i < 300u; i++)
        pattern[i] = (uint8_t)(i * 7u + 3u);
    step(9, xb_flash_write(lo + 0x80u, pattern, 300u) == XB_FLASH_OK);

    /* 10. Read back independently, with the blank bytes on either side. */
    ok = xb_flash_read(lo + 0x7Fu, readback, 302u) == XB_FLASH_OK &&
         readback[0] == 0xFFu && readback[301] == 0xFFu;
    for (i = 0; ok && i < 300u; i++)
        ok = readback[1u + i] == pattern[i];
    step(10, ok);

    /* 11. Writing 1s over 0s cannot work on NOR: the read-back must catch it. */
    for (i = 0; i < 300u; i++)
        pattern[i] = (uint8_t)~pattern[i];
    step(11, xb_flash_write(lo + 0x80u, pattern, 300u) == XB_FLASH_EVERIFY);

    /* 12. Erase again. */
    step(12, xb_flash_erase_4k(lo) == XB_FLASH_OK);

    /* 13. And the start of the sector is blank again. */
    step(13, xb_flash_read(lo, readback, 512u) == XB_FLASH_OK && all(readback, 512u, 0xFFu));

    xb_flash_test.ram_calls = xb_flash_ram_calls;
    xb_flash_test.magic = 0x54464258u;   /* 'XBFT' */
    xb_flash_test.done = (xb_flash_test.passed == (1u << STEPS) - 1u) ? 1u : 2u;
    xb_flash_test_park();
}
