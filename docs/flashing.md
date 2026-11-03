# Installing firmware

## First install (needs a PC once)

iPad Safari cannot talk to USB serial devices, so the very first install uses a PC
with Chrome or Edge. Nothing needs installing on the PC.

**Easiest:** open https://tweeeko.github.io/PocketOS/install.html in Chrome or Edge on the PC,
plug the board in and press **Install PocketOS**. The steps below do the same by hand.

1. Get `pocketos-full.bin`. Easiest: on the PC, open the repository's **Releases** page and
   download it from the latest release. (For an unreleased build: **Actions** → latest
   **Build firmware** run → **pocketos-firmware** artifact, then unzip it.)
2. Use a release build for the units you give away, so they look for updates in the right place.
3. Open https://espressif.github.io/esptool-js/ in Chrome or Edge.
4. Plug PocketOS into the PC by USB-C. Click **Connect** and choose the port
   (often "USB JTAG/serial debug unit").
   If it does not appear: hold **BOOT**, tap **PWR** (or unplug/replug), release BOOT, try again.
5. Under **Flash address** enter `0x0`, choose `pocketos-full.bin`, click **Program**.
6. When it finishes, unplug and replug. The device should show its setup screen.

Repeat for the second unit.

## Updates after that (from the iPad, over Wi-Fi)

The proof-of-concept settings page has a **Firmware** section: pair, choose
`pocketos.bin` from the latest Actions artifact, upload. The device writes it to its
spare slot and restarts. If the new version fails to boot properly, it rolls back.

## Building in a Codespace instead

```
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py merge-bin -o pocketos-full.bin
```

Files appear in `firmware/build/`. Download them via the Codespace file explorer.

## After that

Updates arrive over Wi-Fi. See `docs/releasing.md` for publishing one, or install a
`pocketos.bin` by hand from the settings website (**More → Install from a file**).
