#include "storage.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "cJSON.h"

static const char *TAG = "storage";
static const char *NS = "pocketos";
static char s_id[5];

const char *device_id(void)
{
    if (!s_id[0]) {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(s_id, sizeof(s_id), "%02x%02x", mac[4], mac[5]);
    }
    return s_id;
}

esp_err_t storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

bool storage_load_settings(settings_t *s)
{
    settings_defaults(s);
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = 0;
    bool ok = false;
    if (nvs_get_blob(h, "cfg", NULL, &len) == ESP_OK && len > 2) {
        char *buf = malloc(len + 1);
        if (buf && nvs_get_blob(h, "cfg", buf, &len) == ESP_OK) {
            buf[len] = 0;
            cJSON *j = cJSON_Parse(buf);
            settings_err_t err;
            if (j && settings_patch(s, j, true, &err)) ok = true;
            else ESP_LOGE(TAG, "saved settings invalid (%s): using defaults", err.field ? err.field : "?");
            cJSON_Delete(j);
        }
        free(buf);
    }
    nvs_close(h);
    return ok;
}

esp_err_t storage_save_settings(const settings_t *s)
{
    cJSON *j = settings_to_json(s, true);
    cJSON_DeleteItemFromObject(j, "schema");
    char *txt = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!txt) return ESP_ERR_NO_MEM;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, "cfg", txt, strlen(txt));
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    free(txt);
    if (err != ESP_OK) ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    return err;
}

bool storage_wifi_get(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t a = ssid_len, b = pass_len;
    bool ok = nvs_get_str(h, "ssid", ssid, &a) == ESP_OK && nvs_get_str(h, "pass", pass, &b) == ESP_OK && ssid[0];
    nvs_close(h);
    return ok;
}

esp_err_t storage_wifi_set(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "pass", pass);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

void storage_ap_password(char *out, size_t len)
{
    nvs_handle_t h;
    out[0] = 0;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) { snprintf(out, len, "pocketos"); return; }
    size_t n = len;
    if (nvs_get_str(h, "ap_pass", out, &n) != ESP_OK || strlen(out) < 8) {
        static const char A[] = "abcdefghjkmnpqrstuvwxyz23456789";
        char p[9];
        for (int i = 0; i < 8; i++) p[i] = A[esp_random() % (sizeof(A) - 1)];
        p[8] = 0;
        nvs_set_str(h, "ap_pass", p);
        nvs_commit(h);
        snprintf(out, len, "%s", p);
    }
    nvs_close(h);
}

bool storage_blob_get(const char *key, void *buf, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t n = len;
    bool ok = nvs_get_blob(h, key, buf, &n) == ESP_OK && n == len;
    nvs_close(h);
    return ok;
}

esp_err_t storage_blob_set(const char *key, const void *buf, size_t len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, key, buf, len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

bool storage_tokens_get(void *buf, size_t len) { return storage_blob_get("tokens", buf, len); }
esp_err_t storage_tokens_set(const void *buf, size_t len) { return storage_blob_set("tokens", buf, len); }

esp_err_t storage_factory_reset(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
