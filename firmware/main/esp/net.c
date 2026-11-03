#include "net.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mdns.h"
#include "storage.h"
#include "app.h"
#include "captive_dns.h"

static const char *TAG = "net";
static EventGroupHandle_t s_ev;
#define GOT_IP   BIT0
#define FAILED   BIT1

static net_status_t s_st;
static net_listener_t s_listener;
static esp_netif_t *s_sta, *s_ap;
static bool s_joining;   // setup mode: a test join is in progress
static int s_retries;
static esp_timer_handle_t s_retry_timer;

static void retry_cb(void *arg) { esp_wifi_connect(); }

// Wi-Fi config fields are fixed-size and need not be NUL-terminated (a 32-char SSID fills them).
static void copy_field(uint8_t *dst, size_t dst_len, const char *src)
{
    size_t n = strlen(src);
    memcpy(dst, src, n < dst_len ? n : dst_len);
}

void net_set_listener(net_listener_t fn) { s_listener = fn; }
static void notify(void)
{
    app_set_wifi(s_st.mode == NET_SETUP, s_st.mode == NET_NORMAL && s_st.connected, s_st.ip, s_st.host_local, s_st.ap_ssid, s_st.ap_pass);
    if (s_listener) s_listener();
}

void net_status(net_status_t *out)
{
    *out = s_st;
    if (s_st.mode == NET_NORMAL && s_st.connected) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) out->rssi = ap.rssi;
    }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_st.connected = false;
        s_st.ip[0] = 0;
        if (s_joining) {
            if (++s_retries >= 3) xEventGroupSetBits(s_ev, FAILED);
            else esp_wifi_connect();
        } else if (s_st.mode == NET_NORMAL) {
            // Keep trying forever in normal mode, every 5 s, without blocking the event loop.
            if (!esp_timer_is_active(s_retry_timer)) esp_timer_start_once(s_retry_timer, 5 * 1000 * 1000);
        }
        notify();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_st.ip, sizeof(s_st.ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_st.connected = true;
        s_retries = 0;
        xEventGroupSetBits(s_ev, GOT_IP);
        ESP_LOGI(TAG, "got IP %s", s_st.ip);
        notify();
    }
}

static void start_mdns(void)
{
    if (mdns_init() != ESP_OK) { ESP_LOGW(TAG, "mDNS failed"); return; }
    mdns_hostname_set(s_st.host);
    mdns_instance_name_set("PocketOS");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
}

static void start_sntp(void)
{
    esp_sntp_config_t sc = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sc);
}

esp_err_t net_start(void)
{
    s_ev = xEventGroupCreate();
    const esp_timer_create_args_t ta = {.callback = retry_cb, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&ta, &s_retry_timer));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta = esp_netif_create_default_wifi_sta();
    snprintf(s_st.host, sizeof(s_st.host), "pocketos-%s", device_id());
    esp_netif_set_hostname(s_sta, s_st.host);
    snprintf(s_st.host_local, sizeof(s_st.host_local), "%s.local", s_st.host);

    wifi_init_config_t wc = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wc));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));  // PocketOS stores credentials itself
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

    char ssid[33] = {0}, pass[65] = {0};
    if (storage_wifi_get(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_st.mode = NET_NORMAL;
        strncpy(s_st.ssid, ssid, sizeof(s_st.ssid) - 1);
        wifi_config_t sta = {0};
        copy_field(sta.sta.ssid, sizeof(sta.sta.ssid), ssid);
        copy_field(sta.sta.password, sizeof(sta.sta.password), pass);
        sta.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);   // radio sleeps between beacons
        esp_wifi_connect();
        start_mdns();
        start_sntp();
        ESP_LOGI(TAG, "normal mode, joining %s", ssid);
    } else {
        s_st.mode = NET_SETUP;
        s_ap = esp_netif_create_default_wifi_ap();
        snprintf(s_st.ap_ssid, sizeof(s_st.ap_ssid), "PocketOS-%s", device_id());
        for (char *p = s_st.ap_ssid + 9; *p; p++) if (*p >= 'a' && *p <= 'f') *p -= 32;
        storage_ap_password(s_st.ap_pass, sizeof(s_st.ap_pass));
        wifi_config_t ap = {0};
        copy_field(ap.ap.ssid, sizeof(ap.ap.ssid), s_st.ap_ssid);
        ap.ap.ssid_len = strlen(s_st.ap_ssid);
        copy_field(ap.ap.password, sizeof(ap.ap.password), s_st.ap_pass);
        ap.ap.channel = 1;
        ap.ap.max_connection = 4;
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));   // STA needed to scan and test-join
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
        ESP_ERROR_CHECK(esp_wifi_start());
        captive_dns_start();
        ESP_LOGI(TAG, "setup mode: %s", s_st.ap_ssid);
    }
    notify();
    return ESP_OK;
}

int net_scan(net_ap_t *out, int max)
{
    if (s_st.mode != NET_SETUP) return 0;
    wifi_scan_config_t sc = {0};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) return 0;
    uint16_t n = 20;
    wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
    if (!recs) return 0;
    esp_wifi_scan_get_ap_records(&n, recs);
    int k = 0;
    for (int i = 0; i < n && k < max; i++) {
        if (!recs[i].ssid[0]) continue;
        bool dup = false;
        for (int j = 0; j < k; j++) if (!strcmp(out[j].ssid, (char *)recs[i].ssid)) dup = true;
        if (dup) continue;
        strncpy(out[k].ssid, (char *)recs[i].ssid, sizeof(out[k].ssid) - 1);
        out[k].ssid[sizeof(out[k].ssid) - 1] = 0;
        out[k].rssi = recs[i].rssi;
        out[k].secure = recs[i].authmode != WIFI_AUTH_OPEN;
        k++;
    }
    free(recs);
    return k;
}

esp_err_t net_try_join(const char *ssid, const char *pass, char *ip_out, size_t ip_len)
{
    if (s_st.mode != NET_SETUP || s_joining) return ESP_ERR_INVALID_STATE;
    wifi_config_t sta = {0};
    copy_field(sta.sta.ssid, sizeof(sta.sta.ssid), ssid);
    copy_field(sta.sta.password, sizeof(sta.sta.password), pass);
    s_joining = true;
    s_retries = 0;
    xEventGroupClearBits(s_ev, GOT_IP | FAILED);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    esp_wifi_connect();
    EventBits_t b = xEventGroupWaitBits(s_ev, GOT_IP | FAILED, pdTRUE, pdFALSE, pdMS_TO_TICKS(20000));
    s_joining = false;
    if (!(b & GOT_IP)) {
        esp_wifi_disconnect();
        return ESP_FAIL;
    }
    snprintf(ip_out, ip_len, "%s", s_st.ip);
    strncpy(s_st.ssid, ssid, sizeof(s_st.ssid) - 1);
    return storage_wifi_set(ssid, pass);
}

bool net_home_visible(int timeout_ms)
{
    // Used after waking from away-sleep: is the saved home network in range?
    char ssid[33] = {0}, pass[65] = {0};
    if (!storage_wifi_get(ssid, sizeof ssid, pass, sizeof pass)) return false;
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wc = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wc) != ESP_OK) return false;
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    wifi_scan_config_t sc = {.ssid = (uint8_t *)ssid, .scan_time.active = {.min = 60, .max = 120}};
    bool found = false;
    if (esp_wifi_scan_start(&sc, true) == ESP_OK) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        found = n > 0;
        esp_wifi_clear_ap_list();
    }
    esp_wifi_stop();
    esp_wifi_deinit();
    (void)timeout_ms;
    return found;
}
