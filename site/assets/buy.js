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
    el.innerHTML = `<div class="avail-rows"><div><dt>Price</dt><dd>${esc(a.price)}</dd></div><div><dt>Availability</dt><dd>${esc(a.status)}</dd></div></div>`
      + (el.hasAttribute('data-note') ? `<p class="avail-note">${esc(a.note)}</p>` : '')
      + (a.links && a.links.length ? `<div class="row">${a.links.map((l, i) => `<a class="btn${i ? ' ghost' : ''}" href="${esc(l.href)}">${esc(l.label)}</a>`).join('')}</div>` : '');
  });
})();
