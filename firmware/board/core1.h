/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - hold the second core (see core1.c).
 */
#ifndef XB_CORE1_H
#define XB_CORE1_H

/* Hold core 1, whatever ran on it before Xebec (the loader, another firmware).
 * Registers only: safe before the C runtime is set up. */
void xb_core1_hold(void);

#endif
