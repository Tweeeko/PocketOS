// Local settings website + API v2 (docs/api.md). Served by the device itself, so the browser
// talks to the same address it loaded the page from (no CORS, no internet needed).
#include "web.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "app.h"
#include "storage.h"
#include "net.h"
#include "pmu.h"
#include "ha_client.h"
#include "ui.h"
#include "power.h"
#include "updater.h"

static const char *TAG = "web";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

#define MAX_TOKENS 4
// Saved to flash so phones stay paired across restarts. "Forget all phones" clears them.
static struct { char tok[MAX_TOKENS][33]; int next; } s_auth;
#define s_tokens s_auth.tok
#define s_next_token s_auth.next
static char s_pin[7];
static int s_bad_pins;
static int64_t s_locked_until_us, s_reset_deadline_us;
static esp_timer_handle_t s_restart_timer, s_reset_timer;

/* ---------- helpers ---------- */

static void new_pin(void)
{
    snprintf(s_pin, sizeof(s_pin), "%06lu", (unsigned long)(esp_random() % 1000000));
    app_set_pin(s_pin);
}

static bool same(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    unsigned char d = la != lb;
    for (size_t i = 0; i < la && i < lb; i++) d |= (unsigned char)(a[i] ^ b[i]);
    return d == 0;
}

static esp_err_t send_json(httpd_req_t *req, const char *status, cJSON *obj)
{
    char *s = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t r = httpd_resp_sendstr(req, s ? s : "{}");
    free(s);
    return r;
}

static esp_err_t send_error(httpd_req_t *req, const char *status, const char *msg, const char *field)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "error", msg);
    if (field) cJSON_AddStringToObject(o, "field", field);
    return send_json(req, status, o);
}

static cJSON *ok_obj(void) { cJSON *o = cJSON_CreateObject(); cJSON_AddBoolToObject(o, "ok", true); return o; }

static bool authorised(httpd_req_t *req)
{
    char h[64];
    if (httpd_req_get_hdr_value_str(req, "Authorization", h, sizeof(h)) != ESP_OK) return false;
    if (strncmp(h, "Bearer ", 7) != 0) return false;
    for (int i = 0; i < MAX_TOKENS; i++) if (s_tokens[i][0] && same(h + 7, s_tokens[i])) return true;
    return false;
}

#define REQUIRE_AUTH(req) do { if (!authorised(req)) return send_error(req, "401 Unauthorized", "Pair with the PIN on the device first", NULL); } while (0)

static cJSON *read_json(httpd_req_t *req, int limit)
{
    if (req->content_len <= 0 || req->content_len > (size_t)limit) return NULL;
    char *buf = heap_caps_calloc(1, req->content_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) return NULL;
    size_t got = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, buf + got, req->content_len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) { heap_caps_free(buf); return NULL; }
        got += (size_t)r;
    }
    cJSON *j = cJSON_Parse(buf);
    heap_caps_free(buf);
    return j;
}

static bool in_setup(void) { app_status_t st; app_status(&st); return st.setup_mode; }

static const char *ha_text(ha_status_t s) { return s == HA_OK ? "connected" : s == HA_CONNECTING ? "connecting" : s == HA_ERROR ? "error" : "not_set"; }

/* ---------- pages ---------- */

static esp_err_t h_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

