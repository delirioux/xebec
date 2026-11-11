/*
 * Xebec - memory layout.
 *
 * Code and constants run in place from flash (XIP), entered at
 * 0x02000120. Initialised data is copied to RAM at start-up and .bss is
 * cleared there (crt.c). The stacks sit at the top of our RAM window.
 */
MEMORY
{
    XIP (rx)  : ORIGIN = 0x02000120, LENGTH = 0x00080000
    RAM (rwx) : ORIGIN = 0x01C00000, LENGTH = 0x00060000
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
        *(.data*)
        *(.sdata*)
        . = ALIGN(4);
        __xb_data_end = .;
    } > RAM AT > XIP
    __xb_data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        __xb_bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __xb_bss_end = .;
    } > RAM

    /* Stacks: 8 KiB main stack, 4 KiB system (exception) stack above it. */
    __xb_stack_top     = ORIGIN(RAM) + LENGTH(RAM) - 0x1000;
    __xb_sys_stack_top = ORIGIN(RAM) + LENGTH(RAM);
    __xb_stack_limit   = __xb_stack_top - 0x2000;

    ASSERT(__xb_bss_end <= __xb_stack_limit, "Xebec: RAM full, .data + .bss run into the stack")

    /DISCARD/ : { *(.comment) *(.note*) }
}
