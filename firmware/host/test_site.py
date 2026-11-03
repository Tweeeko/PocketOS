#!/usr/bin/env python3
# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
"""End-to-end test of the settings website against mock_device.py, in a phone-sized Chromium.

  python3 firmware/host/test_site.py [--shots DIR]
"""
import subprocess, sys, time, json, struct, tempfile, os, urllib.request, argparse
from pathlib import Path
from playwright.sync_api import sync_playwright, expect

HERE = Path(__file__).resolve().parent
ap = argparse.ArgumentParser(); ap.add_argument("--shots", default=""); args = ap.parse_args()
SHOTS = Path(args.shots) if args.shots else None
if SHOTS: SHOTS.mkdir(parents=True, exist_ok=True)
fails = 0
def check(cond, what):
    global fails
    print(("PASS " if cond else "FAIL ") + what, flush=True)
    if not cond: fails += 1

def server(port, setup=False):
    p = subprocess.Popen([sys.executable, str(HERE / "mock_device.py"), "--port", str(port)] + (["--setup"] if setup else []),
                         stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    for _ in range(50):
        try: urllib.request.urlopen(f"http://127.0.0.1:{port}/mock/pin", timeout=1); return p
        except Exception: time.sleep(0.1)
    raise SystemExit("mock did not start")

def mock(port, path, body=None):
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=json.dumps(body).encode() if body is not None else None,
                                 headers={"Content-Type": "application/json"})
    return json.loads(urllib.request.urlopen(req, timeout=5).read() or b"{}")

def shot(page, name):
    if SHOTS: page.screenshot(path=str(SHOTS / f"{name}.png"), full_page=True)

def toast(page, text, timeout=8000):
    expect(page.locator("#toast")).to_contain_text(text, timeout=timeout)

