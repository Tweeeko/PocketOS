// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#include "voice.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_codec_dev.h"
#include "bsp/esp-bsp.h"
#include "app.h"
#include "ha_client.h"
#include "speaker.h"
#include "ui.h"

static const char *TAG = "voice";
#define RATE      16000
#define CHUNK     320                     // 20 ms of mono samples
#define MAX_SEC   10
#define BUF_SAMPLES (RATE * 3)            // audio kept until HA is ready (3 s)

static esp_codec_dev_handle_t s_mic;
static volatile bool s_recording;
static volatile int s_handler = -1;       // HA binary handler id once the run has started
static TaskHandle_t s_task;
static int16_t *s_pre;                    // PSRAM buffer for audio captured before HA is ready
static size_t s_pre_n;

static void ui_state(voice_state_t st, const char *txt)
{
    UI_LOCK();
    ui_voice(st, txt);
    UI_UNLOCK();
}

static bool mic_open(void)
{
    if (!s_mic) {
        s_mic = bsp_audio_codec_microphone_init();
        if (!s_mic) return false;
    }
    // Same settings as Waveshare's microphone demo: 16 kHz, 16-bit, two microphones
    esp_codec_dev_sample_info_t fs = {.sample_rate = RATE, .channel = 2, .bits_per_sample = 16};
    if (esp_codec_dev_open(s_mic, &fs) != ESP_CODEC_DEV_OK) return false;
    esp_codec_dev_set_in_gain(s_mic, 24.0f);
    return true;
}

static void flush_pre(int hid)
{
    for (size_t i = 0; i < s_pre_n; i += CHUNK) {
        size_t n = s_pre_n - i < CHUNK ? s_pre_n - i : CHUNK;
        ha_client_voice_audio(hid, s_pre + i, n);
    }
    s_pre_n = 0;
}

static void capture_task(void *arg)
{
    static int16_t stereo[CHUNK * 2];
    int16_t mono[CHUNK];
    int64_t chunks = 0;
    while (s_recording && chunks < (int64_t)MAX_SEC * RATE / CHUNK) {
        if (esp_codec_dev_read(s_mic, stereo, sizeof stereo) != ESP_CODEC_DEV_OK) break;
        for (int i = 0; i < CHUNK; i++) mono[i] = (int16_t)(((int32_t)stereo[2 * i] + stereo[2 * i + 1]) / 2);
        int hid = s_handler;
        if (hid >= 0) {
            if (s_pre_n) flush_pre(hid);
            ha_client_voice_audio(hid, mono, CHUNK);
        } else if (s_pre && s_pre_n + CHUNK <= BUF_SAMPLES) {
            memcpy(s_pre + s_pre_n, mono, sizeof mono);
            s_pre_n += CHUNK;
        }
        chunks++;
    }
    esp_codec_dev_close(s_mic);
    s_recording = false;
    // Wait briefly for HA to accept the run if the press was very short, then end the stream
    for (int i = 0; i < 50 && s_handler < 0; i++) vTaskDelay(pdMS_TO_TICKS(20));
    if (s_handler >= 0) {
        if (s_pre_n) flush_pre(s_handler);
        ha_client_voice_end(s_handler);
        ui_state(VOICE_THINKING, NULL);
    } else {
        ui_state(VOICE_ERROR, "Home Assistant didn't start listening");
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

void voice_start(void)
{
    if (s_recording || s_task) return;
    speaker_stop();                       // talking again cuts off a reply that is still playing
    if (!ha_client_connected()) {
        ui_state(VOICE_LISTENING, NULL);
        ui_state(VOICE_ERROR, "Connect Home Assistant first");
        return;
    }
    if (!s_pre) s_pre = heap_caps_malloc(BUF_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!mic_open()) { ui_state(VOICE_LISTENING, NULL); ui_state(VOICE_ERROR, "Microphone didn't start"); return; }
    s_handler = -1;
    s_pre_n = 0;
    s_recording = true;
    ui_state(VOICE_LISTENING, NULL);
    if (!ha_client_voice_start()) { s_recording = false; esp_codec_dev_close(s_mic); ui_state(VOICE_ERROR, "Home Assistant isn't connected"); return; }
    xTaskCreatePinnedToCore(capture_task, "voice", 6144, NULL, 6, &s_task, 1);
    ESP_LOGI(TAG, "listening");
}

void voice_stop(void)
{
    s_recording = false;   // capture task finishes and ends the stream
}

void voice_on_event(const ha_event_t *ev)
{
    switch (ev->kind) {
    case HA_EV_VOICE_READY: s_handler = ev->handler_id; break;
    // These arrive on the Home Assistant connection's task: post to the screen, don't lock it
    case HA_EV_VOICE_HEARD: app_ui_post(UI_POST_VOICE, VOICE_HEARD, ev->text); break;
    case HA_EV_VOICE_REPLY: app_ui_post(UI_POST_VOICE, VOICE_REPLY, ev->text); break;
    case HA_EV_VOICE_AUDIO: {
        int vol;
        char base[96], url[160];
        if (app_speak_settings(&vol, base, sizeof base) && ha_audio_url(base, ev->text, url, sizeof url))
            speaker_play(url, ev->mime, vol);
        break;
    }
    case HA_EV_VOICE_ERROR: s_recording = false; app_ui_post(UI_POST_VOICE, VOICE_ERROR, ev->text[0] ? ev->text : "Voice didn't work"); break;
    case HA_EV_VOICE_END: s_handler = -1; break;
    default: break;
    }
}
