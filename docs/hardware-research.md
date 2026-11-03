# PocketOS hardware research report

Research date: 3 November 2026. This is the source of truth for the website's Tech Specs.

## 1. Hardware identified

**Waveshare ESP32-S3-Touch-AMOLED-1.75C**, the version in the one-piece aluminium case, with battery
(Waveshare SKU **33691**; SKU 33692 is the same board without a battery). Waveshare also sells
an **ESP32-S3-Touch-AMOLED-1.75** and a **1.75-B** without the case. Those have different specs
(16 MB flash, 8-pin headers, an RTC), so none of their figures are used. No separate hardware
revision number is published for the 1.75C. The schematic used is the one Waveshare currently
publishes for it.

## 2. Sources

Manufacturer (primary):
- Product page: https://www.waveshare.com/esp32-s3-touch-amoled-1.75c.htm
- Documentation: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75C
- Resources (schematic, datasheets): https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75C/Resources-And-Documents
- Schematic PDF: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/raw/main/hardware/schematic/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf
- Board repository (README feature table): https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C
- Espressif ESP32-S3 datasheet: https://documentation.espressif.com/esp32-s3_datasheet_en.pdf

Distributors (secondary):
- OpenELAB: https://openelab.io/products/waveshare-esp32-s3-touch-amoled-1-75c
- GroboTronics: https://grobotronics.com/waveshare-esp32-s3-touch-amoled-display-with-aluminum-alloy-case-1-75-466466.html?sl=en
- RLX: https://rlx.sk/en/esp32-esp8266/10926-esp32-s3-175inch-amoled-round-touch-display-devboard-466466-aluminumdual-microphones-array-ws-33692.html
- Spotpear wiki: https://spotpear.com/wiki/ESP32-S3-1.75-inch-AMOLED-Round-Display-Capacitive-TouchScreen.html

## 3. Verified specifications (manufacturer)

| Area | Specification | Source |
|---|---|---|
| Processor | ESP32-S3R8, Xtensa 32-bit LX7 dual-core, up to 240 MHz | Docs, product page, schematic |
| Memory | 512 KB SRAM, 384 KB ROM, 8 MB PSRAM (in package) | Docs |
| Storage | 32 MB external NOR flash | Docs, product page |
| Display | 1.75-inch AMOLED, 466 × 466, 16.7 million colours | Docs, product page |
| Display panel | 700 cd/m² brightness, 100000:1 contrast, 178° viewing angle | Product page |
| Display controller | CO5300, QSPI | Docs, product page |
| Touch | Capacitive, CST9217 controller, I2C | Docs, product page |
| Wi-Fi | 2.4 GHz 802.11 b/g/n, onboard antenna | Docs, product page |
| Bluetooth | Bluetooth 5 (LE) | Docs, product page |
| Microphones | Dual-microphone array with echo cancellation, ES7210 ADC | Docs, schematic |
| Audio codec | ES8311 | Docs, schematic |
| Speaker | Built-in speaker / onboard speaker pads; NS4150B amplifier on the schematic | Product page, schematic |
| Motion sensor | QMI8658 6-axis IMU | Docs, product page features, schematic |
| Power management | AXP2101 | Docs, schematic |
| Battery | 3.7 V lithium, 2-pin 1.25 mm connector (Waveshare text: MX1.25), charge and discharge | Docs, product page |
| USB | USB Type-C (ESP32-S3 native USB: flashing and logs) | Product page |
| Buttons | PWR and BOOT, both programmable | Docs, product page |
| Case | One-piece CNC-machined aluminium alloy, anodised, lanyard holes | Product page |

## 4. Secondary sources only

| Specification | Value | Source | Status on the website |
|---|---|---|---|
| Size | Ø 55.00 × 15.05 mm | OpenELAB | Shown as "about 55 mm across, 15 mm deep", marked as a supplier figure |
| Weight | 38 g without battery, 104 g with battery (OpenELAB); 0.032 kg (GroboTronics) | Distributors | Not shown: "to be confirmed" |

## 5. Conflicts and how they were resolved

- **Motion sensor.** The product page's comparison table shows "–" for the 1.75C's IMU, but
  its feature list, the documentation, the README and the schematic (QMI8658C) all include it.
  **Resolved:** fitted. PocketOS doesn't use it today, and the website says so.
- **RTC.** The documentation's introduction mentions an RTC, but the comparison table shows "–"
  and the 1.75C schematic has no separate RTC chip (no PCF85063). **Resolved:** not listed.
  `docs/hardware.md` previously listed a PCF85063 RTC and TCA9554 expander, taken from the
  non-case board. That has been corrected.
- **microSD.** The comparison table shows "TF √", but the 1.75C schematic has no card slot.
  **Resolved:** not listed.
- **Battery connector name.** Waveshare's text says MX1.25 and the schematic label says PH1.25.
  Both are 2-pin, 1.25 mm pitch. **Resolved:** the website says "built-in rechargeable 3.7 V
  lithium battery" and doesn't name the connector.
- **Weight.** Distributor figures disagree (32 g, 38 g, 104 g). **Resolved:** not published until
  a finished unit is weighed.

## 6. Not verified

- Battery capacity (mAh): not published by Waveshare or any distributor checked
- Battery life with PocketOS: needs real-world testing
- Overall dimensions from the manufacturer: Waveshare's dimension drawing is an image that
  couldn't be read here
- Weight
- Speaker size, power and loudness
- Physical positions of the Power and Boot buttons and the USB-C port. The website no longer says
  "upper" or "lower" button; check on a real unit.
- Charging current and time
- Whether the battery is included and fitted inside the case on delivery (SKU 33691 says "can be
  installed inside the case")

## 7. Hardware checks needed

See `hardware-test-checklist.md`. In particular: measure size and weight, note the button and
port positions, read the battery's capacity label, time a full charge, and measure battery life
with PocketOS.