static esp_err_t h_info(httpd_req_t *req)
{
    app_status_t st;
    app_status(&st);
    net_status_t n;
    net_status(&n);
    settings_t *s = malloc(sizeof(settings_t));
    if (!s) return send_error(req, "500 Internal Server Error", "Out of memory", NULL);
    app_settings_copy(s);
    int devices = 0, fresh = 0;
    const settings_t *sl = app_settings_lock();
    model_t *m = app_model_lock();
    for (int i = 0; i < m->n; i++) { if (model_visible(m, sl, i)) devices++; if (model_is_new(m, sl, i)) fresh++; }
    app_model_unlock();
    app_settings_unlock();

    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "api", 2);
    cJSON_AddNumberToObject(o, "schema", SETTINGS_SCHEMA);
    cJSON_AddStringToObject(o, "id", device_id());
    cJSON_AddStringToObject(o, "name", s->name);
    cJSON_AddStringToObject(o, "firmware", st.firmware);
    cJSON_AddStringToObject(o, "mode", st.setup_mode ? "setup" : "normal");
    cJSON *w = cJSON_AddObjectToObject(o, "wifi");
    cJSON_AddStringToObject(w, "ssid", st.setup_mode ? n.ap_ssid : n.ssid);
    cJSON_AddNumberToObject(w, "rssi", n.rssi);
    cJSON_AddStringToObject(w, "ip", n.ip);
    cJSON_AddStringToObject(o, "host", st.host);
    cJSON *b = cJSON_AddObjectToObject(o, "battery");
    if (st.battery >= 0) cJSON_AddNumberToObject(b, "percent", st.battery); else cJSON_AddNullToObject(b, "percent");
    cJSON_AddBoolToObject(b, "charging", st.charging);
    cJSON_AddBoolToObject(b, "plugged", st.plugged);
    if (st.battery_mv) cJSON_AddNumberToObject(b, "mv", st.battery_mv);
    if (st.battery_hours >= 0) cJSON_AddNumberToObject(b, "hours_left", st.battery_hours);
    cJSON *ha = cJSON_AddObjectToObject(o, "ha");
    cJSON_AddStringToObject(ha, "status", ha_text(st.ha));
    cJSON_AddStringToObject(ha, "error", st.ha_error);
    cJSON_AddNumberToObject(ha, "devices", devices);
    cJSON_AddNumberToObject(ha, "new", fresh);
    cJSON_AddNumberToObject(o, "uptime_s", (double)(esp_timer_get_time() / 1000000));
    if (!st.setup_mode) updater_info(o);
    free(s);
    return send_json(req, "200 OK", o);
}

static esp_err_t h_pair(httpd_req_t *req)
{
    int64_t now = esp_timer_get_time();
    if (now < s_locked_until_us) return send_error(req, "429 Too Many Requests", "Too many wrong codes. Wait a minute and try again.", NULL);
    cJSON *j = read_json(req, 256);
    const cJSON *pin = j ? cJSON_GetObjectItem(j, "pin") : NULL;
    bool ok = cJSON_IsString(pin) && same(pin->valuestring, s_pin);
    cJSON_Delete(j);
    if (!ok) {
        if (++s_bad_pins >= 5) { s_bad_pins = 0; s_locked_until_us = now + 60LL * 1000000; }
        return send_error(req, "401 Unauthorized", "That code doesn't match the one on PocketOS", "pin");
    }
    s_bad_pins = 0;
    char *tok = s_tokens[s_next_token];
    s_next_token = (s_next_token + 1) % MAX_TOKENS;
    for (int i = 0; i < 16; i++) snprintf(tok + i * 2, 3, "%02x", (unsigned)(esp_random() & 0xff));
    storage_tokens_set(&s_auth, sizeof s_auth);
    new_pin();   // a code works once
    UI_LOCK(); ui_toast("Phone connected", 1500); UI_UNLOCK();
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "token", tok);
    return send_json(req, "200 OK", o);
}

static cJSON *config_json(void)
{
    settings_t *s = malloc(sizeof(settings_t));
    if (!s) return cJSON_CreateObject();
    app_settings_copy(s);
    cJSON *o = settings_to_json(s, false);   // never includes the token
    free(s);
    return o;
}

static esp_err_t h_config_get(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    return send_json(req, "200 OK", config_json());
}

static esp_err_t h_config_post(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    cJSON *j = read_json(req, 32768);
    if (!j) return send_error(req, "400 Bad Request", "That wasn't valid settings data", NULL);
    settings_err_t err;
    bool ok = app_update_settings(j, false, &err);
    cJSON_Delete(j);
    if (!ok) return send_error(req, "400 Bad Request", err.message ? err.message : "Not saved", err.field);
    return send_json(req, "200 OK", config_json());   // confirmation = what is now saved
}

