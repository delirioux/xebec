/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - flash driver (the FM-1's 1 MiB SPI NOR), see flash.h.
 *
 * Two halves:
 *  - RAM half (XB_RAM_CODE): runs with the SFC off. Touches only
 *    registers, the stack and RAM; no constants from flash, no calls out
 *    of the RAM code, no switch tables, no library calls. Checked on
 *    every build by tools/check_ram_code.py.
 *  - Flash half (normal code): splits work into chunks, calls into RAM
 *    with interrupts off, and reads every result back.
 *
 * Sequence as in JieLi's SDK (enter_spi_code, norflash_erase,
 * _norflash_write, norflash_wait_ok, exit_spi_code): wait for the cache
 * and the SFC to go idle, save SFC and SPI0 set-up, SFC off, pins to SPI0,
 * the command, wait for the chip, drop the cached lines of the range,
 * restore SPI0, pins back to the SFC, SFC back on.
 */
#include "flash.h"
#include "fm1_regs.h"

#define XB_RAM_CODE __attribute__((section(".xb_ram_code"), noinline, used))

/* From xebec.ld: the write window start (a flash offset) and Xebec's RAM. */
extern const char __xb_flash_safe_lo[];
extern const char __xb_ram_lo[], __xb_ram_hi[];

#define CS_BIT        (1u << XB_FLASH_CS_PIN)
#define SPI_WAIT_MAX  50000000u   /* polls for one SPI byte (the SDK's count) */
#define CHIP_WAIT_MAX 5000000u    /* status reads for one erase or program (the SDK's count) */
#define IDLE_WAIT_MAX 1000000u    /* polls for the cache / the SFC to go idle */

uint32_t xb_flash_ram_calls;

/* Set by a byte transfer that never finished; checked at the end of a command. */
static volatile uint32_t spi_err;

typedef struct {
    uint32_t sfc_con;
    uint32_t spi_con;
} saved_t;

/* ---- RAM half ------------------------------------------------------- */

XB_RAM_CODE static void ram_csync(void)
{
    __asm__ volatile("csync" ::: "memory");
}

XB_RAM_CODE static void ram_delay(uint32_t n)
{
    volatile uint32_t i = n;

    while (i--)
        ;
}

XB_RAM_CODE static int ram_cache_idle(void)
{
    uint32_t n = IDLE_WAIT_MAX;

    while (!(XB_CACHE_CON & XB_CACHE_IDLE))
        if (--n == 0)
            return 0;
    return 1;
}

XB_RAM_CODE static int ram_sfc_idle(void)
{
    uint32_t n = IDLE_WAIT_MAX;

    ram_csync();
    while (XB_SFC_CON & XB_SFC_CON_BUSY)
        if (--n == 0)
            return 0;
    return 1;
}

XB_RAM_CODE static void ram_cs(int high)
{
    XB_PORT_DIR(XB_PORT_D) &= ~CS_BIT;
    if (high)
        XB_PORT_OUT(XB_PORT_D) |= CS_BIT;
    else
        XB_PORT_OUT(XB_PORT_D) &= ~CS_BIT;
    ram_delay(10);
}

XB_RAM_CODE static void ram_spi_wait(void)
{
    uint32_t n = SPI_WAIT_MAX;

    ram_csync();
    while (!(XB_SPI0_CON & XB_SPI0_CON_PND))
        if (--n == 0) {
            spi_err = 1;
            break;
        }
    XB_SPI0_CON |= XB_SPI0_CON_PCLR;
}

XB_RAM_CODE static void ram_tx(uint32_t b)
{
    XB_SPI0_CON |= XB_SPI0_CON_ON;
    XB_SPI0_CON &= ~XB_SPI0_CON_RX;
    XB_SPI0_BUF = b & 0xFFu;
    ram_spi_wait();
    XB_SPI0_CON &= ~XB_SPI0_CON_ON;
}

XB_RAM_CODE static uint32_t ram_rx(void)
{
    uint32_t v;

    XB_SPI0_CON |= XB_SPI0_CON_ON;
    XB_SPI0_CON |= XB_SPI0_CON_RX;
    XB_SPI0_BUF = 0xFFu;
    ram_spi_wait();
    v = XB_SPI0_BUF & 0xFFu;
    XB_SPI0_CON &= ~XB_SPI0_CON_ON;
    return v;
}

XB_RAM_CODE static void ram_addr(uint32_t off)
{
    ram_tx(off >> 16);
    ram_tx(off >> 8);
    ram_tx(off);
}

XB_RAM_CODE static uint32_t ram_status(void)
{
    uint32_t v;

    ram_cs(0);
    ram_tx(0x05);              /* RDSR: status register 1 */
    v = ram_rx();
    ram_cs(1);
    return v;
}

XB_RAM_CODE static uint32_t ram_jedec(void)
{
    uint32_t id;

    ram_cs(0);
    ram_tx(0x9F);              /* RDID */
    id = ram_rx() << 16;
    id |= ram_rx() << 8;
    id |= ram_rx();
    ram_cs(1);
    return id;
}

