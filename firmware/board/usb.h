/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 delirioux
 * Xebec - USB device: one USB-MIDI port ("FM-1 Xebec"), polled.
 */
#ifndef XB_USB_H
#define XB_USB_H

#include <stdint.h>

/* Attach to the bus. 0 = the controller answered, < 0 = it did not (no cable). */
int xb_usb_start(void);

/* Service the controller; call often from the main loop. */
void xb_usb_poll(void);

/* 1 while the controller answers (a cable is in). */
int xb_usb_up(void);

/* 1 once the computer has configured the device. */
int xb_usb_configured(void);

/* Bulk transfers on EP1 so far (diagnostics). */
extern uint32_t xb_usb_rx_count, xb_usb_tx_count, xb_usb_tx_leftovers;
/* SIE access timeouts, controller starts, bus resets from the host, all since power-on. */
extern uint32_t xb_usb_sie_timeouts, xb_usb_starts, xb_usb_bus_resets;

/* USB-MIDI event packets (4 bytes, cable 0) to and from the computer. */
int xb_usb_midi_send(uint32_t pkt);           /* 0 = queued, < 0 = queue full */
int xb_usb_midi_take(uint32_t *pkt);          /* 1 = got one */

#endif
