/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - hand the FM-1 over to M-VAVE's OTA loader.
 *
 * OCT- and OCT+ held at power-on: stage the loader (ota_stage.h), write
 * the update record to flash and to RAM, and reset. M-VAVE's boot loader
 * finds the record and runs the loader, which then takes a firmware from
 * the web updater over USB.
 *
 * The record (112 bytes; the first 80 count, the rest 0), as M-VAVE's own
 * update code (update_state) and Felucca write it:
 *   +0   CRC-16 of bytes 2..79
 *   +2   type 0x5A0D: the USB loader (usb_hid_ota.bin)
 *   +4   0x5A01
 *   +6   magic 0x5441
 *   +8   the loader's name on USB, "ota-FM-1_015"
 *   +72  flash offset of the staged loader's head
 * In RAM at 0x01C7FD88 for the boot loader after the reset, and in flash at
 * the end of the staging area (area + 0x4F00, a 4 KiB boundary less 256,
 * where M-VAVE's code looks for it) so that an update cut off by a power
 * loss can start over.
 */
#ifndef XB_UPDATE_H
#define XB_UPDATE_H

#include <stdint.h>

#define XB_UPDATE_RECORD_LEN 112u
#define XB_UPDATE_TYPE_USB   0x5A0Du
#define XB_UPDATE_STATUS     0x5A01u
#define XB_UPDATE_MAGIC      0x5441u
#define XB_UPDATE_NAME       "ota-FM-1_015"

/* The record for a loader staged at flash offset `area`. */
void xb_update_record(uint8_t *rec, uint32_t area);

/* Stage the loader, then the record in flash (read back) and in RAM.
 * 0 = ready for xb_update_restart(); on failure everything is undone. */
int xb_update_prepare(void);

/* Reset into the boot loader. Does not return. */
void xb_update_restart(void);

/* At a normal boot: a staging area or a RAM record left over (Xebec is
 * running, so that update finished or never started) is cleared, so the
 * boot loader does not start the loader again. 1 = something was cleared,
 * 0 = nothing there, < 0 = the erase failed. */
int xb_update_cleanup(void);

#endif
