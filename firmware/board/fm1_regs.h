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

/* Code copied to RAM at start-up (checked by tools/check_ram_code.py). */
#define XB_RAM_CODE __attribute__((section(".xb_ram_code"), noinline, used))

/* TIMER4: free-running from the 24 MHz crystal (measured on an FM-1 by
 * Felucca). CON bits 1:0 = 1 run, 3:2 = 2 crystal, bit 14 clears pending. */
#define XB_T4_CON         XB_REG32(0x10800u)
#define XB_T4_CNT         XB_REG32(0x10804u)
#define XB_T4_PRD         XB_REG32(0x10808u)
#define XB_T4_RUN_XTAL    ((2u << 2) | 1u)
#define XB_T4_HZ          24000000u

/* GPIO ports A..H, 0x40 apart: OUT +0x00, IN +0x04, DIR +0x08 (1 = input),
 * DIE +0x0C (digital input on), PU +0x10 / PD +0x14 (pull-up / pull-down). */
#define XB_PORT_BASE(n)   (0x50000u + 0x40u * (n))
#define XB_PORT_A         0
#define XB_PORT_B         1
#define XB_PORT_C         2
#define XB_PORT_OUT(n)    XB_REG32(XB_PORT_BASE(n) + 0x00u)
#define XB_PORT_IN(n)     XB_REG32(XB_PORT_BASE(n) + 0x04u)
#define XB_PORT_DIR(n)    XB_REG32(XB_PORT_BASE(n) + 0x08u)
#define XB_PORT_DIE(n)    XB_REG32(XB_PORT_BASE(n) + 0x0Cu)
#define XB_PORT_PU(n)     XB_REG32(XB_PORT_BASE(n) + 0x10u)
#define XB_PORT_PD(n)     XB_REG32(XB_PORT_BASE(n) + 0x14u)

/* SPI1, the display bus: CON, BAUD, BUF (one byte), DMA address, DMA count. */
#define XB_SPI1_CON       XB_REG32(0x11D00u)
#define XB_SPI1_BAUD      XB_REG32(0x11D04u)
#define XB_SPI1_BUF       XB_REG32(0x11D08u)
#define XB_SPI1_ADR       XB_REG32(0x11D0Cu)
#define XB_SPI1_CNT       XB_REG32(0x11D10u)
#define XB_SPI_CON_EN     (1u << 0)
#define XB_SPI_CON_MASTER (1u << 5)
#define XB_SPI_CON_PCLR   (1u << 14)   /* write 1: clear the done flag */
#define XB_SPI_CON_PND    (1u << 15)   /* read: transfer done */

/* Display: ST7789V, 240 x 240 RGB565, on SPI1. */
#define XB_LCD_CS_PIN     7u   /* PC7, active low */
#define XB_LCD_DC_PIN     8u   /* PC8, low = command, high = data */
#define XB_LCD_SCK_PIN    9u   /* PC9 */
#define XB_LCD_MOSI_PIN   10u  /* PC10 */
#define XB_LCD_BL_PIN     2u   /* PA2, backlight, active low */
#define XB_IOMAP_CON1     XB_REG32(0x51020u)
#define XB_IOMAP_SPI1_LCD (1u << 4)   /* SPI1 drives PC9 (clock) and PC10 (data) */

/* Key matrix: 11 columns from two chained 74HC595s (PA4 SER, PA3 SRCLK,
 * PA1 RCLK; a low output selects its column), 6 rows PA0, PA5..PA8, PB7
 * with pull-ups (low = closed). Same wiring in M-VAVE's fm1_input.h,
 * Felucca's input code and the fm1-emu panel model. */
#define XB_SR_SER_PIN     4u   /* PA4 */
#define XB_SR_CLK_PIN     3u   /* PA3 */
#define XB_SR_LATCH_PIN   1u   /* PA1 */

/* Chip reset (the update path): PWR_CON bit 4, as M-VAVE's update code does. */
#define XB_PWR_CON        XB_REG32(0x10000u)
#define XB_PWR_CON_RESET  (1u << 4)

/* The update record the boot loader reads after a reset (112 bytes). */
#define XB_UPDATE_RECORD_RAM 0x01C7FD88u

/* USB0: a Mentor MUSB full-speed core behind JieLi's window (Felucca's
 * fm1_usb.h and fm1-emu's model). CON1 reaches the core's own registers
 * one byte at a time; endpoint packets move by DMA to and from RAM. */
#define XB_USB_CON0       XB_REG32(0x11800u)
#define XB_USB_CON1       XB_REG32(0x11804u)
#define XB_USB_EP_CNT(n)  XB_REG32(0x11808u + 4u * (n))           /* EP0..3 */
#define XB_USB_EP0_ADR    XB_REG32(0x11818u)
#define XB_USB_EP_TADR(n) XB_REG32(0x1181Cu + 8u * ((n) - 1u))    /* EP1..3 */
#define XB_USB_EP_RADR(n) XB_REG32(0x11820u + 8u * ((n) - 1u))    /* EP1..3 */
#define XB_USB_EP4_RADR   XB_REG32(0x1183Cu)
#define XB_USB_IO_CON0    XB_REG32(0x51000u)                      /* bit 6: D+ pull-up */
#define XB_CLK_CON1       XB_REG32(0x10010u)                      /* bits 1:0 = 0: USB on PLL48M */

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
