# PocketOS

**Your home, at a glance.** PocketOS is a pocket-sized smart-home companion with a round
touchscreen. It connects to [Home Assistant](https://www.home-assistant.io) and puts every
light, thermostat, lock and speaker in your hand, with a friendly companion on the home screen.

Designed and written by **Leeko**. Sold by Leeeko LTD in the UK (coming soon, £99.99).

- **Website:** [www.pocketos.co.uk](https://www.pocketos.co.uk) — features, [try it in your browser](https://www.pocketos.co.uk/demo), setup and support
- **What's new:** [CHANGELOG.md](CHANGELOG.md) and [Releases](https://github.com/Tweeeko/PocketOS/releases)
- **Report a security problem:** [SECURITY.md](SECURITY.md)

## What it does

- **Every room, every device.** Lights (brightness, warmth and colour), heating, fans, blinds,
  locks, plugs, speakers, scenes and scripts, organised by room or by type, straight from Home Assistant.
- **Confirmed control.** Turn the dial; a change shows as done only when your home confirms it.
- **Talk to your home.** Hold the Boot button and speak. Your own Home Assistant voice assistant
  answers, on screen and out loud.
- **Yours to arrange.** Favourites, hidden devices, your own names and menu order, on the device
  or from your phone.
- **Private by design.** No PocketOS account, app, cloud, subscription, advertising or tracking.
  Everything stays between PocketOS and your Home Assistant.
- **Looked after.** Signed updates over Wi-Fi, released gradually, with security updates until at
  least 31 December 2029.

## What you need

Home Assistant on your home network, 2.4 GHz Wi-Fi, and a phone to set it up. PocketOS arrives
with its software installed.

## Status

The software is complete and tested on a PC (unit tests, a full simulator of the screens, and
browser tests of the settings website); every push is built for the device by GitHub Actions,
and releases are signed. It hasn't run on the production board yet: the hardware checklist in
[docs/hardware-test-checklist.md](docs/hardware-test-checklist.md) covers what to check when it arrives.

---

*The rest of this file is for developers.*

## Website

The product page with the promo film, a one-click browser installer for new boards, help, and
what's new. It lives in `site/` and is hosted on Vercel (`vercel.json`, `site/build.sh`): every
deploy fetches the latest release's firmware for the installer, and each release triggers a
deploy. No cookies or analytics. Setup: `docs/website.md`.

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
site/              the public website (Vercel)
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

PocketOS is created and written by Leeko. Copyright (c) 2026 Leeko.


Board support from Waveshare's `esp32_s3_touch_amoled_1_75c` component (Apache-2.0),
pulled in as a dependency, not copied. AXP2101 register usage follows Waveshare's
Apache-2.0 example `01_AXP2101`. See `NOTICE`.
