/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - USB device (see usb.h). Controller facts from Felucca's usb.c and fm1-emu's usb.rs.
 */
#include "usb.h"
#include "fm1_regs.h"
#include "../src/delay.h"

enum { S_FADDR = 0, S_POWER = 1, S_INTRTX1 = 2, S_INTRRX1 = 4, S_INTRUSB = 6, S_INTRTX1E = 7,
       S_INTRTX2E = 8, S_INTRRX1E = 9, S_INTRRX2E = 10, S_INTRUSBE = 11, S_INDEX = 14,
       S_TXMAXP = 16, S_CSR0 = 17, S_TXCSR1 = 17, S_TXCSR2 = 18, S_RXMAXP = 19, S_RXCSR1 = 20,
       S_RXCSR2 = 21, S_COUNT0 = 22, S_RXCOUNT1 = 22, S_RXCOUNT2 = 23 };

#define VID 0x1209u     /* pid.codes */
#define PID 0x0001u     /* pid.codes test PID */

static const uint8_t DEV_DESC[18] = {
    18, 1, 0x10, 0x01, 0, 0, 0, 64, VID & 0xFF, VID >> 8, PID & 0xFF, PID >> 8, 0x01, 0x00, 1, 2, 0, 1,
};

#define CFG_LEN 101u
static const uint8_t CFG_DESC[CFG_LEN] = {
    9, 2, CFG_LEN, 0, 2, 1, 0, 0x80, 50,
    9, 4, 0, 0, 0, 1, 1, 0, 0,                     /* audio control */
    9, 0x24, 1, 0x00, 0x01, 9, 0, 1, 1,
    9, 4, 1, 0, 2, 1, 3, 0, 0,                     /* MIDI streaming */
    7, 0x24, 1, 0x00, 0x01, 65, 0,
    6, 0x24, 2, 1, 1, 0,                           /* IN jack 1, embedded */
    6, 0x24, 2, 2, 2, 0,                           /* IN jack 2, external */
    9, 0x24, 3, 1, 3, 1, 2, 1, 0,                  /* OUT jack 3, embedded, from 2 */
    9, 0x24, 3, 2, 4, 1, 1, 1, 0,                  /* OUT jack 4, external, from 1 */
    9, 5, 0x01, 2, 64, 0, 0, 0, 0,                 /* EP1 OUT bulk */
    5, 0x25, 1, 1, 1,
    9, 5, 0x81, 2, 64, 0, 0, 0, 0,                 /* EP1 IN bulk */
    5, 0x25, 1, 1, 3,
};

static const uint8_t STR0[4] = {4, 3, 0x09, 0x04};
static const uint8_t STR1[20] = {20, 3, 'd', 0, 'e', 0, 'l', 0, 'i', 0, 'r', 0, 'i', 0, 'o', 0, 'u', 0, 'x', 0};
static const uint8_t STR2[22] = {22, 3, 'F', 0, 'M', 0, '-', 0, '1', 0, ' ', 0, 'X', 0, 'e', 0, 'b', 0, 'e', 0, 'c', 0};

static uint8_t ep0buf[68] __attribute__((aligned(4)));
static uint8_t ep1tx[64] __attribute__((aligned(4)));
static uint8_t ep1rx[68] __attribute__((aligned(4)));
static uint8_t zero2[2];

uint32_t xb_usb_rx_count, xb_usb_tx_count;

#define QN 64u
static uint32_t txq[QN], rxq[QN];
static uint32_t tx_w, tx_r, rx_w, rx_r;

static struct {
    uint8_t up, config, pend_addr, has_pend_addr, rx_pend, e0_tx, e0_zlp;
    uint32_t timeouts;
    const uint8_t *e0_src;
    uint32_t e0_left;
} usb;

static void sync_out(void) { __asm__ volatile("csync" ::: "memory"); }
static void sync_in(void) { __asm__ volatile("ssync" ::: "memory"); }

