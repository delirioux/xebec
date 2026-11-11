/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - stage M-VAVE's OTA loader in flash.
 *
 * Xebec carries M-VAVE's update loader (vendor/m-vave/ota.bin, Apache-2.0,
 * unchanged). To update, it is written into free flash, where M-VAVE's
 * boot loader picks it up after the restart and runs it from RAM.
 *
 * The staging area, 20 KiB (five 4 KiB sectors):
 *   area + 0x0000   32-byte file head "LOADER.BIN", written last
 *   area + 0x0020   the loader (ota.bin without its own 32-byte head)
 *   area + 0x4F00   kept free for the update record's flash copy
 * The area is flash 0xE0000, the flash driver's whole write window: past
 * the update package (0 to 0x93000) on a 128 KiB boundary, as the official
 * firmware places its loader, and the very place Felucca stages the same
 * loader. Place, layout, head and record are byte for byte what Felucca
 * writes, proven on FM-1 units.
 */
#ifndef XB_OTA_STAGE_H
#define XB_OTA_STAGE_H

#include <stdint.h>

#define XB_OTA_AREA_LEN    0x5000u
#define XB_OTA_RECORD_OFF  0x4F00u   /* the record's flash copy, inside the area */
#define XB_OTA_HEAD_LEN    32u

#define XB_OTA_ELOADER    -20        /* the carried loader failed its own checks */

/* M-VAVE's ota.bin, generated from vendor/m-vave/ota.bin (tools/gen_loader.py). */
extern const uint32_t xb_ota_loader_len;
extern const uint8_t xb_ota_loader[];

/* Flash offset of the staging area. */
uint32_t xb_ota_area(void);

/* CRC-16 (polynomial 0x1021) as JieLi's file heads use it; start with c = 0. */
uint32_t xb_ota_crc16(const uint8_t *p, uint32_t n, uint32_t c);

/* Erase the area, write the loader and read it back (CRC), then the head,
 * read back too. On any failure the area is erased again. 0 = staged. */
int xb_ota_stage(void);

/* Erase the whole area. 0 = done. */
int xb_ota_unstage(void);

#endif