static const char *controls_text(const entity_t *e)
{
    switch (e->dom) {
    case D_LIGHT: return (e->caps & CAP_LEVEL) ? "On, off and brightness" : "On and off";
    case D_SWITCH: return "On and off";
    case D_FAN: return (e->caps & CAP_LEVEL) ? "On, off and speed" : "On and off";
    case D_CLIMATE: return (e->caps & CAP_LEVEL) ? "Target temperature and heating on/off" : "Heating on and off";
    case D_MEDIA: return (e->caps & CAP_LEVEL) ? ((e->caps & CAP_PREVNEXT) ? "Play, pause, skip and volume" : "Play, pause and volume") : "Play and pause";
    case D_COVER: return (e->caps & CAP_LEVEL) ? "Open, close and position" : "Open and close";
    case D_LOCK: return "Lock and unlock";
    case D_SENSOR: return "Shows a live reading";
    case D_SCENE: case D_SCRIPT: return "Runs with one tap";
    default: return "";
    }
}

static esp_err_t h_entities(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    cJSON *o = cJSON_CreateObject();
    cJSON *areas = cJSON_AddArrayToObject(o, "areas");
    cJSON *arr = cJSON_AddArrayToObject(o, "entities");
    const settings_t *s = app_settings_lock();
    model_t *m = app_model_lock();
    for (int a = 0; a < m->na; a++) cJSON_AddItemToArray(areas, cJSON_CreateString(m->a[a].name));
    for (int i = 0; i < m->n; i++) {
        const entity_t *e = &m->e[i];
        if (e->registry_hidden) continue;
        char st[48];
        model_status(e, st, sizeof st);
        cJSON *x = cJSON_CreateObject();
        cJSON_AddStringToObject(x, "id", e->id);
        cJSON_AddStringToObject(x, "name", e->name);
        cJSON_AddStringToObject(x, "domain", dom_name(e->dom));
        cJSON_AddStringToObject(x, "area", e->area >= 0 ? m->a[e->area].name : "");
        cJSON_AddStringToObject(x, "status", st);
        cJSON_AddStringToObject(x, "controls", controls_text(e));
        cJSON_AddBoolToObject(x, "fav", settings_has((char (*)[ID_LEN])s->fav, s->nfav, e->id));
        cJSON_AddBoolToObject(x, "hidden", settings_has((char (*)[ID_LEN])s->hidden, s->nhidden, e->id));
        cJSON_AddBoolToObject(x, "new", model_is_new(m, s, i));
        cJSON_AddItemToArray(arr, x);
    }
    app_model_unlock();
    app_settings_unlock();
    return send_json(req, "200 OK", o);
}

static esp_err_t h_entities_post(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    cJSON *j = read_json(req, 4096);
    if (!j) return send_error(req, "400 Bad Request", "Bad request", NULL);
    bool ok = true;
    if (cJSON_IsTrue(cJSON_GetObjectItem(j, "all_known"))) {
        app_mark_all_known();
    } else {
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(j, "id"));
        if (!id) { cJSON_Delete(j); return send_error(req, "400 Bad Request", "Missing device id", "id"); }
        const char *keys[] = {"fav", "hidden", "known"};
        for (int k = 0; k < 3 && ok; k++) {
            const cJSON *v = cJSON_GetObjectItem(j, keys[k]);
            if (cJSON_IsBool(v)) ok = app_set_flag(keys[k], id, cJSON_IsTrue(v));
        }
    }
    cJSON_Delete(j);
    if (!ok) return send_error(req, "400 Bad Request", "Couldn't save that (list full?)", NULL);
    return send_json(req, "200 OK", ok_obj());
}

static esp_err_t h_ha_post(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    cJSON *j = read_json(req, 2048);
    const char *url = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "url")) : NULL;
    const char *token = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "token")) : NULL;
    char ws[128];
    if (!url || !ha_ws_url(url, ws, sizeof ws)) { cJSON_Delete(j); return send_error(req, "400 Bad Request", "The address should look like http://homeassistant.local:8123", "url"); }
    cJSON *patch = cJSON_CreateObject();
    cJSON_AddStringToObject(patch, "ha_url", url);
    if (token && token[0]) cJSON_AddStringToObject(patch, "ha_token", token);
    settings_err_t err;
    bool ok = app_update_settings(patch, true, &err);   // reconnects when anything changed
    cJSON_Delete(patch);
    cJSON_Delete(j);
    if (!ok) return send_error(req, "400 Bad Request", err.message ? err.message : "Not saved", err.field);
    return send_json(req, "200 OK", ok_obj());
}

