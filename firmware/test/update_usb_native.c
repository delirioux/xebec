/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - native test of the USB update checks (update_usb.c, sysex.c) against a
 * simulated installer that serves a real package. Built and run by tools/check_update_usb.py:
 *
 *   update_usb_native XEBEC.ufw STOCK.ufw HEAD.bin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../src/update_usb.h"
#include "../src/sysex.h"
#include "../board/flash.h"

/* ---- the board, mocked ---- */
static uint8_t head[0x4000];
static uint32_t ticks;

uint32_t xb_ticks(void) { return ticks += 24000u; }          /* 1 ms per look at the clock */
int xb_flash_read(uint32_t off, void *dst, uint32_t n)
{
    if (off + n > sizeof head)
        return XB_FLASH_ERANGE;
    memcpy(dst, head + off, n);
    return XB_FLASH_OK;
}
uint32_t xb_flash_safe_lo(void) { return 0xE0000u; }
int xb_flash_erase_4k(uint32_t off) { (void)off; fprintf(stderr, "erase called\n"); exit(1); }
int xb_flash_write(uint32_t off, const void *src, uint32_t n)
{
    (void)off; (void)src; (void)n;
    fprintf(stderr, "write called\n");
    exit(1);
}

/* ---- the installer, simulated: USB-MIDI packets both ways ---- */
static const uint8_t *image;
static uint32_t image_len;
static int silent;
static uint32_t last_addr, confirms;
static uint32_t to_dev[1 << 16], to_dev_w, to_dev_r;
static uint8_t from_dev[700];
static uint32_t from_dev_n;

static uint32_t unpack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, bits = 0, m = 0, i;

    for (i = 0; i < n; i++) {
        acc |= (uint32_t)(in[i] & 0x7Fu) << bits;
        bits += 7u;
        if (bits >= 8u) {
            out[m++] = (uint8_t)acc;
            acc >>= 8;
            bits -= 8u;
        }
    }
    return m;
}

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, bits = 0, m = 0, i;

    for (i = 0; i < n; i++) {
        acc |= (uint32_t)in[i] << bits;
        bits += 8u;
        while (bits >= 7u) {
            out[m++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            bits -= 7u;
        }
    }
    if (bits)
        out[m++] = (uint8_t)(acc & 0x7Fu);
    return m;
}

static void host_send(const uint8_t *p, uint32_t n)
{
    uint32_t i = 0;

    while (n - i > 3u) {
        to_dev[to_dev_w++ % (1u << 16)] = 0x04u | (uint32_t)p[i] << 8 | (uint32_t)p[i + 1] << 16 | (uint32_t)p[i + 2] << 24;
        i += 3u;
    }
    if (n - i == 1u)
        to_dev[to_dev_w++ % (1u << 16)] = 0x05u | (uint32_t)p[i] << 8;
    else if (n - i == 2u)
        to_dev[to_dev_w++ % (1u << 16)] = 0x06u | (uint32_t)p[i] << 8 | (uint32_t)p[i + 1] << 16;
    else
        to_dev[to_dev_w++ % (1u << 16)] = 0x07u | (uint32_t)p[i] << 8 | (uint32_t)p[i + 1] << 16 | (uint32_t)p[i + 2] << 24;
}

/* A read request (cmd 0x30) from Xebec: answer it like the web installer. */
static void host_frame(const uint8_t *m, uint32_t n)
{
    uint8_t raw[700], ans[6 + 8 + 512 + 1], wire[700];
    uint32_t len = unpack7(m + 1, n - 2u, raw), addr, l, i, sum = 0, w;

    if (len != 7u + 8u || raw[0] != 0 || raw[1] != 0x59u || raw[2] != 0x30u) {
        fprintf(stderr, "unexpected frame from Xebec (%u bytes, cmd %02x)\n", len, raw[2]);
        exit(1);
    }
    addr = raw[7] | (uint32_t)raw[8] << 8 | (uint32_t)raw[9] << 16 | (uint32_t)raw[10] << 24;
    l = raw[11] | (uint32_t)raw[12] << 8 | (uint32_t)raw[13] << 16;
    last_addr = addr;
    if (silent)
        return;
    if (l > 512u || (addr != 0xE0000000u && addr + l > image_len)) {
        fprintf(stderr, "read outside the package: %08x + %u\n", addr, l);
        exit(1);
    }
    ans[0] = 0;
    ans[1] = 0x59;
    ans[2] = 0x30;
    ans[3] = (uint8_t)(8u + l);
    ans[4] = (uint8_t)((8u + l) >> 8);
    ans[5] = 0;
    memcpy(ans + 6, raw + 6, 8);
    if (addr == 0xE0000000u) {
        memcpy(ans + 14, "success", 8);
        confirms++;
    } else {
        memcpy(ans + 14, image + addr, l);
    }
    for (i = 6; i < 14u + l; i++)
        sum += ans[i];
    ans[14 + l] = (uint8_t)~sum;
    wire[0] = 0xF0;
    w = 1u + pack7(ans, 15u + l, wire + 1);
    wire[w++] = 0xF7;
    host_send(wire, w);
}

