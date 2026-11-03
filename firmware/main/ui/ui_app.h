// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// What the UI needs from the rest of PocketOS. Implemented by esp/app.c on the device and by
// host/sim/sim.c in the PC simulator, so the same UI code runs in both.
#include <stdbool.h>
#include <stdint.h>
#include "model.h"
#include "settings.h"
#include "ha_proto.h"

typedef enum { HA_NOT_SET = 0, HA_CONNECTING, HA_OK, HA_ERROR } ha_status_t;
typedef enum { UPD_IDLE = 0, UPD_CHECKING, UPD_LATEST, UPD_AVAILABLE, UPD_DOWNLOADING, UPD_INSTALLED, UPD_FAILED } upd_state_t;

typedef struct {
    bool setup_mode;          // first-time Wi-Fi setup network is up
    bool wifi;                // on home Wi-Fi with an IP
    char ip[16];
    char host[40];            // pocketos-xxxx.local
    char ap_ssid[33];
    char ap_pass[16];
    char pin[8];              // pairing PIN for the settings website
    int battery;              // -1 unknown
    bool charging;
    bool plugged;             // USB power connected
    int battery_mv;           // 0 unknown
    int battery_hours;        // estimated hours left on battery, -1 unknown or plugged in
    upd_state_t update;
    char update_version[16];  // newer version found
    int update_progress;      // 0..100 while downloading
    ha_status_t ha;
    char ha_error[80];
    bool time_ok;
    char firmware[16];
} app_status_t;

// Model and settings are shared with other tasks: lock while reading, keep it short.
model_t *app_model_lock(void);
void app_model_unlock(void);
const settings_t *app_settings_lock(void);
void app_settings_unlock(void);

void app_status(app_status_t *out);
bool app_settings_patch(const char *json);   // small changes made on the device itself (eye colour etc.)
void app_act(const char *entity_id, act_t act, float value);   // sends to Home Assistant
void app_brightness(int percent);                               // 0 = screen off
void app_voice(bool start);                                     // push-to-talk
uint32_t app_ms(void);
