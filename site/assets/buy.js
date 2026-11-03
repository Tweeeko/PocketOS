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
      + (im.kind === 'render' ? '<span class="badge">Concept render</span>' : '');
  };
  document.querySelectorAll('[data-image]').forEach(f => { f.innerHTML = shot(f.dataset.image, f.hasAttribute('data-eager')); });

  // Gallery: thumbnails swap the main picture
  const main = document.querySelector('[data-main]'), thumbs = document.querySelector('[data-thumbs]');
  if (main && thumbs) {
    const keys = ['hero', 'angle', 'contents', 'boxClosed'].filter(k => P.images[k]);
    thumbs.innerHTML = keys.map((k, i) => `<button type="button" class="thumb${i ? '' : ' on'}" data-k="${k}" aria-label="${esc(P.images[k].alt)}" aria-pressed="${!i}"><img src="${esc(P.images[k].src)}" alt="" loading="lazy" decoding="async"></button>`).join('');
    thumbs.addEventListener('click', e => {
      const b = e.target.closest('.thumb'); if (!b) return;
      main.innerHTML = shot(b.dataset.k, true);
      thumbs.querySelectorAll('.thumb').forEach(t => { const on = t === b; t.classList.toggle('on', on); t.setAttribute('aria-pressed', String(on)); });
    });
  }

  const closer = document.querySelector('[data-closer]');
  if (closer && P.closer) closer.innerHTML = P.closer.map(c =>
    `<figure class="closer-item"><div class="shot${c.image ? '' : ' empty'}">${c.image ? shot(c.image) : '<span class="ph"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 8h3l2-3h6l2 3h3v11H4z"/><circle cx="12" cy="13" r="3.5"/></svg>Photo coming with the finished PocketOS</span>'}</div><figcaption><h3>${esc(c.title)}</h3><p>${esc(c.text)}</p></figcaption></figure>`).join('');

  // Price and availability come from the same place as everywhere else
  const a0 = P.availability, pr = document.querySelector('[data-price]'), stt = document.querySelector('[data-status]'), rn = document.querySelector('[data-region-note]');
  if (pr) pr.textContent = a0.price;
  if (stt) stt.textContent = a0.status;
  if (rn && a0.region) rn.textContent = 'Sold and delivered in the ' + (a0.region === 'UK only' ? 'UK only.' : a0.region + '.');

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
