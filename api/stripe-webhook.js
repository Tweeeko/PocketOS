// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Stripe sends payment events here. Every event's signature is checked before anything happens.
// A paid pre-order is emailed to Leeeko LTD (to the CONTACT_TO address set up for the contact form)
// with everything needed to dispatch it. Stripe emails the customer their receipt.
//
// In Stripe: Developers → Webhooks → Add endpoint → https://www.pocketos.co.uk/api/stripe-webhook
// Events: checkout.session.completed, checkout.session.async_payment_succeeded,
//         checkout.session.async_payment_failed, charge.refunded, charge.dispute.created
'use strict';
const { PREORDER, verifySignature, money, orderNumber } = require('./_stripe');
const { smtpSend } = require('./contact')._test;

async function rawBody(req) {
  if (typeof req.rawBody === 'string') return req.rawBody;
  const chunks = []; let size = 0;
  for await (const c of req) { size += c.length; if (size > 1e6) throw new Error('too large'); chunks.push(Buffer.from(c)); }
  return Buffer.concat(chunks).toString('utf8');
}

const addr = a => !a ? '' : [a.line1, a.line2, a.city, a.state, a.postal_code, a.country].filter(Boolean).join('\n');

// What the email says for each event we act on. Returns null for events we don't need.
function describe(ev) {
  const o = ev.data && ev.data.object || {};
  if (ev.type === 'checkout.session.completed' || ev.type === 'checkout.session.async_payment_succeeded') {
    if (!o.metadata || o.metadata.edition !== PREORDER.edition) return null;
    if (o.payment_status === 'unpaid') return null;   // a bank payment still on its way: wait for async_payment_succeeded
    const c = o.customer_details || {}, ship = (o.collected_information && o.collected_information.shipping_details) || o.shipping_details || {};
    const n = orderNumber(o.id);
    return { subject: `New First Edition pre-order ${n}`, body: [
      `A PocketOS First Edition pre-order has been paid.`, '',
      `Order: ${n}`, `Paid: ${money(o.amount_total || 0)} (${(o.currency || 'gbp').toUpperCase()})`,
      o.total_details && o.total_details.amount_tax ? `Of which VAT: ${money(o.total_details.amount_tax)}` : null,
      `Deliver by: ${PREORDER.deliveryBy}`, '',
      `Name: ${ship.name || c.name || ''}`, `Email: ${c.email || ''}`, `Phone: ${c.phone || ''}`, '',
      `Delivery address:`, addr(ship.address) || addr(c.address), '',
      `Stripe: ${o.payment_intent || o.id}`,
    ].filter(l => l !== null).join('\n') };
  }
  if (ev.type === 'checkout.session.async_payment_failed' && o.metadata && o.metadata.edition === PREORDER.edition)
    return { subject: `Pre-order payment failed ${orderNumber(o.id)}`, body: `A bank payment for pre-order ${orderNumber(o.id)} failed, so it isn't an order. The customer can try again.\n\nStripe: ${o.id}` };
  if (ev.type === 'charge.refunded')
    return { subject: `Refund issued: ${money(o.amount_refunded || 0)}`, body: `A refund of ${money(o.amount_refunded || 0)} went through.\n\nStripe: ${o.payment_intent || o.id}` };
  if (ev.type === 'charge.dispute.created')
    return { subject: `Payment disputed: ${money(o.amount || 0)}`, body: `A customer disputed a payment. Respond in the Stripe Dashboard before the deadline.\n\nStripe: ${o.id}` };
  return null;
}

async function handler(req, res, env = process.env, send = smtpSend, now = Date.now()) {
  const done = (s, msg) => { res.statusCode = s; res.setHeader('Content-Type', 'text/plain'); res.end(msg); };
  if (req.method !== 'POST') return done(405, 'POST only');
  let payload;
  try { payload = await rawBody(req); } catch (_) { return done(400, 'bad body'); }
  if (!verifySignature(payload, req.headers['stripe-signature'], env.STRIPE_WEBHOOK_SECRET, 300, now)) return done(400, 'bad signature');
  let ev; try { ev = JSON.parse(payload); } catch (_) { return done(400, 'bad json'); }
  const msg = describe(ev);
  if (!msg) return done(200, 'ignored');
  const user = env.CONTACT_SMTP_USER, pass = env.CONTACT_SMTP_PASS, to = env.CONTACT_TO || user;
  if (!user || !pass) { console.error('webhook: email not set up'); return done(500, 'email not set up'); }   // Stripe retries
  const date = new Date(now).toUTCString().replace('GMT', '+0000');
  const data = [
    'From: =?UTF-8?B?' + Buffer.from('PocketOS orders').toString('base64') + '?= <' + user + '>', 'To: <' + to + '>',
    'Subject: =?UTF-8?B?' + Buffer.from(msg.subject).toString('base64') + '?=', 'Date: ' + date,
    'Message-ID: <' + ev.id + '@pocketos.website>', 'MIME-Version: 1.0', 'Content-Type: text/plain; charset=UTF-8', 'Content-Transfer-Encoding: base64', '',
    Buffer.from(msg.body).toString('base64').replace(/.{76}/g, '$&\r\n'),
  ].join('\r\n');
  try {
    await send({ host: env.CONTACT_SMTP_HOST || 'smtp.mail.me.com', port: +(env.CONTACT_SMTP_PORT || 587), user, pass, from: user, to, data, rejectUnauthorized: env.CONTACT_SMTP_INSECURE !== '1' });
  } catch (e) {
    console.error('webhook: email not sent (' + (e && e.message) + ')');
    return done(500, 'retry');   // Stripe retries for up to three days, so no order is lost
  }
  return done(200, 'ok');
}

module.exports = (req, res) => handler(req, res);
module.exports._test = { handler, describe };
