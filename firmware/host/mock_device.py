#!/usr/bin/env python3
# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
"""Pretend PocketOS for trying the settings website on a computer.

  python3 firmware/host/mock_device.py [--setup] [--port 8080]
  then open http://localhost:8080  (PIN is printed below)

It mirrors the device's API v2 (firmware/main/esp/web.c, docs/api.md) closely enough to test
the website: same paths, status codes, validation rules and error messages. Nothing is real.
Test hooks (not on the device): POST /mock/down {"down":true} makes every API call fail,
POST /mock/new adds a "new" device, POST /mock/boot presses BOOT during a reset.
"""
import json, re, sys, time, threading, copy, secrets, argparse
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INDEX = ROOT / "main" / "www" / "index.html"

TILE_KEYS = [("favourites", "Favourites"), ("lights", "Lights"), ("heating", "Heating"), ("media", "Media"),
             ("locks", "Locks"), ("rooms", "Rooms"), ("scenes", "Scenes"), ("plugs", "Plugs"), ("fans", "Fans"),
             ("blinds", "Blinds"), ("sensors", "Sensors")]

def defaults():
    return {"schema": 4, "name": "PocketOS", "brightness": 80, "clock_24h": True, "date_format": "dmy", "sleep_s": 30,
            "eye": "blue", "anim": "lively", "tz": "GMT0BST,M3.5.0/1,M10.5.0", "auto_add": False, "alerts": True, "speak": True, "volume": 60, "updates": "notify",
            "tiles": [{"key": k, "label": l, "show": i < 7} for i, (k, l) in enumerate(TILE_KEYS)],
            "favourites": [], "hidden": [], "known": [], "names": [], "ha_url": "", "ha_token": ""}

ENTITIES = [
    ("light.floor_lamp", "Floor lamp", "light", "Living room", "On · 60%", "On, off and brightness"),
    ("media_player.tv", "TV", "media_player", "Living room", "Paused", "Play, pause, skip and volume"),
    ("climate.thermostat", "Thermostat", "climate", "Living room", "Heating to 20.5°", "Target temperature and heating on/off"),
    ("light.kitchen", "Ceiling light", "light", "Kitchen", "Off", "On, off and brightness"),
    ("switch.kettle", "Kettle plug", "switch", "Kitchen", "Off", "On and off"),
    ("cover.blinds", "Blinds", "cover", "Bedroom", "Open", "Open, close and position"),
    ("fan.bedroom", "Fan", "fan", "Bedroom", "Off", "On, off and speed"),
    ("lock.front", "Front door", "lock", "Hallway", "Locked", "Lock and unlock"),
    ("sensor.hall_temp", "Hall temperature", "sensor", "Hallway", "19.2 °C", "Shows a live reading"),
    ("scene.goodnight", "Goodnight", "scene", "", "", "Runs with one tap"),
    ("scene.movie", "Movie time", "scene", "", "", "Runs with one tap"),
]

class Dev:
    def __init__(self, setup):
        self.lock = threading.Lock()
        self.setup = setup
        self.cfg = defaults()
        self.pin = "%06d" % secrets.randbelow(1000000)
        self.tokens = []
        self.bad = 0
        self.locked_until = 0
        self.ha = "not_set"
        self.ha_error = ""
        self.ents = list(ENTITIES)
        self.down = False
        self.reset_deadline = 0
        self.booted = time.time()
        self.firmware = "0.1.0"
        self.restarting_until = 0
        self.release = None            # (version, notes) published by /mock/release
        self.upd = {"state": "idle", "progress": 0, "error": "", "checked": None}
        now = int(time.time())
        # 48 h of battery history: on battery for a day at ~4 %/h, charged overnight, now on battery again
        self.hist = []
        for i in range(48 * 6):
            t = now - (48 * 6 - i) * 600
            h = i / 6
            if h < 18: pct, fl = 100 - h * 4.5, 0
            elif h < 21: pct, fl = min(100, 19 + (h - 18) * 30), 3
            elif h < 26: pct, fl = 100, 2
            else: pct, fl = 100 - (h - 26) * 4, 0
            self.hist.append([t, int(pct), int(3500 + pct * 6.5), fl])
        self.battery = self.hist[-1][1]

