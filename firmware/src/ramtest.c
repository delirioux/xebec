/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 *
 * Xebec - boot-time RAM test.
 *
 * Three passes over the block: each word's own address (catches address
 * lines that short or float), its inverse (every bit the other way), and
 * a walking-ones pattern (catches stuck data bits).
 */
#include "ramtest.h"

#define WORDS (XB_RAMTEST_BYTES / 4u)

static int pass(uint32_t (*pattern)(uint32_t i))
{
    volatile uint32_t *ram = (volatile uint32_t *)(uintptr_t)XB_RAMTEST_BASE;
    uint32_t i;

    for (i = 0; i < WORDS; i++)
        ram[i] = pattern(i);
    for (i = 0; i < WORDS; i++)
        if (ram[i] != pattern(i))
            return 0;
    return 1;
}

static uint32_t own_address(uint32_t i) { return XB_RAMTEST_BASE + i * 4u; }
static uint32_t inverse(uint32_t i)     { return ~(XB_RAMTEST_BASE + i * 4u); }
static uint32_t walking_one(uint32_t i) { return 1u << (i & 31u); }

int xb_ramtest(void)
{
    return pass(own_address) && pass(inverse) && pass(walking_one);
}
