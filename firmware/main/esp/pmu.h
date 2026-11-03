// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// AXP2101 power chip: battery level, charging state and the PWR side button.
#include <stdbool.h>
#include "esp_err.h"

typedef enum { PWR_NONE = 0, PWR_SHORT, PWR_LONG } pwr_event_t;

esp_err_t pmu_init(void);           // after bsp_i2c_init()
int pmu_battery_percent(void);      // -1 if no battery detected or read failed
bool pmu_is_charging(void);

typedef struct {
    bool ok;          // the power chip answered
    bool present;     // battery connected
    int percent;      // fuel gauge, 0..100, -1 unknown
    int mv;           // battery voltage, 0 unknown
    bool charging;    // charge current flowing
    bool full;        // charger reports "done"
    bool plugged;     // USB power present
} pmu_reading_t;
void pmu_read(pmu_reading_t *r);
pwr_event_t pmu_poll_pwr_key(void); // reads and clears the power-key interrupt flags
