/*
 * Xebec - coarse busy-wait delays.
 */
#ifndef XB_DELAY_H
#define XB_DELAY_H

/* Waits at least about `ms` milliseconds. A plain loop sized for the
 * fastest CPU clock the FM-1 runs at, so it errs on the long side.
 * Good enough for start-up waits; real timing comes with the timers. */
void xb_delay_ms(unsigned ms);

#endif