static void sie_wr(uint32_t r, uint32_t v)
{
    uint32_t n = 20000;

    if (!(XB_USB_CON0 & 4u))
        return;
    XB_USB_CON1 = ((r << 8) & 0xFFFF00u) | (v & 0xFFu);
    sync_out();
    while (!(XB_USB_CON1 & 0x8000u) && --n)
        ;
    if (n)
        usb.timeouts = 0;
    else if (++usb.timeouts > 50u)
        usb.up = 0;
}

static uint32_t sie_rd(uint32_t r)
{
    uint32_t n = 20000;

    if (!(XB_USB_CON0 & 4u))
        return 0;
    XB_USB_CON1 = ((r << 8) & 0xFFBF00u) | 0x4000u;
    sync_out();
    while (!(XB_USB_CON1 & 0x8000u))
        if (!--n) {
            if (++usb.timeouts > 50u)
                usb.up = 0;
            return 0;
        }
    usb.timeouts = 0;
    return XB_USB_CON1 & 0xFFu;
}

static void ep_enable(uint32_t eps) { XB_USB_CON0 &= ~(eps << 19); }

int xb_usb_start(void)
{
    uint32_t n;

    usb.timeouts = 0;
    XB_USB_CON0 = 0;
    XB_USB_CON1 = 0;
    XB_USB_IO_CON0 = 0x0Cu;
    xb_delay_ms(25);

    XB_CLK_CON1 &= ~3u;
    XB_USB_EP0_ADR = (uint32_t)(uintptr_t)ep0buf;
    for (n = 1; n <= 3u; n++)
        XB_USB_EP_RADR(n) = (uint32_t)(uintptr_t)ep0buf;
    XB_USB_EP4_RADR = (uint32_t)(uintptr_t)ep0buf;
    XB_USB_IO_CON0 |= (1u << 10) | (1u << 9);
    XB_USB_IO_CON0 &= ~(1u << 11);
    XB_USB_IO_CON0 |= 1u << 12;
    XB_USB_CON0 &= ~0x4035u;
    XB_USB_CON1 = 0;
    XB_USB_CON0 |= 1u;
    XB_USB_IO_CON0 &= ~0xF0u;
    XB_USB_IO_CON0 |= 0x40u;
    XB_USB_CON0 |= 0x3Cu;

    sie_wr(S_POWER, 0x60);
    sie_wr(S_INTRUSBE, 0x07);
    sie_wr(S_INTRTX1E, 0x01);
    sie_wr(S_INTRTX2E, 0);
    sie_wr(S_INTRRX1E, 0);
    sie_wr(S_INTRRX2E, 0);
    ep_enable(0x1Fu);
    usb.config = 0;
    usb.up = usb.timeouts < 3u;
    return usb.up ? 0 : -1;
}

int xb_usb_up(void)
{
    return usb.up;
}

int xb_usb_configured(void)
{
    return usb.up && usb.config == 1u;
}

static int get_desc(uint32_t wvalue, const uint8_t **d, uint32_t *l)
{
    switch (wvalue >> 8) {
    case 1: *d = DEV_DESC; *l = sizeof DEV_DESC; return 1;
    case 2: *d = CFG_DESC; *l = sizeof CFG_DESC; return 1;
    case 3:
        switch (wvalue & 0xFFu) {
        case 0: *d = STR0; *l = sizeof STR0; return 1;
        case 1: *d = STR1; *l = sizeof STR1; return 1;
        case 2: *d = STR2; *l = sizeof STR2; return 1;
        }
    }
    return 0;
}

static void ep1_config(void)
{
    XB_USB_EP_TADR(1) = (uint32_t)(uintptr_t)ep1tx;
    XB_USB_EP_RADR(1) = (uint32_t)(uintptr_t)ep1rx;
    sie_wr(S_INDEX, 1);
    sie_wr(S_TXMAXP, 0xFF);
    sie_wr(S_TXCSR1, 0x48);
    sie_wr(S_TXCSR2, 0);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);
    sie_wr(S_RXCSR2, 0);
    sie_wr(S_INTRRX1E, 0x02);
    ep_enable(1u << 1);
}

