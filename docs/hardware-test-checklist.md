# PocketOS hardware test checklist

For the first production boards (Waveshare ESP32-S3-Touch-AMOLED-1.75C). Work top to bottom: each
section relies on the ones before it. Tick only what you have done on a real unit.

Unit: ________  MAC / device code: PocketOS-______  Firmware: ______  Date: ______
Phone and browser used: ______________________  Home Assistant version: ______

Write down anything odd, even if it seems small. A note like “dial jumps a little at the top” is
exactly what's needed.

---

## 1. Out of the box (before flashing)

- [ ] Board, case and battery match the order; no damage, loose parts or rattles
- [ ] The two side buttons click cleanly. Note which is which: Power is ______, Boot is ______
- [ ] Waveshare's own demo starts when switched on (proves the board works before we change it)
- [ ] Charges from the USB-C cable that will go in the box; note the charge light behaviour: ______

## 2. Flashing

- [ ] **Factory tool** (`tools/factory`, on a PC): device found, “PASS PocketOS-XXXX”, line written to `factory-log.csv`
- [ ] Or **browser installer** on the website's Software recovery page, from Chrome or Edge on a PC
- [ ] If it won't connect: hold Power 6 s (off), then hold Boot and press Power 1 s, plug in again. Note whether this was needed: ______
- [ ] After flashing, the screen shows the welcome / setup code
- [ ] Flash a second unit. Its setup network name and password are **different** from the first

## 3. Screen, touch and buttons

- [ ] Picture is sharp, colours right, nothing upside down or offset; round edge not cut off
- [ ] Touch lands where you tap, all the way to the edge (try all four sides and the centre)
- [ ] Swipes and scrolling feel smooth; note any lag: ______
- [ ] Brightness 10 % is readable indoors; 100 % is readable near a window
- [ ] **Power** press: screen off / on
- [ ] **Power** hold 6 s: “Keep holding to switch off” then off. Hold 1 s: switches on again
- [ ] **Boot** press: shows the settings code. Press twice: home screen. Hold: voice screen
- [ ] Companion animates; “Calm” and “Off” animation settings visibly change it

## 4. First-time setup (as a customer would)

- [ ] iPhone camera reads the setup QR code and joins `PocketOS-XXXX`
- [ ] Android camera does the same. Phone model: ______
- [ ] Setup page opens on its own; if not, `http://192.168.4.1` opens it
- [ ] Wrong Wi-Fi password: clear message, nothing saved
- [ ] A 5 GHz-only network doesn't appear in the list, and the page explains PocketOS uses 2.4 GHz
- [ ] Right password: PocketOS joins home Wi-Fi, the phone returns to home Wi-Fi, and the link opens settings
- [ ] Pairing code works; an old code doesn't; 5 wrong codes lock for 60 s
- [ ] Still paired after restarting PocketOS

## 5. Home Assistant

- [ ] Follow the website's guide with a **non-administrator** Home Assistant user; Test shows the right device count
- [ ] Wrong token: clear message, and Home Assistant doesn't fill with failed log-ins
- [ ] Rooms and devices appear; hidden and diagnostic entities don't
- [ ] Each type controls the real thing, and shows “Sending…” then done:
  - [ ] Light: on/off and brightness
  - [ ] Light with **warmth**: dial goes warm to cool; the bulb follows
  - [ ] Light with **colour**: dial goes round the colours; the bulb follows; colour name is right
  - [ ] Heating: target temperature and on/off
  - [ ] Speaker / TV: play, pause, skip, volume
  - [ ] Fan, blind, lock, plug, scene, script
- [ ] Change something in the Home Assistant app: PocketOS updates within a second or two
- [ ] Switch a device off at the wall, then use it on PocketOS: “No response” after about 6 s
- [ ] Add a device, a room or rename something in Home Assistant: PocketOS picks it up without a restart
- [ ] Rename, hide, favourite and reorder on PocketOS **and** on the settings page; both stay in step
- [ ] °F home (if available to test): temperatures and the cold mood make sense
- [ ] Moods: heating on → warm; music → dancing; Home Assistant off → worried; door unlocked → alert

## 6. Voice and speaker

- [ ] Home Assistant has a voice assistant with speech-to-text **and** text-to-speech
- [ ] Hold Boot, say “turn on the floor lamp”, let go: words appear, lamp turns on, reply shows **and is spoken**
- [ ] Spoken reply is clear, not distorted, not too quiet at volume 60. Best default volume: ______
- [ ] Volume 10 and 100 both sound right (no crackle at 100)
- [ ] Turn “Say replies out loud” off: reply shows only
- [ ] Hold Boot while a reply is playing: it stops and listens again
- [ ] Microphones work at arm's length in a normal room; how often is it misheard (out of 10): ______
- [ ] Note the audio format Home Assistant sent (in the serial log: “spoken reply not played” must **not** appear): ______
- [ ] The serial log never shows the Home Assistant token or the spoken reply's address

## 7. Battery and power

- [ ] Battery % and voltage on the website match a multimeter (± 0.05 V)
- [ ] Plug in / unplug shows within 15 s, and on the chart
- [ ] Charging from flat to full takes ______ h; it stops when full
- [ ] Leave home Wi-Fi 10 min: PocketOS sleeps. Back home: “Welcome home” within about 5 min
- [ ] Battery life, normal use at home: ______ h. Screen mostly off: ______ h
- [ ] “About N hours left”: estimated ______ h, really lasted ______ h
- [ ] Device gets no hotter than warm while charging and while using voice

## 8. Wi-Fi changes and reset

- [ ] Change the router password: PocketOS shows “Wi-Fi changed? Press Boot”, and Boot starts setup
- [ ] **More → This PocketOS → Change Wi-Fi network** (confirmed with Boot) keeps your settings
- [ ] **Reset PocketOS** asks for Boot, then erases everything, including the Home Assistant token
- [ ] Settings page: reset without pressing Boot does nothing

## 9. Updates

- [ ] **More → Updates → Check now** finds the latest release
- [ ] Install at under 30 % battery off the charger is refused; plugged in, it installs and comes back on the new version
- [ ] “Install overnight” installs while charging between 2 and 5 am
- [ ] Wi-Fi off mid-download: “Update failed. Nothing changed.” and still on the old version
- [ ] Upload a file that isn't signed on the settings page: refused, nothing changed
- [ ] A build that fails to start rolls back to the previous version by itself

## 10. Two units side by side

- [ ] Different names, settings and favourites on each; changing one doesn't touch the other
- [ ] Both connect to Home Assistant at once without either dropping

## 11. Packaging and printed material (Pillbox Design samples)

- [ ] Device, cable and quick start guide fit the insert; nothing moves when shaken
- [ ] Quick start guide QR code opens the setup page on iPhone and Android
- [ ] Quick start guide shows the security update date and Leeeko LTD details
- [ ] Box shows Leeeko LTD name and address, and the UKCA marking if it's needed (check before printing)

## Sign-off

Everything above passed on ____ units. Problems found and fixed in firmware version ______.

Tested by: ______________________  Date: ______