static esp_err_t h_ha_test(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    cJSON *j = read_json(req, 2048);
    const char *url = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "url")) : NULL;
    const char *token = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "token")) : NULL;
    settings_t *s = malloc(sizeof(settings_t));
    if (!s) { cJSON_Delete(j); return send_error(req, "500 Internal Server Error", "Out of memory", NULL); }
    app_settings_copy(s);
    char msg[96];
    int devices = 0;
    bool ok = ha_client_test(url ? url : s->ha_url, (token && token[0]) ? token : s->ha_token, msg, sizeof msg, &devices);
    free(s);
    cJSON_Delete(j);
    if (!ok) return send_error(req, "400 Bad Request", msg, "url");
    cJSON *o = ok_obj();
    cJSON_AddNumberToObject(o, "devices", devices);
    cJSON_AddStringToObject(o, "message", msg);
    return send_json(req, "200 OK", o);
}

static esp_err_t h_ha_disconnect(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ha_token", "");
    settings_err_t err;
    bool ok = app_update_settings(p, true, &err);
    cJSON_Delete(p);
    if (!ok) return send_error(req, "500 Internal Server Error", "Couldn't save", NULL);
    model_t *m = app_model_lock();
    model_clear(m);
    app_model_unlock();
    return send_json(req, "200 OK", ok_obj());
}

static esp_err_t h_scan(httpd_req_t *req)
{
    if (!in_setup()) return send_error(req, "403 Forbidden", "Only available during first setup", NULL);
    net_ap_t aps[15];
    int n = net_scan(aps, 15);
    cJSON *o = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(o, "networks");
    for (int i = 0; i < n; i++) {
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "ssid", aps[i].ssid);
        cJSON_AddNumberToObject(e, "rssi", aps[i].rssi);
        cJSON_AddBoolToObject(e, "secure", aps[i].secure);
        cJSON_AddItemToArray(arr, e);
    }
    return send_json(req, "200 OK", o);
}

static void restart_cb(void *arg) { esp_restart(); }

static esp_err_t h_wifi(httpd_req_t *req)
{
    if (!in_setup()) return send_error(req, "403 Forbidden", "Only available during first setup", NULL);
    cJSON *j = read_json(req, 512);
    const cJSON *ssid = j ? cJSON_GetObjectItem(j, "ssid") : NULL;
    const cJSON *pass = j ? cJSON_GetObjectItem(j, "password") : NULL;
    if (!cJSON_IsString(ssid) || !ssid->valuestring[0] || strlen(ssid->valuestring) > 32 ||
        (pass && (!cJSON_IsString(pass) || strlen(pass->valuestring) > 63))) {
        cJSON_Delete(j);
        return send_error(req, "400 Bad Request", "Choose a network and enter its password", "ssid");
    }
    UI_LOCK(); ui_toast("Joining your Wi-Fi...", 0); UI_UNLOCK();
    char ip[16] = "";
    esp_err_t err = net_try_join(ssid->valuestring, pass ? pass->valuestring : "", ip, sizeof(ip));
    cJSON_Delete(j);
    if (err != ESP_OK) {
        UI_LOCK(); ui_toast("Couldn't join that network", 3000); UI_UNLOCK();
        return send_error(req, "400 Bad Request", "Couldn't join that network. Check the password, and that it's 2.4 GHz.", "password");
    }
    UI_LOCK(); ui_toast("Joined! Restarting...", 0); UI_UNLOCK();
    net_status_t n;
    net_status(&n);
    cJSON *o = ok_obj();
    cJSON_AddStringToObject(o, "ip", ip);
    cJSON_AddStringToObject(o, "host", n.host_local);
    esp_timer_start_once(s_restart_timer, 3 * 1000 * 1000);
    return send_json(req, "200 OK", o);
}

