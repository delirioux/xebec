/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - flash driver (the FM-1's 1 MiB SPI NOR).
 *
 * Erase and program while Xebec itself runs from that same flash: every
 * instruction that runs with the SFC switched off lives in RAM (section
 * .xb_ram_code, copied there at start-up), and tools/check_ram_code.py
 * checks on every build that it never reaches back into flash.
 *
 * Safety:
 *  - erase and program only inside the write window, checked in RAM right
 *    before the flash is touched: flash 0xE0000 up to XB_FLASH_SAFE_HI, the
 *    update staging area and nothing else (ota_stage.h). Below it are the
 *    boot loader, the head and Xebec's package (0 to 0x93000) and free
 *    space; above it the official firmware's areas (BTIF 0xE9000, user
 *    patches 0xEA000, settings, key_mac 0xFF000). It is the same 20 KiB
 *    where Felucca stages the update loader.
 *  - only on a known flash chip (JEDEC ID checked before every erase and
 *    program); write enable is checked before every command and dropped
 *    again after an error.
 *  - every erase is read back blank and every write read back and
 *    compared before the call returns OK.
 *  - buffers must lie in RAM: flash is not readable while the SFC is off.
 *
 * Interrupts are switched off for each call into RAM. Xebec does not
 * switch them on yet; once it does, these calls must restore them.
 */
#ifndef XB_FLASH_H
#define XB_FLASH_H

#include <stdint.h>

#define XB_FLASH_SIZE     0x100000u
#define XB_FLASH_SECTOR   0x1000u
#define XB_FLASH_PAGE     0x100u
#define XB_FLASH_SAFE_HI  0xE5000u

/* The two 1 MiB parts found in FM-1 units: Puya (85 60 14) and, in newer
 * units, UC25HQ80 (B3 60 14). */
#define XB_FLASH_KNOWN(id) ((id) == 0x856014u || (id) == 0xB36014u)

#define XB_FLASH_OK        0
#define XB_FLASH_ERANGE   -1   /* outside the write window, misaligned, too long */
#define XB_FLASH_EBUF     -2   /* buffer not in RAM */
#define XB_FLASH_EMODE    -3   /* SFC busy, in continuous read, or cache not idle */
#define XB_FLASH_EID      -4   /* unknown flash chip */
#define XB_FLASH_EWEL     -5   /* write enable did not set */
#define XB_FLASH_ETIMEOUT -6   /* the chip stayed busy */
#define XB_FLASH_ESPI     -7   /* SPI0 did not finish a byte */
#define XB_FLASH_EVERIFY  -8   /* read back differs */

/* Start of the write window (a flash offset, from xebec.ld). */
uint32_t xb_flash_safe_lo(void);

/* JEDEC ID, e.g. 0x856014. */
int xb_flash_id(uint32_t *id);

/* Erase the 4 KiB sector at `off` (sector aligned, inside the window),
 * then check that it reads back blank. */
int xb_flash_erase_4k(uint32_t off);

/* Program `n` bytes from `src` (in RAM) at `off` (inside the window, on
 * erased flash), page by page, each page read back and compared. */
int xb_flash_write(uint32_t off, const void *src, uint32_t n);

/* Read `n` bytes at `off` into `dst` (in RAM), straight from the chip. */
int xb_flash_read(uint32_t off, void *dst, uint32_t n);

/* Calls made into the RAM code so far (the flash test counts them). */
extern uint32_t xb_flash_ram_calls;

#endif
