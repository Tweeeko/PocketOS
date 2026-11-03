// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// PocketOS on-screen interface (LVGL 9). All functions must be called with the LVGL lock held
// (on the device: bsp_display_lock), except where noted.
#include <stdbool.h>

typedef enum { VOICE_IDLE, VOICE_LISTENING, VOICE_THINKING, VOICE_HEARD, VOICE_REPLY, VOICE_ERROR } voice_state_t;

void ui_init(void);                 // builds the theme and the first screen, behind the start-up splash
void ui_boot_splash(void);          // the eye mark at start-up (called by ui_init)
bool ui_boot_showing(void);
void ui_home(void);                 // go to the companion
void ui_back(void);
void ui_open_menu(void);
void ui_toggle_settings_qr(void);   // BOOT button
void ui_settings_changed(void);     // settings saved from the website
void ui_set_asleep(bool asleep);    // screen off: shows the sleeping face when it wakes
bool ui_is_asleep(void);
void ui_wake(void);
void ui_toast(const char *text, int ms);
void ui_voice(voice_state_t st, const char *text);
void ui_reset_prompt(bool on);      // "Press BOOT to erase" after the website asks for a reset
void ui_react_happy(void);
