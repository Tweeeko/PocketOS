// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// PocketOS firmware entry point.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "app.h"
#include "storage.h"
#include "pmu.h"
#include "net.h"
#include "web.h"
#include "ha_client.h"
#include "buttons.h"
#include "power.h"
#include "voice.h"
#include "updater.h"
#include "ui.h"

static const char *TAG = "pocketos";

static void on_button(button_event_t ev)
{
    UI_LOCK();
    bool asleep = ui_is_asleep();
    switch (ev) {
    case BTN_BOOT_CLICK:
        if (web_confirm_pending()) { UI_UNLOCK(); web_confirm_do(); return; }
        if (asleep) { ui_wake(); break; }
        {   // The home Wi-Fi turned down the saved password: the Boot button offers to set it up again
            app_status_t st;
            app_status(&st);
            if (st.wifi_rejected) { UI_UNLOCK(); web_confirm_start(CONFIRM_WIFI); return; }
        }
        ui_toggle_settings_qr();
        break;
    case BTN_BOOT_DOUBLE:
        ui_wake();
        ui_home();
        break;
    case BTN_BOOT_HOLD_START:
        ui_wake();
        UI_UNLOCK();
        voice_start();   // takes the UI lock itself
        return;
    case BTN_BOOT_HOLD_END:
        UI_UNLOCK();
        voice_stop();
        return;
    case BTN_PWR_SHORT:
        if (asleep) ui_wake(); else ui_set_asleep(true);
        break;
    case BTN_PWR_LONG:
        ui_toast("Keep holding to switch off", 3000);
        break;
    }
    UI_UNLOCK();
}

void app_main(void)
{
    ESP_LOGI(TAG, "PocketOS starting");
    ESP_ERROR_CHECK(storage_init());

    // Woken from away-sleep by the timer: look for home with the screen off, else sleep again
    power_check_arrival();

    app_init();
    ESP_ERROR_CHECK(bsp_i2c_init());
    if (pmu_init() != ESP_OK) ESP_LOGW(TAG, "power chip not found; battery info unavailable");

    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "display failed to start");
        return;   // not marked valid: a broken update rolls back on the next restart
    }
    const settings_t *s = app_settings_lock();
    int bright = s->brightness;
    app_settings_unlock();
    bsp_display_brightness_set(bright);

    ESP_ERROR_CHECK(net_start());
    UI_LOCK();
    ui_init();
    if (power_just_arrived()) { ui_toast("Welcome home!", 2500); ui_react_happy(); }
    UI_UNLOCK();

    ESP_ERROR_CHECK(web_start());
    ha_client_start();
    updater_start();   // checks only run once on the home Wi-Fi
    buttons_start(on_button);
    power_start();

    // Everything essential started: keep this firmware (cancels automatic rollback).
    esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "ready, device id %s", device_id());
}
