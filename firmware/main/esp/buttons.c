#include "buttons.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "pmu.h"

#define BOOT_GPIO   GPIO_NUM_0
#define TICK_MS     20
#define HOLD_MS     600     // press longer than this = push-to-talk
#define DOUBLE_MS   350     // second click within this = double-click

static button_handler_t s_handler;

static void button_task(void *arg)
{
    int down_ms = 0, since_click = -1, pmu_tick = 0;
    bool was_down = false, holding = false;
    for (;;) {
        bool down = gpio_get_level(BOOT_GPIO) == 0;
        if (down) {
            down_ms += TICK_MS;
            if (down_ms >= HOLD_MS && !holding) { holding = true; since_click = -1; s_handler(BTN_BOOT_HOLD_START); }
        } else if (was_down) {
            if (holding) { holding = false; s_handler(BTN_BOOT_HOLD_END); }
            else if (down_ms >= 40) {                     // debounce
                if (since_click >= 0) { since_click = -1; s_handler(BTN_BOOT_DOUBLE); }
                else since_click = 0;                     // wait to see if a second click follows
            }
            down_ms = 0;
        }
        if (since_click >= 0 && !down) {
            since_click += TICK_MS;
            if (since_click > DOUBLE_MS) { since_click = -1; s_handler(BTN_BOOT_CLICK); }
        }
        was_down = down;
        if (++pmu_tick >= 5) {   // power chip every 100 ms
            pmu_tick = 0;
            pwr_event_t p = pmu_poll_pwr_key();
            if (p == PWR_SHORT) s_handler(BTN_PWR_SHORT);
            else if (p == PWR_LONG) s_handler(BTN_PWR_LONG);
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

void buttons_start(button_handler_t handler)
{
    s_handler = handler;
    gpio_config_t io = {.pin_bit_mask = 1ULL << BOOT_GPIO, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&io);
    xTaskCreate(button_task, "buttons", 4096, NULL, 5, NULL);
}