D = None

def validate(cur, patch, trusted):
    s = copy.deepcopy(cur)
    if not isinstance(patch, dict): return None, ("Expected a JSON object", None)
    for k, v in patch.items():
        if k == "schema": continue
        if k == "name":
            if not isinstance(v, str) or not 1 <= len(v.encode()) <= 24: return None, ("Name must be 1 to 24 characters", k)
        elif k == "brightness":
            if type(v) is not int or not 10 <= v <= 100: return None, ("Brightness must be 10 to 100", k)
        elif k == "volume":
            if type(v) is not int or not 10 <= v <= 100: return None, ("Volume must be 10 to 100", k)
        elif k in ("clock_24h", "auto_add", "alerts", "speak"):
            if type(v) is not bool: return None, ("Must be true or false", k)
        elif k == "date_format":
            if v not in ("dmy", "mdy"): return None, ("Must be dmy or mdy", k)
        elif k == "sleep_s":
            if type(v) is not int or v not in (15, 30, 60, 0): return None, ("Sleep time must be 15, 30, 60 or 0", k)
        elif k == "eye":
            if v not in ("blue", "violet", "white"): return None, ("Eye colour must be blue, violet or white", k)
        elif k == "anim":
            if v not in ("calm", "lively", "off"): return None, ("Animation must be calm, lively or off", k)
        elif k == "tz":
            if not isinstance(v, str) or not 3 <= len(v) <= 47 or not re.fullmatch(r"[A-Za-z0-9<>+\-:,./]+", v): return None, ("Time zone looks wrong", k)
        elif k == "tiles":
            keys = [t[0] for t in TILE_KEYS]
            if not isinstance(v, list) or len(v) != len(keys): return None, ("Tiles list must include every tile once", k)
            seen = set()
            for t in v:
                if not isinstance(t, dict) or t.get("key") not in keys or t["key"] in seen: return None, ("Unknown or repeated tile", k)
                if not isinstance(t.get("label"), str) or not 1 <= len(t["label"].encode()) <= 12: return None, ("Tile names must be 1 to 12 characters", k)
                if type(t.get("show")) is not bool: return None, ("Tile show must be true or false", k)
                seen.add(t["key"])
            v = [{"key": t["key"], "label": t["label"], "show": t["show"]} for t in v]
        elif k == "updates":
            if v not in ("off", "notify", "auto"): return None, ("Updates must be off, notify or auto", k)
        elif k in ("favourites", "hidden", "known"):
            if not isinstance(v, list) or len(v) > 256 or any(not isinstance(x, str) or not 0 < len(x) < 64 for x in v): return None, ("List of device ids expected", k)
        elif k == "names":
            if not isinstance(v, list) or len(v) > 48: return None, ("You can rename up to 48 devices", k)
            seen = set()
            for x in v:
                if not isinstance(x, dict) or not isinstance(x.get("id"), str) or not isinstance(x.get("name"), str): return None, ("Contains an invalid device ID", k)
                if x["id"] in seen: return None, ("A device is named twice", k)
                if not 1 <= len(x["name"].encode()) <= 32 or not x["name"].strip(): return None, ("Names can be up to 32 characters", k)
                seen.add(x["id"])
            v = [{"id": x["id"], "name": x["name"]} for x in v]
        elif trusted and k in ("ha_url", "ha_token"):
            if not isinstance(v, str): return None, ("Must be text", k)
        else:
            return None, ("Unknown setting", k)
        s[k] = v
    return s, None

