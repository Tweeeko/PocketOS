// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Over-the-air updates from published releases. See updater.h and docs/releasing.md.
#include "updater.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "mbedtls/sha256.h"
#include "sdkconfig.h"
#include "update.h"
#include "app.h"
#include "ui.h"

static const char *TAG = "update";

#define BIT_CHECK    (1 << 0)
#define BIT_INSTALL  (1 << 1)
#define FIRST_CHECK_S     120          // after start-up, once Wi-Fi has settled
#define CHECK_EVERY_S     (24 * 3600)
#define MIN_BATTERY_PCT   30           // unless plugged in

static TaskHandle_t s_task;
static SemaphoreHandle_t s_lock;
static update_manifest_t s_man;         // last manifest that offered a newer version
static bool s_have;                     // s_man is valid and newer than this firmware
static volatile bool s_busy;
static char s_err[96];
static time_t s_checked;                // last successful check (0 = never)

static const char *url_base(void) { return CONFIG_POCKETOS_UPDATE_URL; }

static void toast(const char *msg, int ms) { UI_LOCK(); ui_toast(msg, ms); UI_UNLOCK(); }

static void check_failed(const char *msg);
static void set_err(const char *msg)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snprintf(s_err, sizeof s_err, "%s", msg);
    xSemaphoreGive(s_lock);
}

// A failed check keeps any update that was already found on offer.
static void check_failed(const char *msg)
{
    set_err(msg);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool have = s_have;
    xSemaphoreGive(s_lock);
    app_set_update(have ? UPD_AVAILABLE : UPD_FAILED, NULL, 0);
}

/* ---------- HTTPS with GitHub's redirects ---------- */

static esp_http_client_handle_t open_url(const char *url, int64_t *len, const char **why)
{
    esp_http_client_config_t c = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,   // checks the server certificate
        .timeout_ms = 15000,
        .buffer_size = 8192,        // GitHub's redirect responses have very long headers
        .buffer_size_tx = 1024,
        .max_redirection_count = 5,
        .user_agent = "PocketOS",
    };
    esp_http_client_handle_t h = esp_http_client_init(&c);
    if (!h) { *why = "Out of memory"; return NULL; }
    for (int hop = 0; hop < 6; hop++) {
        if (esp_http_client_open(h, 0) != ESP_OK) { *why = "Couldn't reach the update server"; break; }
        int64_t n = esp_http_client_fetch_headers(h);
        int st = esp_http_client_get_status_code(h);
        if (st == 301 || st == 302 || st == 303 || st == 307 || st == 308) {
            if (esp_http_client_set_redirection(h) != ESP_OK) { *why = "Update server redirect failed"; break; }
            char tmp[256];
            while (esp_http_client_read(h, tmp, sizeof tmp) > 0) {}   // empty the redirect body
            continue;
        }
        if (st == 200) { *len = n; return h; }
        *why = st == 404 ? "No release has been published yet" : "The update server refused the request";
        ESP_LOGW(TAG, "HTTP %d", st);
        break;
    }
    esp_http_client_cleanup(h);
    return NULL;
}

/* ---------- checking ---------- */

static void do_check(bool manual)
{
    const char *why = NULL;
    if (!url_base()[0]) { check_failed("Online updates are off in this build"); return; }
    app_status_t st;
    app_status(&st);
    if (!st.wifi) { check_failed("Not on the home Wi-Fi"); return; }
    app_set_update(UPD_CHECKING, NULL, 0);

    int64_t len = 0;
    esp_http_client_handle_t h = open_url(url_base(), &len, &why);
    char *buf = NULL;
    int got = 0;
    if (h) {
        buf = calloc(1, 4096);
        while (buf && got < 4095) {
            int r = esp_http_client_read(h, buf + got, 4095 - got);
            if (r <= 0) break;
            got += r;
        }
        esp_http_client_close(h);
        esp_http_client_cleanup(h);
        if (!buf) why = "Out of memory";
        else if (got == 0) why = "The update information was empty";
    }
    update_manifest_t m;
    const char *perr = NULL;
    bool ok = !why && update_parse_manifest(buf, &m, &perr);
    free(buf);
    if (!ok) {
        ESP_LOGW(TAG, "check failed: %s", why ? why : perr);
        check_failed(why ? why : perr);
        return;
    }
    const char *mine = esp_app_get_description()->version;
    bool newer = version_cmp(m.version, mine) > 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool was = s_have && !strcmp(s_man.version, m.version);
    s_have = newer;
    if (newer) s_man = m;
    s_err[0] = 0;
    s_checked = time(NULL);
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "this is %s, latest release is %s", mine, m.version);
    if (newer) {
        app_set_update(UPD_AVAILABLE, m.version, 0);
        if (!was || manual) {
            char t[64];
            snprintf(t, sizeof t, "Update %s is ready. Install it from the Settings website", m.version);
            toast(t, 4000);
        }
    } else {
        app_set_update(UPD_LATEST, mine, 0);
        if (manual) toast("PocketOS is up to date", 2000);
    }
}

