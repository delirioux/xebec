/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - SysEx over USB-MIDI: M-VAVE's update protocol frames (identity, UPGRADE, read requests).
 */
#ifndef XB_SYSEX_H
#define XB_SYSEX_H

#include <stdint.h>

#define XB_PRODUCT_ID "FM-1_47932320"

/* Take the computer's USB-MIDI packets and answer what Xebec knows. */
void xb_sysex_service(void);

/* Identity questions answered so far. */
extern uint32_t xb_sysex_identities;

/* Set when the installer sent UPGRADE (F0 22 24 35 7F F7); the main loop clears it. */
extern int xb_sysex_upgrade;

/* Ask the installer for `len` (<= 512) bytes of the package at `addr` (command 0x30). */
void xb_sysex_request(uint32_t addr, uint32_t len);

/* 1 = the answer for exactly (addr, len) came in and is copied to `dst`.
 * Any other answer waiting is dropped. */
int xb_sysex_data(uint32_t addr, uint8_t *dst, uint32_t len);

#endif
