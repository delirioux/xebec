/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - SysEx over USB-MIDI: M-VAVE's update protocol frames, the identity for now.
 */
#ifndef XB_SYSEX_H
#define XB_SYSEX_H

#include <stdint.h>

#define XB_PRODUCT_ID "FM-1_47932320"

/* Take the computer's USB-MIDI packets and answer what Xebec knows. */
void xb_sysex_service(void);

/* Identity questions answered so far. */
extern uint32_t xb_sysex_identities;

#endif
