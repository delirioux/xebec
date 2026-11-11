/*
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

#endif
