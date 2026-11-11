/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - FM-1 (JieLi AC791N / WL82) registers and board wiring.
 *
 * Addresses and pins are hardware facts collected from public sources:
 * the AC79 register map, AL-255's FM-1-RE notes, ip2k's lunar-modulator
 * hardware notes and the fm1-emu chip model. Every value here is written
 * fresh for Xebec; no other firmware's code is used.
 */
#ifndef XB_FM1_REGS_H
#define XB_FM1_REGS_H

#include <stdint.h>

#define XB_REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

/* GPIO ports A..H, 0x40 apart: OUT +0x00, DIR +0x08 (1 = input). */
#define XB_PORT_BASE(n)   (0x50000u + 0x40u * (n))
#define XB_PORT_A         0
#define XB_PORT_C         2
#define XB_PORT_OUT(n)    XB_REG32(XB_PORT_BASE(n) + 0x00u)
#define XB_PORT_DIR(n)    XB_REG32(XB_PORT_BASE(n) + 0x08u)

/* SPI1, the display bus: CON, BAUD, BUF (one byte), DMA address, DMA count. */
#define XB_SPI1_CON       XB_REG32(0x11D00u)
#define XB_SPI1_BAUD      XB_REG32(0x11D04u)
#define XB_SPI1_BUF       XB_REG32(0x11D08u)
#define XB_SPI1_ADR       XB_REG32(0x11D0Cu)
#define XB_SPI1_CNT       XB_REG32(0x11D10u)
#define XB_SPI_CON_EN     (1u << 0)
#define XB_SPI_CON_PCLR   (1u << 14)   /* write 1: clear the done flag */
#define XB_SPI_CON_PND    (1u << 15)   /* read: transfer done */

/* Display: ST7789V, 240 x 240 RGB565, on SPI1. */
#define XB_LCD_CS_PIN     7u   /* PC7, active low */
#define XB_LCD_DC_PIN     8u   /* PC8, low = command, high = data */
#define XB_LCD_SCK_PIN    9u   /* PC9 */
#define XB_LCD_MOSI_PIN   10u  /* PC10 */
#define XB_LCD_BL_PIN     2u   /* PA2, backlight, active low */

/* The external NOR flash (1 MiB). Normally the SFC owns it and maps it at
 * XIP 0x02000000 (flash offset 0x4000 and up); to erase or program, the SFC
 * is switched off, the pins go back to SPI0 and chip select to PD0 as a
 * GPIO. Register use as in JieLi's SDK flash routines (enter_spi_code,
 * exit_spi_code, spi_writebyte, spi_readbyte), matched against the fm1-emu
 * flash model. */
#define XB_PORT_D         3
#define XB_FLASH_CS_PIN   0u           /* PD0, active low */

#define XB_SFC_CON        XB_REG32(0x40200u)
#define XB_SFC_CON_BUSY   (1u << 31)
#define XB_SFC_MODE(con)  (((con) >> 8) & 0xFu)   /* 6, 7: continuous read */

#define XB_SPI0_CON       XB_REG32(0x11C00u)
#define XB_SPI0_BUF       XB_REG32(0x11C08u)
#define XB_SPI0_CON_ON    (1u << 3)    /* set around each byte */
#define XB_SPI0_CON_RX    (1u << 12)   /* 1 = receive */
#define XB_SPI0_CON_PCLR  (1u << 14)   /* write 1: clear the done flag */
#define XB_SPI0_CON_PND   (1u << 15)   /* read: byte done */
#define XB_SPI0_CON_FLASH 0x1029u      /* the SDK's default set-up for the flash */

#define XB_IOMAP_CON0     XB_REG32(0x5101Cu)
#define XB_IOMAP_SFC_PINS (1u << 5)    /* 1 = the SFC owns the flash pins */

#define XB_CACHE_CON      XB_REG32(0x1EEE008u)
#define XB_CACHE_IDLE     (1u << 14)

/* XIP address of a flash offset (offsets below 0x4000 are not mapped). */
#define XB_FLASH_XIP(off) (0x02000000u + (uint32_t)(off) - 0x4000u)

#endif