with sync_playwright() as pw:
    br = pw.chromium.launch()
    errors = []
    def newpage():
        ctx = br.new_context(viewport={"width": 390, "height": 844}, device_scale_factor=2, accept_downloads=True)
        pg = ctx.new_page()
        pg.on("pageerror", lambda e: errors.append(str(e)))
        pg.on("console", lambda m: m.type == "error" and "Failed to load resource" not in m.text and errors.append(m.text))  # expected 4xx/offline responses are not bugs
        return pg

    # ---------- 1. first-time setup on PocketOS's own network ----------
    srv = server(8091, setup=True)
    try:
        pg = newpage(); pg.goto("http://127.0.0.1:8091/")
        expect(pg.get_by_text("Let’s set up PocketOS")).to_be_visible(); shot(pg, "01-setup-welcome")
        pg.get_by_role("button", name="Start").click()
        expect(pg.locator(".opt.net")).to_have_count(3)   # duplicate HomeNet merged
        check(pg.locator(".opt.net b").first.inner_text() == "HomeNet", "networks sorted strongest first and de-duplicated")
        pg.locator(".opt.net", has_text="HomeNet").click()
        pg.get_by_role("button", name="Connect").click()
        expect(pg.locator("#werr")).to_have_text("Enter the Wi-Fi password.")
        pg.fill("#wpw", "wrongpass1"); pg.get_by_role("button", name="Connect").click()
        expect(pg.locator("#werr")).to_contain_text("Couldn't join", timeout=8000); shot(pg, "02-setup-wrong-password")
        pg.fill("#wpw", "correct horse"); pg.get_by_role("button", name="Connect").click()
        expect(pg.get_by_text("PocketOS is on your Wi-Fi ✓")).to_be_visible(timeout=8000)
        check(pg.get_by_role("link", name="Open pocketos-a1b2.local").get_attribute("href") == "http://pocketos-a1b2.local/", "setup ends with a link to the home-network address")
        shot(pg, "03-setup-joined")
    finally: srv.terminate()

    # ---------- 2. pairing and the Home Assistant guide ----------
    srv = server(8092); P = 8092; U = f"http://127.0.0.1:{P}/"
    try:
        pg = newpage(); pg.goto(U)
        expect(pg.locator("#pin")).to_be_visible(); shot(pg, "04-pair")
        pg.fill("#pin", "000000")
        expect(pg.locator("#perr")).to_contain_text("doesn't match")
        pin = mock(P, "/mock/pin")["pin"]
        pg.fill("#pin", pin)   # auto-submits at 6 digits
        expect(pg.get_by_text("Do you use Home Assistant?")).to_be_visible(); shot(pg, "05-wizard-ha")
        old = pin; check(mock(P, "/mock/pin")["pin"] != old, "a pairing code works only once")
        pg.get_by_text("Yes, I use it").click()
        pg.fill("#wurl", "http://homeassistant.local:8123"); pg.fill("#wtok", "wrong-token")
        pg.get_by_role("button", name="Test and connect").click()
        expect(pg.locator("#wmsg")).to_contain_text("rejected the token", timeout=8000); shot(pg, "06-wizard-connect-error")
        pg.fill("#wtok", "good-token"); pg.get_by_role("button", name="Test and connect").click()
        expect(pg.get_by_text("Pick your favourites")).to_be_visible(timeout=15000)
        pg.locator('.row', has_text="Floor lamp").locator("label.sw").click()
        toast(pg, "Saved on PocketOS"); shot(pg, "07-wizard-favourites")
        check("light.floor_lamp" in mock_cfg if (mock_cfg := json.loads(urllib.request.urlopen(urllib.request.Request(U + "api/v2/config", headers={"Authorization": "Bearer " + pg.evaluate("localStorage.getItem('pocketos-token-a1b2')")})).read())["favourites"]) else False, "favourite saved on the device")
        pg.get_by_role("button", name="Finish").click()
        pg.get_by_role("button", name="Go to my dashboard").click()
        expect(pg.get_by_text("Everything is working.")).to_be_visible(); shot(pg, "08-home")
        tok = pg.evaluate("localStorage.getItem('pocketos-token-a1b2')")
        check(tok and len(tok) == 32, "token kept on this phone")
        check(tok not in pg.url, "token never in the address bar")

        # reload: stays paired, no wizard
        pg.reload(); expect(pg.get_by_text("Everything is working.")).to_be_visible()
        check(pg.locator("#pin").count() == 0, "still paired after reload")

        # ---------- 3. devices ----------
        pg.locator("nav button", has_text="Devices").click()
        expect(pg.locator("h3", has_text="Living room")).to_be_visible(); shot(pg, "09-devices")
        dev = lambda name: pg.locator(".grp .row", has_text=name)
        dev("Ceiling light").locator("[data-a=fav]").click(); toast(pg, "Saved on PocketOS")
        expect(dev("Ceiling light").locator("[data-a=fav]")).to_have_class("ib on")
        if "on" not in (dev("Floor lamp").locator("[data-a=fav]").get_attribute("class") or ""):
            dev("Floor lamp").locator("[data-a=fav]").click(); toast(pg, "Saved on PocketOS")
        favs = pg.locator("h3:text-is('Favourites') + .card .row b")
        check(favs.all_inner_texts() == ["Floor lamp", "Ceiling light"], "favourites listed in the order they were added")
        pg.get_by_role("button", name="Move Ceiling light up").click(); toast(pg, "Order saved")
        check(favs.all_inner_texts() == ["Ceiling light", "Floor lamp"], "a favourite moves up")
        # rename on PocketOS, then go back to Home Assistant's name
        dev("Floor lamp").locator("[data-a=edit]").click()
        pg.fill("#dn", "Reading lamp"); pg.screenshot(path=str(SHOTS / "09a-edit-device.png")) if SHOTS else None; pg.get_by_role("button", name="Save name").click(); toast(pg, "Name saved")
        expect(dev("Reading lamp")).to_contain_text("In Home Assistant: Floor lamp"); shot(pg, "09b-renamed")
        dev("Reading lamp").locator("[data-a=edit]").click()
        pg.get_by_role("button", name="Use that name").click(); toast(pg, "Name saved")
        expect(dev("Floor lamp")).to_be_visible()
        dev("Floor lamp").locator("[data-a=edit]").click()
        pg.fill("#dn", "x" * 40); check(pg.locator("#dn").input_value() == "x" * 32, "names are limited to 32 characters")
        pg.get_by_role("button", name="Close").click()
        dev("Kettle plug").locator("label.sw").click(); toast(pg, "Saved on PocketOS")
        expect(dev("Kettle plug").locator("[data-a=fav]")).to_be_disabled()
        pg.fill("#q", "blind")
        check(pg.locator(".grp:not(.hide)").count() == 1, "search narrows to one room")
        pg.fill("#q", "zzz"); expect(pg.locator("#noq")).to_be_visible(); pg.fill("#q", "")
        mock(P, "/mock/new", {})
        pg.get_by_role("button", name="Refresh list").click()
        expect(pg.get_by_text("New in Home Assistant")).to_be_visible(); shot(pg, "10-devices-new")
        expect(pg.locator("nav button", has_text="Devices")).to_contain_text("•")
        pg.locator(".row", has_text="Hue Go lamp").get_by_role("button", name="Add", exact=True).click(); toast(pg, "Added to PocketOS")
        expect(pg.get_by_text("New in Home Assistant")).to_have_count(0)

        # ---------- 4. look ----------
        pg.locator("nav button", has_text="Look").click()
        pg.get_by_role("button", name="Violet").click(); toast(pg, "Saved on PocketOS")
        expect(pg.get_by_role("button", name="Violet")).to_have_class("on")
        pg.get_by_role("button", name="12-hour").click(); toast(pg, "Saved on PocketOS")
        tn = pg.locator('input[data-k="tn"]').nth(1)
        tn.fill("Lumières💡"); tn.press("Tab"); toast(pg, "1 to 12 characters")   # 10 letters but 13 bytes
        check(tn.input_value() == "Lights", "too-long tile name is put back")
        tn.fill("Lamps"); tn.press("Tab"); toast(pg, "Tile renamed")
        pg.locator('[data-a="mv"][data-i="1"][data-v="1"]').click(); toast(pg, "Menu order saved")
        check(pg.locator('input[data-k="tn"]').nth(2).input_value() == "Lamps", "tile moved down")
        pg.select_option("#tz", "CET-1CEST,M3.5.0,M10.5.0/3"); toast(pg, "Time zone saved")
        pg.locator('input[data-k="brightness"]').fill("40"); toast(pg, "Saved on PocketOS")
        check("Brightness 40%" in pg.locator("#bl").inner_text(), "brightness confirmed")
        shot(pg, "11-look")

        # ---------- 5. PocketOS unreachable: nothing pretends to save ----------
        mock(P, "/mock/down", {"down": True})
        pg.get_by_role("button", name="White").click(); toast(pg, "Couldn’t reach PocketOS. Nothing was changed.")
        expect(pg.get_by_role("button", name="Violet")).to_have_class("on")
        expect(pg.locator("#pill")).to_have_text("Can’t reach"); shot(pg, "12-unreachable")
        mock(P, "/mock/down", {"down": False})

        # ---------- 6. smart home tab ----------
        pg.locator("nav button", has_text="Smart home").click()
        expect(pg.get_by_text("Connected. PocketOS can see")).to_be_visible()
        check(pg.locator("#tok").input_value() == "" and "A token is saved" in pg.locator("#tok").get_attribute("placeholder"), "saved token is never shown")
        shot(pg, "13-smarthome")
        pg.get_by_role("button", name="Disconnect").click(); shot(pg, "14-confirm-disconnect")
        pg.get_by_role("button", name="Cancel").click()
        check(pg.get_by_role("button", name="Disconnect").count() == 1, "cancel leaves it connected")

        # ---------- 7. backup and restore ----------
        pg.locator("nav button", has_text="More").click(); shot(pg, "15-more")
        with pg.expect_download() as dl: pg.get_by_role("button", name="Download backup").click()
        data = json.loads(Path(dl.value.path()).read_text())
        check("ha_token" not in data and data.get("ha_token_set") is True, "backup has no token")
        data["name"] = "Bedroom PocketOS"; data["ha_url"] = "http://evil.example"
        f = Path(tempfile.mkdtemp()) / "b.json"; f.write_text(json.dumps(data))
        pg.set_input_files("#imp", str(f))
        pg.get_by_role("button", name="Restore", exact=True).click(); toast(pg, "Settings restored")
        expect(pg.locator("#hn")).to_have_text("Bedroom PocketOS")
        tok = pg.evaluate("localStorage.getItem('pocketos-token-a1b2')")
        cfg = json.loads(urllib.request.urlopen(urllib.request.Request(U + "api/v2/config", headers={"Authorization": "Bearer " + tok})).read())
        check(cfg["ha_url"] == "http://homeassistant.local:8123", "restore can't change the Home Assistant address")

        # ---------- 8. firmware update ----------
        def fake_bin(ver, proj="pocketos"):
            b = bytearray(4096); b[0] = 0xE9
            struct.pack_into("<I", b, 32, 0xABCD5432)
            b[48:48 + len(ver)] = ver.encode(); b[80:80 + len(proj)] = proj.encode()
            p = Path(tempfile.mkdtemp()) / "pocketos.bin"; p.write_bytes(b); return str(p)
        pg.set_input_files("#fw", fake_bin("0.2.0", "otherapp"))
        expect(pg.locator("#fwm")).to_contain_text("not PocketOS")
        pg.set_input_files("#fw", fake_bin("0.2.0"))
        expect(pg.get_by_text("Install version 0.2.0?")).to_be_visible(); shot(pg, "16-ota-confirm")
        pg.locator("#modal").get_by_role("button", name="Install").click()
        expect(pg.locator("#fwm")).to_contain_text("Now running v0.2.0", timeout=30000); shot(pg, "17-ota-done")


        # ---------- 8b. battery ----------
        pg.locator("nav button[data-v=home]").click()
        expect(pg.get_by_text("left, going by how you’ve used it lately")).to_be_visible(timeout=8000)
        check(pg.locator('svg[aria-label="Battery level over the last 48 hours"] path').count() == 1, "battery chart drawn")
        check("Last fully charged" in pg.locator(".card:has(svg)").first.inner_text(), "last full charge shown")
        shot(pg, "17b-battery")

        # ---------- 8c. online updates ----------
        pg.locator("nav button", has_text="More").click()
        pg.get_by_role("button", name="Check now").click()
        toast(pg, "PocketOS is up to date")
        expect(pg.get_by_text("You’re on the latest version")).to_be_visible()
        pg.get_by_role("button", name="Install overnight").click(); toast(pg, "Saved on PocketOS")
        expect(pg.get_by_text("Installs between 2 and 5 am")).to_be_visible()
        mock(P, "/mock/release", {"version": "0.3.0", "notes": "Smoother menus and better battery life."})
        pg.get_by_role("button", name="Check now").click()
        toast(pg, "Version 0.3.0 is ready")
        shot(pg, "17c-update-ready")
        pg.locator("nav button[data-v=home]").click()
        expect(pg.get_by_text("PocketOS 0.3.0 is ready")).to_be_visible()
        mock(P, "/mock/battery", {"percent": 20})
        pg.get_by_role("button", name="Install", exact=True).click()
        toast(pg, "Charge PocketOS to 30%")
        mock(P, "/mock/battery", {"percent": 80}); pg.reload()
        expect(pg.get_by_text("PocketOS 0.3.0 is ready")).to_be_visible()
        pg.get_by_role("button", name="Install", exact=True).click()
        expect(pg.get_by_text("Install version 0.3.0?")).to_be_visible(); shot(pg, "17d-update-confirm")
        pg.locator("#modal").get_by_role("button", name="Install").click()
        toast(pg, "Now running v0.3.0", timeout=30000)
        check(pg.locator("text=PocketOS 0.3.0 is ready").count() == 0, "update banner gone after install")
        shot(pg, "17e-update-done")

        # ---------- 9. reset needs BOOT ----------
        pg.locator("nav button[data-v=more]").click()
        pg.get_by_role("button", name="Reset everything").click()
        pg.get_by_role("button", name="Erase it").click()
        expect(pg.get_by_text("Confirm on PocketOS")).to_be_visible(); shot(pg, "18-reset-boot")
        time.sleep(1.5); mock(P, "/mock/boot", {})
        expect(pg.get_by_text("PocketOS was reset")).to_be_visible(timeout=8000)
        check(pg.evaluate("localStorage.getItem('pocketos-token-a1b2')") is None, "token forgotten after reset")
    finally: srv.terminate()

    # ---------- 10. lockout after 5 wrong codes ----------
    srv = server(8093)
    try:
        pg = newpage(); pg.goto("http://127.0.0.1:8093/")
        for i in range(5):
            pg.fill("#pin", "%06d" % (111111 * (i % 9 + 1) % 1000000 if i else 999998))
            pg.wait_for_timeout(250)
        pg.fill("#pin", mock(8093, "/mock/pin")["pin"])
        expect(pg.locator("#perr")).to_contain_text("Too many wrong codes")
    finally: srv.terminate()

    check(not errors, "no page errors" + ("" if not errors else ": " + "; ".join(errors[:5])))
    br.close()

print(f"\n{'ALL PASSED' if not fails else str(fails) + ' FAILED'}")
sys.exit(1 if fails else 0)
