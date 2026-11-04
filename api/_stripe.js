// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Shared by the pre-order and webhook functions. Files starting with "_" aren't web addresses on Vercel.
//
// Talks to Stripe's REST API directly (no SDK to install), on a pinned API version.
// Settings come from Vercel environment variables, never from code:
//   STRIPE_SECRET_KEY        a restricted key (rk_...) with Checkout Sessions: Write, nothing else
//   STRIPE_WEBHOOK_SECRET    the signing secret of the webhook endpoint (whsec_...)
//   PREORDERS_OPEN           "1" to take pre-orders
//   PREORDER_DELIVERY_PENCE  optional: overrides the £4.99 UK delivery charge (in pence)
//   PREORDER_DELIVERY_NAME   optional, e.g. "Royal Mail Tracked 48"
//   STRIPE_AUTOMATIC_TAX     "1" once Stripe Tax's head office address is set (collects VAT only where registered)
//   STRIPE_TERMS_CONSENT     "1" once the terms of service URL is set in Stripe's public business details
'use strict';
const crypto = require('crypto');

const API_VERSION = '2026-10-28.endive';
const SITE = 'https://www.pocketos.co.uk';

// The First Edition pre-order. Change these here if the plan changes.
const PREORDER = {
  edition: 'first-edition',
  name: 'PocketOS First Edition',
  description: 'Pre-order: one of the first 25 PocketOS, expected to be delivered by the end of April 2027.',
  pricePence: 9999,
  deliveryPence: 499,                               // UK tracked delivery, £4.99
  limit: 25,
  closesAt: Date.parse('2027-01-31T23:59:59Z'),     // 23:59 UK time (GMT in January)
  deliveryBy: 'the end of April 2027',
  taxCode: 'txcd_99999999',                         // Stripe: General - Tangible Goods
  image: SITE + '/media/product/device-front.jpg',
  integrationId: 'pocketos-preorder-qhzkmwtr',      // labels these sessions in the Stripe Dashboard
  holdMinutes: 31,                                  // an unpaid checkout holds a unit this long (Stripe's minimum is 30)
};

function formEncode(obj, prefix, out = []) {
  for (const [k, v] of Object.entries(obj)) {
    if (v === undefined || v === null) continue;
    const key = prefix ? `${prefix}[${k}]` : k;
    if (Array.isArray(v)) v.forEach((item, i) => typeof item === 'object' ? formEncode(item, `${key}[${i}]`, out) : out.push(`${encodeURIComponent(`${key}[${i}]`)}=${encodeURIComponent(item)}`));
    else if (typeof v === 'object') formEncode(v, key, out);
    else out.push(`${encodeURIComponent(key)}=${encodeURIComponent(v)}`);
  }
  return out;
}

// One Stripe API call. Throws with Stripe's error message on failure (never the key).
async function stripe(method, path, params, env = process.env) {
  const key = env.STRIPE_SECRET_KEY;
  if (!key) throw new Error('Stripe key not set');
  const base = env.STRIPE_API_BASE || 'https://api.stripe.com';
  let url = base + path, body;
  const qs = params ? formEncode(params).join('&') : '';
  if (method === 'GET') { if (qs) url += '?' + qs; } else body = qs;
  const r = await fetch(url, {
    method,
    headers: { Authorization: 'Bearer ' + key, 'Stripe-Version': API_VERSION, 'Content-Type': 'application/x-www-form-urlencoded' },
    body,
  });
  const data = await r.json().catch(() => ({}));
  if (!r.ok) { const e = new Error((data.error && data.error.message) || 'Stripe error ' + r.status); e.status = r.status; e.code = data.error && data.error.code; throw e; }
  return data;
}

// Every Checkout Session for this edition with a given status (Stripe lists newest first, 100 a page).
async function sessions(status, sinceSec, env) {
  const out = []; let after;
  for (let page = 0; page < 20; page++) {
    const p = { status, limit: 100, created: sinceSec ? { gte: sinceSec } : undefined, starting_after: after };
    const res = await stripe('GET', '/v1/checkout/sessions', p, env);
    for (const s of res.data) if (s.metadata && s.metadata.edition === PREORDER.edition) out.push(s);
    if (!res.has_more || !res.data.length) break;
    after = res.data[res.data.length - 1].id;
  }
  return out;
}

// Units taken: paid (or awaiting a bank payment) plus checkouts still open, which hold a unit until they expire.
async function unitsTaken(env, now = Date.now()) {
  const [done, open] = await Promise.all([sessions('complete', 0, env), sessions('open', Math.floor(now / 1000) - PREORDER.holdMinutes * 60, env)]);
  return done.length + open.length;
}

// Is the pre-order open? Returns { open, reason, remaining }.
async function availability(env = process.env, now = Date.now()) {
  const delivery = env.PREORDER_DELIVERY_PENCE != null && env.PREORDER_DELIVERY_PENCE !== '' ? parseInt(env.PREORDER_DELIVERY_PENCE, 10) : PREORDER.deliveryPence;
  if (env.PREORDERS_OPEN !== '1' || !env.STRIPE_SECRET_KEY || !(delivery >= 0)) return { open: false, reason: 'soon', remaining: PREORDER.limit, delivery: null };
  if (now > PREORDER.closesAt) return { open: false, reason: 'closed', remaining: 0, delivery };
  const remaining = Math.max(0, PREORDER.limit - await unitsTaken(env, now));
  return { open: remaining > 0, reason: remaining > 0 ? 'open' : 'sold_out', remaining, delivery };
}

// Stripe-Signature check: HMAC-SHA256 of "timestamp.payload", within 5 minutes, constant-time compare.
function verifySignature(payload, header, secret, toleranceSec = 300, now = Date.now()) {
  if (!header || !secret) return false;
  const v1 = [];
  let t = null;
  for (const kv of String(header).split(',')) {
    const [k, v] = kv.split('=');
    if (k === 't') t = v; else if (k === 'v1' && v) v1.push(v);
  }
  if (!t || !v1.length || !/^\d+$/.test(t)) return false;
  if (Math.abs(now / 1000 - Number(t)) > toleranceSec) return false;
  const expected = crypto.createHmac('sha256', secret).update(`${t}.${payload}`, 'utf8').digest('hex');
  const exp = Buffer.from(expected, 'hex');
  return v1.some(sig => { const b = Buffer.from(sig, 'hex'); return b.length === exp.length && crypto.timingSafeEqual(b, exp); });
}

const money = pence => '£' + (pence / 100).toFixed(2);
const orderNumber = id => 'PO-' + String(id).slice(-8).toUpperCase();

module.exports = { API_VERSION, SITE, PREORDER, stripe, sessions, unitsTaken, availability, verifySignature, formEncode, money, orderNumber };
