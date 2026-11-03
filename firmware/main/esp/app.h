// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Central device state shared by the UI, web API, Home Assistant client, voice and power tasks.
// Implements ui_app.h for the on-screen UI.
#include <stdbool.h>
#include "ui_app.h"
#include "bsp/esp-bsp.h"

// Take the LVGL lock before calling any ui_* function from a task other than LVGL's own.
#define UI_LOCK()   bsp_display_lock((uint32_t)-1)
#define UI_UNLOCK() bsp_display_unlock()

void app_init(void);   // loads settings; call before anything else

// Settings changes. `trusted` allows the Home Assistant address/token (only the dedicated API
// endpoint and flash loading use it). On success the change is saved and applied everywhere.
bool app_update_settings(const cJSON *patch, bool trusted, settings_err_t *err);
void app_settings_copy(settings_t *out);
bool app_set_flag(const char *list, const char *entity_id, bool on);   // fav/hidden/known
bool app_fav_move(const char *entity_id, int dir);
bool app_set_name(const char *entity_id, const char *name, const char **why);   // "" = use Home Assistant's name
void app_mark_all_known(void);
void app_ui_post(ui_post_kind_t kind, int arg, const char *text);   // never blocks   // "add all new devices" in one save

// Status fields updated by other modules
void app_set_wifi_rejected(bool rejected, const char *ssid);
void app_set_wifi(bool setup_mode, bool connected, const char *ip, const char *host, const char *ap_ssid, const char *ap_pass);
void app_set_ha(ha_status_t st, const char *error);
void app_set_pin(const char *pin);
void app_set_battery(int percent, bool charging, bool plugged, int mv, int hours_left);
void app_set_update(upd_state_t st, const char *version, int progress);
void app_set_time_ok(bool ok);

// Called by the Home Assistant client after the first full sync
void app_on_ha_synced(void);
