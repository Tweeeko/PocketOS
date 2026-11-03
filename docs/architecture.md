# Architecture

## The short version

The **device is the source of truth**. It stores its own settings, serves its own settings
website, and talks to Home Assistant directly. The phone is only an editor. There is no
PocketOS server anywhere, and nothing is sent to the people who made it.

```
 Phone (Safari)  ──HTTP, home Wi-Fi──▶  PocketOS  ──WebSocket, home Wi-Fi──▶  Home Assistant
   website served by the device          settings in flash (NVS)             your devices
                                          API /api/v2
```

## Code layout

| Folder | What | Tested how |
|---|---|---|
| `firmware/main/core/` | Settings rules, device model, moods, Home Assistant messages, battery history and estimate, update manifest and version checks. Plain C, no ESP code | 151 unit checks on a PC (`make -C firmware/host test`) |
| `firmware/main/ui/` | LVGL 9 screens: companion, glass menus, dials, lists, QR, voice | Built against LVGL 9.5 in a PC simulator with a fake Home Assistant; screenshots checked (`make -C firmware/host sim`) |
| `firmware/main/esp/` | ESP-IDF glue: Wi-Fi, web server, WebSocket client, mic, buttons, power, storage | Syntax-checked against ESP-IDF 5.5 and Waveshare BSP 3.0 headers; full build in GitHub Actions; **not yet run on hardware** |
| `firmware/main/www/index.html` | The settings website (one file, no internet needed) | End-to-end tests in a phone-sized browser against `firmware/host/mock_device.py` |

## Why the device serves the website

A website hosted on the internet (HTTPS) is blocked by browsers from calling a device's
plain-HTTP address on the local network ("mixed content"). Serving the page from the device
avoids that: same origin, nothing to host, works with no internet. Cost: no HTTPS on the home
network. Mitigations are under "Security".

## Lifecycle

1. **Setup mode** (no saved Wi-Fi): PocketOS creates `PocketOS-XXXX`, a WPA2 network whose
   password is generated once per device and shown on screen with a Wi-Fi QR code. The phone
   joins; a captive-portal redirect opens the setup page. The user picks the home Wi-Fi.
   PocketOS tests it while the setup network is still up, reports success and its new address,
   saves it and restarts. **This network is used once**, or again only after "Change Wi-Fi
   network" or a full reset.
2. **Normal mode**: PocketOS is on the home Wi-Fi as `pocketos-xxxx.local`. The **Settings**
   tile (always first on the menu) shows a QR code of that address plus a pairing code.
   Adding new smart devices never needs the setup network: they're added in Home Assistant and
   PocketOS picks them up through its live connection.
3. **Pairing**: the website asks for the 6-digit code on screen and gets a token. Every
   change needs that token.
4. **Home Assistant**: a WebSocket connection (`auth`, then `get_states`, area/device/entity
   registry lists, `subscribe_events state_changed`). Actions are `call_service`. Voice uses
   `assist_pipeline/run` (speech-to-text through intent) with 16 kHz audio streamed as binary frames.

## Two units

Each unit's ID is the last two bytes of its Wi-Fi MAC (`a1b2`). It's in the network name, the
`.local` name, API responses and the website, so one phone can't confuse the two. Each keeps
its own settings, paired phones and Home Assistant connection.

## Home Assistant credentials

PocketOS needs a Home Assistant **long-lived access token**. Home Assistant doesn't offer
tokens limited to certain devices or actions: a token can do whatever its user can. So the
setup guide has the owner create a separate **non-administrator** user just for PocketOS and
make the token there. PocketOS never asks for, or stores, an administrator password, and the
token:

- is entered once on the website, sent to the device only, and stored in its flash;
- is never returned by any API call, never put in a URL, never logged (the firmware logs only
  whether a token is set);
- isn't included in settings backups;
- is erased by "Disconnect" or a reset.

If Home Assistant rejects the token, PocketOS stops retrying (so Home Assistant doesn't ban
its address) and shows the problem on the website.

## Security

| Risk | Mitigation |
|---|---|
| Anyone on the home Wi-Fi changing settings | Every change needs a token from the on-screen code |
| Code guessing | Each code works once; 5 wrong tries locks pairing for 60 s |
| A lost or sold phone | "Forget all phones" on the website; a reset also forgets them |
| Wiping the device remotely | Reset needs a token **and** a BOOT press on the device within 15 s |
| Rogue firmware upload | Needs a token; the website checks the file; rollback if the new firmware doesn't start cleanly |
| Tampered online update | HTTPS with certificate check, SHA-256 and size from the release manifest, image check, project-name check. Release publishing is protected by the GitHub account (use two-factor login) |
| Bad settings data | Every field validated before anything is saved; unknown keys refused; the Home Assistant address and token can't be changed by a settings restore |
| Sniffing on home Wi-Fi (no HTTPS) | The token travels once, when it's entered. Use `https://` for the Home Assistant address if it has one (the certificate is checked) |
| Setup network | WPA2 with a per-device random password, only on until Wi-Fi is set |

## Power

- Screen off after the chosen time (default 30 s); the CPU slows to 80 MHz when idle; Wi-Fi
  modem power-save stays on.
- **Away mode**: if the home Wi-Fi has been missing for 3 minutes, the screen is off and it
  isn't charging, PocketOS goes into deep sleep. Every 5 minutes it wakes briefly, scans for
  the home Wi-Fi, and if it's back, restarts and greets you ("Welcome home!"). BOOT wakes it
  at any time.
- **Flat battery**: at 3 % or less and under 3.45 V on battery, PocketOS says so, saves its battery
  history and sleeps. It checks every 10 minutes for USB power (or BOOT) and starts again once
  it's plugged in.

## Battery

The AXP2101 power chip reports the charge level (its own fuel gauge), the battery voltage, and
whether USB power is connected and charging. PocketOS reads it every 15 seconds, and stores a
history sample every 10 minutes (and whenever it's plugged in or unplugged): 48 hours, kept in
memory that survives sleep and restarts, and copied to flash hourly. During away-sleep it still
records a sample at each 5-minute wake-up. From the latest stretch on battery (at least an hour
with a visible drop) it estimates the drain per hour, time left and how long a full charge lasts.
These show on the device (This PocketOS) and on the website with a 48-hour chart.

## Updates

Firmware updates are GitHub releases of this repository (`docs/releasing.md`). PocketOS checks a
small `manifest.json` daily, offers newer versions on its screen and the website, and installs
on request (or overnight on charge if chosen). The download is checked against the manifest's
size and SHA-256, ESP-IDF checks the image, and the bootloader rolls back if the new version
doesn't start properly.

## Settings contract

`docs/config-schema.json` lists every setting with its type, limits and default.
`core/settings.c` enforces the same rules (unit-tested) and the website follows them.
