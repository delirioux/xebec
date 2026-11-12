/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - memory layout.
 *
 * Code and constants run in place from flash (XIP), entered at
 * 0x02000120. Initialised data is copied to RAM at start-up and .bss is
 * cleared there (crt.c). The stacks sit at the top of our RAM window.
 *
 * The flash driver's RAM code (.xb_ram_code) rides at the start of .data:
 * the same start-up copy puts it in RAM. It must run from RAM, because
 * the flash is off the bus while it erases or programs.
 *
 * Flash offsets: XIP 0x02000000 shows flash offset 0x4000.
 *
 * RAM starts at 0x01C08000. Up to 0.0.17 it started at 0x01C00000, and on an
 * FM-1 about half of the USB replies came out as babble with code that is
 * register for register Felucca's. Every firmware known to work on the FM-1
 * keeps its data above 0x01C08000 (Felucca's app; Felucca's and M-VAVE's
 * update loaders at 0x01C0A800).
 */
MEMORY
{
    XIP (rx)  : ORIGIN = 0x02000120, LENGTH = 0x00080000
    RAM (rwx) : ORIGIN = 0x01C08000, LENGTH = 0x00058000   /* as Felucca: no data below 0x01C08000 */
}

ENTRY(xb_start)

SECTIONS
{
    .text :
    {
        KEEP(*(.text.start))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > XIP

    .data :
    {
        . = ALIGN(4);
        __xb_data_start = .;
        __xb_ram_code_start = .;
        KEEP(*(.xb_ram_code*))
        . = ALIGN(4);
        __xb_ram_code_end = .;
        *(.data*)
        *(.sdata*)
        . = ALIGN(4);
        __xb_data_end = .;
    } > RAM AT > XIP
    __xb_data_load = LOADADDR(.data);
    __xb_ram_code_load = LOADADDR(.data);

    /* The image must fit the app slot of the update package (tools/package.py:
     * 0x8DFBC bytes from XIP 0x02000120, as in M-VAVE's stock layout). */
    __xb_image_end = LOADADDR(.data) + SIZEOF(.data);
    ASSERT(__xb_image_end <= 0x02000120 + 0x8DFBC, "Xebec: the image does not fit the package's app slot")

    /* The flash write window starts here (a flash offset; board/flash.h):
     * the update staging area, the same as Felucca's. */
    __xb_flash_safe_lo = 0xE0000;

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        __xb_bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(64);
        __xb_usb_dma_start = .;
        *(.xb_usb_dma)
        . = ALIGN(4);
        __xb_bss_end = .;
    } > RAM

    /* Stacks: 8 KiB main stack, 4 KiB system (exception) stack above it. */
    __xb_stack_top     = ORIGIN(RAM) + LENGTH(RAM) - 0x1000;
    __xb_sys_stack_top = ORIGIN(RAM) + LENGTH(RAM);
    __xb_stack_limit   = __xb_stack_top - 0x2000;

    /* Xebec's RAM, for the flash driver's buffer checks. */
    __xb_ram_lo = ORIGIN(RAM);
    __xb_ram_hi = ORIGIN(RAM) + LENGTH(RAM);

    ASSERT(__xb_bss_end <= __xb_stack_limit, "Xebec: RAM full, .data + .bss run into the stack")

    /DISCARD/ : { *(.comment) *(.note*) }
}
