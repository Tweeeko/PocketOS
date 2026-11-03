// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#include "ha_client.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "app.h"
#include "voice.h"
#include "ui.h"

static const char *TAG = "ha";
#define MAX_MSG (1024 * 1024)   // get_states / registries can be large; buffer lives in PSRAM

static esp_websocket_client_handle_t s_ws;
static SemaphoreHandle_t s_ctx_lock;
static ha_ctx_t s_ctx;
static bool s_synced, s_auth_failed;
static int64_t s_fail_since;   // first failed attempt since the last good connection (0 = none)
static char *s_buf;
static size_t s_cap, s_len;
static char s_token[320];
static esp_timer_handle_t s_stop_timer;
static void stop_client(void);
static void stop_cb(void *arg) { stop_client(); }
static esp_timer_handle_t s_reg_timer;
static void send_text(esp_websocket_client_handle_t ws, char *msg);
static void reg_cb(void *arg)
{
    if (!s_ws || !s_synced) return;
    for (int st = 1; st <= 3; st++) {   // areas, entity registry, device registry
        xSemaphoreTake(s_ctx_lock, portMAX_DELAY);
        char *msg = ha_msg_sync(&s_ctx, st);
        xSemaphoreGive(s_ctx_lock);
        send_text(s_ws, msg);
    }
}

bool ha_ws_url(const char *u, char *out, size_t len)
{
    const char *rest;
    const char *scheme;
    if (!strncmp(u, "http://", 7)) { scheme = "ws://"; rest = u + 7; }
    else if (!strncmp(u, "https://", 8)) { scheme = "wss://"; rest = u + 8; }
    else return false;
    size_t n = strlen(rest);
    while (n && rest[n - 1] == '/') n--;
    if (!n || n > 80) return false;
    snprintf(out, len, "%s%.*s/api/websocket", scheme, (int)n, rest);
    return true;
}

bool ha_client_connected(void) { return s_ws && s_synced; }

static void send_text(esp_websocket_client_handle_t ws, char *msg)
{
    if (!msg) return;
    if (esp_websocket_client_send_text(ws, msg, (int)strlen(msg), pdMS_TO_TICKS(5000)) < 0) ESP_LOGW(TAG, "send failed");
    free(msg);
}

static void handle_message(const char *txt, size_t len)
{
    ha_event_t ev;
    model_t *m = app_model_lock();
    xSemaphoreTake(s_ctx_lock, portMAX_DELAY);
    ha_handle(&s_ctx, m, txt, len, &ev);
    xSemaphoreGive(s_ctx_lock);
    app_model_unlock();

    switch (ev.kind) {
    case HA_EV_AUTH_REQUIRED:
        send_text(s_ws, ha_msg_auth(s_token));
        break;
    case HA_EV_AUTH_OK:
        ESP_LOGI(TAG, "authenticated (HA %s)", s_ctx.ha_version);
        for (int st = 0; st < HA_SYNC_STEPS; st++) {
            xSemaphoreTake(s_ctx_lock, portMAX_DELAY);
            char *msg = ha_msg_sync(&s_ctx, st);
            xSemaphoreGive(s_ctx_lock);
            send_text(s_ws, msg);
        }
        break;
    case HA_EV_AUTH_INVALID:
        // Stop retrying: Home Assistant can ban addresses after repeated failed logins.
        ESP_LOGE(TAG, "token rejected: %s", ev.text);
        s_auth_failed = true;
        app_set_ha(HA_ERROR, "Token was rejected");
        // The client can't be stopped from inside its own event handler: do it from a timer.
        esp_timer_start_once(s_stop_timer, 1000);
        break;
    case HA_EV_SYNCED:
        s_synced = true;
        s_fail_since = 0;
        app_set_ha(HA_OK, NULL);
        app_on_ha_synced();
        ESP_LOGI(TAG, "synced");
        break;
    case HA_EV_CALL_FAILED:
        // Not UI_LOCK here: the UI task may be waiting on this connection to send a command
        app_ui_post(UI_POST_TOAST, 2500, ev.text[0] ? ev.text : "Home Assistant refused that");
        break;
    case HA_EV_ERROR:
        ESP_LOGW(TAG, "error: %s", ev.text);
        break;
    case HA_EV_REGISTRY_CHANGED:
        // A room or device changed in Home Assistant. Changes come in bursts: wait for it to settle.
        esp_timer_stop(s_reg_timer);
        esp_timer_start_once(s_reg_timer, 2000 * 1000);
        break;
    case HA_EV_VOICE_READY: case HA_EV_VOICE_HEARD: case HA_EV_VOICE_REPLY: case HA_EV_VOICE_ERROR: case HA_EV_VOICE_END:
        voice_on_event(&ev);
        break;
    default: break;
    }
}

