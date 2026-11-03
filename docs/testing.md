# Testing

## Already automated (no hardware needed)

| What | Command | Result when last run |
|---|---|---|
| Core logic: settings, device model, moods, Home Assistant messages, battery estimate, update checks | `make -C firmware/host test` | 151 checks, 0 failed |
| Device screens in a PC simulator with a fake Home Assistant | `make -C firmware/host sim LVGL_DIR=<lvgl 9.5 checkout>` | 42 screenshots, 0 failed checks |
| Settings website, end to end, phone-sized browser | `python3 firmware/host/test_site.py` (needs Python Playwright) | All passed |
| Full firmware build for the ESP32-S3 | GitHub Actions, "Build firmware" | **Not run yet.** Expect a few compile fixes on the first run |

The website can also be tried by hand: `python3 firmware/host/mock_device.py`, then open
http://localhost:8080 (it prints the pairing code; the test Wi-Fi password is `correct horse`
and the test Home Assistant token is `good-token`).

## On real hardware

Tick only after doing it on a real unit. Note the phone and browser used.

### Build and flash
- [ ] GitHub Actions build passes and produces both .bin files
- [ ] First flash via the browser flasher on a PC works (unit A), then unit B
- [ ] Screen, touch and both buttons respond; companion appears

### First-time setup
- [ ] Welcome screen explains the steps; Wi-Fi QR code joins `PocketOS-XXXX` from an iPhone
- [ ] Setup page opens on its own (captive portal); if not, http://192.168.4.1 works
- [ ] Wrong Wi-Fi password: clear error, nothing saved
- [ ] Right password: page shows the new address; PocketOS restarts on home Wi-Fi
- [ ] iPhone goes back to home Wi-Fi and the link opens the settings page

### Settings website (normal mode)
- [ ] Settings tile QR code opens `http://pocketos-xxxx.local` on iPhone; note whether Android needs the IP
- [ ] Pairing with the on-screen code works; a used code doesn't work again; 5 wrong codes lock for 60 s
- [ ] Still paired after restarting PocketOS
- [ ] Brightness, clock, eye colour change on the device within a second and survive a restart
- [ ] Tile rename, reorder and show/hide appear on the device menu
- [ ] Unplug PocketOS mid-change: the website says nothing was changed
- [ ] Add to Home Screen works and opens straight to the dashboard
- [ ] Two units: changing A's name doesn't change B

### Home Assistant
- [ ] Guide works for a new non-admin user; Test reports the right device count
- [ ] Wrong token: clear error, and Home Assistant shows no repeated failed logins afterwards
- [ ] Rooms, devices and scenes appear on PocketOS; hidden/diagnostic entities don't
- [ ] Each control type moves the real device: light dial, heating dial and on/off, speaker volume and skip, fan, blind, lock, plug, scene
- [ ] Changes made in the Home Assistant app show on PocketOS live
- [ ] "Sending…" then confirmation; turning a device off at the wall gives "No response"
- [ ] New device added in Home Assistant: banner on the website, review works; with auto-add it appears by itself
- [ ] Moods: heating on → warm, music playing → dancing, Home Assistant down → worried, door unlocked → alert

### Voice
- [ ] Home Assistant has a voice assistant with speech-to-text set up
- [ ] Hold BOOT, say "turn on the floor lamp", release: the words appear, the lamp turns on, the reply shows
- [ ] Note how reliable the microphones are at arm's length: ____

### Buttons and power
- [ ] BOOT press: Settings code; press twice: home; hold: voice
- [ ] PWR press: screen off/on; note the PWR hold time that powers off: ____ s
- [ ] Battery % and charging look plausible
- [ ] Leave home (or switch the router off) 10 minutes: PocketOS goes to sleep; back home: "Welcome home!" within about 5 minutes
- [ ] Battery life, screen mostly off, at home: ____ hours. Away: ____ hours

### Battery measurement
- [ ] Battery % and voltage on the website match a multimeter on the battery (± 0.05 V)
- [ ] Unplugging and plugging in shows within 15 s, and as a shaded band on the chart
- [ ] After a few hours off the charger, "about N hours left" appears; note it, then check how long it really lasted: estimated ____ h, real ____ h
- [ ] History survives a restart and an away-sleep

### Online updates
- [ ] Repository is public; Release workflow publishes v0.1.1 with three files
- [ ] More → Updates → Check now finds it; the device shows the update row
- [ ] Install at 20 % on battery is refused; plugged in it installs and comes back on v0.1.1
- [ ] "Install overnight" installs on charge between 2 and 5 am with the screen off
- [ ] Unplug Wi-Fi mid-download: "Update failed. Nothing changed." and still on the old version

### Updates and recovery
- [ ] Upload `pocketos.bin` from iPad Safari; PocketOS restarts on the new version
- [ ] Upload a deliberately broken build: it rolls back to the old version
- [ ] "Change Wi-Fi network" goes back to setup and keeps settings
- [ ] Reset only happens after pressing BOOT; afterwards it's back in setup mode
