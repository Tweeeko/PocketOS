#!/usr/bin/env python3
# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
"""PocketOS Factory: install PocketOS on new devices from a PC, as many as you plug in.

Plug a device into any USB port. It is erased, PocketOS is written and checked, the device
restarts, and the tool confirms PocketOS has started. Plug in the next one, or several at once.
Every device is recorded in factory-log.csv (date, port, MAC, device name, version, result).

    python pocketos_factory.py                 # uses pocketos-full.bin next to this file, or the latest release
    python pocketos_factory.py --firmware X    # a specific pocketos-full.bin
    python pocketos_factory.py --download      # fetch the latest release first
    python pocketos_factory.py --once          # flash one device, then stop

Each device makes its own identity on first start (see README.md): nothing here is shared
between devices, and nothing personal is written.
"""
import argparse, csv, datetime, hashlib, json, os, re, subprocess, sys, threading, time, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = "Tweeeko/PocketOS"
RELEASE = f"https://github.com/{REPO}/releases/latest/download/"
ESPRESSIF_USB_VID = 0x303A        # the ESP32-S3's built-in USB port
BRIDGE_VIDS = {0x1A86, 0x10C4}    # CH340 / CP210x USB-serial chips, in case a board uses one
BOOT_WAIT_S = 30
LOG = os.path.join(HERE, "factory-log.csv")

try:
    import serial                          # installed with esptool
    from serial.tools import list_ports
except ImportError:
    print("This needs esptool. Run the 'Flash PocketOS' launcher, or: pip install esptool==4.8.1")
    sys.exit(1)

COLOUR = sys.stdout.isatty() and os.name != "nt" or os.environ.get("WT_SESSION")
def c(code, s): return f"\033[{code}m{s}\033[0m" if COLOUR else s
OK, BAD, DIM, BOLD = (lambda s: c("32", s)), (lambda s: c("31", s)), (lambda s: c("90", s)), (lambda s: c("1", s))

print_lock = threading.Lock()
def say(port, msg):
    with print_lock:
        print(f"{datetime.datetime.now():%H:%M:%S}  {port:<14} {msg}", flush=True)

# ---------- firmware ----------
def fetch(url):
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read()

def get_firmware(path, download):
    """Returns (bytes, version). Prefers a file next to this tool so it works offline."""
    local = path or os.path.join(HERE, "pocketos-full.bin")
    if download or not os.path.exists(local):
        print("Downloading the latest PocketOS release...")
        man = json.loads(fetch(RELEASE + "manifest.json"))
        data = fetch(RELEASE + "pocketos-full.bin")
        want = man.get("full_sha256")
        if want and hashlib.sha256(data).hexdigest() != want:
            sys.exit(BAD("The download didn't match the release. Try again."))
        with open(local, "wb") as f: f.write(data)
        with open(local + ".json", "w") as f: json.dump(man, f)
        return data, man.get("version", "?")
    data = open(local, "rb").read()
    ver = "?"
    if os.path.exists(local + ".json"):
        info = json.load(open(local + ".json"))
        ver = info.get("version", "?")
        if info.get("full_sha256") and hashlib.sha256(data).hexdigest() != info["full_sha256"]:
            sys.exit(BAD("pocketos-full.bin is damaged (it doesn't match its release). Download the factory zip again."))
    else:   # the app's version string sits in its header (esp_app_desc_t), after the bootloader and partition table
        m = re.search(rb"\x32\x54\xcd\xab.{12}([0-9][0-9A-Za-z.\-]{0,30})\x00", data[0x30000:0x30100], re.S)
        if m: ver = m.group(1).decode()
    return data, ver

# ---------- one device ----------
def esptool(port, *args):
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port, "--baud", "921600", *args]
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr

def wait_for_start(port):
    """After the restart, PocketOS prints 'ready, device id xxxx' on its USB port."""
    end = time.time() + BOOT_WAIT_S
    buf = ""
    while time.time() < end:
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = port, 115200, 0.5
            s.dtr = s.rts = False          # don't touch the reset lines: just listen
            s.open()
            with s:
                while time.time() < end:
                    buf += s.read(512).decode("utf-8", "replace")
                    m = re.search(r"ready, device id ([0-9a-f]{4})", buf)
                    if m: return m.group(1)
        except (serial.SerialException, OSError):
            time.sleep(0.5)   # the port disappears for a moment while the device restarts
    return None

