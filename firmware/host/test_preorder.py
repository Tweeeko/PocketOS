#!/usr/bin/env python3
# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
"""Tests the pre-order and Stripe webhook functions against a pretend Stripe API and mail server.

    python3 firmware/host/test_preorder.py
"""
import hashlib, hmac, json, os, subprocess, sys, threading, time, urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FAILS = []
def check(cond, what):
    print(("PASS " if cond else "FAIL ") + what)
    if not cond: FAILS.append(what)

# ---------- pretend Stripe ----------
class Stripe:
    complete, open_, created, requests = [], [], [], []
class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, code, obj):
        b = json.dumps(obj).encode(); self.send_response(code); self.send_header("Content-Type", "application/json"); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query)
        Stripe.requests.append(("GET", u.path, q, dict(self.headers)))
        if u.path == "/v1/checkout/sessions":
            data = Stripe.complete if q.get("status") == ["complete"] else Stripe.open_
            return self._send(200, {"data": data, "has_more": False})
        if u.path.startswith("/v1/checkout/sessions/"):
            sid = u.path.rsplit("/", 1)[1]
            for s in Stripe.complete + Stripe.open_:
                if s["id"] == sid: return self._send(200, s)
            return self._send(404, {"error": {"message": "No such checkout.session"}})
        self._send(404, {"error": {"message": "unknown"}})
    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0); body = self.rfile.read(n).decode()
        Stripe.requests.append(("POST", self.path, urllib.parse.parse_qs(body), dict(self.headers)))
        if self.path == "/v1/checkout/sessions":
            Stripe.created.append(urllib.parse.parse_qs(body))
            return self._send(200, {"id": "cs_test_new", "url": "https://checkout.stripe.com/c/pay/cs_test_new"})
        self._send(404, {"error": {"message": "unknown"}})