static void e0_chunk(void)
{
    uint32_t n = usb.e0_left > 64u ? 64u : usb.e0_left, i;
    int last;

    for (i = 0; i < n; i++)
        ep0buf[i] = usb.e0_src[i];
    sync_out();
    XB_USB_EP0_ADR = (uint32_t)(uintptr_t)ep0buf;
    XB_USB_EP_CNT(0) = n;
    usb.e0_src += n;
    usb.e0_left -= n;
    last = usb.e0_left == 0 && !(n == 64u && usb.e0_zlp);
    if (usb.e0_left == 0 && n == 64u)
        usb.e0_zlp = 0;
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, last ? 0x0A : 0x02);
    if (last)
        usb.e0_tx = 0;
}

static void e0_send(const uint8_t *p, uint32_t len, uint32_t wlen)
{
    if (len > wlen)
        len = wlen;
    usb.e0_src = p;
    usb.e0_left = len;
    usb.e0_zlp = len < wlen && (len % 64u) == 0;
    usb.e0_tx = 1;
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x40);
    e0_chunk();
}

static void ep0_service(void)
{
    uint8_t s[8];
    uint32_t csr, i, wvalue, wlength, l;
    const uint8_t *d;

    if (usb.has_pend_addr) {
        sie_wr(S_FADDR, usb.pend_addr);
        usb.has_pend_addr = 0;
    }
    sie_wr(S_INDEX, 0);
    csr = sie_rd(S_CSR0);
    if (csr & 0x04u) {                     /* SentStall */
        sie_wr(S_CSR0, 0);
        usb.e0_tx = 0;
        return;
    }
    if (csr & 0x10u) {                     /* SetupEnd */
        sie_wr(S_CSR0, 0x80);
        usb.e0_tx = 0;
    }
    if (usb.e0_tx) {
        if (!(csr & 0x02u))
            e0_chunk();
        return;
    }
    if (!(csr & 0x01u))
        return;
    sync_in();
    for (i = 0; i < 8u; i++)
        s[i] = ep0buf[i];
    wvalue = s[2] | (uint32_t)s[3] << 8;
    wlength = s[6] | (uint32_t)s[7] << 8;

    switch ((uint32_t)s[0] << 8 | s[1]) {
    case 0x0005:                           /* SET_ADDRESS, applied after the status stage */
        usb.pend_addr = s[2] & 0x7Fu;
        usb.has_pend_addr = 1;
        goto ack;
    case 0x8006:                           /* GET_DESCRIPTOR */
        if (!get_desc(wvalue, &d, &l))
            goto stall;
        if (!wlength)
            goto ack;
        e0_send(d, l, wlength);
        return;
    case 0x0009:                           /* SET_CONFIGURATION */
        if (s[2] > 1u)
            goto stall;
        usb.config = s[2];
        if (usb.config)
            ep1_config();
        else
            sie_wr(S_INTRRX1E, 0);
        goto ack;
    case 0x8008:                           /* GET_CONFIGURATION */
        e0_send(&usb.config, 1, wlength);
        return;
    case 0x8000: case 0x8100: case 0x8200: /* GET_STATUS */
        e0_send(zero2, 2, wlength);
        return;
    case 0x010B:                           /* SET_INTERFACE: alternate 0 only */
        if (wvalue)
            goto stall;
        goto ack;
    case 0x810A:                           /* GET_INTERFACE */
        e0_send(zero2, 1, wlength);
        return;
    case 0x0201:                           /* CLEAR_FEATURE(ENDPOINT_HALT) */
        if (wvalue || (s[4] & 0x0Fu) > 1u)
            goto stall;
        if (s[4] & 0x0Fu) {
            sie_wr(S_INDEX, 1);
            if (s[4] & 0x80u)
                sie_wr(S_TXCSR1, 0x40);
            else
                sie_wr(S_RXCSR1, 0x80);
        }
        goto ack;
    default:
        goto stall;
    }
ack:
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x48);
    return;
