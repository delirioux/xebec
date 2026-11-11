/*
 * Xebec - coarse busy-wait delays.
 */
#include "delay.h"

/* Loop passes per millisecond at up to 360 MHz, a few cycles a pass. */
#define XB_LOOPS_PER_MS 120000u

void xb_delay_ms(unsigned ms)
{
    volatile unsigned n;

    while (ms--)
        for (n = 0; n < XB_LOOPS_PER_MS; n++)
            ;
}
