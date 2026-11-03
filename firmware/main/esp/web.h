#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Local settings website + API v1 (see docs/api.md).
esp_err_t web_start(void);
bool web_reset_pending(void);     // true for 15 s after the website asks for a reset
void web_reset_confirmed(void);   // BOOT pressed in time: erase and restart
