// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Local settings website + API v1 (see docs/api.md).
esp_err_t web_start(void);
#include "ui_app.h"
// A reset (from the website or PocketOS) or a Wi-Fi change (from PocketOS) waits 15 s for the Boot button
void web_confirm_start(confirm_kind_t kind);
bool web_confirm_pending(void);
void web_confirm_do(void);        // Boot pressed in time
