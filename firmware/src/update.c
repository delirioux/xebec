/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - hand the FM-1 over to M-VAVE's OTA loader (see update.h).
 */
#include "update.h"
#include "ota_stage.h"
#include "../board/flash.h"
#include "../board/fm1_regs.h"

#define RAM_RECORD ((volatile uint8_t *)(uintptr_t)XB_UPDATE_RECORD_RAM)

static uint8_t record[XB_UPDATE_RECORD_LEN];
static uint8_t probe[8];

static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

void xb_update_record(uint8_t *rec, uint32_t area)
{
    static const char name[] = XB_UPDATE_NAME;
    uint32_t i;

    for (i = 0; i < XB_UPDATE_RECORD_LEN; i++)
        rec[i] = 0;
    wr16(rec + 2, XB_UPDATE_TYPE_USB);
    wr16(rec + 4, XB_UPDATE_STATUS);
    wr16(rec + 6, XB_UPDATE_MAGIC);
    for (i = 0; i + 1u < sizeof name; i++)
        rec[8 + i] = (uint8_t)name[i];
    wr16(rec + 72, area);
    wr16(rec + 74, area >> 16);
    wr16(rec, xb_ota_crc16(rec + 2, 78u, 0));
}

static void ram_record_clear(void)
{
    uint32_t i;

    for (i = 0; i < XB_UPDATE_RECORD_LEN; i++)
        RAM_RECORD[i] = 0;
}

int xb_update_prepare(void)
{
    uint32_t area = xb_ota_area(), i;
    int rc = xb_ota_stage();

    if (rc)
        return rc;
    xb_update_record(record, area);
    rc = xb_flash_write(area + XB_OTA_RECORD_OFF, record, XB_UPDATE_RECORD_LEN);
    if (rc) {
        xb_ota_unstage();
        return rc;
    }
    for (i = 0; i < XB_UPDATE_RECORD_LEN; i++)
        RAM_RECORD[i] = record[i];
    for (i = 0; i < XB_UPDATE_RECORD_LEN; i++)
        if (RAM_RECORD[i] != record[i]) {
            ram_record_clear();
            xb_ota_unstage();
            return XB_FLASH_EVERIFY;
        }
    return XB_FLASH_OK;
}

void xb_update_restart(void)
{
    __asm__ volatile("cli" ::: "memory");
    XB_PWR_CON |= XB_PWR_CON_RESET;
    for (;;)
        ;
}

int xb_update_cleanup(void)
{
    uint32_t area = xb_ota_area(), i, used = 0;
    int rc;

    /* A RAM record of ours: the boot loader has had it, so drop it. */
    if (RAM_RECORD[6] == (XB_UPDATE_MAGIC & 0xFFu) && RAM_RECORD[7] == (XB_UPDATE_MAGIC >> 8)) {
        ram_record_clear();
        used = 1;
    }

    /* The staging area: the head and the record slot read blank, or it goes. */
    if (xb_flash_read(area, probe, 4u) != XB_FLASH_OK)
        return -1;
    for (i = 0; i < 4u; i++)
        used |= probe[i] != 0xFFu ? 2u : 0u;
    if (xb_flash_read(area + XB_OTA_RECORD_OFF, probe, 8u) != XB_FLASH_OK)
        return -1;
    for (i = 0; i < 8u; i++)
        used |= probe[i] != 0xFFu ? 2u : 0u;
    if (used & 2u) {
        rc = xb_ota_unstage();
        if (rc)
            return rc;
    }
    return used ? 1 : 0;
}
