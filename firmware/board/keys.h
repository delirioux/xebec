/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - key matrix, the minimum the boot needs: read one column.
 *
 * The full panel (all keys, buttons, encoders, LEDs, debounced and
 * scanned all the time) comes with milestone 3. Until then Xebec only
 * reads the update keys at power-on.
 */
#ifndef XB_KEYS_H
#define XB_KEYS_H

#include <stdint.h>

/* Matrix positions (column, row) of the buttons used at boot. */
#define XB_KEY_OCT_DOWN_COL 0u
#define XB_KEY_OCT_UP_COL   1u
#define XB_KEY_OCT_ROW      4u    /* both on row 4 (PA8) */

/* Rows, columns and the 595 lines set up; no column selected. */
void xb_keys_init(void);

/* The rows closed in column `col` (bit n = row n), then no column selected. */
uint32_t xb_keys_column(uint32_t col);

/* OCT- and OCT+ both held, in every one of several reads over about 20 ms. */
int xb_keys_update_held(void);

#endif
