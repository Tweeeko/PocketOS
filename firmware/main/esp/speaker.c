// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#include "speaker.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_codec_dev.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "speaker";
#define IN_CHUNK   2048
#define MAX_BYTES  (2 * 1024 * 1024)   // a spoken reply is a few seconds; refuse anything absurd

typedef struct { char url[160]; char mime[24]; int volume; } job_t;

static esp_codec_dev_handle_t s_spk;
static TaskHandle_t s_task;
static volatile bool s_stop;
static bool s_registered;

static esp_audio_simple_dec_type_t dec_type(const char *mime, const char *url)
{
    const char *dot = strrchr(url, '.');
    if (strstr(mime, "mpeg") || strstr(mime, "mp3") || (dot && !strcmp(dot, ".mp3"))) return ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
    if (strstr(mime, "wav") || (dot && !strcmp(dot, ".wav"))) return ESP_AUDIO_SIMPLE_DEC_TYPE_WAV;
    if (strstr(mime, "flac") || (dot && !strcmp(dot, ".flac"))) return ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC;
    return ESP_AUDIO_SIMPLE_DEC_TYPE_NONE;
}

static void play_task(void *arg)
{
    job_t *job = arg;
    esp_http_client_handle_t h = NULL;
    esp_audio_simple_dec_handle_t dec = NULL;
    uint8_t *in = NULL, *out = NULL;
    uint32_t out_cap = 4608;            // one MP3 frame of 16-bit stereo; grows if the decoder asks
    bool spk_open = false;
    const char *why = NULL;

    esp_audio_simple_dec_type_t type = dec_type(job->mime, job->url);
    if (type == ESP_AUDIO_SIMPLE_DEC_TYPE_NONE) { why = "audio type not supported"; goto done; }
    if (!s_registered) { esp_audio_simple_dec_register_default(); s_registered = true; }
    esp_audio_simple_dec_cfg_t dcfg = {.dec_type = type};
    if (esp_audio_simple_dec_open(&dcfg, &dec) != ESP_AUDIO_ERR_OK) { why = "decoder didn't start"; goto done; }

    in = heap_caps_malloc(IN_CHUNK, MALLOC_CAP_SPIRAM);
    out = heap_caps_malloc(out_cap, MALLOC_CAP_SPIRAM);
    if (!in || !out) { why = "out of memory"; goto done; }

    esp_http_client_config_t c = {
        .url = job->url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 8000,
        .user_agent = "PocketOS",
    };
    h = esp_http_client_init(&c);
    if (!h || esp_http_client_open(h, 0) != ESP_OK) { why = "couldn't reach Home Assistant"; goto done; }
    esp_http_client_fetch_headers(h);
    if (esp_http_client_get_status_code(h) != 200) { why = "Home Assistant didn't send the audio"; goto done; }

    if (!s_spk) s_spk = bsp_audio_codec_speaker_init();
    if (!s_spk) { why = "speaker didn't start"; goto done; }

    size_t total = 0;
    bool eos = false;
    while (!s_stop && !eos) {
        int n = esp_http_client_read(h, (char *)in, IN_CHUNK);
        if (n < 0) { why = "audio download stopped"; break; }
        if (n == 0) eos = true;
        total += (size_t)n;
        if (total > MAX_BYTES) { why = "reply too long"; break; }
        esp_audio_simple_dec_raw_t raw = {.buffer = in, .len = (uint32_t)n, .eos = eos};
        do {
            esp_audio_simple_dec_out_t frame = {.buffer = out, .len = out_cap};
            esp_audio_err_t r = esp_audio_simple_dec_process(dec, &raw, &frame);
            if (r == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                uint8_t *bigger = heap_caps_realloc(out, frame.needed_size, MALLOC_CAP_SPIRAM);
                if (!bigger) { why = "out of memory"; goto done; }
                out = bigger; out_cap = frame.needed_size;
                continue;
            }
            if (r != ESP_AUDIO_ERR_OK) { why = "audio couldn't be decoded"; goto done; }
            if (frame.decoded_size) {
                if (!spk_open) {
                    esp_audio_simple_dec_info_t info = {0};
                    esp_audio_simple_dec_get_info(dec, &info);
                    esp_codec_dev_sample_info_t fs = {.sample_rate = info.sample_rate, .channel = info.channel,
                                                      .bits_per_sample = info.bits_per_sample};
                    if (esp_codec_dev_open(s_spk, &fs) != ESP_CODEC_DEV_OK) { why = "speaker didn't open"; goto done; }
                    esp_codec_dev_set_out_vol(s_spk, job->volume);
                    spk_open = true;
                }
                esp_codec_dev_write(s_spk, out, (int)frame.decoded_size);
            }
            raw.len -= raw.consumed;
            raw.buffer += raw.consumed;
        } while (raw.len && !s_stop);
    }

done:
    if (why) ESP_LOGW(TAG, "spoken reply not played: %s", why);   // never the address: it carries a token
    if (spk_open) esp_codec_dev_close(s_spk);
    if (h) { esp_http_client_close(h); esp_http_client_cleanup(h); }
    if (dec) esp_audio_simple_dec_close(dec);
    heap_caps_free(in);
    heap_caps_free(out);
    free(job);
    s_task = NULL;
    vTaskDelete(NULL);
}

void speaker_play(const char *url, const char *mime, int volume)
{
    if (s_task) return;                 // one reply at a time
    job_t *job = calloc(1, sizeof(job_t));
    if (!job) return;
    snprintf(job->url, sizeof job->url, "%s", url);
    snprintf(job->mime, sizeof job->mime, "%s", mime ? mime : "");
    job->volume = volume < 10 ? 10 : volume > 100 ? 100 : volume;
    s_stop = false;
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);   // keep the token-bearing address out of the logs
    if (xTaskCreatePinnedToCore(play_task, "speak", 8192, job, 5, &s_task, 1) != pdPASS) { s_task = NULL; free(job); }
}

void speaker_stop(void)
{
    s_stop = true;
    for (int i = 0; i < 50 && s_task; i++) vTaskDelay(pdMS_TO_TICKS(20));   // let it close the speaker
}

bool speaker_busy(void) { return s_task != NULL; }
