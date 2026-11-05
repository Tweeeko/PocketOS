// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// First Edition pre-orders.
//   GET  /api/preorder                    is it open, how many are left, the delivery charge
//   GET  /api/preorder?session_id=cs_...  how a finished checkout went (for the confirmation page)
//   POST /api/preorder                    starts a Stripe Checkout and returns its address
// The order itself is recorded by /api/stripe-webhook when Stripe confirms payment, not here.
'use strict';
const { PREORDER, SITE, stripe, availability, money, orderNumber } = require('./_stripe');

const recent = new Map();   // per-address checkout starts, so nobody can hold every unit with open checkouts
const PER_HOUR = 4;

function reply(res, status, obj) {
  res.statusCode = status;
  res.setHeader('Content-Type', 'application/json');
  res.setHeader('Cache-Control', 'no-store');
  res.end(JSON.stringify(obj));
}

function limited(ip, now) {
  const t = (recent.get(ip) || []).filter(x => now - x < 3600e3);
  if (t.length >= PER_HOUR) { recent.set(ip, t); return true; }
  t.push(now); recent.set(ip, t); if (recent.size > 5000) recent.clear();
  return false;
}

function sessionParams(delivery, env, now) {
  return {
    mode: 'payment',
    line_items: [{
      quantity: 1,
      price_data: {
        currency: 'gbp',
        unit_amount: PREORDER.pricePence,
        tax_behavior: 'inclusive',                       // the price the customer pays includes any VAT
        product_data: { name: PREORDER.name, description: PREORDER.description, images: [PREORDER.image], tax_code: PREORDER.taxCode, metadata: { edition: PREORDER.edition } },
      },
    }],
    shipping_address_collection: { allowed_countries: ['GB'] },
    shipping_options: [{
      shipping_rate_data: {
        type: 'fixed_amount',
        display_name: env.PREORDER_DELIVERY_NAME || 'UK tracked delivery, by ' + PREORDER.deliveryBy,
        fixed_amount: { amount: delivery, currency: 'gbp' },
        tax_behavior: 'inclusive',
      },
    }],
    phone_number_collection: { enabled: true },          // for the courier
    customer_creation: 'always',
    automatic_tax: env.STRIPE_AUTOMATIC_TAX === '1' ? { enabled: true } : undefined,
    consent_collection: env.STRIPE_TERMS_CONSENT === '1' ? { terms_of_service: 'required' } : undefined,
    custom_text: {
      submit: { message: `You’re reserving one of only ${PREORDER.limit} PocketOS First Editions at the First Edition price of ${money(PREORDER.pricePence)} (usually ${money(PREORDER.standardPence)}). Payment is taken today, and your PocketOS is expected to arrive by ${PREORDER.deliveryBy}. We’ll email you as soon as it’s on its way.` },
      terms_of_service_acceptance: env.STRIPE_TERMS_CONSENT === '1' ? { message: `I agree to the [terms of sale](${SITE}/terms), including the pre-order terms.` } : undefined,
    },
    metadata: { edition: PREORDER.edition },
    payment_intent_data: { description: PREORDER.name + ' pre-order', metadata: { edition: PREORDER.edition } },
    success_url: SITE + '/order-confirmed?session_id={CHECKOUT_SESSION_ID}',
    cancel_url: SITE + '/buy',
    expires_at: Math.floor(now / 1000) + PREORDER.holdMinutes * 60,
    integration_identifier: PREORDER.integrationId,
  };
}

async function handler(req, res, env = process.env, now = Date.now()) {
  const url = new URL(req.url, 'http://x');
  try {
    if (req.method === 'GET') {
      const sid = url.searchParams.get('session_id');
      if (sid) {   // only what the confirmation page needs; nothing personal
        if (!/^cs_[A-Za-z0-9_]{10,200}$/.test(sid)) return reply(res, 400, { ok: false });
        const s = await stripe('GET', '/v1/checkout/sessions/' + encodeURIComponent(sid), null, env);
        if (!s.metadata || s.metadata.edition !== PREORDER.edition) return reply(res, 404, { ok: false });
        return reply(res, 200, { ok: true, status: s.status, payment_status: s.payment_status, order: orderNumber(s.id) });
      }
      const a = await availability(env, now);
      return reply(res, 200, { ok: true, ...a, limit: PREORDER.limit, price: money(PREORDER.pricePence), standard_price: money(PREORDER.standardPence),
        delivery_text: a.delivery == null ? null : (a.delivery ? money(a.delivery) : 'Free'), closes: new Date(PREORDER.closesAt).toISOString(), delivery_by: PREORDER.deliveryBy });
    }
    if (req.method !== 'POST') { res.setHeader('Allow', 'GET, POST'); return reply(res, 405, { ok: false, error: 'Not allowed.' }); }
    const origin = req.headers.origin, host = req.headers['x-forwarded-host'] || req.headers.host;
    if (origin && host && origin.replace(/^https?:\/\//, '') !== host) return reply(res, 403, { ok: false, error: 'Please pre-order on the PocketOS website.' });
    const ip = String(req.headers['x-forwarded-for'] || '').split(',')[0].trim() || 'unknown';
    if (limited(ip, now)) return reply(res, 429, { ok: false, error: 'Please finish the checkout you already started, or try again later.' });
    const a = await availability(env, now);
    if (!a.open) return reply(res, 409, { ok: false, reason: a.reason, error: a.reason === 'sold_out' ? 'All 25 First Editions have been reserved.' : a.reason === 'closed' ? 'First Edition pre-orders have closed.' : 'Pre-orders aren’t open yet.' });
    const s = await stripe('POST', '/v1/checkout/sessions', sessionParams(a.delivery, env, now), env);
    return reply(res, 200, { ok: true, url: s.url });
  } catch (e) {
    console.error('preorder: ' + (e && e.message));   // Stripe's message only; never keys or customer details
    return reply(res, 502, { ok: false, error: 'Pre-orders aren’t available right now. Nothing was charged. Please try again in a few minutes.' });
  }
}

module.exports = (req, res) => handler(req, res);
module.exports._test = { handler, sessionParams, recent };
