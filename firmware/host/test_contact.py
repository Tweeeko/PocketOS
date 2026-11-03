#!/usr/bin/env python3
# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
"""Tests the website's contact form function (api/contact.js) against a pretend mail server
with STARTTLS and login, the way iCloud Mail works. Needs node and openssl.

    python3 firmware/host/test_contact.py
"""
import base64, json, os, socket, ssl, subprocess, sys, tempfile, threading

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FAILS = []
def check(cond, what):
    print(("PASS " if cond else "FAIL ") + what)
    if not cond: FAILS.append(what)

tmp = tempfile.mkdtemp()
key, crt = os.path.join(tmp, "k.pem"), os.path.join(tmp, "c.pem")
subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", key, "-out", crt,
                "-days", "1", "-subj", "/CN=localhost"], check=True, capture_output=True)

class FakeSMTP:
    """One-connection-at-a-time SMTP server: EHLO, STARTTLS, AUTH PLAIN, MAIL, RCPT, DATA."""
    def __init__(self, password="app-pass", fail_rcpt=False):
        self.password, self.fail_rcpt = password, fail_rcpt
        self.messages, self.logins = [], []
        self.srv = socket.socket(); self.srv.bind(("127.0.0.1", 0)); self.srv.listen(5)
        self.port = self.srv.getsockname()[1]
        threading.Thread(target=self.loop, daemon=True).start()
    def loop(self):
        while True:
            c, _ = self.srv.accept()
            try: self.session(c)
            except Exception: pass
            finally: c.close()
    def session(self, c):
        f = c.makefile("rwb"); tls = False
        def out(s): f.write((s + "\r\n").encode()); f.flush()
        out("220 fake ESMTP")
        mail = {}
        while True:
            line = f.readline().decode().rstrip("\r\n")
            if not line: return
            cmd = line.split(" ")[0].upper()
            if cmd == "EHLO": out("250-fake"); out("250-AUTH PLAIN" if tls else "250-STARTTLS"); out("250 OK")
            elif cmd == "STARTTLS":
                out("220 go ahead")
                ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.load_cert_chain(crt, key)
                c = ctx.wrap_socket(c, server_side=True); f = c.makefile("rwb"); tls = True
            elif cmd == "AUTH":
                if not tls: out("530 TLS first"); continue
                _, user, pw = base64.b64decode(line.split(" ")[2]).split(b"\0")
                self.logins.append(user.decode())
                out("235 OK" if pw.decode() == self.password else "535 bad login")
            elif cmd == "MAIL": mail["from"] = line; out("250 OK")
            elif cmd == "RCPT": mail["to"] = line; out("550 no" if self.fail_rcpt else "250 OK")
            elif cmd == "DATA":
                out("354 send"); data = []
                while True:
                    l = f.readline().decode()
                    if l == ".\r\n": break
                    data.append(l)
                mail["data"] = "".join(data); self.messages.append(mail); out("250 queued")
            elif cmd == "QUIT": out("221 bye"); return
            else: out("500 ?")

RUNNER = r"""
const { handler, recent } = require(process.argv[1])._test;
const cases = JSON.parse(process.argv[2]);
(async () => {
  const out = [];
  for (const c of cases) {
    recent.clear();
    for (let i = 0; i < (c.repeat || 1); i++) {
      const res = { statusCode: 0, headers: {}, setHeader(k, v) { this.headers[k] = v; }, end(b) { this.body = b; } };
      const req = { method: c.method || 'POST', headers: Object.assign({ host: 'site.test', 'x-forwarded-for': '1.2.3.4' }, c.headers || {}), body: c.body };
      await handler(req, res, c.env);
      out.push({ name: c.name, i, status: res.statusCode, body: JSON.parse(res.body) });
    }
  }
  console.log(JSON.stringify(out));
})();
"""

def run(cases):
    p = subprocess.run(["node", "-e", RUNNER, os.path.join(ROOT, "api", "contact.js"), json.dumps(cases)],
                       capture_output=True, text=True, timeout=60)
    if p.returncode: print(p.stderr); sys.exit(1)
    return json.loads(p.stdout.strip().splitlines()[-1]), p.stdout + p.stderr

