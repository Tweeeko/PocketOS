// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Builds the Where to Buy page from assets/product-media.js, so images, box contents and
// availability are all edited in one place.
(() => {
  'use strict';
  const P = window.POCKETOS_PRODUCT; if (!P) return;
  const esc = s => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  const shot = (key, eager) => {
    const im = P.images[key]; if (!im) return '';
    return `<img src="${esc(im.src)}" alt="${esc(im.alt)}" ${eager ? 'fetchpriority="high"' : 'loading="lazy"'} decoding="async">`
      + (im.kind === 'render' && !document.querySelector('main.buy') ? '<span class="badge">Concept render</span>' : '');   // not shown on the Buy page for now
  };
  document.querySelectorAll('[data-image]').forEach(f => { f.innerHTML = shot(f.dataset.image, f.hasAttribute('data-eager')); });

  // Gallery: thumbnails swap the main picture
  const main = document.querySelector('[data-main]'), thumbs = document.querySelector('[data-thumbs]');
  if (main && thumbs) {
    const keys = ['hero', 'angle', 'size', 'contents'].filter(k => P.images[k]);
    thumbs.innerHTML = keys.map((k, i) => `<button type="button" class="thumb${i ? '' : ' on'}" data-k="${k}" aria-label="${esc(P.images[k].alt)}" aria-pressed="${!i}"><img src="${esc(P.images[k].src)}" alt="" loading="lazy" decoding="async"></button>`).join('');
    thumbs.addEventListener('click', e => {
      const b = e.target.closest('.thumb'); if (!b) return;
      main.innerHTML = shot(b.dataset.k, true);
      thumbs.querySelectorAll('.thumb').forEach(t => { const on = t === b; t.classList.toggle('on', on); t.setAttribute('aria-pressed', String(on)); });
    });
  }

  const closer = document.querySelector('[data-closer]');
  if (closer && P.closer) {
    const n = P.closer.length;
    closer.innerHTML = P.closer.map((c, i) =>
      `<figure class="slide" role="group" aria-roledescription="slide" aria-label="${i + 1} of ${n}: ${esc(c.title)}"><div class="shot${c.image ? '' : ' empty'}">${c.image ? shot(c.image) : '<span class="ph"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 8h3l2-3h6l2 3h3v11H4z"/><circle cx="12" cy="13" r="3.5"/></svg>Photo coming with the finished PocketOS</span>'}</div><figcaption><span class="count">${i + 1} / ${n}</span><h3>${esc(c.title)}</h3><p>${esc(c.text)}</p></figcaption></figure>`).join('');
    // Slider: swipe or scroll sideways, use the arrows or dots, or the keyboard's arrow keys
    const dots = document.querySelector('[data-dots]'), slides = [...closer.children];
    dots.innerHTML = slides.map((s, i) => `<button type="button" aria-label="Picture ${i + 1}"${i ? '' : ' aria-current="true"'}></button>`).join('');
    const go = i => { i = Math.max(0, Math.min(n - 1, i)); closer.scrollTo({ left: slides[i].offsetLeft - closer.offsetLeft, behavior: 'smooth' }); };
    const at = () => Math.round(closer.scrollLeft / (slides[0].offsetWidth || 1));
    const mark = () => { const i = at(); [...dots.children].forEach((d, k) => d.toggleAttribute('aria-current', k === i));
      document.querySelectorAll('[data-sl]').forEach(b => { b.disabled = (b.dataset.sl === '-1' && i === 0) || (b.dataset.sl === '1' && i === n - 1); }); };
    closer.addEventListener('scroll', () => { clearTimeout(mark.t); mark.t = setTimeout(mark, 60); }, { passive: true });
    dots.addEventListener('click', e => { const b = e.target.closest('button'); if (b) go([...dots.children].indexOf(b)); });
    document.querySelectorAll('[data-sl]').forEach(b => b.addEventListener('click', () => go(at() + +b.dataset.sl)));
    closer.addEventListener('keydown', e => { if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') { e.preventDefault(); go(at() + (e.key === 'ArrowRight' ? 1 : -1)); } });
    mark();
  }

  // Price and availability come from the same place as everywhere else
  const a0 = P.availability, pr = document.querySelector('[data-price]'), stt = document.querySelector('[data-status]'), rn = document.querySelector('[data-region-note]');
  if (pr) pr.textContent = a0.price;
  if (stt) stt.textContent = a0.status;
  if (rn && a0.region) rn.textContent = 'Sold and delivered in the ' + (a0.region === 'UK only' ? 'UK only.' : a0.region + '.');

  // First Edition pre-orders: live status from /api/preorder; the page stays "Coming soon" if it can't tell
  const preBtn = document.querySelector('[data-preorder]');
  if (preBtn) {
    const soon = document.querySelector('[data-soon]'), box = document.querySelector('[data-preorder-box]'), msg = document.querySelector('[data-preorder-msg]');
    fetch('api/preorder', { cache: 'no-store' }).then(r => r.json()).then(s => {
      if (!s || !s.ok) return;
      const pill = document.querySelector('[data-status]'), ship = document.querySelector('[data-region-note]');
      if (s.reason === 'open') {
        preBtn.hidden = false; soon.hidden = true; box.hidden = false;
        if (pill) pill.textContent = s.remaining <= 10 ? `First Edition · ${s.remaining} left` : 'First Edition pre-order';
        if (ship && s.delivery_text) ship.textContent = s.delivery_text === 'Free' ? 'Free UK delivery.' : `Plus ${s.delivery_text} UK delivery. UK only.`;
      } else if (s.reason === 'sold_out') { if (pill) pill.textContent = 'First Edition sold out'; soon.textContent = 'First Edition sold out'; }
      else if (s.reason === 'closed') { if (pill) pill.textContent = 'Pre-orders closed'; soon.textContent = 'Pre-orders closed'; }
    }).catch(() => {});
    preBtn.addEventListener('click', async () => {
      preBtn.disabled = true; preBtn.textContent = 'Opening checkout…'; msg.textContent = '';
      try {
        const r = await fetch('api/preorder', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
        const j = await r.json().catch(() => ({}));
        if (r.ok && j.ok && /^https:\/\/checkout\.stripe\.com\//.test(j.url)) { location.href = j.url; return; }
        msg.textContent = j.error || 'Checkout couldn’t open. Nothing was charged. Please try again.';
      } catch (_) { msg.textContent = 'Couldn’t reach the server. Check your connection and try again. Nothing was charged.'; }
      preBtn.disabled = false; preBtn.textContent = 'Pre-order First Edition';
    });
  }

  const pack = document.querySelector('[data-packaging]');
  if (pack) pack.innerHTML = P.packaging.map((p, i) =>
    `<figure class="pack-item${i === 0 ? ' lead' : ''}"><div class="shot">${shot(p.image)}</div><figcaption><h3>${esc(p.title)}</h3><p>${esc(p.text)}</p></figcaption></figure>`).join('');

  const box = document.querySelector('[data-in-the-box]');
  if (box) box.innerHTML = P.inTheBox.items.map(it =>
    `<li class="${it.confirmed ? 'yes' : 'tbc'}"><b>${esc(it.name)}</b>${it.confirmed ? '' : '<span class="tag">To be confirmed</span>'}<span>${esc(it.detail)}</span></li>`).join('');

  const note = document.querySelector('[data-tbc-note]');
  if (note) note.hidden = P.inTheBox.items.every(it => it.confirmed);

  const a = P.availability;
  document.querySelectorAll('[data-availability]').forEach(el => {
    el.innerHTML = `<div class="avail-rows"><div><dt>Price</dt><dd>${esc(a.price)}</dd></div><div><dt>Availability</dt><dd>${esc(a.status)}</dd></div>${a.region ? `<div><dt>Delivers to</dt><dd>${esc(a.region)}</dd></div>` : ''}</div>`
      + (el.hasAttribute('data-note') ? `<p class="avail-note">${esc(a.note)}</p>` : '')
      + (a.links && a.links.length ? `<div class="row">${a.links.map((l, i) => `<a class="btn${i ? ' ghost' : ''}" href="${esc(l.href)}">${esc(l.label)}</a>`).join('')}</div>` : '');
  });
})();