def public_cfg(c):
    o = {k: v for k, v in c.items() if k != "ha_token"}
    o["ha_token_set"] = bool(c["ha_token"])
    return o

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def send(self, code, obj=None, raw=None, ctype="application/json"):
        body = raw if raw is not None else json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def err(self, code, msg, field=None):
        o = {"error": msg}
        if field: o["field"] = field
        self.send(code, o)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(n) if n else b""

    def jbody(self):
        try: return json.loads(self.body() or b"null")
        except Exception: return None

    def authed(self):
        h = self.headers.get("Authorization", "")
        return h.startswith("Bearer ") and h[7:] in D.tokens

    def unavailable(self):
        if D.down or time.time() < D.restarting_until:
            self.close_connection = True
            try: self.connection.shutdown(2)
            except Exception: pass
            return True
        return False

    def do_GET(self):
        p = self.path.split("?")[0]
        if p == "/" or p == "/index.html":
            return self.send(200, raw=INDEX.read_bytes(), ctype="text/html")
        if p == "/mock/pin": return self.send(200, {"pin": D.pin})
        if not p.startswith("/api/v2/"): return self.err(404, "Not found")
        if self.unavailable(): return
        with D.lock: return self.get(p[7:])

    def do_POST(self):
        p = self.path.split("?")[0]
        if p.startswith("/mock/"):
            j = self.jbody() or {}
            with D.lock:
                if p == "/mock/down": D.down = bool(j.get("down"))
                if p == "/mock/new": D.ents.append(("light.hue_go", "Hue Go lamp", "light", "Bedroom", "Off", "On, off and brightness"))
                if p == "/mock/boot" and time.time() < D.reset_deadline:
                    D.__init__(True); D.restarting_until = time.time() + 4
                if p == "/mock/release": D.release = (j.get("version"), j.get("notes", ""))
                if p == "/mock/battery": D.battery = j.get("percent", D.battery)
                if p == "/mock/ha":
                    D.ha = j.get("status", D.ha); D.ha_error = j.get("error", "")
            return self.send(200, {"ok": True})
        if not p.startswith("/api/v2/"): return self.err(404, "Not found")
        if self.unavailable(): return
        with D.lock: return self.post(p[7:])

    def upd_info(self):
        u = {"online": True, "mode": D.cfg["updates"], "state": D.upd["state"], "progress": D.upd["progress"], "error": D.upd["error"]}
        if D.upd["checked"]: u["checked"] = D.upd["checked"]
        if D.release and D.release[0] > D.firmware and D.upd["state"] in ("available", "downloading"):
            u.update(available=D.release[0], notes=D.release[1], size=1843200)
        return u

    def visible(self, e):
        c = D.cfg
        return (c["auto_add"] or e[0] in c["known"]) and e[0] not in c["hidden"]

    def get(self, p):
        c = D.cfg
        if p == "/info":
            if not D.setup and not self.authed():   # before pairing: only what's needed to pair
                return self.send(200, {"api": 2, "schema": 4, "id": "a1b2", "name": c["name"], "firmware": D.firmware,
                                       "mode": "normal", "host": "pocketos-a1b2.local", "paired": False})
            return self.send(200, {"api": 2, "schema": 4, "id": "a1b2", "name": c["name"], "firmware": D.firmware, "paired": True,
                "mode": "setup" if D.setup else "normal",
                "wifi": {"ssid": "PocketOS-A1B2" if D.setup else "HomeNet", "rssi": -58, "ip": "192.168.4.1" if D.setup else "192.168.1.42"},
                "host": "pocketos-a1b2.local", "battery": {"percent": D.battery, "charging": False, "plugged": False, "mv": 3500 + D.battery * 6 + 100, "hours_left": round(D.battery / 4)},
                "ha": {"status": D.ha, "error": D.ha_error, "devices": sum(1 for e in D.ents if self.visible(e)) if D.ha == "connected" else 0,
                       "new": sum(1 for e in D.ents if e[0] not in c["known"]) if D.ha == "connected" and not c["auto_add"] else 0},
                "uptime_s": int(time.time() - D.booted), "update": self.upd_info()})
        if p == "/wifi/scan":
            if not D.setup: return self.err(403, "Only available during first setup")
            time.sleep(0.6)
            return self.send(200, {"networks": [{"ssid": "HomeNet", "rssi": -48, "secure": True}, {"ssid": "BT-Hub-7F2", "rssi": -71, "secure": True},
                                                {"ssid": "HomeNet", "rssi": -80, "secure": True}, {"ssid": "Cafe free", "rssi": -83, "secure": False}]})
        if not self.authed(): return self.err(401, "Pair with the PIN on the device first")
        if p == "/config": return self.send(200, public_cfg(c))
        if p == "/export": return self.send(200, public_cfg(c))
        if p == "/battery":
            return self.send(200, {"percent": D.battery, "mv": 3500 + D.battery * 6 + 100, "present": True, "charging": False, "full": False,
                                   "plugged": False, "now": int(time.time()), "last_full": D.hist[21 * 6][0],
                                   "estimate": {"known": True, "pct_per_h": 4.0, "hours_left": round(D.battery / 4, 1), "full_h": 25.0, "basis_h": 22.0},
                                   "history": D.hist})
        if p == "/entities":
            known = c["known"]
            return self.send(200, {"areas": sorted({e[3] for e in D.ents if e[3]}), "entities": [
                {"id": e[0], "name": next((n["name"] for n in c["names"] if n["id"] == e[0]), e[1]), "ha_name": e[1], "domain": e[2], "area": e[3], "status": e[4], "controls": e[5],
                 "fav": e[0] in c["favourites"], "hidden": e[0] in c["hidden"], "new": (not c["auto_add"]) and e[0] not in known}
                for e in D.ents] if D.ha == "connected" else []})
        return self.err(404, "Not found")

    def post(self, p):
        if p == "/pair":
            now = time.time()
            if now < D.locked_until: return self.err(429, "Too many wrong codes. Wait a minute and try again.")
            j = self.jbody() or {}
            if j.get("pin") != D.pin:
                D.bad += 1
                if D.bad >= 5: D.bad = 0; D.locked_until = now + 60
                return self.err(401, "That code doesn't match the one on PocketOS", "pin")
            D.bad = 0
            t = secrets.token_hex(16)
            D.tokens = (D.tokens + [t])[-4:]
            D.pin = "%06d" % secrets.randbelow(1000000)
            print("PIN is now", D.pin, flush=True)
            return self.send(200, {"token": t})
        if p == "/wifi":
            if not D.setup: return self.err(403, "Only available during first setup")
            j = self.jbody() or {}
            ssid, pw = j.get("ssid"), j.get("password", "")
            if not isinstance(ssid, str) or not ssid or len(ssid) > 32 or not isinstance(pw, str) or len(pw) > 63:
                return self.err(400, "Choose a network and enter its password", "ssid")
            time.sleep(1.0)
            if pw != "correct horse":
                return self.err(400, "Couldn't join that network. Check the password, and that it's 2.4 GHz.", "password")
            D.setup = False
            return self.send(200, {"ok": True, "ip": "192.168.1.42", "host": "pocketos-a1b2.local"})
        if not self.authed(): return self.err(401, "Pair with the PIN on the device first")
        if p == "/config":
            j = self.jbody()
            if j is None: return self.err(400, "That wasn't valid settings data")
            s, e = validate(D.cfg, j, False)
            if e: return self.err(400, e[0], e[1])
            D.cfg = s
            return self.send(200, public_cfg(s))
        if p == "/entities":
            j = self.jbody() or {}
            c = D.cfg
            if j.get("all_known") is True:
                c["known"] = sorted(set(c["known"]) | {e[0] for e in D.ents})
                return self.send(200, {"ok": True})
            i = j.get("id")
            if not isinstance(i, str): return self.err(400, "Missing device id", "id")
            if "name" in j:
                ent = next((e for e in D.ents if e[0] == i), None)
                if not ent: return self.err(404, "That device isn't in Home Assistant any more", "id")
                nm = j["name"]
                if not isinstance(nm, str): return self.err(400, "Name must be text", "name")
                if len(nm.encode()) > 32: return self.err(400, "Names can be up to 32 characters", "name")
                if nm and not nm.strip(): return self.err(400, "Names can't be only spaces", "name")
                rest = [n for n in c["names"] if n["id"] != i]
                if nm and nm != ent[1]:
                    if len(rest) >= 48: return self.err(400, "You can rename up to 48 devices", "name")
                    rest.append({"id": i, "name": nm})
                c["names"] = rest
            if isinstance(j.get("move"), int) and i in c["favourites"]:
                f = c["favourites"]; a = f.index(i); b = a + (-1 if j["move"] < 0 else 1)
                if 0 <= b < len(f): f[a], f[b] = f[b], f[a]
            for k, lst in (("fav", "favourites"), ("hidden", "hidden"), ("known", "known")):
                if isinstance(j.get(k), bool):
                    if lst == "favourites":   # order matters: new favourites go to the end
                        c[lst] = [x for x in c[lst] if x != i] + ([i] if j[k] else [])
                    else:
                        s = set(c[lst]); (s.add if j[k] else s.discard)(i); c[lst] = sorted(s)
            return self.send(200, {"ok": True})
        if p in ("/ha", "/ha/test"):
            j = self.jbody() or {}
            url, tok = j.get("url"), j.get("token") or D.cfg["ha_token"]
            if not isinstance(url, str) or not url.startswith(("http://", "https://")):
                return self.err(400, "The address should look like http://homeassistant.local:8123", "url")
            if p == "/ha/test":
                time.sleep(0.5)
                if "bad" in url: return self.err(400, "Couldn't reach Home Assistant at that address", "url")
                if tok != "good-token": return self.err(400, "Home Assistant rejected the token", "url")
                return self.send(200, {"ok": True, "devices": len(D.ents), "message": "Home Assistant 2026.10"})
            D.cfg["ha_url"] = url
            if j.get("token"): D.cfg["ha_token"] = j["token"]
            D.ha = "connecting"
            def later():
                time.sleep(1.2)
                with D.lock:
                    D.ha = "connected" if D.cfg["ha_token"] == "good-token" else "error"
                    D.ha_error = "" if D.ha == "connected" else "Home Assistant rejected the token"
                    if D.ha == "connected" and not D.cfg["known"]:
                        D.cfg["known"] = sorted(e[0] for e in D.ents)   # first sync: everything counts as known
            threading.Thread(target=later, daemon=True).start()
            return self.send(200, {"ok": True})
        if p == "/update/check":
            if D.upd["state"] in ("checking", "downloading"): return self.err(409, "PocketOS is already checking or updating")
            D.upd.update(state="checking", error="")
            def later():
                time.sleep(1.0)
                with D.lock:
                    D.upd["checked"] = int(time.time())
                    if D.release and D.release[0] > D.firmware: D.upd["state"] = "available"
                    else: D.upd["state"] = "latest"
            threading.Thread(target=later, daemon=True).start()
            return self.send(202, {"ok": True})
        if p == "/update/install":
            if not (D.release and D.release[0] > D.firmware): return self.err(400, "There's no newer version to install")
            if D.battery < 30: return self.err(400, "Charge PocketOS to 30% or plug it in first")
            D.upd.update(state="downloading", progress=0)
            def run():
                for i in range(1, 11):
                    time.sleep(0.3)
                    with D.lock: D.upd["progress"] = i * 10
                with D.lock:
                    D.firmware = D.release[0]; D.upd.update(state="idle", progress=0)
                    D.restarting_until = time.time() + 3; D.booted = time.time() + 3
            threading.Thread(target=run, daemon=True).start()
            return self.send(202, {"ok": True})
        if p == "/ha/disconnect":
            D.cfg["ha_token"] = ""; D.ha = "not_set"
            return self.send(200, {"ok": True})
        if p == "/restart":
            D.restarting_until = time.time() + 3; D.booted = time.time() + 3
            return self.send(202, {"ok": True})
        if p == "/wifi/forget":
            D.setup = True; D.restarting_until = time.time() + 3
            return self.send(202, {"ok": True})
        if p == "/unpair_all":
            D.tokens = []
            return self.send(200, {"ok": True})
        if p == "/reset":
            D.reset_deadline = time.time() + 15
            return self.send(202, {"ok": True, "confirm": "Press the Boot button on PocketOS within 15 seconds"})
        if p == "/ota":
            data = self.body()
            if len(data) < 112 or data[0] != 0xE9: return self.err(400, "Update failed. PocketOS is still on the old version.")
            D.firmware = data[48:80].split(b"\0")[0].decode()
            D.restarting_until = time.time() + 6; D.booted = time.time() + 6
            return self.send(200, {"ok": True})
        return self.err(404, "Not found")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--setup", action="store_true", help="start in first-time setup mode")
    ap.add_argument("--port", type=int, default=8080)
    a = ap.parse_args()
    D = Dev(a.setup)
    print(f"Mock PocketOS on http://localhost:{a.port}  PIN {D.pin}  (Wi-Fi password: correct horse, HA token: good-token)", flush=True)
    ThreadingHTTPServer(("127.0.0.1", a.port), H).serve_forever()