/* Wait until the chip is no longer busy (status bit 0, WIP). */
XB_RAM_CODE static int ram_wait_ready(void)
{
    uint32_t n = CHIP_WAIT_MAX;

    while (ram_status() & 1u)
        if (--n == 0)
            return XB_FLASH_ETIMEOUT;
    return XB_FLASH_OK;
}

XB_RAM_CODE static int ram_write_enable(void)
{
    ram_cs(0);
    ram_tx(0x06);              /* WREN */
    ram_cs(1);
    return (ram_status() & 2u) ? XB_FLASH_OK : XB_FLASH_EWEL;   /* status bit 1, WEL */
}

XB_RAM_CODE static void ram_write_disable(void)
{
    ram_cs(0);
    ram_tx(0x04);              /* WRDI */
    ram_cs(1);
}

/* [off, off + n) inside the write window, without wrapping. */
XB_RAM_CODE static int ram_in_window(uint32_t off, uint32_t n)
{
    uint32_t lo = (uint32_t)(uintptr_t)__xb_flash_safe_lo;

    return off >= lo && off <= XB_FLASH_SAFE_HI && n <= XB_FLASH_SAFE_HI - off;
}

/* [p, p + n) inside Xebec's RAM, without wrapping. */
XB_RAM_CODE static int ram_in_ram(const void *p, uint32_t n)
{
    uint32_t a = (uint32_t)(uintptr_t)p;
    uint32_t lo = (uint32_t)(uintptr_t)__xb_ram_lo;
    uint32_t hi = (uint32_t)(uintptr_t)__xb_ram_hi;

    return a >= lo && a <= hi && n <= hi - a;
}

/* Drop the cached XIP lines of a flash range, so XIP reads see the chip. */
XB_RAM_CODE static void ram_drop_cache(uint32_t off, uint32_t n)
{
    uint32_t a, end;

    if (off < 0x4000u)         /* not mapped at XIP */
        return;
    a = XB_FLASH_XIP(off) & ~31u;
    end = XB_FLASH_XIP(off) + n;
    ram_cache_idle();
    for (; a < end; a += 32u) {
        ram_csync();
        __asm__ volatile("flushinv [%0]" :: "r"(a) : "memory");
        ram_csync();
        ram_cache_idle();
    }
}

/* SFC off, pins to SPI0. Refuses (and changes nothing) unless the cache
 * and the SFC are idle and the SFC is not in continuous read mode. */
XB_RAM_CODE static int ram_enter(saved_t *s)
{
    uint32_t mode;

    if (!ram_cache_idle())
        return XB_FLASH_EMODE;
    s->sfc_con = XB_SFC_CON;
    s->spi_con = XB_SPI0_CON;
    mode = XB_SFC_MODE(s->sfc_con);
    if (mode == 6u || mode == 7u)
        return XB_FLASH_EMODE;
    if (!ram_sfc_idle())
        return XB_FLASH_EMODE;

    XB_SFC_CON = 0;                            /* XIP off from here */
    XB_IOMAP_CON0 &= ~XB_IOMAP_SFC_PINS;
    XB_SPI0_CON = 0;
    XB_SPI0_CON = XB_SPI0_CON_FLASH;
    ram_csync();
    spi_err = 0;
    return XB_FLASH_OK;
}

/* SPI0 back as it was, pins back to the SFC, SFC back on. */
XB_RAM_CODE static void ram_exit(const saved_t *s)
{
    XB_SPI0_CON = s->spi_con;
    XB_IOMAP_CON0 |= XB_IOMAP_SFC_PINS;
    ram_sfc_idle();
    XB_SFC_CON = s->sfc_con;                   /* XIP on again */
    ram_csync();
}

/* Known chip and not busy: the start of every erase and program. */
XB_RAM_CODE static int ram_ready_to_write(void)
{
    uint32_t id = ram_jedec();

    if (!XB_FLASH_KNOWN(id))
        return XB_FLASH_EID;
    if (ram_wait_ready())
        return XB_FLASH_ETIMEOUT;
    return ram_write_enable();
}

XB_RAM_CODE static int ram_id(uint32_t *id)
{
    saved_t s;
    int rc;

    if (!ram_in_ram(id, 4u))
        return XB_FLASH_EBUF;
    rc = ram_enter(&s);
    if (rc)
        return rc;
    *id = ram_jedec();
    rc = spi_err ? XB_FLASH_ESPI : XB_FLASH_OK;
    ram_exit(&s);
    return rc;
}

XB_RAM_CODE static int ram_erase_4k(uint32_t off)
{
    saved_t s;
    int rc;

    if ((off & (XB_FLASH_SECTOR - 1u)) || !ram_in_window(off, XB_FLASH_SECTOR))
        return XB_FLASH_ERANGE;
    rc = ram_enter(&s);
    if (rc)
        return rc;
    rc = ram_ready_to_write();
    if (rc == XB_FLASH_OK) {
        ram_cs(0);
        ram_tx(0x20);          /* SE: 4 KiB sector erase */
        ram_addr(off);
        ram_cs(1);
        rc = ram_wait_ready();
        ram_drop_cache(off, XB_FLASH_SECTOR);
    }
    if (rc == XB_FLASH_OK && spi_err)
        rc = XB_FLASH_ESPI;
    if (rc != XB_FLASH_OK && rc != XB_FLASH_EID)
        ram_write_disable();
    ram_exit(&s);
    return rc;
}

