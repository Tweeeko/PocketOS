// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Small helpers so the same code builds on the device and on a PC.
#if defined(ESP_PLATFORM)
#include "esp_attr.h"
// Large static buffers go in the 8 MB PSRAM; the ESP32-S3's fast internal RAM is only ~320 KB
// and Wi-Fi, the display and FreeRTOS need most of it.
#define PSRAM_BSS EXT_RAM_BSS_ATTR
#else
#define PSRAM_BSS
#endif