/* ---------- installing ---------- */

static void do_install(void)
{
    update_manifest_t m;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool have = s_have;
    m = s_man;
    xSemaphoreGive(s_lock);
    if (!have) return;

    const char *why = NULL;
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part || m.size > part->size) { set_err("No room for this update"); app_set_update(UPD_FAILED, NULL, 0); return; }

    app_set_update(UPD_DOWNLOADING, m.version, 0);
    toast("Downloading update...", 0);
    int64_t len = 0;
    esp_http_client_handle_t h = open_url(m.url, &len, &why);
    if (!h) { set_err(why); app_set_update(UPD_FAILED, NULL, 0); toast("Update failed. Nothing changed.", 3000); return; }
    if (len > 0 && len != (int64_t)m.size) why = "The download is the wrong size";

    esp_ota_handle_t ota = 0;
    bool ota_open = false;
    if (!why) {
        if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota) == ESP_OK) ota_open = true;
        else why = "Couldn't prepare the spare firmware slot";
    }
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    char *buf = malloc(4096);
    if (!buf && !why) why = "Out of memory";
    uint32_t total = 0;
    int last_pct = -1;
    while (!why) {
        int r = esp_http_client_read(h, buf, 4096);
        if (r < 0) { why = "The download stopped"; break; }
        if (r == 0) {
            if (esp_http_client_is_complete_data_received(h)) break;
            why = "The download stopped";
            break;
        }
        if (total + (uint32_t)r > m.size) { why = "The download is bigger than expected"; break; }
        mbedtls_sha256_update(&sha, (const unsigned char *)buf, r);
        if (esp_ota_write(ota, buf, r) != ESP_OK) { why = "Couldn't write the update"; break; }
        total += (uint32_t)r;
        int pct = (int)((uint64_t)total * 100 / m.size);
        if (pct != last_pct) {
            app_set_update(UPD_DOWNLOADING, NULL, pct);
            if (pct / 10 != last_pct / 10) { char t[32]; snprintf(t, sizeof t, "Updating... %d%%", pct); toast(t, 0); }
            last_pct = pct;
        }
    }
    free(buf);
    esp_http_client_close(h);
    esp_http_client_cleanup(h);
    unsigned char digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);

    if (!why && total != m.size) why = "The download was incomplete";
    if (!why && memcmp(digest, m.sha256, 32) != 0) why = "The download didn't match its checksum";
    if (ota_open) {
        if (why) esp_ota_abort(ota);
        else if (esp_ota_end(ota) != ESP_OK) why = "The update isn't a valid PocketOS firmware";   // checks the image
    }
    if (!why) {
        esp_app_desc_t d;
        if (esp_ota_get_partition_description(part, &d) != ESP_OK || strcmp(d.project_name, "pocketos") != 0)
            why = "That firmware isn't PocketOS";
    }
    if (!why && esp_ota_set_boot_partition(part) != ESP_OK) why = "Couldn't switch to the new version";

    if (why) {
        ESP_LOGE(TAG, "install failed: %s", why);
        set_err(why);
        app_set_update(UPD_AVAILABLE, m.version, 0);   // still on offer; nothing changed
        toast("Update failed. Nothing changed.", 3000);
        return;
    }
    ESP_LOGI(TAG, "installed %s, restarting", m.version);
    app_set_update(UPD_INSTALLED, m.version, 100);
    toast("Updated! Restarting...", 0);
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

