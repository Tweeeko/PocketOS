// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Flash storage (NVS): settings as one JSON blob, Wi-Fi credentials, per-device secrets.
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "settings.h"

esp_err_t storage_init(void);
bool storage_load_settings(settings_t *s);            // false = nothing saved yet (defaults kept)
esp_err_t storage_save_settings(const settings_t *s);
bool storage_wifi_get(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
esp_err_t storage_wifi_set(const char *ssid, const char *pass);
void storage_ap_password(char *out, size_t len);      // generated once per device
// Paired-phone tokens, kept so a restart (or waking from away-sleep) doesn't unpair phones.
bool storage_tokens_get(void *buf, size_t len);
esp_err_t storage_tokens_set(const void *buf, size_t len);
bool storage_blob_get(const char *key, void *buf, size_t len);   // false unless exactly len bytes stored
esp_err_t storage_blob_set(const char *key, const void *buf, size_t len);
esp_err_t storage_factory_reset(void);
const char *device_id(void);                          // "a1b2" from the Wi-Fi MAC
