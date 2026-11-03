// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Home Assistant WebSocket client (esp_websocket_client + core/ha_proto).
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ha_proto.h"

void ha_client_start(void);        // connects if an address and token are saved
void ha_client_reconfigure(void);  // address/token changed: reconnect (or stop if cleared)
bool ha_client_connected(void);    // authenticated and synced

// Called with the model lock held. Returns false if not connected or not possible.
bool ha_client_action(model_t *m, int i, act_t act, float value);

// One-off connection test for the settings website. Blocks up to ~10 s.
bool ha_client_test(const char *url, const char *token, char *msg, size_t len, int *devices);

// Voice (Assist pipeline): start a run, stream 16 kHz mono 16-bit PCM, then end it.
bool ha_client_voice_start(void);
void ha_client_voice_audio(int handler_id, const int16_t *pcm, size_t samples);
void ha_client_voice_end(int handler_id);

// URL helper shared with the web API: http(s)://host:port -> ws(s)://host:port/api/websocket
bool ha_ws_url(const char *http_url, char *out, size_t len);
