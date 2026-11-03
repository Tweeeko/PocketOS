# Testing

## Already automated (no hardware needed)

| What | Command | Result when last run |
|---|---|---|
| Core logic: settings, device model, moods, Home Assistant messages, battery estimate, update checks | `make -C firmware/host test` | 275 checks, 0 failed |
| Device screens in a PC simulator with a fake Home Assistant | `make -C firmware/host sim LVGL_DIR=<lvgl 9.5 checkout>` | 53 screenshots, 0 failed checks |
| Settings website, end to end, phone-sized browser | `python3 firmware/host/test_site.py` (needs Python Playwright) | All passed |
| Full firmware build for the ESP32-S3 | GitHub Actions, "Build firmware" | Passes on every push; releases are signed |

The website can also be tried by hand: `python3 firmware/host/mock_device.py`, then open
http://localhost:8080 (it prints the pairing code; the test Wi-Fi password is `correct horse`
and the test Home Assistant token is `good-token`).

## On real hardware

Use [hardware-test-checklist.md](hardware-test-checklist.md). It's ordered for bringing up a new board, from flashing to packaging.