static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_websocket_event_data_t *d = (esp_websocket_event_data_t *)data;
    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        xSemaphoreTake(s_ctx_lock, portMAX_DELAY);
        ha_reset(&s_ctx);
        xSemaphoreGive(s_ctx_lock);
        s_synced = false;
        s_len = 0;
        app_set_ha(HA_CONNECTING, NULL);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
        s_synced = false;
        if (!s_fail_since) s_fail_since = esp_timer_get_time();
        if (!s_auth_failed && esp_timer_get_time() - s_fail_since <= 30LL * 1000000) app_set_ha(HA_CONNECTING, "Reconnecting...");
        else if (!s_auth_failed) app_set_ha(HA_ERROR, "Can't reach Home Assistant");
        break;
    case WEBSOCKET_EVENT_ERROR: {
        // Wi-Fi takes a few seconds after starting up, and Home Assistant restarts now and then:
        // only call it a problem once it has been unreachable for a while.
        s_synced = false;
        int64_t now = esp_timer_get_time();
        if (!s_fail_since) s_fail_since = now;
        if (s_auth_failed) break;
        if (now - s_fail_since > 30LL * 1000000) app_set_ha(HA_ERROR, "Can't reach Home Assistant");
        else app_set_ha(HA_CONNECTING, NULL);
        break;
    }
    case WEBSOCKET_EVENT_DATA: {
        // Text messages may arrive in pieces (payload_offset) and/or as continuation frames.
        if (d->op_code == 0x1 && d->payload_offset == 0) s_len = 0;          // new text message
        else if (d->op_code != 0x0 && d->op_code != 0x1) break;              // ping/pong/binary
        if (d->payload_len > MAX_MSG || s_len + (size_t)d->data_len > MAX_MSG) { ESP_LOGE(TAG, "message too large"); s_len = 0; break; }
        if (s_len + (size_t)d->data_len + 1 > s_cap) {
            size_t ncap = s_cap ? s_cap : 16384;
            while (ncap < s_len + (size_t)d->data_len + 1) ncap *= 2;
            char *nb = heap_caps_realloc(s_buf, ncap, MALLOC_CAP_SPIRAM);
            if (!nb) { ESP_LOGE(TAG, "out of memory"); s_len = 0; break; }
            s_buf = nb;
            s_cap = ncap;
        }
        memcpy(s_buf + s_len, d->data_ptr, (size_t)d->data_len);
        s_len += (size_t)d->data_len;
        bool frame_done = d->payload_offset + d->data_len >= d->payload_len;
        if (frame_done && d->fin) {
            s_buf[s_len] = 0;
            handle_message(s_buf, s_len);
            s_len = 0;
        }
        break;
    }
    }
}

static void stop_client(void)
{
    if (!s_ws) return;
    esp_websocket_client_stop(s_ws);
    esp_websocket_client_destroy(s_ws);
    s_ws = NULL;
    s_synced = false;
}

