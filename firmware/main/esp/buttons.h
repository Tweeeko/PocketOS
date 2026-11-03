#pragma once
// Side buttons (documented in docs/controls.md):
//   BOOT  click        open/close the Settings QR code, or confirm a reset requested from the website
//   BOOT  double-click go home (the companion)
//   BOOT  hold         talk to Home Assistant while held (push-to-talk)
//   PWR   click        screen off / on
//   PWR   long press   reported by the power chip; holding longer switches the board off
typedef enum { BTN_BOOT_CLICK, BTN_BOOT_DOUBLE, BTN_BOOT_HOLD_START, BTN_BOOT_HOLD_END, BTN_PWR_SHORT, BTN_PWR_LONG } button_event_t;
typedef void (*button_handler_t)(button_event_t ev);
void buttons_start(button_handler_t handler);
