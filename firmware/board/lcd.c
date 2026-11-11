/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - display driver (ST7789V on SPI1).
 *
 * Commands go out one byte at a time with D/C low; pixel data goes out
 * by DMA with D/C high. Pixels travel big-endian (high byte first).
 *
 * Status: runs in fm1-emu. Not yet checked on a real FM-1: before the
 * first hardware build, confirm the SPI1 pin routing to PC9/PC10, the
 * SPI1 control bits and the panel timing on the bench.
 */
#include "lcd.h"
#include "fm1_regs.h"
#include "../src/delay.h"

#define CS_BIT (1u << XB_LCD_CS_PIN)
#define DC_BIT (1u << XB_LCD_DC_PIN)

/* One row of pixels, already in wire order, used as the DMA source. */
static uint16_t row_buf[XB_LCD_W];

static void spi_wait(void)
{
    uint32_t guard = 4000000u;

    while (!(XB_SPI1_CON & XB_SPI_CON_PND) && --guard)
        ;
    XB_SPI1_CON |= XB_SPI_CON_PCLR;
}

static void lcd_cmd(uint8_t cmd)
{
    XB_PORT_OUT(XB_PORT_C) &= ~DC_BIT;
    XB_PORT_OUT(XB_PORT_C) &= ~CS_BIT;
    XB_SPI1_CON |= XB_SPI_CON_PCLR;
    XB_SPI1_BUF = cmd;
    spi_wait();
    XB_PORT_OUT(XB_PORT_C) |= CS_BIT;
}

static void lcd_data(const void *buf, uint32_t bytes)
{
    XB_PORT_OUT(XB_PORT_C) |= DC_BIT;
    XB_PORT_OUT(XB_PORT_C) &= ~CS_BIT;
    XB_SPI1_CON |= XB_SPI_CON_PCLR;
    XB_SPI1_ADR = (uint32_t)(uintptr_t)buf;
    XB_SPI1_CNT = bytes;
    spi_wait();
    XB_PORT_OUT(XB_PORT_C) |= CS_BIT;
}

static void lcd_cmd_arg(uint8_t cmd, uint8_t arg)
{
    static uint8_t a;

    a = arg;
    lcd_cmd(cmd);
    lcd_data(&a, 1);
}

static void lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
    static uint8_t a[4];

    a[0] = (uint8_t)(x0 >> 8); a[1] = (uint8_t)x0;
    a[2] = (uint8_t)(x1 >> 8); a[3] = (uint8_t)x1;
    lcd_cmd(0x2A);                         /* CASET: column range */
    lcd_data(a, 4);

    a[0] = (uint8_t)(y0 >> 8); a[1] = (uint8_t)y0;
    a[2] = (uint8_t)(y1 >> 8); a[3] = (uint8_t)y1;
    lcd_cmd(0x2B);                         /* RASET: row range */
    lcd_data(a, 4);

    lcd_cmd(0x2C);                         /* RAMWR: pixels follow */
}

void xb_lcd_init(void)
{
    /* Display lines as outputs, chip select idle high. */
    XB_PORT_OUT(XB_PORT_C) |= CS_BIT;
    XB_PORT_DIR(XB_PORT_C) &= ~((1u << XB_LCD_CS_PIN) | (1u << XB_LCD_DC_PIN) |
                                (1u << XB_LCD_SCK_PIN) | (1u << XB_LCD_MOSI_PIN));

    /* SPI1 at about 12 MHz. */
    XB_SPI1_BAUD = 4;
    XB_SPI1_CON |= XB_SPI_CON_EN;

    lcd_cmd(0x01);                         /* SWRESET */
    xb_delay_ms(150);
    lcd_cmd(0x11);                         /* SLPOUT */
    xb_delay_ms(120);
    lcd_cmd_arg(0x3A, 0x55);               /* COLMOD: 16-bit RGB565 */
    lcd_cmd_arg(0x36, 0x00);               /* MADCTL: top-left origin, RGB */
    lcd_cmd(0x21);                         /* INVON: the IPS panel's normal mode */
    lcd_cmd(0x13);                         /* NORON */
    lcd_cmd(0x29);                         /* DISPON */

    /* Backlight on (active low). */
    XB_PORT_OUT(XB_PORT_A) &= ~(1u << XB_LCD_BL_PIN);
    XB_PORT_DIR(XB_PORT_A) &= ~(1u << XB_LCD_BL_PIN);
}

void xb_lcd_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t rgb565)
{
    uint16_t wire = (uint16_t)((rgb565 >> 8) | (rgb565 << 8));
    unsigned i;

    if (x >= XB_LCD_W || y >= XB_LCD_H || w == 0 || h == 0)
        return;
    if (w > XB_LCD_W - x)
        w = XB_LCD_W - x;
    if (h > XB_LCD_H - y)
        h = XB_LCD_H - y;

    for (i = 0; i < w; i++)
        row_buf[i] = wire;

    lcd_window(x, y, x + w - 1, y + h - 1);
    for (i = 0; i < h; i++)
        lcd_data(row_buf, w * 2u);
}
