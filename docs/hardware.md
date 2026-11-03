# Hardware

## Board

Waveshare **ESP32-S3-Touch-AMOLED-1.75C-EN** (SKU 33691 includes the battery, 33692 does not).

| Part | Detail | Source |
|---|---|---|
| SoC | ESP32-S3R8, dual-core LX7 240 MHz, 8 MB octal PSRAM | Waveshare product page |
| Flash | 32 MB | Waveshare 1.75C wiki; `sdkconfig.defaults` of Waveshare demos |
| Display | 1.75" AMOLED 466×466, CO5300 driver, QSPI | Waveshare wiki + BSP |
| Touch | CST9217, I2C | Waveshare wiki + BSP |
| Power | AXP2101 PMU, I2C address 0x34, charging and fuel gauge | Waveshare `01_AXP2101` example |
| Audio | ES7210 dual-mic ADC + speaker codec, I2S | Waveshare BSP |
| Other | QMI8658 IMU (on the 1.75C schematic). The 1.75C has no PCF85063 RTC or TCA9554 expander: those are on the non-case 1.75 board | Waveshare 1.75C schematic |
| Buttons | PWR and BOOT on the side of the case | Waveshare wiki |

## Pins (from the official BSP header `esp32_s3_touch_amoled_1_75c.h`, v3.0.0)

| Function | GPIO |
|---|---|
| I2C SCL / SDA (touch, PMU, codec, IMU) | 14 / 15 |
| LCD CS, PCLK | 12, 38 |
| LCD DATA0–3 (QSPI) | 4, 5, 6, 7 |
| LCD reset | 1 |
| Touch reset / interrupt | 2 / 11 |
| I2S SCLK, MCLK, LRCLK, DOUT, DIN | 9, 16, 45, 8, 10 |
| Speaker amplifier enable | 46 |
| BOOT button | 0 (used directly by Waveshare's `04_Immersive_block` demo) |
| PWR button | Not a GPIO: reported by the AXP2101 as "power key short/long press" interrupts (IRQ bits 10 and 11, register 0x49 bits 2 and 3) |

The BSP sets `BSP_CAPS_BUTTONS 0`, so PocketOS handles both buttons itself (`firmware/main/buttons.c`).

## Brightness

The AMOLED has no backlight pin. The BSP sets brightness with display command 0x51
(`bsp_display_brightness_set(0..100)`), which PocketOS uses directly.

## Battery

AXP2101 registers PocketOS reads (from Waveshare's example):

| Register | Meaning |
|---|---|
| 0x00 bit 3 | Battery connected |
| 0x01 bits 7–5 | 001 = charging, 010 = discharging, 000 = standby |
| 0xA4 | Battery percentage (fuel gauge) |

The fuel gauge reading needs real-world checking against a full charge/discharge cycle.

## Still to confirm on the real unit

- Exact case dimensions and button positions (wiki only shows an image).
- Hold time at which the PWR button hard-powers off the board (AXP2101 default).
- Battery capacity of the included cell.
