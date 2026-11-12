/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - hold the second core (see core1.h). Register facts from Melodee's and
 * Felucca's notes: core 1's control word, and the bus-error flag a hold can raise.
 *
 * A reset does not stop core 1: it can keep running what an earlier firmware or the
 * loader gave it, and its bus traffic starves the USB DMA (babble on EP1 IN).
 */
#include "core1.h"
#include "fm1_regs.h"

void xb_core1_hold(void)
{
    uint32_t locked = !(XB_DBG_WR_EN & 1u), en;
    volatile uint32_t n;

    if (locked)
        XB_DBG_WR_EN = XB_DBG_UNLOCK;
    en = XB_DBG_EN;
    XB_DBG_EN = en & ~XB_DBG_BUS_TRAPS;        /* a cut-off access of core 1's must not trap core 0 */
    __asm__ volatile("csync" ::: "memory");
    XB_C1_CON |= XB_C1_HOLD;
    XB_C1_CON &= ~XB_C1_RUN;
    __asm__ volatile("csync" ::: "memory");
    for (n = 0; n < 256u; n++)                 /* the bus-error flag arrives late */
        ;
    XB_DBG_MSG_CLR = XB_DBG_BUS_TRAPS;
    __asm__ volatile("csync" ::: "memory");
    XB_DBG_EN = en;
    if (locked && (XB_DBG_WR_EN & 1u))
        XB_DBG_WR_EN = XB_DBG_UNLOCK;          /* writing it toggles the lock */
    __asm__ volatile("csync" ::: "memory");
}