int xb_usb_midi_send(uint32_t pkt)
{
    uint32_t cin = pkt & 0x0Fu, k, nb = cin == 5u ? 1u : cin == 6u ? 2u : 3u;

    if (cin < 4u || cin > 7u) {
        fprintf(stderr, "not SysEx: %08x\n", pkt);
        exit(1);
    }
    for (k = 0; k < nb; k++) {
        uint8_t b = (uint8_t)(pkt >> (8u + 8u * k));

        if (b == 0xF0u)
            from_dev_n = 0;
        from_dev[from_dev_n++] = b;
        if (b == 0xF7u)
            host_frame(from_dev, from_dev_n);
    }
    return 0;
}

int xb_usb_midi_take(uint32_t *pkt)
{
    if (to_dev_r == to_dev_w)
        return 0;
    *pkt = to_dev[to_dev_r++ % (1u << 16)];
    return 1;
}

/* ---- the tests ---- */
static uint8_t *load(const char *path, uint32_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *p;
    long len;

    if (!f || fseek(f, 0, SEEK_END) || (len = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET)) {
        fprintf(stderr, "%s: cannot read\n", path);
        exit(1);
    }
    p = malloc((size_t)len);
    if (!p || fread(p, 1, (size_t)len, f) != (size_t)len)
        exit(1);
    fclose(f);
    *n = (uint32_t)len;
    return p;
}

static void idle(void) { xb_sysex_service(); }

static int failures;

static void run(const char *name, const uint8_t *img, uint32_t n, int quiet, int want, int want_confirm)
{
    int rc;

    image = img;
    image_len = n;
    silent = quiet;
    confirms = 0;
    to_dev_r = to_dev_w = 0;
    rc = xb_update_usb_check(idle);
    printf("%-34s rc %3d (want %3d), %3u reads, confirmed %u\n", name, rc, want, xb_update_usb_reads, confirms);
    if (rc != want || (confirms != 0) != want_confirm || (want_confirm && last_addr != 0xE0000000u)) {
        printf("  FAILED\n");
        failures++;
    }
}

int main(int argc, char **argv)
{
    uint32_t xn, sn, hn, i, ota_off = 0;
    uint8_t *xebec, *stock, *bad, *h;

    if (argc != 4)
        return 2;
    xebec = load(argv[1], &xn);
    stock = load(argv[2], &sn);
    h = load(argv[3], &hn);
    if (hn != sizeof head)
        return 2;
    memcpy(head, h, sizeof head);

    xb_update_usb_prepare();
    run("Xebec's package", xebec, xn, 0, XB_UU_OK, 1);
    run("M-VAVE's stock package", stock, sn, 0, XB_UU_OK, 1);

    /* where ota.bin sits: the entry with type 100 (entries deciphered like update_usb.c) */
    for (i = 0; i < 11u && !ota_off; i++) {
        uint8_t e[80];
        uint32_t k = 0xFFFFu, j;

        memcpy(e, xebec + 64 + i * 80u, 80);
        for (j = 0; j < 80u; j++) {
            e[j] ^= (uint8_t)k;
            k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
        }
        if ((e[0] | e[1] << 8) == 100)
            ota_off = e[8] | (uint32_t)e[9] << 8 | (uint32_t)e[10] << 16 | (uint32_t)e[11] << 24;
    }
    if (!ota_off)
        return 2;

    bad = malloc(xn);
    memcpy(bad, xebec, xn);
    bad[ota_off + 0x100] ^= 0x01;
    run("loader one bit off", bad, xn, 0, XB_UU_ELOADER, 0);

    memcpy(bad, xebec, xn);
    bad[20] ^= 0x01;
    run("UFW head one bit off", bad, xn, 0, XB_UU_EUFW, 0);

    head[0x1234] ^= 0x01;
    xb_update_usb_prepare();
    run("this FM-1's head differs", xebec, xn, 0, XB_UU_EHEAD, 0);
    head[0x1234] ^= 0x01;
    xb_update_usb_prepare();

    run("installer silent", xebec, xn, 1, XB_UU_EHOST, 0);
    run("Xebec's package again", xebec, xn, 0, XB_UU_OK, 1);

    if (failures) {
        printf("USB update checks FAILED (%d)\n", failures);
        return 1;
    }
    printf("USB update checks passed\n");
    return 0;
}
