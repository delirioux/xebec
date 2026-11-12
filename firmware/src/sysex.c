/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - SysEx over USB-MIDI (see sysex.h). Frame format from M-VAVE's WebUpdate.js.
 *
 * A frame: F0, then 7-bit packed [00 59 cmd len(3) payload sum], F7, where
 * sum makes the payload plus itself add up to 0xFF. Command 0x11 asks who is
 * there; the answer is the product ID in 27 bytes. Command 0x30 asks the
 * installer for package bytes: flags 0, address (4), length (3); the answer
 * repeats those 8 bytes, followed by the data.
 */
#include "sysex.h"
#include "../board/usb.h"

#define MAX_DATA 512u
#define MAX_RAW  (6u + 8u + MAX_DATA + 1u)       /* the largest frame: a 512-byte answer */
#define MAX_WIRE (2u + (MAX_RAW * 8u + 6u) / 7u)

uint32_t xb_sysex_identities;
int xb_sysex_upgrade;

static const uint8_t UPGRADE[6] = { 0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7 };

static uint8_t msg[MAX_WIRE + 8u];
static uint8_t answer[8u + MAX_DATA];      /* the last 0x30 answer: addr/len head, then data */
static uint32_t answer_len;
static uint32_t msg_len;
static int collecting;

static void send_sysex(const uint8_t *p, uint32_t n)
{
    uint32_t i = 0;

    while (n - i > 3u) {
        xb_usb_midi_send(0x04u | (uint32_t)p[i] << 8 | (uint32_t)p[i + 1] << 16 | (uint32_t)p[i + 2] << 24);
        i += 3u;
    }
    switch (n - i) {
    case 1: xb_usb_midi_send(0x05u | (uint32_t)p[i] << 8); break;
    case 2: xb_usb_midi_send(0x06u | (uint32_t)p[i] << 8 | (uint32_t)p[i + 1] << 16); break;
    case 3: xb_usb_midi_send(0x07u | (uint32_t)p[i] << 8 | (uint32_t)p[i + 1] << 16 | (uint32_t)p[i + 2] << 24); break;
    }
}

static uint32_t unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t acc = 0, bits = 0, m = 0, i;

    for (i = 0; i < n; i++) {
        acc |= (uint32_t)(in[i] & 0x7Fu) << bits;
        bits += 7u;
        if (bits >= 8u) {
            if (m == max)
                return 0;
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

static void send_frame(uint32_t cmd, const uint8_t *payload, uint32_t n)
{
    static uint8_t raw[7u + 32u], out[2u + ((7u + 32u) * 8u + 6u) / 7u];   /* payloads up to 32 bytes */
    uint32_t i, sum = 0, m;

    if (n > 32u)
        return;

    raw[0] = 0x00;
    raw[1] = 0x59;
    raw[2] = (uint8_t)cmd;
    raw[3] = (uint8_t)n;
    raw[4] = (uint8_t)(n >> 8);
    raw[5] = 0;
    for (i = 0; i < n; i++) {
        raw[6 + i] = payload[i];
        sum += payload[i];
    }
    raw[6 + n] = (uint8_t)~sum;
    out[0] = 0xF0;
    m = 1u + pack7(raw, 7u + n, out + 1);
    out[m++] = 0xF7;
    send_sysex(out, m);
}

static void frame_in(const uint8_t *m, uint32_t n)
{
    static uint8_t raw[MAX_RAW];
    static const char id[] = XB_PRODUCT_ID;
    uint8_t reply[27];
    uint32_t len, i, sum = 0;

    if (n < 10u || m[0] != 0xF0u || m[n - 1] != 0xF7u)
        return;
    len = unpack7(m + 1, n - 2u, raw, MAX_RAW);
    if (len < 7u || raw[0] != 0 || raw[1] != 0x59u)
        return;
    if ((raw[3] | (uint32_t)raw[4] << 8 | (uint32_t)raw[5] << 16) != len - 7u)
        return;
    for (i = 6; i < len; i++)
        sum += raw[i];
    if ((sum & 0xFFu) != 0xFFu)
        return;
    if (raw[2] == 0x11u) {
        for (i = 0; i < sizeof reply; i++)
            reply[i] = i + 1u < sizeof id ? (uint8_t)id[i] : 0;
        send_frame(0x11, reply, sizeof reply);
        xb_sysex_identities++;
    } else if (raw[2] == 0x30u && len >= 7u + 8u) {
        answer_len = len - 7u;
        for (i = 0; i < answer_len; i++)
            answer[i] = raw[6 + i];
    }
}

void xb_sysex_request(uint32_t addr, uint32_t len)
{
    uint8_t q[8];

    q[0] = 0;
    q[1] = (uint8_t)addr;
    q[2] = (uint8_t)(addr >> 8);
    q[3] = (uint8_t)(addr >> 16);
    q[4] = (uint8_t)(addr >> 24);
    q[5] = (uint8_t)len;
    q[6] = (uint8_t)(len >> 8);
    q[7] = (uint8_t)(len >> 16);
    answer_len = 0;
    send_frame(0x30, q, sizeof q);
}

int xb_sysex_data(uint32_t addr, uint8_t *dst, uint32_t len)
{
    uint32_t i, a, l;

    if (!answer_len)
        return 0;
    a = answer[1] | (uint32_t)answer[2] << 8 | (uint32_t)answer[3] << 16 | (uint32_t)answer[4] << 24;
    l = answer[5] | (uint32_t)answer[6] << 8 | (uint32_t)answer[7] << 16;
    if (a != addr || l != len || answer_len != 8u + len) {
        answer_len = 0;
        return 0;
    }
    for (i = 0; i < len; i++)
        dst[i] = answer[8 + i];
    answer_len = 0;
    return 1;
}

static void byte_in(uint8_t b)
{
    if (b >= 0xF8u)
        return;                            /* real-time bytes may sit inside SysEx */
    if (b == 0xF0u) {
        collecting = 1;
        msg_len = 0;
    }
    if (!collecting)
        return;
    if ((b & 0x80u) && b != 0xF0u && b != 0xF7u) {
        collecting = 0;                    /* a status byte ends a broken SysEx */
        return;
    }
    if (msg_len == sizeof msg) {
        collecting = 0;
        return;
    }
    msg[msg_len++] = b;
    if (b == 0xF7u) {
        uint32_t i;

        collecting = 0;
        for (i = 0; i < sizeof UPGRADE && msg_len == sizeof UPGRADE && msg[i] == UPGRADE[i]; i++)
            ;
        if (i == sizeof UPGRADE)
            xb_sysex_upgrade = 1;
        else
            frame_in(msg, msg_len);
    }
}

void xb_sysex_service(void)
{
    uint32_t pkt, n, i;

    while (xb_usb_midi_take(&pkt)) {
        switch (pkt & 0x0Fu) {             /* code index number */
        case 0x4: case 0x7: n = 3; break;
        case 0x6: n = 2; break;
        case 0x5: n = 1; break;
        default: n = 0; break;             /* not SysEx: nothing Xebec plays yet */
        }
        for (i = 0; i < n; i++)
            byte_in((uint8_t)(pkt >> (8u + 8u * i)));
    }
}
