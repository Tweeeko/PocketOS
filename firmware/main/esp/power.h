// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Screen sleep, battery readings, leaving/arriving home.
//
// Leaving home: if the home Wi-Fi has been gone for 3 minutes, the screen is asleep and the
// device isn't charging, PocketOS goes into deep sleep. It wakes every 5 minutes for a quick,
// screen-off scan for the home network (arrival), or immediately when BOOT is pressed.
#include <stdbool.h>
#include "cJSON.h"

void power_start(void);
bool power_check_arrival(void);   // call first in app_main; returns false if it went back to sleep
bool power_just_arrived(void);    // true for the boot right after arriving home

// Battery details and 48 h history for the settings website (GET /api/v2/battery).
cJSON *power_battery_json(void);