good = FakeSMTP()
bad_pass = FakeSMTP(password="different")
ADDR = "owner@example.test"
def env(srv): return {"CONTACT_SMTP_USER": ADDR, "CONTACT_SMTP_PASS": "app-pass", "CONTACT_SMTP_HOST": "127.0.0.1",
                      "CONTACT_SMTP_PORT": str(srv.port), "CONTACT_SMTP_INSECURE": "1"}
OK = {"name": "Sam Jones", "email": "sam@example.com", "topic": "Returns or a fault", "order": "PO-1042",
      "message": "Hello, the screen flickers when it charges.\n.\nSecond paragraph with ü."}

res, log = run([
    {"name": "good", "body": OK, "env": env(good)},
    {"name": "bad email", "body": dict(OK, email="sam@"), "env": env(good)},
    {"name": "header injection", "body": dict(OK, email="sam@example.com\r\nBcc: x@y.z"), "env": env(good)},
    {"name": "bad topic", "body": dict(OK, topic="Free stuff"), "env": env(good)},
    {"name": "short", "body": dict(OK, message="hi"), "env": env(good)},
    {"name": "too long", "body": dict(OK, message="x" * 5000), "env": env(good)},
    {"name": "robot", "body": dict(OK, website="http://spam"), "env": env(good)},
    {"name": "other site", "body": OK, "headers": {"origin": "https://evil.example"}, "env": env(good)},
    {"name": "get", "method": "GET", "body": None, "env": env(good)},
    {"name": "not set up", "body": OK, "env": {}},
    {"name": "login refused", "body": OK, "env": env(bad_pass)},
    {"name": "flood", "body": OK, "env": env(good), "repeat": 6},
])
by = {}
for r in res: by.setdefault(r["name"], []).append(r)

check(by["good"][0]["status"] == 200 and by["good"][0]["body"]["ok"], "message sent when the mail server accepts it")
m = good.messages[0]
check(m["to"] == f"RCPT TO:<{ADDR}>" and m["from"] == f"MAIL FROM:<{ADDR}>", "delivered to the address from the settings, sent from it")
check(good.logins == [ADDR] * len(good.messages), "logged in over TLS for each message sent")
hdrs, _, body64 = m["data"].partition("\r\n\r\n")
text = base64.b64decode("".join(body64.split())).decode()
check("Reply-To: Sam Jones <sam@example.com>" in hdrs, "reply goes to the visitor")
check("Sam Jones" in text and "Order number: PO-1042" in text and "ü" in text and "\n.\n" in text.replace("\r", ""), "message text arrives intact")
check(by["bad email"][0]["status"] == 400, "bad email address refused")
check(by["header injection"][0]["status"] == 400 and len(good.messages) <= 7, "line breaks in the email field refused")
check(by["bad topic"][0]["status"] == 400 and by["short"][0]["status"] == 400 and by["too long"][0]["status"] == 400, "topic and message checked")
check(by["robot"][0]["status"] == 200, "robots told it was sent")
check(by["other site"][0]["status"] == 403, "other websites can't post")
check(by["get"][0]["status"] == 405, "only POST")
check(by["not set up"][0]["status"] == 503 and not by["not set up"][0]["body"]["ok"], "without settings it says it can't send")
check(by["login refused"][0]["status"] == 502 and not by["login refused"][0]["body"]["ok"], "failed send isn't reported as sent")
flood = by["flood"]
check([r["status"] for r in flood] == [200] * 5 + [429], "sixth message in an hour refused")
check(len(good.messages) == 6, "exactly the accepted messages were delivered (1 + 5)")
check(ADDR not in log and "sam@example.com" not in log and "flickers" not in log, "logs hold no addresses or message text")
src = open(os.path.join(ROOT, "api", "contact.js")).read() + "".join(open(os.path.join(ROOT, "site", f)).read() for f in ("contact.html", "assets/contact.js"))
check("@icloud" not in src.lower(), "no email address in the website or function code")

print("\nALL PASSED" if not FAILS else f"\n{len(FAILS)} FAILED")
sys.exit(1 if FAILS else 0)
