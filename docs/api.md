# Device API v2

Served by each PocketOS at `http://<device-address>/api/v2/...` (`firmware/main/esp/web.c`).
The settings website (`firmware/main/www/index.html`) is the only client. JSON in and out.
Calls marked **auth** need `Authorization: Bearer <token>` from pairing.

Errors always look like `{"error": "<human message>", "field": "<setting, if any>"}` with a
4xx/5xx status. The website treats anything other than a 2xx response as "nothing was changed".

`firmware/host/mock_device.py` implements the same API on a computer for testing the website.

## Identity and pairing

### GET /info (no auth)
Health and identity, safe to show before pairing. No secrets, no Home Assistant address.
```json
{"api": 2, "schema": 2, "id": "a1b2", "name": "PocketOS", "firmware": "0.1.0", "mode": "normal",
 "wifi": {"ssid": "HomeNet", "rssi": -58, "ip": "192.168.1.42"}, "host": "pocketos-a1b2.local",
 "battery": {"percent": 78, "charging": false, "plugged": false, "mv": 3950, "hours_left": 14},
 "ha": {"status": "connected", "error": "", "devices": 11, "new": 0},
 "uptime_s": 3600,
 "update": {"online": true, "mode": "notify", "state": "available", "progress": 0, "error": "",
            "available": "0.2.0", "notes": "Smoother menus", "size": 1843200, "checked": 1793600000}}
```
`mode` is `setup` or `normal`. `battery.percent` is `null` without a battery; `hours_left` is only
there when PocketOS has learned its drain rate and it's on battery. `update` is left out in setup mode;
`update.state` is `idle`, `checking`, `latest`, `available`, `downloading`, `installed` or `failed`.
`ha.status` is `not_set`, `connecting`, `connected` or `error`. `ha.devices` counts devices shown on
PocketOS; `ha.new` counts devices Home Assistant has that haven't been reviewed yet.

### POST /pair (no auth)
`{"pin": "482913"}` → `200 {"token": "<32 hex>"}`. The PIN is on the device screen (Settings tile,
or press BOOT). Each PIN works once. Wrong PIN → `401`; 5 wrong in a row → `429` for 60 s.
Up to 4 phones stay paired; tokens are kept in flash so a restart doesn't unpair them.

### POST /unpair_all (auth)
Forgets every paired phone, including the caller.

## Settings

### GET /config (auth)
All settings (see `config-schema.json`), plus `"ha_token_set": true|false`. The Home Assistant
token itself is never returned by any call.

### POST /config (auth)
Any subset of settings, e.g. `{"brightness": 60}`. Every field is validated before anything is
saved; one bad field → `400` with `field` and nothing changes. Unknown keys are refused.
`ha_url` and `ha_token` can't be set here (use `/ha`). On success the device saves to flash,
applies it on screen and returns the full config exactly as `GET` would. That response is the
confirmation the website waits for.

### GET /export (auth)
Same as `GET /config`, as a download. Contains no Wi-Fi password and no token. When restoring,
the website removes `ha_url`, `ha_token_set` and `schema` before posting it to `/config`.

## Devices

### GET /entities (auth)
```json
{"areas": ["Kitchen", "Living room"],
 "entities": [{"id": "light.floor_lamp", "name": "Floor lamp", "domain": "light", "area": "Living room",
   "status": "On · 60%", "controls": "On, off and brightness", "fav": true, "hidden": false, "new": false}]}
```
Entities Home Assistant marks as hidden or as configuration/diagnostic are left out.

### POST /entities (auth)
`{"id": "light.floor_lamp", "fav": true}`; any of `fav`, `hidden`, `known` (booleans) in one call.
`{"all_known": true}` marks every current device as reviewed (shown).

## Home Assistant

### POST /ha/test (auth)
`{"url": "http://homeassistant.local:8123", "token": "..."}`. Opens a one-off connection (10 s limit),
logs in and counts devices. Doesn't save anything. Empty `token` = test the saved one.
→ `200 {"ok": true, "devices": 42, "message": "..."}` or `400` with a plain-English reason.

### POST /ha (auth)
Same body. Saves the address (and the token, if not empty) and reconnects. The website then polls
`/info` until `ha.status` is `connected` or `error`.

### POST /ha/disconnect (auth)
Erases the saved token and clears the device list.

## Battery

### GET /battery (auth)
```json
{"percent": 78, "mv": 3950, "present": true, "charging": false, "full": false, "plugged": false,
 "now": 1793600000, "last_full": 1793500000,
 "estimate": {"known": true, "pct_per_h": 4.1, "hours_left": 19.0, "full_h": 24.4, "basis_h": 9.5},
 "history": [[1793427200, 100, 4150, 2], [1793427800, 99, 4140, 0]]}
```
History covers 48 hours: `[unix time, percent, millivolts, flags]`, one row every 10 minutes and
whenever USB power is connected or removed (flags: 1 charging, 2 plugged in). The estimate is a
straight-line fit over the latest stretch on battery of at least an hour with a visible drop
(`core/battery.c`). Until then `known` is false.

## Updates (see docs/releasing.md)

### POST /update/check (auth)
`202`; the result appears in `/info` → `update` a few seconds later. `409` if already busy.

### POST /update/install (auth)
`202` and the download starts; follow `update.progress` in `/info`. PocketOS restarts when it's
done. `400` with a reason if there's nothing newer, or the battery is under 30 % and it isn't
plugged in.

The update mode is the `updates` setting (`off`, `notify`, `auto`) in `/config`.

## Wi-Fi

### GET /wifi/scan (setup mode only)
`{"networks": [{"ssid": "HomeNet", "rssi": -50, "secure": true}]}`

### POST /wifi (setup mode only)
`{"ssid": "HomeNet", "password": "..."}`. Tries to join while the setup network stays up (up to
about 20 s). Success → `200 {"ok": true, "ip": "...", "host": "pocketos-a1b2.local"}`, then saves
and restarts on the home Wi-Fi. Failure → `400`, nothing saved.

### POST /wifi/forget (auth)
For a new router or a move: forgets only the Wi-Fi and restarts into setup mode. Settings,
devices and paired phones are kept.

## Maintenance

### POST /restart (auth)
`202`, restarts after about 1 s.

### POST /reset (auth)
`202 {"ok": true, "confirm": "Press BOOT on the device within 15 seconds"}`. Only if BOOT is pressed
in time does it erase everything (Wi-Fi, Home Assistant, settings, paired phones) and restart into
setup. Otherwise nothing happens.

### POST /ota (auth)
Body: raw `pocketos.bin` (`application/octet-stream`). Written to the spare slot, then `200` and a
restart. If the new firmware doesn't start properly it rolls back by itself. The website checks the
file first (ESP app image, project name `pocketos`) and shows the version it contains.

## Not on the device
`firmware/host/mock_device.py` adds `/mock/*` hooks for tests (go offline, add a device, press BOOT).
The real firmware doesn't have them.
