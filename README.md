# PocketOS

A pocket-sized smart-home companion: a round 1.75" AMOLED screen with an animated
character, controlled by touch and two side buttons, configured from a phone over
the home Wi-Fi. Local-first: no PocketOS account, cloud, subscription or tracking.

Built for two units, each with its own independent settings.

## Status

Everything is written. Nothing has run on the real board yet, because it hasn't been bought.

| Part | Written | Checked so far |
|---|---|---|
| Device OS: companion and moods, glass menus, dials, lists, Settings QR, first-run welcome | Yes | Runs in a PC simulator with a fake Home Assistant; screenshots checked |
| Home Assistant connection: rooms, devices, live updates, every control type | Yes | Message building and parsing unit-tested against real Home Assistant message shapes |
| Settings website served by the device | Yes | End-to-end browser tests against a mock device |
| Voice (hold BOOT), power saving, away/arrival detection | Yes | Checked against the ESP-IDF, board and codec headers only |
| Battery measurement: level, voltage, 48 h history, time-left estimate | Yes | Estimate unit-tested; website chart tested; chip readings checked against XPowersLib |
| Online updates from GitHub releases, release workflow | Yes | Manifest and version checks unit-tested; website flow tested; download code syntax-checked |
| ESP32 firmware build | Yes | **Not compiled yet.** GitHub Actions does that; the first run will probably need small fixes |
| On real hardware | No | Checklist in `docs/testing.md` |

Things to know:
- Voice needs a voice assistant with speech-to-text set up in Home Assistant.
- Battery life hasn't been measured on a real unit yet. PocketOS measures it itself once it's running.
- Online updates need this repository to be public. See `docs/releasing.md`.
- PocketOS remembers up to 96 favourites, hidden and reviewed devices each, and handles up to
  128 Home Assistant devices.

## Website

**https://tweeeko.github.io/PocketOS/**: the product page with the promo film, a one-click
browser installer for new boards, help, and what's new. It lives in `site/` and is published
by `.github/workflows/site.yml` after every release (it copies in the latest firmware and the
changelog). No cookies or analytics.

## Hardware

Waveshare **ESP32-S3-Touch-AMOLED-1.75C-EN** (aluminium case, battery included).
ESP32-S3R8, 8 MB PSRAM, 32 MB flash, 466×466 CO5300 AMOLED, CST9217 touch,
AXP2101 power chip, PWR and BOOT side buttons. Pins and sources: `docs/hardware.md`.

## Repository layout

```
firmware/          ESP-IDF project for the device (C, LVGL 9)
  main/core/       settings, device model, moods, Home Assistant messages (portable, unit-tested)
  main/ui/         the screens (LVGL)
  main/esp/        Wi-Fi, web server, Home Assistant client, voice, buttons, power, storage
  main/www/        the settings website the device serves itself
  host/            PC tests, simulator and a mock device for the website
design/            browser prototypes of the OS and settings website (reference only)
site/              the public website (GitHub Pages)
docs/              architecture, API, settings schema, hardware, flashing, releasing, testing
.devcontainer/     GitHub Codespaces setup with ESP-IDF, for building from an iPad
.github/workflows/ cloud build that produces flashable firmware files
```

## Building (from an iPad)

1. Open this repository on github.com and choose **Code → Codespaces → Create codespace**.
2. In the Codespace terminal:
   ```
   cd firmware
   idf.py set-target esp32s3
   idf.py build
   ```
3. Or let GitHub do it: every push runs **Actions → Build firmware** and attaches
   `pocketos-full.bin` (first flash) and `pocketos.bin` (over-the-air updates).

First flash needs a PC once; after that updates go over Wi-Fi. To publish an update for every
PocketOS: bump `firmware/version.txt`, add a `CHANGELOG.md` entry, run **Actions → Release**.
See `docs/releasing.md` and `docs/flashing.md`.

## Testing without the board

```
make -C firmware/host test                 # core logic
python3 firmware/host/test_site.py         # settings website (needs Python Playwright)
python3 firmware/host/mock_device.py       # try the website by hand at http://localhost:8080
```

## Credits

Board support from Waveshare's `esp32_s3_touch_amoled_1_75c` component (Apache-2.0),
pulled in as a dependency, not copied. AXP2101 register usage follows Waveshare's
Apache-2.0 example `01_AXP2101`. See `NOTICE`.
