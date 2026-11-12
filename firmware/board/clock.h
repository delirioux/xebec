/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - time base (TIMER4, 24 MHz crystal) and the CPU clock measured against it.
 */
#ifndef XB_CLOCK_H
#define XB_CLOCK_H

#include <stdint.h>

void xb_timer_init(void);
uint32_t xb_ticks(void);

/* Core 0's clock in MHz, from 1,000,000 dependent adds timed by TIMER4; 0 = no measurement. */
uint32_t xb_cpu_mhz(void);

#endif
