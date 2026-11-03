// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Contact form: checks it, posts to /api/contact, and only says "sent" when the server confirms.
(() => {
  const form = document.getElementById('contact');
  if (!form) return;
  const msg = form.querySelector('.form-msg');
  const btn = form.querySelector('button[type=submit]');
  const sent = document.querySelector('.sent');
  const say = (t, bad) => { msg.textContent = t; msg.classList.toggle('bad', !!bad); };

  function check() {
    const v = n => form.elements[n].value.trim();
    const bad = (n, t) => { form.elements[n].focus(); form.elements[n].setAttribute('aria-invalid', 'true'); say(t, true); return false; };
    form.querySelectorAll('[aria-invalid]').forEach(e => e.removeAttribute('aria-invalid'));
    if (!v('name')) return bad('name', 'Please add your name.');
    if (!/^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/.test(v('email'))) return bad('email', 'Please check your email address.');
    if (!v('topic')) return bad('topic', 'Please choose what it’s about.');
    if (v('message').length < 10) return bad('message', 'Please tell us a little more.');
    return true;
  }

  form.addEventListener('submit', async e => {
    e.preventDefault();
    if (btn.disabled || !check()) return;
    const data = Object.fromEntries(['name', 'email', 'topic', 'order', 'message', 'website'].map(n => [n, form.elements[n].value]));
    btn.disabled = true; btn.textContent = 'Sending…'; say('');
    try {
      const r = await fetch('api/contact', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data) });
      const j = await r.json().catch(() => ({}));
      if (r.ok && j.ok) { form.hidden = true; sent.hidden = false; sent.querySelector('h3').focus?.(); form.reset(); }
      else say(j.error || 'Your message couldn’t be sent. Please try again in a few minutes.', true);
    } catch (_) {
      say('Couldn’t reach the server. Check your connection and try again. Nothing was sent.', true);
    } finally {
      btn.disabled = false; btn.textContent = 'Send message';
    }
  });

  form.addEventListener('input', e => {             // clear a complaint once the visitor fixes it
    if (e.target.getAttribute('aria-invalid')) { e.target.removeAttribute('aria-invalid'); say(''); }
  });

  document.getElementById('again').addEventListener('click', () => { sent.hidden = true; form.hidden = false; say(''); form.elements.name.focus(); });
})();