/* ---------- background task ---------- */

static bool auto_install_now(const app_status_t *st)
{
    const settings_t *s = app_settings_lock();
    bool autom = s->updates == UPD_AUTO;
    app_settings_unlock();
    if (!autom || st->update != UPD_AVAILABLE || !st->plugged || !st->wifi) return false;
    UI_LOCK();
    bool asleep = ui_is_asleep();
    UI_UNLOCK();
    if (!asleep) return false;                   // never while someone is using it
    if (!st->time_ok) return true;
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    return tm.tm_hour >= 2 && tm.tm_hour < 5;    // overnight, on charge
}

static void task(void *arg)
{
    int64_t next_check_s = FIRST_CHECK_S;
    int64_t t = 0;
    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, UINT32_MAX, &bits, pdMS_TO_TICKS(60 * 1000));
        if (!bits) t += 60;
        const settings_t *s = app_settings_lock();
        upd_mode_t mode = s->updates;
        app_settings_unlock();

        s_busy = true;
        if (bits & BIT_CHECK) do_check(true);
        else if (mode != UPD_OFF && t >= next_check_s) {
            do_check(false);
            t = 0;
            xSemaphoreTake(s_lock, portMAX_DELAY);
            bool failed = s_err[0] != 0;
            xSemaphoreGive(s_lock);
            // Daily, spread out; after a failure (offline, server down) try again in an hour
            next_check_s = failed ? 3600 : CHECK_EVERY_S + (esp_random() % 3600);
        }
        app_status_t st;
        app_status(&st);
        if ((bits & BIT_INSTALL) || auto_install_now(&st)) do_install();
        s_busy = false;
    }
}

void updater_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    const esp_app_desc_t *d = esp_app_get_description();
    app_set_update(UPD_IDLE, d->version, 0);
    xTaskCreate(task, "updater", 8192, NULL, 2, &s_task);
}

bool updater_check_now(void)
{
    if (!s_task || s_busy) return false;
    xTaskNotify(s_task, BIT_CHECK, eSetBits);
    return true;
}

bool updater_install(const char **why_not)
{
    app_status_t st;
    app_status(&st);
    *why_not = NULL;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool have = s_have;
    xSemaphoreGive(s_lock);
    if (!s_task || s_busy) *why_not = "PocketOS is already checking or updating";
    else if (!have) *why_not = "There's no newer version to install";
    else if (!st.plugged && st.battery >= 0 && st.battery < MIN_BATTERY_PCT) *why_not = "Charge PocketOS to 30% or plug it in first";
    if (*why_not) return false;
    xTaskNotify(s_task, BIT_INSTALL, eSetBits);
    return true;
}

void updater_info(cJSON *o)
{
    static const char *NAMES[] = {"idle", "checking", "latest", "available", "downloading", "installed", "failed"};
    app_status_t st;
    app_status(&st);
    const settings_t *s = app_settings_lock();
    const char *mode = settings_updates_name(s->updates);
    app_settings_unlock();
    cJSON *u = cJSON_AddObjectToObject(o, "update");
    cJSON_AddBoolToObject(u, "online", url_base()[0] != 0);
    cJSON_AddStringToObject(u, "mode", mode);
    cJSON_AddStringToObject(u, "state", (unsigned)st.update < sizeof NAMES / sizeof NAMES[0] ? NAMES[st.update] : "idle");
    cJSON_AddNumberToObject(u, "progress", st.update_progress);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_have) {
        cJSON_AddStringToObject(u, "available", s_man.version);
        cJSON_AddStringToObject(u, "notes", s_man.notes);
        cJSON_AddNumberToObject(u, "size", s_man.size);
    }
    cJSON_AddStringToObject(u, "error", s_err);
    if (s_checked) cJSON_AddNumberToObject(u, "checked", (double)s_checked);
    xSemaphoreGive(s_lock);
}