stall:
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x60);
}

static void ep1_rx(void)
{
    uint32_t csr, n, i;

    sie_wr(S_INDEX, 1);
    csr = sie_rd(S_RXCSR1) | sie_rd(S_RXCSR2) << 8;
    if (!(csr & 1u)) {
        usb.rx_pend = 0;
        return;
    }
    if (QN - (rx_w - rx_r) < 16u)
        return;                            /* no room: the host waits (NAK) */
    n = sie_rd(S_RXCOUNT1) | sie_rd(S_RXCOUNT2) << 8;
    if (n > 64u)
        n = 64u;
    sync_in();
    for (i = 0; i + 3u < n; i += 4u)
        rxq[rx_w++ % QN] = ep1rx[i] | (uint32_t)ep1rx[i + 1] << 8 | (uint32_t)ep1rx[i + 2] << 16 |
                           (uint32_t)ep1rx[i + 3] << 24;
    usb.rx_pend = 0;
    xb_usb_rx_count++;
    csr = (csr & ~0x164u) | 0x10u;
    sie_wr(S_RXCSR1, csr & 0xFFu);
    sie_wr(S_RXCSR2, csr >> 8);
}

static void ep1_tx(void)
{
    uint32_t csr, n = 0;

    if (tx_w == tx_r)
        return;
    sie_wr(S_INDEX, 1);
    csr = sie_rd(S_TXCSR1);
    if (csr & 0x01u)
        return;                            /* the last packet is still waiting */
    if (csr & 0x80u)
        sie_wr(S_TXCSR1, csr & ~0x80u);
    while (tx_r != tx_w && n < 64u) {
        uint32_t p = txq[tx_r++ % QN];

        ep1tx[n++] = (uint8_t)p;
        ep1tx[n++] = (uint8_t)(p >> 8);
        ep1tx[n++] = (uint8_t)(p >> 16);
        ep1tx[n++] = (uint8_t)(p >> 24);
    }
    sync_out();
    XB_USB_EP_TADR(1) = (uint32_t)(uintptr_t)ep1tx;
    XB_USB_EP_CNT(1) = n;
    sie_wr(S_TXCSR1, sie_rd(S_TXCSR1) | 0x01u);
    xb_usb_tx_count++;
}

void xb_usb_poll(void)
{
    uint32_t iu, it, ir;

    if (!usb.up)
        return;
    iu = sie_rd(S_INTRUSB);
    it = sie_rd(S_INTRTX1);
    ir = sie_rd(S_INTRRX1);
    if (iu & 0x04u) {                      /* bus reset */
        sie_wr(S_FADDR, 0);
        usb.config = 0;
        usb.e0_tx = 0;
        usb.has_pend_addr = 0;
        usb.rx_pend = 0;
        tx_r = tx_w;
        XB_USB_EP0_ADR = (uint32_t)(uintptr_t)ep0buf;
        sie_wr(S_INTRUSBE, 0x07);
        sie_wr(S_INTRTX1E, 0x01);
        sie_wr(S_INTRRX1E, 0);
    }
    if (it & 0x01u)
        ep0_service();
    if (ir & 0x02u)
        usb.rx_pend = 1;
    if (usb.rx_pend)
        ep1_rx();
    if (usb.config)
        ep1_tx();
}

int xb_usb_midi_send(uint32_t pkt)
{
    if (tx_w - tx_r >= QN)
        return -1;
    txq[tx_w++ % QN] = pkt;
    return 0;
}

int xb_usb_midi_take(uint32_t *pkt)
{
    if (rx_r == rx_w)
        return 0;
    *pkt = rxq[rx_r++ % QN];
    return 1;
}
