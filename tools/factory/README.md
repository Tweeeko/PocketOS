# PocketOS Factory

Installs PocketOS on new devices from a PC, ready to box. Plug a device in and it is erased,
PocketOS is written and checked, the device restarts, and the tool confirms PocketOS started.
You can plug in several at once with a USB hub; each one is done in parallel.

## Getting it

Every release has a **pocketos-factory.zip** on the
[releases page](https://github.com/Tweeeko/PocketOS/releases/latest), with that release's
firmware already inside. Unzip it anywhere. It works offline after the first run.

## Using it

1. **Windows:** double-click **Flash PocketOS (Windows).bat**.
   **Mac:** double-click **flash-pocketos.command** (the first time, right-click → Open).
   The first run installs what it needs (Python 3 and esptool), which takes about a minute.
2. Plug a device in with a USB-C **data** cable. A line appears for it:
   `PASS  PocketOS-A1B2  You can unplug it.` takes about a minute.
3. Unplug it and plug in the next. Press **Ctrl+C** to stop. You get a count of passes and failures.

Every device is recorded in **factory-log.csv**: date, MAC address, device name
(`PocketOS-A1B2`), its address (`pocketos-a1b2.local`), firmware version and result. Keep it for
warranty and support.

## If a device fails

- **"couldn't connect"**: try another cable or port. If it still fails, put it in recovery mode:
  hold **Power** for 6 seconds to switch it off, then hold **Boot** and, while holding it, hold
  **Power** for 1 second. Let go of both and plug it in again. The screen stays dark in recovery mode.
- **"start not seen"**: the firmware was written and verified, but the tool couldn't read the
  start-up message. Check the screen shows the PocketOS logo and then the setup code.

## What each device gets

Nothing is shared between devices, and nothing needs to be set per device:

| | How it's set |
|---|---|
| Name on the network | `PocketOS-XXXX` and `pocketos-xxxx.local`, from the last four characters of the device's own MAC address |
| Setup Wi-Fi password | 8 random characters, made by the device on its first start and shown in the code on its screen |
| Pairing code | 6 random digits, new each time a phone pairs |
| Display name | "PocketOS"; the owner can rename it on the settings page |

The whole flash is erased first, so every device starts as new. Don't copy the flash from a
device that has been set up: it would carry that owner's Wi-Fi and Home Assistant details.

## Options

```
pocketos_factory.py --download          # use the newest release instead of the one in the folder
pocketos_factory.py --firmware X.bin    # use a specific pocketos-full.bin
pocketos_factory.py --once              # flash one device and stop
```

No-install alternative for the odd device: the **Software recovery** page on the website installs
PocketOS from Chrome or Edge, one device at a time.
