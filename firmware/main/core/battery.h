// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Battery history and "how long will it last" estimate. Portable C (unit-tested on a PC).
// The device records a sample every 10 minutes (and whenever it's plugged in or unplugged),
// keeps the last 48 hours, and estimates drain from the most recent stretch on battery.
#include <stdbool.h>
#include <stdint.h>

#define BATT_MAX_SAMPLES 288          // 48 h at 10-minute spacing
#define BATT_SAMPLE_S    600
#define BATT_MAGIC       0x50424C31u  // "PBL1"

enum { BATT_CHARGING = 1, BATT_PLUGGED = 2 };

typedef struct {
    uint32_t t;        // unix time, seconds
    uint16_t mv;       // battery voltage, 0 if unknown
    uint8_t pct;       // 0..100
    uint8_t flags;     // BATT_CHARGING | BATT_PLUGGED
} batt_sample_t;

typedef struct {
    uint32_t magic;
    uint16_t head;     // index of the next write
    uint16_t n;        // samples stored
    uint32_t last_full_t;   // last time it reached 100 % while plugged in (0 = never seen)
    batt_sample_t s[BATT_MAX_SAMPLES];
} batt_log_t;

typedef struct {
    bool known;          // enough time on battery to say anything
    float pct_per_h;     // drain while on battery
    float hours_left;    // from now, only when on battery now (else -1)
    float full_h;        // how long a full charge lasts at this rate
    float basis_h;       // how many hours of data the estimate is based on
} batt_est_t;

void batt_log_init(batt_log_t *l);
bool batt_log_valid(const batt_log_t *l);   // false = corrupt or never initialised

// Records a reading. Adds a sample when 10 minutes have passed since the last one, or when the
// plugged-in state changes. Ignores implausible clocks (before 2024 or going backwards).
// Returns true if a sample was stored.
bool batt_log_add(batt_log_t *l, uint32_t now, int pct, int mv, bool charging, bool plugged);

int batt_log_count(const batt_log_t *l);
bool batt_log_get(const batt_log_t *l, int i, batt_sample_t *out);   // i = 0 is the oldest

batt_est_t batt_estimate(const batt_log_t *l, uint32_t now, int pct_now, bool plugged_now);