// Moving house or new router: forget only the Wi-Fi and restart into setup mode. Settings stay.
static esp_err_t h_wifi_forget(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    if (storage_wifi_set("", "") != ESP_OK) return send_error(req, "500 Internal Server Error", "Couldn't save", NULL);
    UI_LOCK(); ui_toast("Wi-Fi forgotten. Restarting into setup...", 0); UI_UNLOCK();
    esp_timer_start_once(s_restart_timer, 1500 * 1000);
    return send_json(req, "202 Accepted", ok_obj());
}

static esp_err_t h_restart(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    UI_LOCK(); ui_toast("Restarting...", 0); UI_UNLOCK();
    esp_timer_start_once(s_restart_timer, 1000 * 1000);
    return send_json(req, "202 Accepted", ok_obj());
}

static void reset_expired_cb(void *arg)
{
    if (s_reset_deadline_us) { s_reset_deadline_us = 0; UI_LOCK(); ui_reset_prompt(false); ui_toast("Reset cancelled", 1500); UI_UNLOCK(); }
}

static esp_err_t h_reset(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    s_reset_deadline_us = esp_timer_get_time() + 15LL * 1000000;
    esp_timer_stop(s_reset_timer);
    esp_timer_start_once(s_reset_timer, 15 * 1000 * 1000);
    UI_LOCK(); ui_reset_prompt(true); UI_UNLOCK();
    cJSON *o = ok_obj();
    cJSON_AddStringToObject(o, "confirm", "Press BOOT on the device within 15 seconds");
    return send_json(req, "202 Accepted", o);
}

bool web_reset_pending(void) { return s_reset_deadline_us && esp_timer_get_time() < s_reset_deadline_us; }

void web_reset_confirmed(void)
{
    s_reset_deadline_us = 0;
    esp_timer_stop(s_reset_timer);
    UI_LOCK(); ui_reset_prompt(false); ui_toast("Erasing...", 0); UI_UNLOCK();
    storage_factory_reset();
    esp_timer_start_once(s_restart_timer, 800 * 1000);
}

static esp_err_t h_unpair_all(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    memset(&s_auth, 0, sizeof s_auth);
    storage_tokens_set(&s_auth, sizeof s_auth);
    UI_LOCK(); ui_toast("All phones forgotten", 2000); UI_UNLOCK();
    return send_json(req, "200 OK", ok_obj());
}

static esp_err_t h_battery(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    return send_json(req, "200 OK", power_battery_json());
}

static esp_err_t h_update_check(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    if (!updater_check_now()) return send_error(req, "409 Conflict", "PocketOS is already checking or updating", NULL);
    return send_json(req, "202 Accepted", ok_obj());
}

static esp_err_t h_update_install(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    const char *why = NULL;
    if (!updater_install(&why)) return send_error(req, "400 Bad Request", why, NULL);
    return send_json(req, "202 Accepted", ok_obj());
}

static esp_err_t h_export(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"pocketos-settings.json\"");
    return send_json(req, "200 OK", config_json());
}

static esp_err_t h_ota(httpd_req_t *req)
{
    REQUIRE_AUTH(req);
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) return send_error(req, "500 Internal Server Error", "No spare firmware slot", NULL);
    if (req->content_len <= 0 || req->content_len > part->size)
        return send_error(req, "400 Bad Request", "That file is the wrong size for PocketOS firmware", NULL);
    esp_ota_handle_t h;
    if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &h) != ESP_OK)
        return send_error(req, "500 Internal Server Error", "Couldn't start the update", NULL);
    UI_LOCK(); ui_toast("Updating...", 0); UI_UNLOCK();
    char *buf = malloc(4096);
    size_t left = req->content_len;
    esp_err_t err = buf ? ESP_OK : ESP_ERR_NO_MEM;
    int timeouts = 0;
    while (err == ESP_OK && left > 0) {
        int r = httpd_req_recv(req, buf, left < 4096 ? left : 4096);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) { if (++timeouts > 5) err = ESP_ERR_TIMEOUT; continue; }
        if (r <= 0) { err = ESP_FAIL; break; }
        timeouts = 0;
        err = esp_ota_write(h, buf, (size_t)r);
        left -= (size_t)r;
    }
    free(buf);
    if (err == ESP_OK) err = esp_ota_end(h); else esp_ota_abort(h);
    if (err == ESP_OK) err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
        UI_LOCK(); ui_toast("Update failed. Nothing changed.", 4000); UI_UNLOCK();
        return send_error(req, "400 Bad Request", "Update failed. PocketOS is still on the old version.", NULL);
    }
    UI_LOCK(); ui_toast("Updated! Restarting...", 0); UI_UNLOCK();
    esp_timer_start_once(s_restart_timer, 1500 * 1000);
    return send_json(req, "200 OK", ok_obj());
}