def flash_one(port, fw_path, version, results):
    t0 = time.time()
    say(port, "Found a device. Erasing and installing PocketOS " + version + " (about a minute)...")
    rc, out = esptool(port, "--before", "default_reset", "--after", "hard_reset",
                      "write_flash", "--erase-all", "--flash_mode", "keep", "--flash_size", "keep", "0x0", fw_path)
    mac = (re.search(r"MAC:\s*([0-9a-f:]{17})", out, re.I) or [None, ""])[1].lower()
    dev = mac.replace(":", "")[-4:] if mac else ""
    if rc != 0 or "Hash of data verified" not in out:
        why = "couldn't connect. Try another cable or port. If it still fails: hold Power for 6 seconds to switch it off, then hold Boot and hold Power for 1 second, let go, and plug it in again." if "Failed to connect" in out \
              else (out.strip().splitlines() or ["unknown error"])[-1]
        say(port, BAD("FAILED ") + why)
        return record(port, mac, dev, version, "failed: " + why, results)
    say(port, "Written and checked. Waiting for PocketOS to start...")
    started = wait_for_start(port)
    secs = int(time.time() - t0)
    if started:
        say(port, OK(BOLD(f"PASS  PocketOS-{started.upper()}")) + DIM(f"  ({secs} s)  You can unplug it."))
        return record(port, mac, started, version, "pass", results)
    say(port, OK("PASS ") + f"written and verified, PocketOS-{dev.upper()}" + DIM("  (couldn't read the start-up message; check the screen shows the setup code)"))
    return record(port, mac, dev, version, "pass (start not seen)", results)

def record(port, mac, dev, version, result, results):
    new = not os.path.exists(LOG)
    with print_lock, open(LOG, "a", newline="") as f:
        w = csv.writer(f)
        if new: w.writerow(["time", "port", "mac", "device", "setup network", "address", "firmware", "result"])
        w.writerow([datetime.datetime.now().isoformat(timespec="seconds"), port, mac, dev,
                    f"PocketOS-{dev.upper()}" if dev else "", f"pocketos-{dev}.local" if dev else "", version, result])
    results.append(result.startswith("pass"))
    return result

# ---------- watching the USB ports ----------
def candidate_ports():
    return {p.device for p in list_ports.comports() if p.vid == ESPRESSIF_USB_VID or p.vid in BRIDGE_VIDS}

def main():
    ap = argparse.ArgumentParser(description="Install PocketOS on new devices.")
    ap.add_argument("--firmware", help="path to pocketos-full.bin")
    ap.add_argument("--download", action="store_true", help="download the latest release first")
    ap.add_argument("--once", action="store_true", help="flash one device, then stop")
    a = ap.parse_args()

    data, version = get_firmware(a.firmware, a.download)
    fw_path = a.firmware or os.path.join(HERE, "pocketos-full.bin")
    if len(data) < 1_000_000 or data[0] != 0xE9:
        sys.exit(BAD("That doesn't look like pocketos-full.bin."))
    print(BOLD(f"\nPocketOS Factory  ·  firmware {version}  ·  {len(data) // 1024} KB"))
    print("Plug in PocketOS devices with a USB-C data cable. Several at once is fine.")
    print(DIM(f"Results are saved to {LOG}. Press Ctrl+C to stop.\n"))

    busy, done, results = {}, set(), []
    present_at_start = candidate_ports()
    if present_at_start:
        print(DIM("Already plugged in: " + ", ".join(sorted(present_at_start)) + ". These will be flashed now.\n"))
    try:
        while True:
            now = candidate_ports()
            done &= now                                   # unplugged: the port can be used again
            for port in sorted(now - done - set(busy)):
                t = threading.Thread(target=flash_one, args=(port, fw_path, version, results), daemon=True)
                busy[port] = t
                t.start()
            for port, t in list(busy.items()):
                if not t.is_alive():
                    del busy[port]; done.add(port)
                    if a.once: raise KeyboardInterrupt
            time.sleep(1)
    except KeyboardInterrupt:
        for t in busy.values(): t.join()
        print(f"\n{sum(results)} passed, {len(results) - sum(results)} failed. Log: {LOG}")

if __name__ == "__main__":
    main()
