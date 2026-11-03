// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef enum { NET_SETUP = 0, NET_NORMAL } net_mode_t;

typedef struct {
    net_mode_t mode;
    bool connected;      // normal mode: has an IP on home Wi-Fi
    char ssid[33];
    char ip[16];
    int rssi;
    char ap_ssid[33];    // setup mode network name
    char ap_pass[16];
    char host[32];       // pocketos-xxxx (without .local)
    char host_local[40]; // pocketos-xxxx.local
} net_status_t;

typedef struct { char ssid[33]; int rssi; bool secure; } net_ap_t;

// Starts setup mode if no Wi-Fi is saved, otherwise joins the saved network.
esp_err_t net_start(void);
void net_status(net_status_t *out);

// Setup mode only
int net_scan(net_ap_t *out, int max);
// Tries to join while keeping the setup network up. Saves on success.
esp_err_t net_try_join(const char *ssid, const char *pass, char *ip_out, size_t ip_len);

// Called whenever connection state changes (set by main.c)
typedef void (*net_listener_t)(void);
void net_set_listener(net_listener_t fn);

// Quick check after waking from away-sleep (Wi-Fi not otherwise started yet)
bool net_home_visible(int timeout_ms);
