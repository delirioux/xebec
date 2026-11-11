/*
 * Xebec - C runtime start-up: copy initialised data to RAM, clear .bss.
 * Runs before anything else, with no .data or .bss available yet.
 */
#include <stdint.h>

extern uint32_t __xb_data_start[], __xb_data_end[], __xb_data_load[];
extern uint32_t __xb_bss_start[], __xb_bss_end[];

void xb_crt_init(void)
{
    volatile uint32_t *dst = __xb_data_start;
    const volatile uint32_t *src = __xb_data_load;

    while (dst < __xb_data_end)
        *dst++ = *src++;

    for (dst = __xb_bss_start; dst < __xb_bss_end; dst++)
        *dst = 0;
}
