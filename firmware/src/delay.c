/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - delays timed by TIMER4 (24 MHz crystal), exact at any CPU clock.
 */
#include "delay.h"
#include "../board/clock.h"

void xb_delay_us(unsigned us)
{
    uint32_t start;

    xb_timer_init();
    start = xb_ticks();
    while (xb_ticks() - start < us * 24u)
        ;
}

void xb_delay_ms(unsigned ms)
{
    while (ms--)
        xb_delay_us(1000u);
}
