/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - key matrix, the minimum the boot needs (see keys.h).
 *
 * The 595 chain is clocked by hand (bit-banged), 16 bits MSB first. Each
 * edge is a read-modify-write of the port: write-only edges were found too
 * short for the 595s on the board. The column word is all ones with the
 * column's bit low; columns 0 and 1 also pull bit 11 + column low, as both
 * M-VAVE's and Felucca's scans do.
 */
#include "keys.h"
#include "fm1_regs.h"
#include "../src/delay.h"

#define SER   (1u << XB_SR_SER_PIN)
#define CLK   (1u << XB_SR_CLK_PIN)
#define LATCH (1u << XB_SR_LATCH_PIN)
#define SR_PINS (SER | CLK | LATCH)

#define ROWS_A ((1u << 0) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8))   /* PA0, PA5..PA8 */
#define ROWS_B (1u << 7)                                                    /* PB7 */

#define NCOL        11u
#define SETTLE_US   10u     /* after the latch, before reading the rows */
#define HOLD_READS  5u      /* reads that must all see both keys */
#define HOLD_GAP_MS 4u

static void sr_word(uint32_t w)
{
    uint32_t i;

    for (i = 0; i < 16u; i++) {
        if (w & (0x8000u >> i))
            XB_PORT_OUT(XB_PORT_A) |= SER;
        else
            XB_PORT_OUT(XB_PORT_A) &= ~SER;
        XB_PORT_OUT(XB_PORT_A) |= CLK;
        XB_PORT_OUT(XB_PORT_A) &= ~CLK;
    }
    XB_PORT_OUT(XB_PORT_A) |= LATCH;
    XB_PORT_OUT(XB_PORT_A) &= ~LATCH;
}

static uint32_t rows(void)
{
    uint32_t a = XB_PORT_IN(XB_PORT_A), b = XB_PORT_IN(XB_PORT_B);

    /* row 0 = PA0, rows 1-4 = PA5..PA8, row 5 = PB7; low = closed */
    return ~((a & 1u) | ((a >> 4) & 0x1Eu) | ((b >> 2) & 0x20u)) & 0x3Fu;
}

void xb_keys_init(void)
{
    XB_PORT_DIE(XB_PORT_A) |= ROWS_A;
    XB_PORT_DIR(XB_PORT_A) |= ROWS_A;
    XB_PORT_PD(XB_PORT_A) &= ~ROWS_A;
    XB_PORT_PU(XB_PORT_A) |= ROWS_A;

    XB_PORT_DIE(XB_PORT_B) |= ROWS_B;
    XB_PORT_DIR(XB_PORT_B) |= ROWS_B;
    XB_PORT_PD(XB_PORT_B) &= ~ROWS_B;
    XB_PORT_PU(XB_PORT_B) |= ROWS_B;

    XB_PORT_DIE(XB_PORT_A) |= SR_PINS;
    XB_PORT_PU(XB_PORT_A) &= ~SR_PINS;
    XB_PORT_PD(XB_PORT_A) &= ~SR_PINS;
    XB_PORT_OUT(XB_PORT_A) &= ~SR_PINS;
    XB_PORT_DIR(XB_PORT_A) &= ~SR_PINS;

    sr_word(0xFFFFu);
}

uint32_t xb_keys_column(uint32_t col)
{
    uint32_t w, r;

    if (col >= NCOL)
        return 0;
    w = 0xFFFFu ^ (1u << col);
    if (col < 2u)
        w ^= 1u << (11u + col);
    sr_word(w);
    xb_delay_us(SETTLE_US);
    r = rows();
    sr_word(0xFFFFu);
    return r;
}

int xb_keys_update_held(void)
{
    uint32_t i, row = 1u << XB_KEY_OCT_ROW;

    for (i = 0; i < HOLD_READS; i++) {
        if (i)
            xb_delay_ms(HOLD_GAP_MS);
        if (!(xb_keys_column(XB_KEY_OCT_DOWN_COL) & row) || !(xb_keys_column(XB_KEY_OCT_UP_COL) & row))
            return 0;
    }
    return 1;
}