/* One page program: 1..256 bytes, not crossing a page boundary. */
XB_RAM_CODE static int ram_program(uint32_t off, const uint8_t *src, uint32_t n)
{
    saved_t s;
    uint32_t i;
    int rc;

    if (n == 0 || n > XB_FLASH_PAGE || (off & (XB_FLASH_PAGE - 1u)) + n > XB_FLASH_PAGE ||
        !ram_in_window(off, n))
        return XB_FLASH_ERANGE;
    if (!ram_in_ram(src, n))
        return XB_FLASH_EBUF;
    rc = ram_enter(&s);
    if (rc)
        return rc;
    rc = ram_ready_to_write();
    if (rc == XB_FLASH_OK) {
        ram_cs(0);
        ram_tx(0x02);          /* PP: page program */
        ram_addr(off);
        for (i = 0; i < n; i++)
            ram_tx(src[i]);
        ram_cs(1);
        rc = ram_wait_ready();
        ram_drop_cache(off, n);
    }
    if (rc == XB_FLASH_OK && spi_err)
        rc = XB_FLASH_ESPI;
    if (rc != XB_FLASH_OK && rc != XB_FLASH_EID)
        ram_write_disable();
    ram_exit(&s);
    return rc;
}

XB_RAM_CODE static int ram_read(uint32_t off, uint8_t *dst, uint32_t n)
{
    saved_t s;
    uint32_t i;
    int rc;

    if (off > XB_FLASH_SIZE || n > XB_FLASH_SIZE - off)
        return XB_FLASH_ERANGE;
    if (!ram_in_ram(dst, n))
        return XB_FLASH_EBUF;
    rc = ram_enter(&s);
    if (rc)
        return rc;
    ram_cs(0);
    ram_tx(0x0B);              /* FAST_READ: address, one dummy byte, data */
    ram_addr(off);
    ram_tx(0xFF);
    for (i = 0; i < n; i++)
        dst[i] = (uint8_t)ram_rx();
    ram_cs(1);
    rc = spi_err ? XB_FLASH_ESPI : XB_FLASH_OK;
    ram_exit(&s);
    return rc;
}

/* ---- Flash half ----------------------------------------------------- */

/* Flash (0x02...) to RAM (0x01C...) is out of range for a direct call:
 * call through a pointer the compiler cannot fold back. */
static void *ram_entry(void *fn)
{
    void *volatile p = fn;

    xb_flash_ram_calls++;
    __asm__ volatile("cli" ::: "memory");
    return p;
}

#define RAM_CALL(fn) ((__typeof__(&fn))ram_entry((void *)&fn))

/* Read-back buffer for the checks. */
static uint8_t check_buf[XB_FLASH_PAGE];

uint32_t xb_flash_safe_lo(void)
{
    return (uint32_t)(uintptr_t)__xb_flash_safe_lo;
}

int xb_flash_id(uint32_t *id)
{
    uint32_t v = 0;
    int rc = RAM_CALL(ram_id)(&v);

    *id = v;
    return rc;
}

int xb_flash_read(uint32_t off, void *dst, uint32_t n)
{
    return RAM_CALL(ram_read)(off, (uint8_t *)dst, n);
}

int xb_flash_erase_4k(uint32_t off)
{
    uint32_t done, i;
    int rc = RAM_CALL(ram_erase_4k)(off);

    if (rc)
        return rc;
    for (done = 0; done < XB_FLASH_SECTOR; done += XB_FLASH_PAGE) {
        rc = xb_flash_read(off + done, check_buf, XB_FLASH_PAGE);
        if (rc)
            return rc;
        for (i = 0; i < XB_FLASH_PAGE; i++)
            if (check_buf[i] != 0xFFu)
                return XB_FLASH_EVERIFY;
    }
    return XB_FLASH_OK;
}

int xb_flash_write(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = (const uint8_t *)src;
    uint32_t lo = xb_flash_safe_lo();

    /* The whole range first, so a write never stops half way on the window
     * edge (each page is checked again in RAM). */
    if (off < lo || off > XB_FLASH_SAFE_HI || n > XB_FLASH_SAFE_HI - off)
        return XB_FLASH_ERANGE;
    while (n) {
        uint32_t chunk = XB_FLASH_PAGE - (off & (XB_FLASH_PAGE - 1u));
        uint32_t i;
        int rc;

        if (chunk > n)
            chunk = n;
        rc = RAM_CALL(ram_program)(off, p, chunk);
        if (rc)
            return rc;
        rc = xb_flash_read(off, check_buf, chunk);
        if (rc)
            return rc;
        for (i = 0; i < chunk; i++)
            if (check_buf[i] != p[i])
                return XB_FLASH_EVERIFY;
        off += chunk;
        p += chunk;
        n -= chunk;
    }
    return XB_FLASH_OK;
}