static void start_client(void)
{
    settings_t *s = malloc(sizeof(settings_t));
    if (!s) return;
    app_settings_copy(s);
    char uri[128];
    bool have = s->ha_url[0] && s->ha_token[0] && ha_ws_url(s->ha_url, uri, sizeof uri);
    snprintf(s_token, sizeof s_token, "%s", s->ha_token);
    free(s);
    if (!have) { app_set_ha(HA_NOT_SET, NULL); return; }
    s_auth_failed = false;
    s_fail_since = 0;
    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .buffer_size = 4096,
        .task_stack = 8192,
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,   // only used for https:// (wss://)
    };
    s_ws = esp_websocket_client_init(&cfg);
    if (!s_ws) { app_set_ha(HA_ERROR, "Couldn't start the connection"); return; }
    esp_websocket_register_events(s_ws, WEBSOCKET_EVENT_ANY, ws_event, NULL);
    app_set_ha(HA_CONNECTING, NULL);
    esp_websocket_client_start(s_ws);
    ESP_LOGI(TAG, "connecting to %s", uri);
}

void ha_client_start(void)
{
    if (!s_ctx_lock) {
        s_ctx_lock = xSemaphoreCreateMutex();
        ha_init(&s_ctx);
        const esp_timer_create_args_t ta = {.callback = stop_cb, .name = "ha_stop"};
        esp_timer_create(&ta, &s_stop_timer);
        const esp_timer_create_args_t tr = {.callback = reg_cb, .name = "ha_reg"};
        esp_timer_create(&tr, &s_reg_timer);
    }
    start_client();
}

void ha_client_reconfigure(void)
{
    stop_client();
    start_client();
}

bool ha_client_action(model_t *m, int i, act_t act, float value)
{
    if (!ha_client_connected()) return false;
    bool has;
    float nl;
    xSemaphoreTake(s_ctx_lock, portMAX_DELAY);
    char *msg = ha_msg_action(&s_ctx, m, i, act, value, &has, &nl);
    xSemaphoreGive(s_ctx_lock);
    if (!msg) return false;
    int r = esp_websocket_client_send_text(s_ws, msg, (int)strlen(msg), pdMS_TO_TICKS(3000));
    free(msg);
    if (r < 0) return false;
    model_set_pending(m, i, app_ms(), has, nl);
    return true;
}

/* ---------- voice ---------- */

bool ha_client_voice_start(void)
{
    if (!ha_client_connected()) return false;
    xSemaphoreTake(s_ctx_lock, portMAX_DELAY);
    char *msg = ha_msg_voice_start(&s_ctx, 16000, app_speak_settings(NULL, NULL, 0));
    xSemaphoreGive(s_ctx_lock);
    send_text(s_ws, msg);
    return true;
}

void ha_client_voice_audio(int handler_id, const int16_t *pcm, size_t samples)
{
    // Binary frame: 1-byte handler id, then raw PCM (16 kHz, 16-bit, mono)
    static uint8_t frame[1 + 2 * 640];
    size_t n = samples > 640 ? 640 : samples;
    frame[0] = (uint8_t)handler_id;
    memcpy(frame + 1, pcm, n * 2);
    if (s_ws) esp_websocket_client_send_bin(s_ws, (const char *)frame, (int)(1 + n * 2), pdMS_TO_TICKS(1000));
}

void ha_client_voice_end(int handler_id)
{
    uint8_t b = (uint8_t)handler_id;   // an empty chunk ends the audio stream
    if (s_ws) esp_websocket_client_send_bin(s_ws, (const char *)&b, 1, pdMS_TO_TICKS(1000));
}

/* ---------- connection test for the website ---------- */

typedef struct { EventGroupHandle_t ev; const char *token; int devices; char msg[96]; char *buf; size_t len, cap; } test_t;
#define T_OK BIT0
#define T_FAIL BIT1

