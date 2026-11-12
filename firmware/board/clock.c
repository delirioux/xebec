/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - time base and CPU clock measurement (see clock.h).
 */
#include "clock.h"
#include "fm1_regs.h"

#define ROUNDS 10000u

void xb_timer_init(void)
{
    if ((XB_T4_CON & 0xFu) == XB_T4_RUN_XTAL)
        return;
    XB_T4_CON = 0;
    XB_T4_PRD = 0xFFFFFFFFu;
    XB_T4_CNT = 0;
    XB_T4_CON = XB_T4_RUN_XTAL | (1u << 14);
}

uint32_t xb_ticks(void)
{
    return XB_T4_CNT;
}

/* The same loop twice, 100 and 200 dependent adds a round: the difference
 * is exactly 1,000,000 adds, one cycle each, without the loop's cost. */
XB_RAM_CODE static uint32_t ram_adds_100(void)
{
    uint32_t i, x = 0, t0 = XB_T4_CNT;

    for (i = 0; i < ROUNDS; i++)
        __asm__ volatile(".rept 100\n\t%0 += 1\n\t.endr" : "+r"(x));
    return XB_T4_CNT - t0;
}

XB_RAM_CODE static uint32_t ram_adds_200(void)
{
    uint32_t i, x = 0, t0 = XB_T4_CNT;

    for (i = 0; i < ROUNDS; i++)
        __asm__ volatile(".rept 200\n\t%0 += 1\n\t.endr" : "+r"(x));
    return XB_T4_CNT - t0;
}

static void *far(void *fn)
{
    void *volatile p = fn;

    return p;
}

uint32_t xb_cpu_mhz(void)
{
    uint32_t a, b, d, rest, mhz = 0;

    xb_timer_init();
    a = ((uint32_t (*)(void))far((void *)ram_adds_100))();
    b = ((uint32_t (*)(void))far((void *)ram_adds_200))();
    if (a == 0 || b <= a)
        return 0;
    d = b - a;                              /* TIMER4 ticks for 1,000,000 cycles */
    for (rest = XB_T4_HZ + (d >> 1); rest >= d; rest -= d)
        mhz++;                              /* 24,000,000 / d, rounded, without a divide */
    return mhz;
}