static esp_err_t not_found(httpd_req_t *req, httpd_err_code_t code)
{
    if (in_setup()) {   // captive portal: every unknown page leads to the setup page
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        return httpd_resp_send(req, NULL, 0);
    }
    return send_error(req, "404 Not Found", "Not found", NULL);
}

esp_err_t web_start(void)
{
    const esp_timer_create_args_t a = {.callback = restart_cb, .name = "restart"};
    const esp_timer_create_args_t b = {.callback = reset_expired_cb, .name = "reset_window"};
    esp_timer_create(&a, &s_restart_timer);
    esp_timer_create(&b, &s_reset_timer);
    if (!storage_tokens_get(&s_auth, sizeof s_auth) || s_auth.next < 0 || s_auth.next >= MAX_TOKENS) memset(&s_auth, 0, sizeof s_auth);
    for (int i = 0; i < MAX_TOKENS; i++) s_auth.tok[i][32] = 0;
    new_pin();

    httpd_handle_t srv = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 24;
    cfg.stack_size = 10240;
    cfg.recv_wait_timeout = 30;
    cfg.send_wait_timeout = 15;
    cfg.lru_purge_enable = true;
    esp_err_t err = httpd_start(&srv, &cfg);
    if (err != ESP_OK) return err;

    const httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = h_index},
        {.uri = "/api/v2/info", .method = HTTP_GET, .handler = h_info},
        {.uri = "/api/v2/pair", .method = HTTP_POST, .handler = h_pair},
        {.uri = "/api/v2/config", .method = HTTP_GET, .handler = h_config_get},
        {.uri = "/api/v2/config", .method = HTTP_POST, .handler = h_config_post},
        {.uri = "/api/v2/entities", .method = HTTP_GET, .handler = h_entities},
        {.uri = "/api/v2/entities", .method = HTTP_POST, .handler = h_entities_post},
        {.uri = "/api/v2/ha", .method = HTTP_POST, .handler = h_ha_post},
        {.uri = "/api/v2/ha/test", .method = HTTP_POST, .handler = h_ha_test},
        {.uri = "/api/v2/ha/disconnect", .method = HTTP_POST, .handler = h_ha_disconnect},
        {.uri = "/api/v2/wifi/scan", .method = HTTP_GET, .handler = h_scan},
        {.uri = "/api/v2/wifi", .method = HTTP_POST, .handler = h_wifi},
        {.uri = "/api/v2/wifi/forget", .method = HTTP_POST, .handler = h_wifi_forget},
        {.uri = "/api/v2/restart", .method = HTTP_POST, .handler = h_restart},
        {.uri = "/api/v2/reset", .method = HTTP_POST, .handler = h_reset},
        {.uri = "/api/v2/unpair_all", .method = HTTP_POST, .handler = h_unpair_all},
        {.uri = "/api/v2/battery", .method = HTTP_GET, .handler = h_battery},
        {.uri = "/api/v2/update/check", .method = HTTP_POST, .handler = h_update_check},
        {.uri = "/api/v2/update/install", .method = HTTP_POST, .handler = h_update_install},
        {.uri = "/api/v2/export", .method = HTTP_GET, .handler = h_export},
        {.uri = "/api/v2/ota", .method = HTTP_POST, .handler = h_ota},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) httpd_register_uri_handler(srv, &uris[i]);
    httpd_register_err_handler(srv, HTTPD_404_NOT_FOUND, not_found);
    ESP_LOGI(TAG, "web server up");
    return ESP_OK;
}
