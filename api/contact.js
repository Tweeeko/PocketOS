// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// The website's contact form. Sends the message by email through the mailbox set in Vercel's
// environment variables, so no address appears in the website or in this code:
//   CONTACT_SMTP_USER  the mailbox to send from (an iCloud Mail address)
//   CONTACT_SMTP_PASS  an app-specific password for it (appleid.apple.com → Sign-In and Security)
//   CONTACT_TO         where messages are delivered (defaults to CONTACT_SMTP_USER)
//   CONTACT_SMTP_HOST / CONTACT_SMTP_PORT   default smtp.mail.me.com:587 (iCloud Mail)
// Nothing is stored. The form only says "sent" once the mail server has accepted the message.
'use strict';
const net = require('net');
const tls = require('tls');

const TOPICS = ['Buying PocketOS', 'An order or delivery', 'Setup or using PocketOS', 'Returns or a fault', 'Something else'];
const LIMITS = { name: 80, email: 254, topic: 40, order: 40, message: 4000 };
const EMAIL = /^[^\s@<>()",;:]+@[^\s@<>()",;:]+\.[^\s@<>()",;:]{2,}$/;
const recent = new Map();               // per-address send times; best effort (each function instance has its own)
const PER_HOUR = 5;

function clean(v, max) {
  if (typeof v !== 'string') return '';
  return v.replace(/\r\n?/g, '\n').replace(/[\u0000-\u0008\u000b-\u001f\u007f]/g, '').trim().slice(0, max);
}
const oneLine = s => s.replace(/\s+/g, ' ');

// Checks the form. Returns [fields, null] or [null, message for the visitor].
function validate(body) {
  if (!body || typeof body !== 'object') return [null, 'Something went wrong. Please try again.'];
  if (body.website) return [null, null];                       // the hidden field only robots fill in
  const f = {
    name: oneLine(clean(body.name, LIMITS.name)),
    email: oneLine(clean(body.email, LIMITS.email)),
    topic: oneLine(clean(body.topic, LIMITS.topic)),
    order: oneLine(clean(body.order, LIMITS.order)),
    message: clean(body.message, LIMITS.message),
  };
  if (!f.name) return [null, 'Please add your name.'];
  if (!EMAIL.test(f.email)) return [null, 'Please check your email address.'];
  if (!TOPICS.includes(f.topic)) return [null, 'Please choose what it’s about.'];
  if (f.message.length < 10) return [null, 'Please tell us a little more.'];
  if (typeof body.message === 'string' && body.message.length > LIMITS.message) return [null, 'Please keep your message under 4,000 characters.'];
  return [f, null];
}

function limited(ip, now) {
  const times = (recent.get(ip) || []).filter(t => now - t < 3600e3);
  if (times.length >= PER_HOUR) { recent.set(ip, times); return true; }
  times.push(now); recent.set(ip, times);
  if (recent.size > 5000) recent.clear();
  return false;
}

// Encodes a header value that may hold any characters (RFC 2047)
const word = s => /^[\x20-\x7e]*$/.test(s) ? s : '=?UTF-8?B?' + Buffer.from(s).toString('base64') + '?=';

function compose(f, from, to, now) {
  const body = [
    `${f.name} wrote through the PocketOS website.`,
    '',
    `About: ${f.topic}`,
    f.order ? `Order number: ${f.order}` : null,
    `Reply to: ${f.email}`,
    '',
    f.message,
  ].filter(l => l !== null).join('\r\n');
  const b64 = Buffer.from(body).toString('base64').replace(/.{76}/g, '$&\r\n');
  return [
    `From: ${word('PocketOS website')} <${from}>`,
    `To: <${to}>`,
    `Reply-To: ${word(f.name)} <${f.email}>`,
    `Subject: ${word('PocketOS: ' + f.topic + ' (' + f.name + ')')}`,
    `Date: ${new Date(now).toUTCString().replace('GMT', '+0000')}`,
    `Message-ID: <${now}.${Math.random().toString(36).slice(2)}@pocketos.website>`,
    'MIME-Version: 1.0',
    'Content-Type: text/plain; charset=UTF-8',
    'Content-Transfer-Encoding: base64',
    '',
    b64,
  ].join('\r\n');
}

// A small SMTP client: connect, STARTTLS (or straight TLS on 465), log in, send one message.
function smtpSend({ host, port, user, pass, from, to, data, rejectUnauthorized = true, timeoutMs = 15000 }) {
  return new Promise((resolve, reject) => {
    let sock = port === 465 ? tls.connect({ host, port, servername: host, rejectUnauthorized }) : net.connect({ host, port });
    let buf = '', waiting = null, secure = port === 465;
    const timer = setTimeout(() => fail(new Error('timeout')), timeoutMs);
    function fail(e) { clearTimeout(timer); try { sock.destroy(); } catch (_) {} reject(e); }
    function attach(s) {
      s.setEncoding('utf8');
      s.on('data', d => {
        buf += d;
        // a reply is complete when a line has "NNN " (not "NNN-")
        const m = buf.match(/(?:^|\r\n)(\d{3}) [^\r\n]*\r\n$/);
        if (m && waiting) { const w = waiting, text = buf; waiting = null; buf = ''; w(+m[1], text); }
      });
      s.on('error', fail);
    }
    const expect = (codes) => new Promise((res, rej) => { waiting = (c, t) => codes.includes(c) ? res(t) : rej(new Error('SMTP ' + c)); });
    const send = (line, codes) => { const p = expect(codes); sock.write(line + '\r\n'); return p; };
    attach(sock);
    (async () => {
      await expect([220]);
      let ehlo = await send('EHLO pocketos.website', [250]);
      if (!secure) {
        if (!/STARTTLS/i.test(ehlo)) throw new Error('no STARTTLS');
        await send('STARTTLS', [220]);
        sock.removeAllListeners('data');
        sock = tls.connect({ socket: sock, servername: host, rejectUnauthorized });
        await new Promise((res, rej) => { sock.once('secureConnect', res); sock.once('error', rej); });
        secure = true; buf = '';
        attach(sock);
        ehlo = await send('EHLO pocketos.website', [250]);
      }
      await send('AUTH PLAIN ' + Buffer.from('\0' + user + '\0' + pass).toString('base64'), [235]);
      await send(`MAIL FROM:<${from}>`, [250]);
      await send(`RCPT TO:<${to}>`, [250, 251]);
      await send('DATA', [354]);
      await send(data.replace(/^\./gm, '..') + '\r\n.', [250]);
      sock.write('QUIT\r\n');
      clearTimeout(timer);
      sock.end();
      resolve();
    })().catch(fail);
  });
}

async function readBody(req) {
  if (req.body && typeof req.body === 'object') return req.body;
  if (typeof req.body === 'string') { try { return JSON.parse(req.body); } catch (_) { return null; } }
  let raw = '';
  for await (const c of req) { raw += c; if (raw.length > 20000) return null; }
  try { return JSON.parse(raw); } catch (_) { return null; }
}

function reply(res, status, obj) {
  res.statusCode = status;
  res.setHeader('Content-Type', 'application/json');
  res.setHeader('Cache-Control', 'no-store');
  res.end(JSON.stringify(obj));
}

async function handler(req, res, env = process.env, send = smtpSend) {
  if (req.method !== 'POST') { res.setHeader('Allow', 'POST'); return reply(res, 405, { ok: false, error: 'Use the form to send a message.' }); }
  // Only this website's own pages may post here
  const origin = req.headers.origin, host = req.headers['x-forwarded-host'] || req.headers.host;
  if (origin && host && origin.replace(/^https?:\/\//, '') !== host) return reply(res, 403, { ok: false, error: 'Please use the form on the PocketOS website.' });
  const [f, err] = validate(await readBody(req));
  if (!f && err === null) return reply(res, 200, { ok: true });     // robot: pretend, send nothing
  if (!f) return reply(res, 400, { ok: false, error: err });
  const ip = String(req.headers['x-forwarded-for'] || '').split(',')[0].trim() || 'unknown';
  if (limited(ip, Date.now())) return reply(res, 429, { ok: false, error: 'You’ve sent several messages already. Please try again later.' });
  const user = env.CONTACT_SMTP_USER, pass = env.CONTACT_SMTP_PASS, to = env.CONTACT_TO || user;
  if (!user || !pass) return reply(res, 503, { ok: false, error: 'Messages can’t be sent right now. Please try again later.' });
  try {
    await send({
      host: env.CONTACT_SMTP_HOST || 'smtp.mail.me.com',
      port: +(env.CONTACT_SMTP_PORT || 587),
      user, pass, from: user, to,
      data: compose(f, user, to, Date.now()),
      rejectUnauthorized: env.CONTACT_SMTP_INSECURE !== '1',   // only for local tests
    });
    return reply(res, 200, { ok: true });
  } catch (e) {
    console.error('contact: not sent (' + (e && e.message) + ')');   // never the message or addresses
    return reply(res, 502, { ok: false, error: 'Your message couldn’t be sent. Nothing was sent. Please try again in a few minutes.' });
  }
}

module.exports = (req, res) => handler(req, res);
module.exports._test = { handler, validate, compose, smtpSend, TOPICS, recent };
