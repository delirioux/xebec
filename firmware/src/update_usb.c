/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - an update asked for over USB (see update_usb.h). Checks as Felucca's ota.c makes them.
 */
#include "update_usb.h"
#include "sysex.h"
#include "ota_stage.h"
#include "../board/flash.h"
#include "../board/clock.h"

#define TICKS_PER_MS  24000u               /* TIMER4 runs from the 24 MHz crystal */
#define TRY_TICKS     (5000u * TICKS_PER_MS)
#define TRIES         3u
#define SESSION_TICKS (120000u * TICKS_PER_MS)
#define CHUNK         512u
#define CONFIRM_ADDR  0xE0000000u

uint32_t xb_update_usb_reads;

static uint8_t ufw[1024], part[CHUNK], own_head[XB_UU_HEAD_LEN];
static uint32_t started;

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }

/* UFW head and entries: XOR with a CRC-16 stream from 0xFFFF, restarted per block. */
static void decipher(uint8_t *p, uint32_t n)
{
    uint32_t k = 0xFFFFu;

    while (n--) {
        *p++ ^= (uint8_t)k;
        k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
    }
}

static int same(const uint8_t *a, const uint8_t *b, uint32_t n)
{
    while (n--)
        if (*a++ != *b++)
            return 0;
    return 1;
}

/* One part of the package from the installer: three tries of 5 s each. */
static int fetch(void (*idle)(void), uint32_t addr, uint8_t *dst, uint32_t len)
{
    uint32_t tries, t;

    for (tries = 0; tries < TRIES; tries++) {
        if (xb_ticks() - started > SESSION_TICKS)
            return XB_UU_EHOST;
        xb_sysex_request(addr, len);
        xb_update_usb_reads++;
        for (t = xb_ticks(); xb_ticks() - t < TRY_TICKS;) {
            idle();
            if (xb_sysex_data(addr, dst, len))
                return XB_UU_OK;
        }
    }
    return XB_UU_EHOST;
}

int xb_update_usb_check(void (*idle)(void))
{
    uint32_t i, n, count, fl_off = 0, fl_len = 0, ota_off = 0, ota_len = 0;
    int have_fl = 0, have_ota = 0;

    xb_update_usb_reads = 0;

    /* This FM-1's flash head first, while the installer waits for our first request:
     * the flash driver spins on SPI0 registers, which must not happen during USB traffic. */
    if (xb_flash_read(0, own_head, XB_UU_HEAD_LEN))
        return XB_UU_EFLASH;
    started = xb_ticks();

    /* 1. UFW head (64 bytes) and entry list (80 bytes each), checked by their CRCs. */
    if (fetch(idle, 0, ufw, CHUNK) || fetch(idle, CHUNK, ufw + CHUNK, CHUNK))
        return XB_UU_EHOST;
    decipher(ufw, 64);
    count = rd16(ufw + 8);
    if (xb_ota_crc16(ufw + 2, 62, 0) != rd16(ufw) || count == 0 || count > 11u ||
        xb_ota_crc16(ufw + 64, count * 80u, 0) != rd16(ufw + 2))
        return XB_UU_EUFW;
    for (i = 0; i < count; i++) {
        uint8_t *e = ufw + 64 + i * 80u;

        decipher(e, 80);
        if (rd16(e) == 0) {                /* flash.bin */
            fl_off = rd32(e + 8);
            fl_len = rd32(e + 12);
            have_fl = 1;
        } else if (rd16(e) == 100) {       /* ota.bin */
            ota_off = rd32(e + 8);
            ota_len = rd32(e + 12);
            have_ota = 1;
        }
    }
    if (!have_fl || !have_ota)
        return XB_UU_EPKG;

    /* 2. This board's package. */
    if (fl_len != XB_UU_FLASH_LEN)
        return XB_UU_EBOARD;

    /* 3. The package's loader is the one Xebec stages. */
    if (ota_len != xb_ota_loader_len)
        return XB_UU_ELOADER;
    for (i = 0; i < ota_len; i += n) {
        n = ota_len - i > CHUNK ? CHUNK : ota_len - i;
        if (fetch(idle, ota_off + i, part, n))
            return XB_UU_EHOST;
        if (!same(part, xb_ota_loader + i, n))
            return XB_UU_ELOADER;
    }

    /* 4. The loader rewrites the flash head: the package's must equal this FM-1's. */
    for (i = 0; i < XB_UU_HEAD_LEN; i += CHUNK) {
        if (fetch(idle, fl_off + i, part, CHUNK))
            return XB_UU_EHOST;
        if (!same(part, own_head + i, CHUNK))
            return XB_UU_EHEAD;
    }

    /* 5. The installer's go-ahead. */
    if (fetch(idle, CONFIRM_ADDR, part, 8) || !same(part, (const uint8_t *)"success", 8))
        return XB_UU_ECONFIRM;
    return XB_UU_OK;
}