static void test_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    test_t *t = (test_t *)arg;
    esp_websocket_event_data_t *d = (esp_websocket_event_data_t *)data;
    esp_websocket_client_handle_t ws = (esp_websocket_client_handle_t)d->client;
    if (id == WEBSOCKET_EVENT_ERROR) { snprintf(t->msg, sizeof t->msg, "Couldn't reach Home Assistant at that address"); xEventGroupSetBits(t->ev, T_FAIL); return; }
    if (id != WEBSOCKET_EVENT_DATA || (d->op_code != 0x1 && d->op_code != 0x0)) return;
    if (d->payload_offset == 0 && d->op_code == 0x1) t->len = 0;
    if (t->len + (size_t)d->data_len + 1 > t->cap) {
        size_t nc = t->cap ? t->cap * 2 : 16384;
        while (nc < t->len + (size_t)d->data_len + 1) nc *= 2;
        if (nc > MAX_MSG) { snprintf(t->msg, sizeof t->msg, "Reply too large"); xEventGroupSetBits(t->ev, T_FAIL); return; }
        char *nb = heap_caps_realloc(t->buf, nc, MALLOC_CAP_SPIRAM);
        if (!nb) return;
        t->buf = nb; t->cap = nc;
    }
    memcpy(t->buf + t->len, d->data_ptr, (size_t)d->data_len);
    t->len += (size_t)d->data_len;
    if (d->payload_offset + d->data_len < d->payload_len || !d->fin) return;
    t->buf[t->len] = 0;
    cJSON *j = cJSON_ParseWithLength(t->buf, t->len);
    t->len = 0;
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(j, "type"));
    if (type && !strcmp(type, "auth_required")) {
        char *a = ha_msg_auth(t->token);
        esp_websocket_client_send_text(ws, a, (int)strlen(a), pdMS_TO_TICKS(3000));
        free(a);
    } else if (type && !strcmp(type, "auth_ok")) {
        const char *q = "{\"id\":1,\"type\":\"get_states\"}";
        esp_websocket_client_send_text(ws, q, (int)strlen(q), pdMS_TO_TICKS(3000));
    } else if (type && !strcmp(type, "auth_invalid")) {
        snprintf(t->msg, sizeof t->msg, "Home Assistant rejected the token");
        xEventGroupSetBits(t->ev, T_FAIL);
    } else if (type && !strcmp(type, "result")) {
        EXT_RAM_BSS_ATTR static model_t probe;   // count what PocketOS would show (~80 KB, in PSRAM)
        model_clear(&probe);
        const cJSON *st;
        cJSON_ArrayForEach(st, cJSON_GetObjectItem(j, "result")) model_apply_state(&probe, st);
        t->devices = probe.n;
        xEventGroupSetBits(t->ev, T_OK);
    }
    cJSON_Delete(j);
}

bool ha_client_test(const char *url, const char *token, char *msg, size_t len, int *devices)
{
    char uri[128];
    if (!ha_ws_url(url, uri, sizeof uri)) { snprintf(msg, len, "The address should start with http:// or https://"); return false; }
    test_t t = {.ev = xEventGroupCreate(), .token = token};
    esp_websocket_client_config_t cfg = {.uri = uri, .buffer_size = 4096, .task_stack = 8192, .disable_auto_reconnect = true,
                                         .network_timeout_ms = 8000, .crt_bundle_attach = esp_crt_bundle_attach};
    esp_websocket_client_handle_t ws = esp_websocket_client_init(&cfg);
    if (!ws) { vEventGroupDelete(t.ev); snprintf(msg, len, "Out of memory"); return false; }
    esp_websocket_register_events(ws, WEBSOCKET_EVENT_ANY, test_event, &t);
    esp_websocket_client_start(ws);
    EventBits_t b = xEventGroupWaitBits(t.ev, T_OK | T_FAIL, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    esp_websocket_client_stop(ws);
    esp_websocket_client_destroy(ws);
    vEventGroupDelete(t.ev);
    heap_caps_free(t.buf);
    if (b & T_OK) { *devices = t.devices; snprintf(msg, len, "Connected. Found %d devices.", t.devices); return true; }
    if (!(b & T_FAIL)) snprintf(msg, len, "No answer from Home Assistant at that address");
    else snprintf(msg, len, "%s", t.msg);
    return false;
}