srv = ThreadingHTTPServer(("127.0.0.1", 0), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
STRIPE = f"http://127.0.0.1:{srv.server_address[1]}"

def sess(i, status="complete", paid="paid", edition="first-edition"):
    return {"id": f"cs_test_{i:026d}", "status": status, "payment_status": paid, "metadata": {"edition": edition}}

RUNNER = r"""
const pre = require(process.argv[1] + '/api/preorder.js')._test, hook = require(process.argv[1] + '/api/stripe-webhook.js')._test;
const cases = JSON.parse(process.argv[2]);
const { Readable } = require('stream');
(async () => {
  const out = [];
  for (const c of cases) {
    pre.recent.clear();
    const res = { statusCode: 0, headers: {}, setHeader(k, v) { this.headers[k] = v; }, end(b) { this.body = b; } };
    const sent = [];
    if (c.fn === 'hook') {
      const req = Readable.from([Buffer.from(c.body || '')]); req.method = c.method || 'POST'; req.headers = c.headers || {};
      await hook.handler(req, res, c.env, async m => { if (c.mailFail) throw new Error('smtp down'); sent.push(m); }, c.now);
    } else {
      const req = { method: c.method || 'GET', url: c.url || '/api/preorder', headers: Object.assign({ host: 'www.pocketos.co.uk', 'x-forwarded-for': '1.2.3.4' }, c.headers || {}) };
      await pre.handler(req, res, c.env, c.now);
    }
    let body = res.body; try { body = JSON.parse(res.body); } catch (_) {}
    out.push({ name: c.name, status: res.statusCode, body, sent: sent.map(m => Buffer.from(m.data.split('\r\n\r\n')[1].replace(/\r\n/g, ''), 'base64').toString() + '\n' + m.data.split('\r\n\r\n')[0]) });
  }
  console.log(JSON.stringify(out));
})();
"""
def run(cases):
    p = subprocess.run(["node", "-e", RUNNER, ROOT, json.dumps(cases)], capture_output=True, text=True, timeout=60)
    if p.returncode: print(p.stdout, p.stderr); sys.exit(1)
    return {r["name"]: r for r in json.loads(p.stdout.strip().splitlines()[-1])}, "\n".join(p.stdout.strip().splitlines()[:-1]) + p.stderr   # what the functions logged

NOW = int(time.mktime(time.strptime("2026-12-01 12:00", "%Y-%m-%d %H:%M")) * 1000)
AFTER = int(time.mktime(time.strptime("2027-02-01 00:30", "%Y-%m-%d %H:%M")) * 1000)
ENV = {"STRIPE_SECRET_KEY": "rk_test_x", "STRIPE_API_BASE": STRIPE, "PREORDERS_OPEN": "1", "PREORDER_DELIVERY_PENCE": "499"}

# ---------- availability ----------
Stripe.complete = [sess(i) for i in range(20)] + [sess(99, edition="other")]
Stripe.open_ = [sess(50, "open", "unpaid"), sess(51, "open", "unpaid")]
r, log = run([
    {"name": "not set up", "env": {}, "now": NOW},
    {"name": "no delivery charge", "env": {k: v for k, v in ENV.items() if k != "PREORDER_DELIVERY_PENCE"}, "now": NOW},
    {"name": "open", "env": ENV, "now": NOW},
    {"name": "after deadline", "env": ENV, "now": AFTER},
])
check(r["not set up"]["body"]["open"] is False and r["not set up"]["body"]["reason"] == "soon", "closed until keys and settings are in place")
check(r["no delivery charge"]["body"]["open"] and r["no delivery charge"]["body"]["delivery_text"] == "£4.99", "delivery is £4.99 unless overridden")
o = r["open"]["body"]
check(o["open"] and o["remaining"] == 3 and o["price"] == "£59.99" and o["standard_price"] == "£99.99" and o["delivery_text"] == "£4.99", "counts paid and open checkouts for this edition only: 3 of 25 left")
check(r["after deadline"]["body"]["reason"] == "closed", "closes after 31 January 2027")
key_hdr = [h for (_, path, _, h) in Stripe.requests if path == "/v1/checkout/sessions"][0]
check(key_hdr.get("Stripe-Version") == "2026-10-28.endive" and key_hdr.get("Authorization") == "Bearer rk_test_x", "pinned API version, key only in the header")

# ---------- starting a checkout ----------
r, log = run([
    {"name": "buy", "method": "POST", "env": ENV, "now": NOW, "headers": {"origin": "https://www.pocketos.co.uk"}},
    {"name": "other site", "method": "POST", "env": ENV, "now": NOW, "headers": {"origin": "https://evil.example"}},
])
check(r["buy"]["status"] == 200 and r["buy"]["body"]["url"].startswith("https://checkout.stripe.com/"), "returns the Stripe Checkout address")
check(r["other site"]["status"] == 403, "other websites can't start checkouts")
p = Stripe.created[-1]; g = lambda k: p.get(k, [None])[0]
check(g("mode") == "payment" and g("line_items[0][quantity]") == "1" and g("line_items[0][price_data][unit_amount]") == "5999" and g("line_items[0][price_data][currency]") == "gbp", "one First Edition at £99.99")
check(g("line_items[0][price_data][tax_behavior]") == "inclusive" and g("line_items[0][price_data][product_data][tax_code]") == "txcd_99999999", "price includes any VAT; tangible goods tax code")
check(g("shipping_address_collection[allowed_countries][0]") == "GB" and g("shipping_options[0][shipping_rate_data][fixed_amount][amount]") == "499", "UK addresses only, £4.99 delivery")
check(not any(k.startswith("payment_method_types") for k in p), "no payment_method_types (Stripe chooses payment methods)")
check(g("metadata[edition]") == "first-edition" and g("payment_intent_data[metadata][edition]") == "first-edition", "tagged as First Edition")
check(g("success_url") == "https://www.pocketos.co.uk/order-confirmed?session_id={CHECKOUT_SESSION_ID}" and g("cancel_url") == "https://www.pocketos.co.uk/buy", "returns to the website")
exp = int(g("expires_at")); check(1800 <= exp - NOW // 1000 <= 1900, "an unpaid checkout expires after about 30 minutes")
check(g("automatic_tax[enabled]") is None and g("consent_collection[terms_of_service]") is None, "tax and terms checkbox off until switched on")
check(g("integration_identifier", ) and len(g("integration_identifier").split("-")[-1]) == 8, "integration identifier with 8-letter suffix")
r, _ = run([{"name": "taxed", "method": "POST", "env": dict(ENV, STRIPE_AUTOMATIC_TAX="1", STRIPE_TERMS_CONSENT="1"), "now": NOW}])
p = Stripe.created[-1]; g = lambda k: p.get(k, [None])[0]
check(g("automatic_tax[enabled]") == "true" and g("consent_collection[terms_of_service]") == "required", "Stripe Tax and terms checkbox switch on with settings")

Stripe.complete = [sess(i) for i in range(25)]; Stripe.open_ = []
r, _ = run([{"name": "sold out", "method": "POST", "env": ENV, "now": NOW}, {"name": "sold out status", "env": ENV, "now": NOW}])
check(r["sold out"]["status"] == 409 and r["sold out"]["body"]["reason"] == "sold_out" and r["sold out status"]["body"]["remaining"] == 0, "26th order refused")
n_before = len(Stripe.created)
Stripe.complete = []; Stripe.open_ = []
r, _ = run([{"name": "flood", "method": "POST", "env": ENV, "now": NOW}])
check(len(Stripe.created) == n_before + 1, "checkout created when available")

# ---------- confirmation page lookups ----------
Stripe.complete = [sess(7, paid="paid"), sess(8, paid="unpaid"), sess(9, edition="other")]
r, _ = run([
    {"name": "paid", "url": "/api/preorder?session_id=cs_test_00000000000000000000000007", "env": ENV, "now": NOW},
    {"name": "pending", "url": "/api/preorder?session_id=cs_test_00000000000000000000000008", "env": ENV, "now": NOW},
    {"name": "other", "url": "/api/preorder?session_id=cs_test_00000000000000000000000009", "env": ENV, "now": NOW},
    {"name": "junk", "url": "/api/preorder?session_id=../../v1/customers", "env": ENV, "now": NOW},
])
check(r["paid"]["body"]["payment_status"] == "paid" and r["paid"]["body"]["order"] == "PO-00000007" and set(r["paid"]["body"]) == {"ok", "status", "payment_status", "order"}, "confirmation shows the order number and nothing personal")
check(r["pending"]["body"]["payment_status"] == "unpaid", "pending bank payments reported as pending")
check(r["other"]["status"] == 404 and r["junk"]["status"] == 400, "only First Edition sessions, only valid session ids")

# ---------- webhook ----------
SECRET = "whsec_test"
def signed(obj, t=None, secret=SECRET):
    body = json.dumps(obj); t = t or NOW // 1000
    sig = hmac.new(secret.encode(), f"{t}.{body}".encode(), hashlib.sha256).hexdigest()
    return body, {"stripe-signature": f"t={t},v1={sig}"}
HENV = {"STRIPE_WEBHOOK_SECRET": SECRET, "CONTACT_SMTP_USER": "owner@example.test", "CONTACT_SMTP_PASS": "x"}
paid = {"id": "evt_1", "type": "checkout.session.completed", "data": {"object": {"id": "cs_test_abcdefgh12345678", "payment_status": "paid", "amount_total": 10498, "currency": "gbp",
        "metadata": {"edition": "first-edition"}, "payment_intent": "pi_123", "customer_details": {"email": "sam@example.com", "name": "Sam Jones", "phone": "+447700900000"},
        "collected_information": {"shipping_details": {"name": "Sam Jones", "address": {"line1": "1 High St", "city": "Leeds", "postal_code": "LS1 1AA", "country": "GB"}}}}}}
unpaid = json.loads(json.dumps(paid)); unpaid["id"] = "evt_2"; unpaid["data"]["object"]["payment_status"] = "unpaid"
later = json.loads(json.dumps(paid)); later["id"] = "evt_3"; later["type"] = "checkout.session.async_payment_succeeded"
other = json.loads(json.dumps(paid)); other["id"] = "evt_4"; other["data"]["object"]["metadata"] = {}
b1, h1 = signed(paid); b2, h2 = signed(unpaid); b3, h3 = signed(later); b4, h4 = signed(other)
bad_b, bad_h = signed(paid, secret="whsec_wrong"); old_b, old_h = signed(paid, t=NOW // 1000 - 3600)
r, log = run([
    {"name": "paid", "fn": "hook", "body": b1, "headers": h1, "env": HENV, "now": NOW},
    {"name": "unpaid", "fn": "hook", "body": b2, "headers": h2, "env": HENV, "now": NOW},
    {"name": "later", "fn": "hook", "body": b3, "headers": h3, "env": HENV, "now": NOW},
    {"name": "other", "fn": "hook", "body": b4, "headers": h4, "env": HENV, "now": NOW},
    {"name": "forged", "fn": "hook", "body": bad_b, "headers": bad_h, "env": HENV, "now": NOW},
    {"name": "replayed", "fn": "hook", "body": old_b, "headers": old_h, "env": HENV, "now": NOW},
    {"name": "tampered", "fn": "hook", "body": b1.replace("10498", "1"), "headers": h1, "env": HENV, "now": NOW},
    {"name": "mail down", "fn": "hook", "body": b1, "headers": h1, "env": HENV, "now": NOW, "mailFail": True},
])
m = r["paid"]["sent"][0] if r["paid"]["sent"] else ""
check(r["paid"]["status"] == 200 and "PO-12345678" in m and "Sam Jones" in m and "1 High St" in m and "LS1 1AA" in m and "£104.98" in m and "+447700900000" in m, "paid pre-order emailed with order number, name, address, phone and amount")
check(r["unpaid"]["status"] == 200 and not r["unpaid"]["sent"], "waits for a bank payment to clear before treating it as an order")
check(r["later"]["status"] == 200 and len(r["later"]["sent"]) == 1, "cleared bank payment becomes an order")
check(r["other"]["status"] == 200 and not r["other"]["sent"], "ignores sessions that aren't First Edition pre-orders")
check(r["forged"]["status"] == 400 and r["replayed"]["status"] == 400 and r["tampered"]["status"] == 400, "forged, replayed and altered events rejected")
check(r["mail down"]["status"] == 500, "if the email can't be sent, Stripe is asked to retry")
check("sam@example.com" not in log and "1 High St" not in log and "rk_test_x" not in log, "logs hold no customer details or keys")

print("\nALL PASSED" if not FAILS else f"\n{len(FAILS)} FAILED")
sys.exit(1 if FAILS else 0)
