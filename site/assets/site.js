// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// The website's PocketOS. The companion is a direct port of the device's home screen
// (firmware/main/ui/ui_home.c): same face shapes on the same 100-unit grid, same mood colours and
// captions, same timings for breathing, blinking and looking around. Nothing is sent anywhere.
(() => {
  'use strict';
  const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const $ = (s, el = document) => el.querySelector(s);
  const $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const rand = (a, b) => a + Math.random() * (b - a);

  /* ---------- the companion, as drawn by the firmware ---------- */
  // The device draws on a 250 px orb from a 100-unit face (orb centre 50,55, radius 40):
  // 1 unit = 3.125 px. Pixel sizes from the firmware are divided by K here.
  const K = 3.125, px = v => +(v / K).toFixed(3);
  const EYE = '#6AA7FF', OK = '#7BE3A4', BAD = '#FF7A8A';
  const MOOD_COLOR = { hot: '#FFA070', dance: '#FF7BD5', cold: '#8FD3FF', tired: '#FFC46B', alert: '#FFD166', charge: '#7BE3A4', worried: '#B6A8FF' };
  const CAPTION = {
    happy: 'All done!', dance: 'Vibing to the music', hot: 'Toasty in here', worried: 'Can’t reach home',
    tired: 'Battery low', charge: 'Charging', sad: 'That didn’t work', alert: 'Front door is unlocked',
    cold: 'Brr... 4° outside', sleep: 'Zzz', away: 'Out and about'
  };
  const greeting = () => { const h = new Date().getHours(); return h < 12 ? 'Good morning' : h < 18 ? 'Good afternoon' : 'Good evening'; };

  const ell = (cx, cy, w, h, c, cls = '') => `<ellipse class="${cls}" cx="${cx}" cy="${cy}" rx="${w / 2}" ry="${h / 2}" fill="${c}"/>`;
  // lv_arc: 0° is 3 o'clock and angles run clockwise
  const arc = (cx, cy, r, a0, a1, w, c) => {
    const p = a => [cx + r * Math.cos(a * Math.PI / 180), cy + r * Math.sin(a * Math.PI / 180)].map(n => n.toFixed(2));
    const [x0, y0] = p(a0), [x1, y1] = p(a1);
    return `<path d="M${x0} ${y0}A${r} ${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1} ${y1}" fill="none" stroke="${c}" stroke-width="${px(w)}" stroke-linecap="round"/>`;
  };
  const line = (pts, w, c) => `<polyline points="${pts.join(' ')}" fill="none" stroke="${c}" stroke-width="${px(w)}" stroke-linecap="round" stroke-linejoin="round"/>`;
  // lower half of a circle plus an eyelid, as half_eye() draws it
  const halfEye = (cx, cy, w, c) => `<path d="M${cx - w / 2} ${cy}A${w / 2} ${w / 2} 0 0 0 ${cx + w / 2} ${cy}Z" fill="${c}"/>` + line([cx - w / 2 - px(6), cy, cx + w / 2 + px(6), cy], 7, c);
  // labels are placed by their top-left corner on the device
  const glyph = (t, size, x, y, c, cls = '') => `<text class="${cls}" x="${x}" y="${y}" font-size="${px(size)}" fill="${c}" dominant-baseline="text-before-edge">${t}</text>`;
  const note = (x, y, size, c, cls) => { const s = px(size) / 24; return `<g class="${cls}"><g transform="translate(${x} ${y}) scale(${s.toFixed(3)})"><path fill="${c}" d="M21 2v13.5a3.5 3.5 0 1 1-2-3.2V7.1L10 9v8.5A3.5 3.5 0 1 1 8 14.3V4.6z"/></g></g>`; };
  const bolt = (x, y, size, c) => { const s = px(size) / 24; return `<g transform="translate(${x} ${y}) scale(${s.toFixed(3)})"><path fill="${c}" d="M13 2 4 14h6l-1 8 9-12h-6z"/></g>`; };

  function faceSVG(m, eyeCol) {
    const c = MOOD_COLOR[m] || eyeCol;
    switch (m) {
      case 'sleep':
        return arc(34, 46, 9, 30, 150, 11, c) + arc(66, 46, 9, 30, 150, 11, c) + glyph('z', 26, 72, 22, c) + glyph('z', 18, 80, 14, c);
      case 'happy': case 'dance': case 'charge': {
        let s = arc(34, 58, 9, 200, 340, 13, c) + arc(66, 58, 9, 200, 340, 13, c);
        if (m === 'dance') s += ell(50, 68, 11, 8, c, 'mouth-dance') + note(6, 26, 28, c, 'note1') + note(84, 32, 20, c, 'note2');
        else { s += arc(50, 60, 10, 30, 150, 9, c); if (m === 'charge') s += bolt(76, 22, 28, c); }
        return s;
      }
      case 'hot': case 'tired': {
        let s = halfEye(34, 52, 16, c) + halfEye(66, 52, 16, c);
        if (m === 'hot') s += ell(50, 69, 9, 6, c, 'mouth-hot') + ell(21, 63, 10, 10, 'rgba(255,90,74,.31)') + ell(79, 63, 10, 10, 'rgba(255,90,74,.31)') + ell(80, 32, 6, 9, '#8FD3FF', 'drop');
        else s += line([43, 71, 57, 71], 9, c);
        return s;
      }
      case 'sad': case 'worried':
        return line([25, 43, 41, 39], 10, c) + line([75, 43, 59, 39], 10, c) + ell(34, 56, 12, 16, c, 'eye') + ell(66, 56, 12, 16, c, 'eye')
          + arc(50, 78, 10, 220, 320, 9, c) + (m === 'worried' ? glyph('?', 26, 78, 22, c) : '');
      case 'alert':
        return line([26, 38, 40, 42], 10, c) + line([74, 38, 60, 42], 10, c) + ell(34, 54, 15, 21, c, 'eye') + ell(66, 54, 15, 21, c, 'eye')
          + ell(50, 72, 7, 6, c) + glyph('!', 40, 78, 16, c);
      case 'cold':
        return line([26, 47, 39, 53, 26, 59], 12, c) + line([74, 47, 61, 53, 74, 59], 12, c) + line([40, 71, 50, 68, 60, 71], 8, c);
      default: {   // idle, wake, listen, away
        const h = m === 'listen' ? 20 : 17;
        return ell(34, 52, 13, h, c, 'eye') + ell(66, 52, 13, h, c, 'eye') + (m === 'away' ? arc(50, 66, 7, 40, 140, 8, c) : '');
      }
    }
  }

  // Blink (2.5–6 s, eyes squash to 6 px for 120 ms) and look around (3.5–7 s, the face slides up
  // to 14 px either way over 450 ms, holds 900 ms, slides back), exactly as blink_cb / look_cb.
  function startLife(el) {
    const blink = () => {
      el._blink = setTimeout(blink, rand(2500, 6000));
      const eyes = $$('.face .eye', el); if (!eyes.length || reduce) return;
      eyes.forEach(e => { e.dataset.ry = e.dataset.ry || e.getAttribute('ry'); e.setAttribute('ry', px(3)); });
      setTimeout(() => eyes.forEach(e => e.setAttribute('ry', e.dataset.ry)), 120);
    };
    const look = () => {
      el._look = setTimeout(look, rand(3500, 7000));
      const f = $('.face', el); if (!f || !$('.face .eye', el) || reduce) return;
      f.style.transform = `translateX(${(rand(-14, 14) / 250 * 100).toFixed(2)}%)`;
      setTimeout(() => { f.style.transform = ''; }, 450 + 900);
    };
    el._blink = setTimeout(blink, 3000);
    el._look = setTimeout(look, 4000);
  }

  function setMood(el, m) {
    if (el.dataset.mood === m && $('.face', el).innerHTML) return;
    el.dataset.mood = m;
    const orb = $('.orb', el), face = $('.face', el), col = MOOD_COLOR[m] || EYE;
    face.style.transform = '';
    face.innerHTML = faceSVG(m, EYE);
    orb.style.setProperty('--glow', col);
    orb.style.borderColor = m === 'hot' ? 'rgba(255,140,90,.63)' : 'rgba(255,255,255,.16)';
    $('.cap-text', el).textContent = CAPTION[m] || greeting();
    $('.cap-dot', el).style.setProperty('--dot', m === 'worried' ? BAD : OK);
  }

  // The demo page (demo.js) draws its companion with the same faces and life.
  window.PocketFace = { faceSVG, startLife, MOOD_COLOR };

  /* ---------- the other screens ---------- */
  const ARC = 207.35, FULL = 276.46;   // 270° of a radius-44 circle, and the whole circle
  const icon = {
    bulb: '<svg viewBox="0 0 24 24"><path d="M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.6.5 1 1.2 1 2.1h5c0-.9.4-1.6 1-2.1A6 6 0 0 0 12 3z"/></svg>',
    flame: '<svg viewBox="0 0 24 24"><path d="M12 22a7 7 0 0 0 7-7c0-4-3-6-4-10-2 2-3 4-3 6-1-1-2-2-2-4-3 3-5 5-5 8a7 7 0 0 0 7 7z"/></svg>',
    note: '<svg viewBox="0 0 24 24"><path d="M9 18V5l12-2v13M9 18a3 3 0 1 1-6 0 3 3 0 0 1 6 0zm12-2a3 3 0 1 1-6 0 3 3 0 0 1 6 0z"/></svg>',
    lock: '<svg viewBox="0 0 24 24"><rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/></svg>',
    wifi: '<svg viewBox="0 0 24 24"><path d="M2 8.8a15 15 0 0 1 20 0M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0"/><circle cx="12" cy="19.5" r="1.2"/></svg>',
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
    el.innerHTML = `<span class="key k1"></span><span class="key k2"></span><div class="case"></div><div class="screen">
      <div class="scr s-companion"><div class="status">${icon.wifi}<span>78%</span></div><div class="clock">08:24</div>
        <div class="orbx"><div class="orb"><svg class="face" viewBox="10 15 80 80" aria-hidden="true"></svg></div></div>
        <div class="cap"><i class="cap-dot"></i><span class="cap-text">Good morning</span></div></div>
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
    el.setAttribute('aria-label', 'PocketOS, a round touchscreen with an animated companion');
    if (!el.dataset.screen) el.dataset.screen = 'companion';
    el.dataset.mood = '';
    setMood(el, 'idle');
    tick(el);
    dialTo(el, 'heat', 0);
    dialTo(el, 'music', 0);
    startLife(el);
  }

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
  setInterval(() => $$('.pocket').forEach(el => { tick(el); if (el.dataset.mood === 'idle') $('.cap-text', el).textContent = greeting(); }), 15000);

  $$('.pocket[data-build]').forEach(build);

  // Tap the orb: happy for 1.3 s, as home_happy(1300) does
  $$('.pocket').forEach(el => el.addEventListener('click', () => {
    if (el.dataset.screen !== 'companion' || el.dataset.mood === 'happy') return;
    const was = el.dataset.mood; setMood(el, 'happy');
    clearTimeout(el._happy); el._happy = setTimeout(() => setMood(el, was), 1300);
  }));

  // Mood buttons drive the story's devices
  $$('.moods button').forEach(b => b.addEventListener('click', () => {
    [$('#story'), $('.chapter .mini')].forEach(el => { if (el) { show(el, 'companion'); setMood(el, b.dataset.mood); } });
    $$('.moods button').forEach(x => x.setAttribute('aria-pressed', String(x === b)));
  }));

  // Chapters switch the screen as they reach the middle of the view. On phones each chapter has
  // its own device (.mini); on wider screens one stays pinned.
  const story = $('#story');
  if (story && 'IntersectionObserver' in window) {
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

  // Pre-order status, shared by the menu bar button and the home page (cached for two minutes per visit)
  const preStatus = () => {
    try { const c = JSON.parse(sessionStorage.getItem('po-pre') || 'null'); if (c && Date.now() - c.t < 120000) return Promise.resolve(c.s); } catch (e) {}
    return fetch('/api/preorder', { cache: 'no-store' }).then(r => r.json()).then(s => { try { sessionStorage.setItem('po-pre', JSON.stringify({ t: Date.now(), s })); } catch (e) {} return s; });
  };
  const preLine = $('[data-pre-line]');
  preStatus().then(s => {
    if (!s || !s.ok) return;
    if (s.reason === 'open') document.querySelectorAll('.cta').forEach(a => { a.textContent = 'Pre-order'; a.setAttribute('aria-label', 'Pre-order the First Edition'); });
    if (preLine && s.reason === 'open') preLine.textContent = 'Pre-orders open · only 25 at this price';
    else if (preLine && s.reason === 'sold_out') preLine.textContent = 'First Edition sold out';
    if (s.reason === 'sold_out' || s.reason === 'closed') document.querySelectorAll('.pl-price').forEach(e => { e.textContent = '£99.99'; });
  }).catch(() => {});

  // Collapsible sections: a link to one opens it; "Open all" opens or closes every spec group
  const openHash = () => { const t = location.hash && document.getElementById(decodeURIComponent(location.hash.slice(1))); if (t && t.tagName === 'DETAILS') t.open = true; };
  openHash(); addEventListener('hashchange', openHash);
  const all = $('[data-open-all]');
  if (all) all.addEventListener('click', () => {
    const open = all.getAttribute('aria-pressed') !== 'true';
    $$('details.spec-group').forEach(d => { d.open = open; });
    all.setAttribute('aria-pressed', String(open)); all.textContent = open ? 'Close all' : 'Open all';
  });

  // Header: firmer glass once the page scrolls; the menu opens a drawer on phones
  const bar = $('.topbar'), menu = $('.menu'), drawer = $('#drawer');
  if (bar) { const f = () => bar.classList.toggle('scrolled', scrollY > 8); f(); addEventListener('scroll', f, { passive: true }); }
  if (menu && drawer) {
    const set = open => { menu.setAttribute('aria-expanded', String(open)); drawer.hidden = !open; };
    menu.addEventListener('click', () => set(drawer.hidden));
    $$('a', drawer).forEach(a => a.addEventListener('click', () => set(false)));
    addEventListener('keydown', e => { if (e.key === 'Escape' && !drawer.hidden) { set(false); menu.focus(); } });
  }

  // The brand film plays (muted) while it's on screen and pauses when it isn't
  const v = $('#film'), snd = $('#film-sound');
  if (v) {
    if ('IntersectionObserver' in window && !reduce) {
      new IntersectionObserver(es => es.forEach(e => {
        if (e.isIntersecting) { const p = v.play(); if (p && p.catch) p.catch(() => {}); } else v.pause();
      }), { threshold: .5 }).observe(v);
    } else v.controls = true;
    if (snd) snd.addEventListener('click', () => {
      v.muted = !v.muted;
      if (!v.muted && v.paused) v.play();
      snd.setAttribute('aria-pressed', String(!v.muted));
      snd.setAttribute('aria-label', v.muted ? 'Turn sound on' : 'Turn sound off');
    });
  }

  // Getting started and the user guide: highlight the section you're reading in the step bar or contents
  document.querySelectorAll('.journey, .toc').forEach(nav => {
    const bar = nav.querySelector('ol') || nav, links = [...nav.querySelectorAll('a[href^="#"]')];
    const secs = links.map(a => document.querySelector(a.getAttribute('href')));
    const steps = nav.classList.contains('journey');
    let last = -1, ticking = false;
    const update = () => {
      ticking = false;
      let cur = 0; secs.forEach((s, i) => { if (s && s.getBoundingClientRect().top < innerHeight * 0.35) cur = i; });
      if (cur === last) return; last = cur;
      links.forEach((a, k) => {
        if (steps) a.classList.toggle('done', k < cur);
        if (k === cur) a.setAttribute('aria-current', steps ? 'step' : 'location'); else a.removeAttribute('aria-current');
      });
      if (bar.scrollWidth > bar.clientWidth + 4) { const a = links[cur], el = steps ? a.parentElement : a; bar.scrollTo({ left: el.offsetLeft - (bar.clientWidth - a.offsetWidth) / 2 }); }
    };
    addEventListener('scroll', () => { if (!ticking) { ticking = true; requestAnimationFrame(update); } }, { passive: true });
    update();
  });
  // Home: short films play in place, one at a time, with sound and controls
  document.querySelectorAll('.reel').forEach(fig => {
    const v = fig.querySelector('video');
    fig.querySelector('.reel-play').addEventListener('click', () => {
      document.querySelectorAll('.reel video').forEach(o => { if (o !== v) o.pause(); });
      v.controls = true; v.play().catch(() => {});
    });
    v.addEventListener('play', () => fig.classList.add('playing'));
    v.addEventListener('ended', () => { fig.classList.remove('playing'); v.controls = false; v.load(); });
  });

})();
