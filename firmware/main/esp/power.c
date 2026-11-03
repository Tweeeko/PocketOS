// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#include "power.h"
#include <time.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_wifi.h"
#include "esp_pm.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "app.h"
#include "pmu.h"
#include "net.h"
#include "ui.h"
#include "battery.h"
#include "storage.h"
#include "freertos/semphr.h"

static const char *TAG = "power";
#define AWAY_AFTER_S   180
#define CHECK_EVERY_S  300

RTC_DATA_ATTR static int s_away;         // survives deep sleep and restarts
RTC_DATA_ATTR static int s_arrive_flag;
RTC_DATA_ATTR static int s_empty;        // asleep because the battery ran out
static bool s_arrived;

// Battery history lives in RTC memory (kept through deep sleep and restarts) and is copied to
// flash every hour, so it also survives the battery running flat.
RTC_DATA_ATTR static batt_log_t s_blog;
static SemaphoreHandle_t s_blog_lock;
static int s_unsaved;

static void blog_load(void)
{
    if (batt_log_valid(&s_blog)) return;
    if (!storage_blob_get("battlog", &s_blog, sizeof s_blog) || !batt_log_valid(&s_blog)) batt_log_init(&s_blog);
}

static void blog_save(void)
{
    if (storage_blob_set("battlog", &s_blog, sizeof s_blog) == ESP_OK) s_unsaved = 0;
}

// Reads the power chip, records history, and returns the reading
static void battery_sample(pmu_reading_t *r, batt_est_t *est)
{
    pmu_read(r);
    time_t now = time(NULL);
    if (s_blog_lock) xSemaphoreTake(s_blog_lock, portMAX_DELAY);
    if (r->ok && r->percent >= 0 && batt_log_add(&s_blog, (uint32_t)now, r->percent, r->mv, r->charging, r->plugged)) s_unsaved++;
    if (est) *est = batt_estimate(&s_blog, (uint32_t)now, r->percent, r->plugged);
    if (s_blog_lock) xSemaphoreGive(s_blog_lock);
}

bool power_just_arrived(void) { return s_arrived; }

static void deep_sleep(void)
{
    s_away = 1;
    esp_sleep_enable_timer_wakeup((uint64_t)CHECK_EVERY_S * 1000000ULL);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);   // BOOT pressed
    ESP_LOGI(TAG, "away: deep sleep, next check in %d s", CHECK_EVERY_S);
    esp_deep_sleep_start();
}

#define EMPTY_PCT       3
#define EMPTY_MV        3450
#define EMPTY_CHECK_S   600

static void empty_sleep(void)
{
    s_empty = 1;
    esp_sleep_enable_timer_wakeup((uint64_t)EMPTY_CHECK_S * 1000000ULL);   // look for USB power
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
    ESP_LOGW(TAG, "battery empty: sleeping until charged");
    esp_deep_sleep_start();
}

bool power_check_arrival(void)
{
    esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
    if (s_empty) {
        // Asleep with a flat battery: carry on only once it's plugged in or BOOT is pressed
        s_empty = 0;
        if (why == ESP_SLEEP_WAKEUP_TIMER && bsp_i2c_init() == ESP_OK && pmu_init() == ESP_OK) {
            pmu_reading_t r;
            pmu_read(&r);
            if (r.ok && r.present && !r.plugged && r.percent >= 0 && r.percent <= EMPTY_PCT) empty_sleep();
        }
    }
    if (s_arrive_flag) { s_arrive_flag = 0; s_arrived = true; }
    if (!s_away) return true;
    if (why == ESP_SLEEP_WAKEUP_EXT0) { s_away = 0; return true; }   // picked up and pressed
    if (why == ESP_SLEEP_WAKEUP_TIMER) {
        // Screen stays off. Note the battery level, then let only the radio look for home.
        blog_load();
        if (bsp_i2c_init() == ESP_OK && pmu_init() == ESP_OK) { pmu_reading_t r; battery_sample(&r, NULL); }
        if (net_home_visible(2000)) {
            // Restart into a normal boot (Wi-Fi was only half set up for the scan)
            ESP_LOGI(TAG, "home network found: waking up");
            s_away = 0;
            s_arrive_flag = 1;
            esp_restart();
        }
        deep_sleep();
    }
    s_away = 0;
    return true;
}

