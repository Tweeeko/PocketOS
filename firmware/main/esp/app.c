// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#include "app.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_app_desc.h"
#include "bsp/display.h"
#include "storage.h"
#include "ha_client.h"
#include "voice.h"
#include "ui.h"

static const char *TAG = "app";

// Large shared state lives in PSRAM (CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY)
EXT_RAM_BSS_ATTR static model_t s_model;
EXT_RAM_BSS_ATTR static settings_t s_settings;
static SemaphoreHandle_t m_lock, s_lock, st_lock;
static app_status_t s_status;

/* ---------- ui_app.h ---------- */

model_t *app_model_lock(void) { xSemaphoreTake(m_lock, portMAX_DELAY); return &s_model; }
void app_model_unlock(void) { xSemaphoreGive(m_lock); }
const settings_t *app_settings_lock(void) { xSemaphoreTake(s_lock, portMAX_DELAY); return &s_settings; }
void app_settings_unlock(void) { xSemaphoreGive(s_lock); }
uint32_t app_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

void app_status(app_status_t *out)
{
    xSemaphoreTake(st_lock, portMAX_DELAY);
    *out = s_status;
    xSemaphoreGive(st_lock);
}

void app_brightness(int percent) { bsp_display_brightness_set(percent); }

void app_voice(bool start) { if (start) voice_start(); else voice_stop(); }

void app_act(const char *entity_id, act_t act, float value)
{
    model_t *m = app_model_lock();
    int i = model_find(m, entity_id);
    bool ok = i >= 0 && ha_client_action(m, i, act, value);
    app_model_unlock();
    if (!ok) {
        // Called from the LVGL task (lock already held), so ui_* is safe here
        ui_toast(ha_client_connected() ? "That device can't do that" : "Home Assistant isn't connected", 2200);
    }
}

static bool update_settings(const cJSON *patch, bool trusted, settings_err_t *err, bool on_ui_task);

// From the on-screen companion settings: runs on the LVGL task, which already holds its lock.
bool app_settings_patch(const char *json)
{
    cJSON *j = cJSON_Parse(json);
    settings_err_t err;
    bool ok = j && update_settings(j, false, &err, true);
    cJSON_Delete(j);
    return ok;
}

/* ---------- settings ---------- */

static void apply_tz(const settings_t *s)
{
    setenv("TZ", s->tz, 1);
    tzset();
}

bool app_update_settings(const cJSON *patch, bool trusted, settings_err_t *err)
{
    return update_settings(patch, trusted, err, false);
}

static bool update_settings(const cJSON *patch, bool trusted, settings_err_t *err, bool on_ui_task)
{
    static settings_t before;   // ~20 KB: keep off task stacks
    xSemaphoreTake(s_lock, portMAX_DELAY);
    before = s_settings;
    bool ok = settings_patch(&s_settings, patch, trusted, err);
    if (ok && storage_save_settings(&s_settings) != ESP_OK) {
        s_settings = before;   // flash write failed: report it, keep the old values
        err->field = NULL;
        err->message = "Couldn't save to the device's memory";
        ok = false;
    }
    bool ha_changed = ok && (strcmp(before.ha_url, s_settings.ha_url) || strcmp(before.ha_token, s_settings.ha_token));
    if (ok) apply_tz(&s_settings);
    xSemaphoreGive(s_lock);
    if (!ok) return false;
    if (on_ui_task) ui_settings_changed();
    else { UI_LOCK(); ui_settings_changed(); UI_UNLOCK(); }
    if (ha_changed) ha_client_reconfigure();
    return true;
}

void app_settings_copy(settings_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_settings;
    xSemaphoreGive(s_lock);
}

bool app_set_flag(const char *list, const char *entity_id, bool on)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = settings_set_flag(&s_settings, list, entity_id, on);
    if (ok) ok = storage_save_settings(&s_settings) == ESP_OK;
    xSemaphoreGive(s_lock);
    if (ok) { xSemaphoreTake(m_lock, portMAX_DELAY); s_model.version++; xSemaphoreGive(m_lock); }
    return ok;
}

bool app_fav_move(const char *entity_id, int dir)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = settings_fav_move(&s_settings, entity_id, dir);
    if (ok) ok = storage_save_settings(&s_settings) == ESP_OK;
    xSemaphoreGive(s_lock);
    if (ok) { xSemaphoreTake(m_lock, portMAX_DELAY); s_model.version++; xSemaphoreGive(m_lock); }
    return ok;
}

