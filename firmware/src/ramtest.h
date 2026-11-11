/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 *
 * Xebec - boot-time RAM test.
 */
#ifndef XB_RAMTEST_H
#define XB_RAMTEST_H

#include <stdint.h>

/* The block the boot test checks: 64 KiB above Xebec's own RAM window
 * and well below the loader's hand-over area at the top of RAM. */
#define XB_RAMTEST_BASE  0x01C60000u
#define XB_RAMTEST_BYTES 0x00010000u

/* Writes and reads back three patterns over the block. Returns 1 when
 * every word held what was written, 0 otherwise. Destroys the block. */
int xb_ramtest(void);

#endif