static void power_task(void *arg)
{
    int away_s = 0, tick = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        tick++;
        if (tick % 15 == 1) {
            pmu_reading_t r;
            batt_est_t e;
            battery_sample(&r, &e);
            app_set_battery(r.present ? r.percent : -1, r.charging, r.plugged, r.mv,
                            (e.known && e.hours_left >= 0) ? (int)(e.hours_left + 0.5f) : -1);
            if (s_unsaved >= 6) blog_save();   // about once an hour
            // Flat battery: stop cleanly rather than browning out mid-write
            if (r.present && !r.plugged && r.percent >= 0 && r.percent <= EMPTY_PCT && r.mv && r.mv < EMPTY_MV) {
                UI_LOCK(); ui_wake(); ui_toast("Battery empty. Please charge me", 0); UI_UNLOCK();
                vTaskDelay(pdMS_TO_TICKS(4000));
                blog_save();
                esp_wifi_stop();
                bsp_display_brightness_set(0);
                empty_sleep();
            }
        }
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        app_set_time_ok(tm.tm_year >= 2024 - 1900);

        app_status_t st;
        app_status(&st);
        const settings_t *s = app_settings_lock();
        int sleep_s = s->sleep_s;
        app_settings_unlock();

        // Screen sleep after inactivity (never during first setup)
        UI_LOCK();
        uint32_t idle = lv_display_get_inactive_time(NULL);
        bool asleep = ui_is_asleep();
        if (!asleep && !st.setup_mode && sleep_s > 0 && idle > (uint32_t)sleep_s * 1000) ui_set_asleep(true);
        else if (asleep && idle < 1500) ui_set_asleep(false);   // touched
        asleep = ui_is_asleep();
        UI_UNLOCK();

        // Away from home: no home Wi-Fi for a while
        if (!st.setup_mode && !st.wifi && !st.wifi_rejected) away_s++;   // a rejected password isn't "away"
        else away_s = 0;
        if (away_s >= AWAY_AFTER_S && asleep && !st.plugged) {
            if (s_unsaved) blog_save();
            esp_wifi_stop();
            bsp_display_brightness_set(0);
            deep_sleep();
        }
    }
}

void power_start(void)
{
    // Let the CPU drop to 80 MHz when idle. No automatic light sleep: touch and screen stay live.
    esp_pm_config_t pm = {.max_freq_mhz = 240, .min_freq_mhz = 80, .light_sleep_enable = false};
    if (esp_pm_configure(&pm) != ESP_OK) ESP_LOGW(TAG, "CPU frequency scaling not available");
    s_blog_lock = xSemaphoreCreateMutex();
    blog_load();
    xTaskCreate(power_task, "power", 4096, NULL, 3, NULL);
}

cJSON *power_battery_json(void)
{
    pmu_reading_t r;
    pmu_read(&r);
    cJSON *o = cJSON_CreateObject();
    if (r.present && r.percent >= 0) cJSON_AddNumberToObject(o, "percent", r.percent); else cJSON_AddNullToObject(o, "percent");
    if (r.mv) cJSON_AddNumberToObject(o, "mv", r.mv); else cJSON_AddNullToObject(o, "mv");
    cJSON_AddBoolToObject(o, "present", r.present);
    cJSON_AddBoolToObject(o, "charging", r.charging);
    cJSON_AddBoolToObject(o, "full", r.full);
    cJSON_AddBoolToObject(o, "plugged", r.plugged);
    time_t now = time(NULL);
    cJSON_AddNumberToObject(o, "now", (double)now);
    xSemaphoreTake(s_blog_lock, portMAX_DELAY);
    batt_est_t e = batt_estimate(&s_blog, (uint32_t)now, r.percent, r.plugged);
    cJSON *est = cJSON_AddObjectToObject(o, "estimate");
    cJSON_AddBoolToObject(est, "known", e.known);
    if (e.known) {
        cJSON_AddNumberToObject(est, "pct_per_h", roundf(e.pct_per_h * 10) / 10);
        if (e.hours_left >= 0) cJSON_AddNumberToObject(est, "hours_left", roundf(e.hours_left * 10) / 10);
        cJSON_AddNumberToObject(est, "full_h", roundf(e.full_h * 10) / 10);
        cJSON_AddNumberToObject(est, "basis_h", roundf(e.basis_h * 10) / 10);
    }
    if (s_blog.last_full_t) cJSON_AddNumberToObject(o, "last_full", s_blog.last_full_t);
    // History as [time, percent, millivolts, flags] (flags: 1 charging, 2 plugged in)
    cJSON *h = cJSON_AddArrayToObject(o, "history");
    for (int i = 0; i < batt_log_count(&s_blog); i++) {
        batt_sample_t b;
        batt_log_get(&s_blog, i, &b);
        cJSON *row = cJSON_CreateArray();
        cJSON_AddItemToArray(row, cJSON_CreateNumber(b.t));
        cJSON_AddItemToArray(row, cJSON_CreateNumber(b.pct));
        cJSON_AddItemToArray(row, cJSON_CreateNumber(b.mv));
        cJSON_AddItemToArray(row, cJSON_CreateNumber(b.flags));
        cJSON_AddItemToArray(h, row);
    }
    xSemaphoreGive(s_blog_lock);
    return o;
}
