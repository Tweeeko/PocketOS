// Register addresses and bit meanings follow Waveshare's Apache-2.0 example 01_AXP2101.
// Battery voltage and charger status follow XPowersLib (MIT) AXP2101 register use.
// PocketOS reads status, turns on battery detection and the battery-voltage ADC channel, and
// enables the power-key interrupt flags. It does not change any power rails or charge settings.
#include "pmu.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

#define AXP_ADDR          0x34
#define REG_STATUS1       0x00  // bit5: VBUS good, bit3: battery present
#define REG_STATUS2       0x01  // bits7..5: 001 charging, 010 discharging; bit3: VBUS insert source; bits2..0: charger state (4 = done)
#define REG_ADC_CTRL      0x30  // bit0: battery voltage measurement
#define REG_VBAT_H        0x34  // bits4..0 high, 0x35 low: millivolts
#define REG_VBAT_L        0x35
#define REG_BAT_DET       0x68  // bit0: battery detection
#define REG_INTEN2        0x41
#define REG_INTSTS2       0x49  // bit2: PKEY long, bit3: PKEY short (IRQ bits 10, 11)
#define REG_BAT_PERCENT   0xA4
#define PKEY_LONG_BIT     (1 << 2)
#define PKEY_SHORT_BIT    (1 << 3)

static const char *TAG = "pmu";
static i2c_master_dev_handle_t s_dev;

static esp_err_t rd(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, 100);
}

static esp_err_t wr(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, 2, 100);
}

esp_err_t pmu_init(void)
{
    if (s_dev) return ESP_OK;   // already set up (the away-sleep check runs it early)
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP_ADDR,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) return err;

    uint8_t en = 0;
    if (rd(REG_INTEN2, &en) == ESP_OK) {
        wr(REG_INTEN2, en | PKEY_LONG_BIT | PKEY_SHORT_BIT);
    }
    wr(REG_INTSTS2, PKEY_LONG_BIT | PKEY_SHORT_BIT);  // clear stale presses (write 1 to clear)
    uint8_t v = 0;
    if (rd(REG_BAT_DET, &v) == ESP_OK && !(v & 1)) wr(REG_BAT_DET, v | 1);
    if (rd(REG_ADC_CTRL, &v) == ESP_OK && !(v & 1)) wr(REG_ADC_CTRL, v | 1);
    ESP_LOGI(TAG, "battery %d%%, charging %d", pmu_battery_percent(), pmu_is_charging());
    return ESP_OK;
}

int pmu_battery_percent(void)
{
    uint8_t s = 0, p = 0;
    if (!s_dev || rd(REG_STATUS1, &s) != ESP_OK || !(s & (1 << 3))) return -1;
    if (rd(REG_BAT_PERCENT, &p) != ESP_OK || p > 100) return -1;
    return p;
}

bool pmu_is_charging(void)
{
    uint8_t s = 0;
    return s_dev && rd(REG_STATUS2, &s) == ESP_OK && (s >> 5) == 0x01;
}

pwr_event_t pmu_poll_pwr_key(void)
{
    uint8_t s = 0;
    if (!s_dev || rd(REG_INTSTS2, &s) != ESP_OK) return PWR_NONE;
    uint8_t hit = s & (PKEY_LONG_BIT | PKEY_SHORT_BIT);
    if (!hit) return PWR_NONE;
    wr(REG_INTSTS2, hit);
    return (hit & PKEY_LONG_BIT) ? PWR_LONG : PWR_SHORT;
}

void pmu_read(pmu_reading_t *r)
{
    *r = (pmu_reading_t){.percent = -1};
    uint8_t s1 = 0, s2 = 0, h = 0, l = 0, p = 0;
    if (!s_dev || rd(REG_STATUS1, &s1) != ESP_OK || rd(REG_STATUS2, &s2) != ESP_OK) return;
    r->ok = true;
    r->present = s1 & (1 << 3);
    r->plugged = (s1 & (1 << 5)) && !(s2 & (1 << 3));   // same test as XPowersLib isVbusIn()
    r->charging = (s2 >> 5) == 0x01;
    r->full = (s2 & 0x07) == 4;
    if (!r->present) return;
    if (rd(REG_BAT_PERCENT, &p) == ESP_OK && p <= 100) r->percent = p;
    if (rd(REG_VBAT_H, &h) == ESP_OK && rd(REG_VBAT_L, &l) == ESP_OK) {
        int mv = ((h & 0x1F) << 8) | l;
        if (mv > 2500 && mv < 4600) r->mv = mv;   // outside this range the reading is not real
    }
}
