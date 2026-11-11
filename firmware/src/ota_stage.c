/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - stage M-VAVE's OTA loader in flash (see ota_stage.h).
 */
#include "ota_stage.h"
#include "../board/flash.h"

/* RAM copies: the flash driver only writes from RAM. */
static uint8_t page_buf[XB_FLASH_PAGE];
static uint8_t head_buf[XB_OTA_HEAD_LEN];

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }
static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v); wr16(p + 2, v >> 16); }

uint32_t xb_ota_crc16(const uint8_t *p, uint32_t n, uint32_t c)
{
    uint32_t i;

    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (i = 0; i < 8u; i++)
            c = (c & 0x8000u) ? (c << 1) ^ 0x1021u : c << 1;
        c &= 0xFFFFu;
    }
    return c;
}

uint32_t xb_ota_area(void)
{
    return xb_flash_safe_lo();
}

/* The carried loader, checked again here (the build checked it too):
 * head CRC, layout, body CRC, and that it fits below the record slot. */
static int loader_ok(uint32_t *body_len, uint32_t *body_crc)
{
    const uint8_t *h = xb_ota_loader;
    uint32_t len = rd32(h + 8);

    if (xb_ota_loader_len < XB_OTA_HEAD_LEN || rd32(h + 4) != XB_OTA_HEAD_LEN || h[12] != 0x41u ||
        len != xb_ota_loader_len - XB_OTA_HEAD_LEN || len > XB_OTA_RECORD_OFF - XB_OTA_HEAD_LEN ||
        xb_ota_crc16(h + 2, XB_OTA_HEAD_LEN - 2u, 0) != rd16(h) ||
        xb_ota_crc16(h + XB_OTA_HEAD_LEN, len, 0) != rd16(h + 2))
        return 0;
    *body_len = len;
    *body_crc = rd16(h + 2);
    return 1;
}

/* The staged file head: name LOADER.BIN, where it sits, its length with the
 * head, attribute 0x41, its own CRC in front (the data CRC field left 0xFFFF). */
static void make_head(uint8_t *h, uint32_t area, uint32_t body_len)
{
    static const char name[] = "LOADER.BIN";
    uint32_t i;

    for (i = 0; i < XB_OTA_HEAD_LEN; i++)
        h[i] = 0;
    wr16(h + 2, 0xFFFFu);
    wr32(h + 4, area);
    wr32(h + 8, body_len + XB_OTA_HEAD_LEN);
    h[12] = 0x41u;
    h[13] = 0xFFu;
    wr16(h + 14, 0xFFFFu);
    for (i = 0; i + 1u < sizeof name; i++)
        h[16 + i] = (uint8_t)name[i];
    wr16(h, xb_ota_crc16(h + 2, XB_OTA_HEAD_LEN - 2u, 0));
}

int xb_ota_unstage(void)
{
    uint32_t area = xb_ota_area(), i;
    int rc, first = 0;

    for (i = 0; i < XB_OTA_AREA_LEN; i += XB_FLASH_SECTOR) {
        rc = xb_flash_erase_4k(area + i);
        if (rc && !first)
            first = rc;
    }
    return first;
}

static int stage(uint32_t area, uint32_t len, uint32_t want_crc)
{
    uint32_t off, end = XB_OTA_HEAD_LEN + len, crc = 0, i;
    int rc;

    for (off = 0; off < XB_OTA_AREA_LEN; off += XB_FLASH_SECTOR) {
        rc = xb_flash_erase_4k(area + off);
        if (rc)
            return rc;
    }

    /* The loader, page by page (each page is read back by the driver). */
    for (off = XB_OTA_HEAD_LEN; off < end;) {
        uint32_t chunk = XB_FLASH_PAGE - ((area + off) & (XB_FLASH_PAGE - 1u));

        if (chunk > end - off)
            chunk = end - off;
        for (i = 0; i < chunk; i++)
            page_buf[i] = xb_ota_loader[off + i];
        rc = xb_flash_write(area + off, page_buf, chunk);
        if (rc)
            return rc;
        off += chunk;
    }

    /* The whole loader read back once more, against its own CRC. */
    for (off = XB_OTA_HEAD_LEN; off < end;) {
        uint32_t chunk = end - off > XB_FLASH_PAGE ? XB_FLASH_PAGE : end - off;

        rc = xb_flash_read(area + off, page_buf, chunk);
        if (rc)
            return rc;
        crc = xb_ota_crc16(page_buf, chunk, crc);
        off += chunk;
    }
    if (crc != want_crc)
        return XB_FLASH_EVERIFY;

    /* The head last: until it is there, the area holds no loader file. */
    make_head(head_buf, area, len);
    return xb_flash_write(area, head_buf, XB_OTA_HEAD_LEN);
}

int xb_ota_stage(void)
{
    uint32_t len, crc;
    int rc;

    if (!loader_ok(&len, &crc))
        return XB_OTA_ELOADER;
    rc = stage(xb_ota_area(), len, crc);
    if (rc)
        xb_ota_unstage();
    return rc;
}
