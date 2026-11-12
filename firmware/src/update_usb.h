/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - an update asked for over USB (the web installer's UPGRADE).
 *
 * The installer sends F0 22 24 35 7F F7, then answers Xebec's read requests
 * (SysEx command 0x30) with parts of the package. Xebec checks the package
 * before anything is written:
 *   1. the UFW head and its entry list (CRCs), with a flash.bin and an ota.bin;
 *   2. flash.bin is this board's size (0x93000): another board's package is refused;
 *   3. ota.bin is byte for byte the loader Xebec carries (M-VAVE's, ota_stage.h);
 *   4. the first 16 KiB of flash.bin equal this FM-1's flash head, which the
 *      loader rewrites (the same check Felucca makes);
 *   5. the installer confirms ("success" at address 0xE0000000).
 * Only then does the caller stage the loader and restart (update.h), as the
 * OCT- + OCT+ path does. Nothing is erased or written here.
 */
#ifndef XB_UPDATE_USB_H
#define XB_UPDATE_USB_H

#include <stdint.h>

#define XB_UU_OK        0
#define XB_UU_EHOST    -1   /* the installer stopped answering */
#define XB_UU_EUFW     -2   /* UFW head or entry list broken */
#define XB_UU_EPKG     -3   /* no flash.bin or no ota.bin */
#define XB_UU_EBOARD   -4   /* flash.bin is not this board's size */
#define XB_UU_ELOADER  -5   /* ota.bin is not the loader Xebec carries */
#define XB_UU_EHEAD    -6   /* the package's flash head differs from this FM-1's */
#define XB_UU_EFLASH   -7   /* this FM-1's flash head could not be read */
#define XB_UU_ECONFIRM -8   /* the installer did not confirm */

#define XB_UU_FLASH_LEN 0x93000u
#define XB_UU_HEAD_LEN  0x4000u

/* Run the checks above. `idle` keeps USB going while waiting (poll the
 * controller, take SysEx); it must not spin on a peripheral register. */
int xb_update_usb_check(void (*idle)(void));

/* Read requests made by the last check (the native test counts them). */
extern uint32_t xb_update_usb_reads;

#endif