bool app_set_name(const char *entity_id, const char *name, const char **why)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = settings_set_name(&s_settings, entity_id, name, why);
    if (ok && storage_save_settings(&s_settings) != ESP_OK) { ok = false; *why = "Couldn't save that"; }
    xSemaphoreGive(s_lock);
    if (ok) { xSemaphoreTake(m_lock, portMAX_DELAY); s_model.version++; xSemaphoreGive(m_lock); }
    return ok;
}

void app_mark_all_known(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    xSemaphoreTake(m_lock, portMAX_DELAY);
    for (int i = 0; i < s_model.n; i++)
        if (!s_model.e[i].registry_hidden) settings_set_flag(&s_settings, "known", s_model.e[i].id, true);
    s_model.version++;
    xSemaphoreGive(m_lock);
    storage_save_settings(&s_settings);
    xSemaphoreGive(s_lock);
}

void app_on_ha_synced(void)
{
    // First ever connection: everything that exists now counts as reviewed, so PocketOS
    // starts out showing the home. Devices added later appear as "new" on the website.
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_settings.nknown == 0) {
        xSemaphoreTake(m_lock, portMAX_DELAY);
        for (int i = 0; i < s_model.n; i++)
            if (!s_model.e[i].registry_hidden) settings_set_flag(&s_settings, "known", s_model.e[i].id, true);
        s_model.version++;
        xSemaphoreGive(m_lock);
        storage_save_settings(&s_settings);
        ESP_LOGI(TAG, "first sync: %d devices marked as reviewed", s_settings.nknown);
    }
    xSemaphoreGive(s_lock);
}

/* ---------- status setters ---------- */

#define ST_SET(code) do { xSemaphoreTake(st_lock, portMAX_DELAY); code; xSemaphoreGive(st_lock); } while (0)

void app_set_wifi(bool setup_mode, bool connected, const char *ip, const char *host, const char *ap_ssid, const char *ap_pass)
{
    ST_SET(
        s_status.setup_mode = setup_mode;
        s_status.wifi = connected;
        snprintf(s_status.ip, sizeof s_status.ip, "%s", ip ? ip : "");
        snprintf(s_status.host, sizeof s_status.host, "%s", host ? host : "");
        snprintf(s_status.ap_ssid, sizeof s_status.ap_ssid, "%s", ap_ssid ? ap_ssid : "");
        snprintf(s_status.ap_pass, sizeof s_status.ap_pass, "%s", ap_pass ? ap_pass : ""));
}

void app_set_ha(ha_status_t st, const char *error)
{
    ST_SET(s_status.ha = st; snprintf(s_status.ha_error, sizeof s_status.ha_error, "%s", error ? error : ""));
}

void app_set_pin(const char *pin) { ST_SET(snprintf(s_status.pin, sizeof s_status.pin, "%s", pin)); }
void app_set_battery(int percent, bool charging, bool plugged, int mv, int hours_left)
{
    ST_SET(s_status.battery = percent; s_status.charging = charging; s_status.plugged = plugged;
           s_status.battery_mv = mv; s_status.battery_hours = hours_left);
}
void app_set_update(upd_state_t st, const char *version, int progress)
{
    ST_SET(s_status.update = st; s_status.update_progress = progress;
           if (version) snprintf(s_status.update_version, sizeof s_status.update_version, "%s", version));
}
void app_set_time_ok(bool ok) { ST_SET(s_status.time_ok = ok); }

void app_init(void)
{
    m_lock = xSemaphoreCreateMutex();
    s_lock = xSemaphoreCreateMutex();
    st_lock = xSemaphoreCreateMutex();
    model_clear(&s_model);
    if (!storage_load_settings(&s_settings)) ESP_LOGI(TAG, "no saved settings: defaults");
    apply_tz(&s_settings);
    s_status.battery = -1;
    s_status.battery_hours = -1;
    snprintf(s_status.firmware, sizeof s_status.firmware, "%s", esp_app_get_description()->version);
    s_status.ha = (s_settings.ha_url[0] && s_settings.ha_token[0]) ? HA_CONNECTING : HA_NOT_SET;
}
