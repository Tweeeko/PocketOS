// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// The website's live PocketOS: built from markup, eyes that follow you, moods, a dial that turns,
// and the glass sheen that follows the pointer. No tracking, no external requests.
(() => {
  'use strict';
  const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const $ = (s, el = document) => el.querySelector(s);
  const $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const ARC = 207.35, FULL = 276.46;   // 270° of a radius-44 circle, and the whole circle

  const icon = {
    bulb: '<svg viewBox="0 0 24 24"><path d="M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.6.5 1 1.2 1 2.1h5c0-.9.4-1.6 1-2.1A6 6 0 0 0 12 3z"/></svg>',
    flame: '<svg viewBox="0 0 24 24"><path d="M12 22a7 7 0 0 0 7-7c0-4-3-6-4-10-2 2-3 4-3 6-1-1-2-2-2-4-3 3-5 5-5 8a7 7 0 0 0 7 7z"/></svg>',
    note: '<svg viewBox="0 0 24 24"><path d="M9 18V5l12-2v13M9 18a3 3 0 1 1-6 0 3 3 0 0 1 6 0zm12-2a3 3 0 1 1-6 0 3 3 0 0 1 6 0z"/></svg>',
    lock: '<svg viewBox="0 0 24 24"><rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/></svg>',
  };
  const play = '<svg viewBox="0 0 24 24"><path d="M7 4l13 8-13 8z"/></svg>';
  const prev = '<svg viewBox="0 0 24 24"><path d="M18 5L8 12l10 7zM6 5h2v14H6z"/></svg>';
  const next = '<svg viewBox="0 0 24 24"><path d="M6 5l10 7-10 7zM16 5h2v14h-2z"/></svg>';
  const dial = (id, a, b) => `<svg class="dial" viewBox="0 0 100 100" aria-hidden="true"><defs><linearGradient id="${id}" x1="0" y1="1" x2="1" y2="0"><stop offset="0" stop-color="${a}"/><stop offset="1" stop-color="${b}"/></linearGradient></defs>
    <circle class="track" cx="50" cy="50" r="44" stroke-dasharray="${ARC} ${FULL}" transform="rotate(135 50 50)"/>
    <circle class="val" cx="50" cy="50" r="44" stroke="url(#${id})" stroke-dasharray="${ARC} ${FULL}" stroke-dashoffset="${ARC}" transform="rotate(135 50 50)"/>
    <circle class="knob" r="3.4" cx="50" cy="50"/></svg>`;

  let n = 0;
  function build(el) {
    const k = n++;
    const ring = el.hasAttribute('data-ring') ? `<svg class="ring" viewBox="0 0 200 200" aria-hidden="true"><defs><path id="rim${k}" d="M100 100m-95 0a95 95 0 1 1 190 0a95 95 0 1 1-190 0"/></defs><text><textPath href="#rim${k}" textLength="592" lengthAdjust="spacing">PocketOS ◦ a tiny companion for your whole home ◦ created by Leeko ◦</textPath></text></svg>` : '';
    el.innerHTML = ring + `<span class="key k1"></span><span class="key k2"></span><div class="screen">
      <div class="scr s-companion"><div class="status"><span class="bat"></span>78%</div><div class="clock">08:24</div>
        <div class="orb"><div class="eyes"><i></i><i></i></div></div><div class="mood-cap">Good morning</div></div>
      <div class="scr s-menu"><div class="ttl">Home</div><div class="tiles">
        <div class="tile on">${icon.bulb}<b>Lights</b><small>3 on</small></div>
        <div class="tile on">${icon.flame}<b>Heating</b><small>21.5°</small></div>
        <div class="tile">${icon.note}<b>Music</b><small>Kitchen</small></div>
        <div class="tile">${icon.lock}<b>Locks</b><small>All locked</small></div></div><div class="dots"><i></i><i></i></div></div>
      <div class="scr s-heat">${dial('gh' + k, '#ff7d93', '#ffb86b')}<div class="big" data-temp>19.0<small>°</small></div><div class="sub">Heating · room 19.8°</div>
        <div class="ctls"><i>−</i><i class="main" style="color:#ffb86b">${icon.flame.replace('<svg', '<svg style="fill:none;stroke:currentColor;stroke-width:2"')}</i><i>+</i></div><div class="back">‹ Living room</div></div>
      <div class="scr s-music">${dial('gm' + k, '#8c7cff', '#7fe3ff')}<div class="big" data-vol>20</div><div class="sub">Volume · Kitchen speaker</div>
        <div class="ctls"><i>${prev}</i><i class="main">${play}</i><i>${next}</i></div><div class="back">‹ Music</div></div>
    </div>`;
    el.setAttribute('role', 'img');
    if (!el.dataset.screen) el.dataset.screen = 'companion';
    if (!el.dataset.mood) el.dataset.mood = 'idle';
    el.setAttribute('aria-label', 'PocketOS, a round touchscreen with an animated companion');
    tick(el);
    dialTo(el, 'heat', 0);
    dialTo(el, 'music', 0);
  }

  // The dial screens animate when they come into view
  // Arc and knob move together, easing out like a finger letting go
  function dialTo(el, which, to, ms = 0) {
    const s = $('.s-' + which, el); if (!s) return;
    const val = $('.val', s), kb = $('.knob', s), from = s._f || 0;
    const set = f => {
      val.style.strokeDashoffset = ARC * (1 - f);
      const a = (135 + 270 * f) * Math.PI / 180;
      kb.setAttribute('cx', (50 + 44 * Math.cos(a)).toFixed(2)); kb.setAttribute('cy', (50 + 44 * Math.sin(a)).toFixed(2));
      s._f = f;
    };
    if (!ms || reduce) return set(to);
    const t0 = performance.now();
    const step = t => { const p = Math.min(1, (t - t0) / ms), e = 1 - Math.pow(1 - p, 3); set(from + (to - from) * e); if (p < 1) requestAnimationFrame(step); };
    requestAnimationFrame(step);
  }
  function count(node, from, to, dec, ms, suffix) {
    if (reduce) { node.innerHTML = to.toFixed(dec) + suffix; return; }
    const t0 = performance.now();
    const step = t => { const p = Math.min(1, (t - t0) / ms), e = 1 - Math.pow(1 - p, 3);
      node.innerHTML = (from + (to - from) * e).toFixed(dec) + suffix; if (p < 1) requestAnimationFrame(step); };
    requestAnimationFrame(step);
  }
  function show(el, screen) {
    if (el.dataset.screen === screen) return;
    el.dataset.screen = screen;
    if (screen === 'heat') { dialTo(el, 'heat', 0); dialTo(el, 'heat', .575, 1400); count($('[data-temp]', el), 19, 21.5, 1, 1400, '<small>°</small>'); }
    if (screen === 'music') { dialTo(el, 'music', 0); dialTo(el, 'music', .35, 1400); count($('[data-vol]', el), 20, 35, 0, 1400, ''); }
  }

  // Live clock on every device
  function tick(el) { const d = new Date(), c = $('.clock', el); if (c) c.textContent = String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0'); }
  setInterval(() => $$('.pocket').forEach(tick), 15000);

  const MOODS = {
    idle: 'Good morning', warm: 'Heating’s on', dance: 'Music’s playing', alert: 'Front door unlocked',
    chilly: 'Cold outside', tired: 'Battery low', worried: 'Can’t reach home', happy: 'Hello!'
  };
  function mood(el, m) { el.dataset.mood = m; const c = $('.mood-cap', el); if (c) c.textContent = MOODS[m] || ''; }

  $$('.pocket[data-build]').forEach(build);

  // Eyes follow the pointer (or the last touch)
  let raf = 0, px = innerWidth / 2, py = innerHeight / 3;
  function look() {
    raf = 0;
    $$('.pocket').forEach(el => {
      const orb = $('.orb', el); if (!orb) return;
      const r = orb.getBoundingClientRect(); if (!r.width) return;
      const dx = px - (r.left + r.width / 2), dy = py - (r.top + r.height / 2), d = Math.hypot(dx, dy) || 1;
      const m = Math.min(1, d / 400) * r.width * .09;
      el.style.setProperty('--ex', (dx / d * m).toFixed(1) + 'px');
      el.style.setProperty('--ey', (dy / d * m * .8).toFixed(1) + 'px');
    });
  }
  if (!reduce) addEventListener('pointermove', e => { px = e.clientX; py = e.clientY; if (!raf) raf = requestAnimationFrame(look); }, { passive: true });

  // Tap the companion: it's pleased to see you
  $$('.pocket').forEach(el => el.addEventListener('click', () => {
    if (el.dataset.screen !== 'companion' || el.dataset.mood === 'happy') return;
    const was = el.dataset.mood; mood(el, 'happy');
    setTimeout(() => mood(el, was), 1400);
  }));

  // Mood buttons drive the pinned device
  $$('.moods button').forEach(b => b.addEventListener('click', () => {
    [$('#story'), $('.chapter .mini')].forEach(el => { if (el) { show(el, 'companion'); mood(el, b.dataset.mood); } });
    $$('.moods button').forEach(x => x.setAttribute('aria-pressed', String(x === b)));
  }));

  // Chapters switch the pinned device's screen as they reach the middle of the view
  const story = $('#story');
  if (story && 'IntersectionObserver' in window) {
    // On phones each chapter has its own device (.mini); on wider screens one stays pinned
    const io = new IntersectionObserver(es => es.forEach(e => {
      if (!e.isIntersecting) return;
      const scr = e.target.dataset.screen, mini = $('.mini', e.target);
      show(story, scr);
      if (mini) show(mini, scr);
    }), { rootMargin: '-35% 0px -35% 0px' });
    $$('.chapter').forEach(c => io.observe(c));
  }

  // Liquid glass: the sheen follows the pointer over each panel
  if (!reduce) addEventListener('pointermove', e => {
    const g = e.target.closest && e.target.closest('.glass'); if (!g) return;
    const r = g.getBoundingClientRect();
    g.style.setProperty('--mx', ((e.clientX - r.left) / r.width * 100).toFixed(1) + '%');
    g.style.setProperty('--my', ((e.clientY - r.top) / r.height * 100).toFixed(1) + '%');
  }, { passive: true });

  // The film plays only when asked, with sound
  const v = $('#film'), p = $('#film-play');
  if (v && p) p.addEventListener('click', () => { v.controls = true; v.muted = false; v.play(); p.hidden = true; });
})();
