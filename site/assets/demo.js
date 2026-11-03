// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// The demo: a working PocketOS and its real settings page, sharing one pretend home.
//
// The device side follows the firmware's screens (firmware/main/ui: ui_home.c, ui_menu.c,
// ui_list.c, ui_control.c, ui_misc.c) with the same layout, wording, colours and button timings.
// The settings page is the device's own page (firmware/main/www/index.html), copied in at build
// time; demo-shim.js routes its requests here instead of to a device. The API below follows
// firmware/host/mock_device.py. Nothing leaves the browser.
(() => {
  'use strict';
  const root = document.getElementById('demo-pocket'); if (!root) return;
  const F = window.PocketFace;
  const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const $ = (s, el = document) => el.querySelector(s);
  const $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const esc = s => String(s == null ? '' : s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
  const num = v => (Math.round(v * 10) / 10).toString();   // fmt_num: one decimal at most

  /* ---------- icons: the firmware's own paths (tools/assetgen.py) ---------- */
  const ICON = {
    star: 'M12 3l2.7 5.6 6.1.9-4.4 4.3 1 6.1L12 17l-5.4 2.9 1-6.1L3.2 9.5l6.1-.9z',
    bulb: 'M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.6.5 1 1.2 1 2.1h5c0-.9.4-1.6 1-2.1A6 6 0 0 0 12 3z',
    flame: 'M12 3c1 3 5 5 5 10a5 5 0 0 1-10 0c0-2 1-3 2-4 0 2 1 3 2 3 0-3-1-6 1-9z',
    play: '!M8 5v14l11-7z', pause: '!M7 5h4v14H7zM13 5h4v14h-4z', prev: '!M18 6v12l-9-6zM6 6v12h2V6z', next: '!M6 6v12l9-6zM16 6v12h2V6z', stop: '!M7 7h10v10H7z',
    lock: 'M6 11h12v9H6zM8 11V8a4 4 0 0 1 8 0v3', unlock: 'M6 11h12v9H6zM8 11V8a4 4 0 0 1 7.6-1.8',
    home: 'M3 11l9-8 9 8M5 10v10h14V10',
    sparkles: 'M12 3l1.8 5.2L19 10l-5.2 1.8L12 17l-1.8-5.2L5 10l5.2-1.8zM18 16l.8 2.2L21 19l-2.2.8L18 22l-.8-2.2L15 19l2.2-.8z',
    dots: 'M5 12h.01M12 12h.01M19 12h.01',
    qr: 'M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 14h2v2h-2zM18 14h2v2h-2zM14 18h2v2h-2zM18 18h2v2h-2z',
    plug: 'M9 3v5M15 3v5M7 8h10v3a5 5 0 0 1-10 0zM12 16v5',
    fan: 'M12 12m-1.5 0a1.5 1.5 0 1 0 3 0a1.5 1.5 0 1 0-3 0M12 10.5C11 7 12 4 14.5 4S17 7.5 13.3 11M13.5 12c3.5-1 6.5 0 6.5 2.5S16.5 17 13 13.3M12 13.5c1 3.5 0 6.5-2.5 6.5S7 16.5 10.7 12.7M10.5 12C7 13 4 12 4 9.5S7.5 7 11.3 10.7',
    blinds: 'M4 4h16M5 4v12M19 4v12M5 8h14M5 12h14M5 16h14M12 16v4',
    thermo: 'M10 14V5a2 2 0 0 1 4 0v9a4 4 0 1 1-4 0zM12 9v7',
    power: 'M12 3v8M6.3 7.5a8 8 0 1 0 11.4 0', minus: 'M6 12h12', plus: 'M12 6v12M6 12h12', up: 'M6 15l6-6 6 6', down: 'M6 9l6 6 6-6',
    mic: 'M9 5a3 3 0 0 1 6 0v6a3 3 0 0 1-6 0zM5 11a7 7 0 0 0 14 0M12 18v3',
    gear: 'M12 9a3 3 0 1 0 0 6a3 3 0 1 0 0-6zM12 2v3M12 19v3M4.2 4.2l2.1 2.1M17.7 17.7l2.1 2.1M2 12h3M19 12h3M4.2 19.8l2.1-2.1M17.7 6.3l2.1-2.1',
    wifi: 'M2 8.5a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8.5 15.5a5 5 0 0 1 7 0M12 19h.01',
    sun: 'M12 8a4 4 0 1 0 0 8a4 4 0 1 0 0-8zM12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4',
    palette: 'M12 3a9 9 0 0 0 0 18c1.1 0 1.6-.9 1.2-1.8-.5-1.1.2-2.2 1.4-2.2H17a4 4 0 0 0 4-4c0-5.5-4-10-9-10zM7.5 11.5h.01M9.5 7.5h.01M14.5 7.5h.01M17 11h.01'
  };
  const icon = (n, size) => {
    const d = ICON[n] || '', fill = d[0] === '!';
    return `<svg class="ic" style="--s:${size}" viewBox="0 0 24 24" aria-hidden="true" fill="${fill ? 'currentColor' : 'none'}" stroke="${fill ? 'none' : 'currentColor'}" stroke-width="${size < 60 ? 1.8 : 1.5}" stroke-linecap="round" stroke-linejoin="round"><path d="${d.replace('!', '')}"/></svg>`;
  };

  /* ---------- the pretend home ---------- */
  const TILE_KEYS = [['favourites', 'Favourites'], ['lights', 'Lights'], ['heating', 'Heating'], ['media', 'Media'], ['locks', 'Locks'], ['rooms', 'Rooms'],
    ['scenes', 'Scenes'], ['plugs', 'Plugs'], ['fans', 'Fans'], ['blinds', 'Blinds'], ['sensors', 'Sensors']];
  const TILE_ICON = { favourites: 'star', lights: 'bulb', heating: 'flame', media: 'play', locks: 'lock', rooms: 'home', scenes: 'sparkles', plugs: 'plug', fans: 'fan', blinds: 'blinds', sensors: 'thermo' };
  const CONTROLS = { light: 'On, off and brightness', media: 'Play, pause, skip and volume', climate: 'Target temperature and heating on/off', switch: 'On and off',
    cover: 'Open, close and position', fan: 'On, off and speed', lock: 'Lock and unlock', sensor: 'Shows a live reading', scene: 'Runs with one tap' };
  const DOMAIN = { light: 'light', media: 'media_player', climate: 'climate', switch: 'switch', cover: 'cover', fan: 'fan', lock: 'lock', sensor: 'sensor', scene: 'scene' };
  const AREAS = ['Living room', 'Kitchen', 'Bedroom', 'Hallway', 'Garden'];
  const ENTITIES = () => [
    { id: 'light.floor_lamp', name: 'Floor lamp', dom: 'light', area: 'Living room', on: true, level: 70, lmin: 1, lmax: 100, lstep: 1, warmth: true, colour: true, kelvin: 2700, kmin: 2200, kmax: 6500, hue: 30, sat: -1 },
    { id: 'media_player.tv', name: 'TV', dom: 'media', area: 'Living room', on: false, level: 35, lmin: 0, lmax: 100, lstep: 1, title: 'Nature documentary', prevnext: true },
    { id: 'climate.thermostat', name: 'Thermostat', dom: 'climate', area: 'Living room', on: false, level: 20.5, lmin: 7, lmax: 30, lstep: 0.5, cur: 19.5 },
    { id: 'light.kitchen', name: 'Ceiling light', dom: 'light', area: 'Kitchen', on: false, level: 100, lmin: 1, lmax: 100, lstep: 1 },
    { id: 'media_player.kitchen', name: 'Kitchen speaker', dom: 'media', area: 'Kitchen', on: false, level: 30, lmin: 0, lmax: 100, lstep: 1, title: 'Morning Jazz', prevnext: true },
    { id: 'switch.kettle', name: 'Kettle plug', dom: 'switch', area: 'Kitchen', on: false },
    { id: 'light.bedside', name: 'Bedside lamp', dom: 'light', area: 'Bedroom', on: false, level: 40, lmin: 1, lmax: 100, lstep: 1, warmth: true, kelvin: 3000, kmin: 2200, kmax: 6500, sat: -1 },
    { id: 'cover.blinds', name: 'Blinds', dom: 'cover', area: 'Bedroom', on: true, level: 100, lmin: 0, lmax: 100, lstep: 1 },
    { id: 'fan.bedroom', name: 'Fan', dom: 'fan', area: 'Bedroom', on: false, level: 50, lmin: 1, lmax: 100, lstep: 1 },
    { id: 'lock.front', name: 'Front door', dom: 'lock', area: 'Hallway', on: true },
    { id: 'sensor.hall_temp', name: 'Hall temperature', dom: 'sensor', area: 'Hallway', level: 19.2, unit: '°C', lmin: -10, lmax: 40, lstep: 0.1 },
    { id: 'sensor.garden_temp', name: 'Garden temperature', dom: 'sensor', area: 'Garden', level: 12.4, unit: '°C', lmin: -10, lmax: 40, lstep: 0.1, outdoor: true },
    { id: 'scene.goodnight', name: 'Goodnight', dom: 'scene', area: '' },
    { id: 'scene.movie', name: 'Movie time', dom: 'scene', area: '' }
  ];
  const defaults = () => ({
    schema: 4, name: 'PocketOS', brightness: 80, clock_24h: true, date_format: 'dmy', sleep_s: 30, eye: 'blue', anim: 'lively',
    tz: 'GMT0BST,M3.5.0/1,M10.5.0', auto_add: false, alerts: true, speak: true, volume: 60, updates: 'notify',
    tiles: TILE_KEYS.map(([key, label], i) => ({ key, label, show: i < 7 })),
    favourites: ['light.floor_lamp', 'climate.thermostat', 'lock.front'], hidden: [], known: [], names: [], ha_url: 'http://homeassistant.local:8123', ha_token: 'demo'
  });
  const EYES = { blue: '#6AA7FF', violet: '#9B7BFF', white: '#E8ECFF' };
  const TOKEN = 'demo-' + Math.random().toString(36).slice(2, 10);

  let S;
  function fresh() {
    const now = Math.floor(Date.now() / 1000), hist = [];
    for (let i = 0; i < 48 * 6; i++) {   // as mock_device.py: a day on battery, then on the charger overnight, now off it for six hours
      const t = now - (48 * 6 - i) * 600, h = i / 6; let pct, fl;
      if (h < 18) { pct = 100 - h * 4.5; fl = 0; } else if (h < 21) { pct = Math.min(100, 19 + (h - 18) * 30); fl = 3; } else if (h < 42) { pct = 100; fl = 2; } else { pct = 100 - (h - 42) * 3.7; fl = 0; }
      hist.push([t, Math.floor(pct), Math.floor(3500 + pct * 6.5), fl]);
    }
    S = { cfg: defaults(), ents: ENTITIES(), pin: pin(), tokens: [TOKEN], bad: 0, lockedUntil: 0, ha: 'connected', haError: '',
      firmware: '0.1.5', hist, battery: hist[hist.length - 1][1], booted: Date.now(), upd: { state: 'idle', progress: 0, error: '', checked: null },
      resetUntil: 0, downUntil: 0 };
    S.cfg.known = S.ents.map(e => e.id);
  }
  const pin = () => String(Math.floor(Math.random() * 1e6)).padStart(6, '0');
  fresh();

  const nm = e => (S.cfg.names.find(n => n.id === e.id) || {}).name || e.name;   // model_name
  const visible = e => (S.cfg.auto_add || S.cfg.known.includes(e.id)) && !S.cfg.hidden.includes(e.id);
  const ents = () => S.ha === 'connected' ? S.ents : [];
  const find = id => S.ents.find(e => e.id === id);
  const catMatch = (k, e) => ({ lights: 'light', heating: 'climate', media: 'media', locks: 'lock', plugs: 'switch', fans: 'fan', blinds: 'cover', sensors: 'sensor', scenes: 'scene' })[k] === e.dom;
  const hasLevel = e => ['light', 'climate', 'media', 'fan', 'cover', 'sensor'].includes(e.dom);

  // model_status / model_value
  function status(e) {
    switch (e.dom) {
      case 'light': case 'fan': return e.on ? `On · ${Math.round(e.level)}%` : 'Off';
      case 'switch': return e.on ? 'On' : 'Off';
      case 'climate': return (e.on ? `${num(e.level)}°` : 'Off') + ` · now ${num(e.cur)}°`;
      case 'media': return `${e.on ? 'Playing' : 'Paused'} · Vol ${Math.round(e.level)}`;
      case 'cover': return e.level >= 100 ? 'Open' : e.level <= 0 ? 'Closed' : `${Math.round(e.level)}% open`;
      case 'lock': return e.on ? 'Locked' : 'Unlocked';
      case 'sensor': return `${num(e.level)} ${e.unit}`;
      case 'scene': return 'Tap to run';
    }
    return '';
  }
  function valueText(e, v, dragging) {
    switch (e.dom) {
      case 'climate': return num(v) + '°';
      case 'sensor': return num(v) + '°';
      case 'cover': return v <= 0 ? 'Closed' : v >= 100 ? 'Open' : Math.round(v) + '%';
      case 'media': return String(Math.round(v));
      default: return (!e.on && !dragging) ? 'Off' : Math.round(v) + '%';
    }
  }
  // model_mood + mood_caption
  function mood() {
    if (S.battery <= 15) return 'tired';
    if (S.ha === 'error') return 'worried';
    let playing = false, heating = false, unlocked = false;
    for (const e of ents()) {
      if (!visible(e)) continue;
      if (e.dom === 'lock' && !e.on && S.cfg.alerts) unlocked = true;
      if (e.dom === 'media' && e.on) playing = true;
      if (e.dom === 'climate' && e.on) heating = true;
    }
    if (unlocked) return 'alert';
    if (playing) return 'dance';
    if (heating) return 'hot';
    const o = ents().find(e => e.outdoor);
    if (o && o.level < 5) return 'cold';
    return 'idle';
  }
  function caption(m) {
    const h = new Date().getHours();
    const c = { sleep: 'Zzz', dance: 'Vibing to the music', hot: 'Toasty in here', worried: 'Can’t reach home', tired: 'Battery low', charge: 'Charging',
      happy: 'All done!', sad: 'That didn’t work', listen: 'Listening...', wake: 'Good to see you', away: 'Out and about' }[m];
    if (c) return c;
    if (m === 'alert') { const l = ents().find(e => e.dom === 'lock' && !e.on); return l ? `${nm(l)} is unlocked` : 'A door is unlocked'; }
    if (m === 'cold') { const o = ents().find(e => e.outdoor); return `Brr... ${Math.round(o.level)}° outside`; }
    if (S.ha === 'not_set') return 'Hello! Scan Settings to set me up';
    return h < 12 ? 'Good morning' : h < 18 ? 'Good afternoon' : 'Good evening';
  }

  /* ---------- acting on the home: "Sending..." until the home confirms ---------- */
  function act(id, what, value) {
    const e = find(id); if (!e) return;
    if (e.dom === 'scene') { runScene(id); toast('Running scene', 1500); return; }
    e.pending = true;
    if (what === 'level') { e.pendingLevel = value; }
    changed();
    setTimeout(() => {
      e.pending = false; delete e.pendingLevel;
      switch (what) {
        case 'toggle': e.on = !e.on; if (e.dom === 'cover') e.level = e.on ? 100 : 0; break;
        case 'level': e.level = value; if (['light', 'fan'].includes(e.dom)) e.on = value > 0; break;
        case 'warmth': e.kelvin = value; e.sat = -1; e.on = true; break;
        case 'hue': e.hue = value; e.sat = 100; e.on = true; break;
        case 'up': e.level = clamp(e.level + e.lstep, e.lmin, e.lmax); break;
        case 'down': e.level = clamp(e.level - e.lstep, e.lmin, e.lmax); break;
        case 'open': e.level = 100; break;
        case 'close': e.level = 0; break;
        case 'on': e.on = true; break;
        case 'off': e.on = false; break;
      }
      changed();
    }, e.dom === 'lock' ? 1400 : 650);
  }
  function runScene(id) {
    const set = (i, k, v) => { const e = find(i); if (e) e[k] = v; };
    if (id === 'scene.goodnight') { ['light.floor_lamp', 'light.kitchen', 'light.bedside'].forEach(i => set(i, 'on', false)); set('lock.front', 'on', true); set('media_player.tv', 'on', false); set('media_player.kitchen', 'on', false); set('cover.blinds', 'level', 0); }
    if (id === 'scene.movie') { set('light.floor_lamp', 'on', true); set('light.floor_lamp', 'level', 20); set('light.kitchen', 'on', false); set('cover.blinds', 'level', 0); set('media_player.tv', 'on', true); }
    setTimeout(changed, 500);
  }

  /* ---------- the screen ---------- */
  const scr = $('.os', root), layer = $('.os-scr', root), over = $('.os-over', root), toastEl = $('.os-toast', root);
  let stack = [{ k: 'home' }], power = 'on', asleep = false, lastTouch = Date.now(), overlay = null, happyUntil = 0, dragging = false;
  const top = () => stack[stack.length - 1];

  function go(ref) { stack.push(ref); render(true); }
  function back() { if (stack.length > 1) { stack.pop(); render(true, true); } }
  function home() { stack = [{ k: 'home' }]; render(true, true); }

  function toast(t, ms = 1800) {
    toastEl.textContent = t; toastEl.classList.add('show');
    clearTimeout(toast.t); toast.t = setTimeout(() => toastEl.classList.remove('show'), ms);
  }

  function title(t, back, bottom) {
    return `<button class="os-title${bottom ? ' bottom' : ''}" data-back>${back ? '<span>‹</span> ' : ''}${esc(t)}</button>`;
  }

  // ----- home (ui_home.c)
  function homeHTML() {
    const t = new Date(), h = t.getHours(), m = String(t.getMinutes()).padStart(2, '0');
    const clock = S.cfg.clock_24h ? `${String(h).padStart(2, '0')}:${m}` : `${(h % 12) || 12}:${m}`;
    return `<div class="home">
      <div class="status">${icon('wifi', 22)}<span>${S.battery}%</span></div>
      <div class="clock">${clock}</div>
      <div class="orbx"><button class="orb" aria-label="Companion"><svg class="face" viewBox="10 15 80 80" aria-hidden="true"></svg></button></div>
      <div class="cap"><i class="cap-dot"></i><span class="cap-text"></span></div></div>`;
  }
  let lastMood = null;
  function homeFace(force) {
    const el = $('.home', layer); if (!el) return;
    const md = asleep ? 'sleep' : Date.now() < happyUntil ? 'happy' : mood();
    const eye = EYES[S.cfg.eye] || EYES.blue;
    if (force || md !== lastMood || el.dataset.eye !== eye) {
      lastMood = md; el.dataset.eye = eye;
      $('.face', el).innerHTML = F.faceSVG(md, eye);
      const col = F.MOOD_COLOR[md] || eye;
      $('.orb', el).style.setProperty('--glow', col);
      $('.orb', el).style.borderColor = md === 'hot' ? 'rgba(255,140,90,.63)' : 'rgba(255,255,255,.16)';
    }
    el.dataset.mood = md; el.dataset.anim = S.cfg.anim;
    $('.cap-text', el).textContent = caption(md);
    $('.cap-dot', el).style.setProperty('--dot', S.ha === 'error' ? '#FF7A8A' : '#7BE3A4');
  }

  // ----- menu (ui_menu.c)
  function summary(key) {
    const list = ents().filter(e => visible(e));
    if (key === 'rooms') { const n = AREAS.filter(a => list.some(e => e.area === a)).length; return [`${n} room${n === 1 ? '' : 's'}`, false]; }
    if (key === 'favourites') { const n = list.filter(e => S.cfg.favourites.includes(e.id)).length; return [`${n} pinned`, false]; }
    const es = list.filter(e => catMatch(key, e)), n = es.length;
    const on = es.filter(e => e.dom === 'lock' ? !e.on : e.dom === 'cover' ? e.level > 0 : e.on).length;
    if (!n) return ['', false];
    if (key === 'locks') return [on ? `${on} unlocked` : 'All locked', on > 0];
    if (key === 'media') return [on ? `${on} playing` : 'Idle', on > 0];
    if (key === 'heating') { const c = es.find(e => e.on); return [c ? status(c) : 'Off', !!c]; }
    if (key === 'blinds') return [on ? `${on} open` : 'All closed', on > 0];
    if (key === 'scenes') return [`${n} scene${n === 1 ? '' : 's'}`, false];
    if (key === 'sensors') return [`${n} sensor${n === 1 ? '' : 's'}`, false];
    return [`${on} of ${n} on`, on > 0];
  }
  function menuItems() {
    const it = [{ key: 'settings', label: 'Settings', icon: 'qr', sub: S.ha === 'not_set' ? 'Set me up' : 'Scan on phone', on: false }];
    let anyHidden = false;
    for (const t of S.cfg.tiles) {
      if (!t.show) { anyHidden = true; continue; }
      const [sub, on] = summary(t.key);
      it.push({ key: t.key, label: t.label, icon: TILE_ICON[t.key], sub, on });
    }
    it.push({ key: 'more', label: 'More', icon: 'dots', sub: anyHidden ? 'Everything else' : 'Companion, device', on: false });
    return it;
  }
  function menuHTML(ref) {
    const it = menuItems(), pages = Math.ceil(it.length / 4);
    ref.page = clamp(ref.page || 0, 0, pages - 1);
    const pg = it.slice(ref.page * 4, ref.page * 4 + 4);
    return `<div class="os-menu"><div class="os-head">PocketOS</div><div class="tiles">${pg.map(t =>
      `<button class="tile${t.on ? ' on' : ''}" data-tile="${t.key}">${icon(t.icon, 40)}<b>${esc(t.label)}</b><small>${esc(t.sub)}</small></button>`).join('')}</div>
      <div class="pdots">${Array.from({ length: pages }, (_, i) => `<i class="${i === ref.page ? 'on' : ''}"></i>`).join('')}</div></div>`;
  }

  // ----- lists (ui_list.c)
  function rows(ref) {
    const r = [], list = ents().filter(visible);
    const eRow = (e, withArea) => { const st = e.pending ? 'Sending...' : status(e); r.push({ kind: 'e', id: e.id, label: nm(e), sub: withArea && e.area ? `${e.area} · ${st}` : st, on: e.dom !== 'lock' && e.dom !== 'sensor' && (e.dom === 'cover' ? e.level > 0 : e.on) }); };
    switch (ref.list) {
      case 'cat': {
        const es = list.filter(e => catMatch(ref.arg, e)), on = es.filter(e => e.dom === 'lock' ? !e.on : e.on).length;
        if (es.length > 1 && on) {
          const all = { lights: ['Turn all off', 'Every light at once'], media: ['Pause all', 'Every speaker and TV'], locks: ['Lock all', 'Every door'], plugs: ['Turn all off', 'Every plug'] }[ref.arg];
          if (all) r.push({ kind: 'all', id: ref.arg, label: all[0], sub: all[1] });
        }
        es.forEach(e => eRow(e, true)); break;
      }
      case 'rooms':
        AREAS.forEach(a => { const es = list.filter(e => e.area === a); if (!es.length) return; const on = es.filter(e => e.on && e.dom !== 'lock' && e.dom !== 'sensor').length;
          r.push({ kind: 'room', id: a, label: a, sub: `${es.length} device${es.length === 1 ? '' : 's'} · ${on} on`, on: on > 0 }); }); break;
      case 'room': list.filter(e => e.area === ref.arg).forEach(e => eRow(e, false)); break;
      case 'favs': S.cfg.favourites.map(find).filter(e => e && visible(e)).forEach(e => eRow(e, true)); break;   // the user's order
      case 'hidden': ents().filter(e => S.cfg.hidden.includes(e.id)).forEach(e => eRow(e, true)); break;
      case 'opts': {
        const e = find(ref.arg); if (!e) break;
        const fav = S.cfg.favourites.includes(e.id), hidden = S.cfg.hidden.includes(e.id), at = S.cfg.favourites.indexOf(e.id);
        if (hidden) r.push({ kind: 'opt', id: 'show', label: 'Show on PocketOS', sub: 'Put it back in menus and lists', on: true });
        else {
          r.push({ kind: 'opt', id: 'fav', label: fav ? 'Remove from favourites' : 'Add to favourites', sub: fav ? 'Takes it off the Favourites list' : 'Adds it to the Favourites list', on: fav });
          if (at > 0) r.push({ kind: 'opt', id: 'up', label: 'Move up', sub: 'Earlier in Favourites' });
          if (at >= 0 && at < S.cfg.favourites.length - 1) r.push({ kind: 'opt', id: 'down', label: 'Move down', sub: 'Later in Favourites' });
        }
        r.push({ kind: 'opt', id: 'rename', label: 'Rename', sub: 'On the settings page' });
        if (!hidden) r.push({ kind: 'opt', id: 'hide', label: 'Hide from PocketOS', sub: 'Stays in Home Assistant' });
        break;
      }
      case 'more':
        S.cfg.tiles.filter(t => !t.show).forEach(t => { const [sub] = summary(t.key); if (sub) r.push({ kind: 'tile', id: t.key, label: t.label, sub }); });
        { const nh = ents().filter(e => S.cfg.hidden.includes(e.id)).length; if (nh) r.push({ kind: 'screen', id: 'hidden', label: 'Hidden devices', sub: `${nh} device${nh === 1 ? '' : 's'}` }); }
        r.push({ kind: 'screen', id: 'companion', label: 'Companion', sub: 'Eyes, animation, brightness' });
        r.push({ kind: 'screen', id: 'info', label: 'This PocketOS', sub: 'Wi-Fi, battery, version' });
        r.push({ kind: 'screen', id: 'settings', label: 'Settings website', sub: 'Scan the code on your phone' }); break;
      case 'info':
        r.push({ kind: 'info', label: 'Wi-Fi', sub: '192.168.1.42', on: true });
        r.push({ kind: 'info', label: 'Home Assistant', sub: S.ha === 'connected' ? 'Connected' : S.ha === 'connecting' ? 'Connecting...' : S.ha === 'error' ? 'Problem connecting' : 'Not set up yet', on: S.ha === 'connected' });
        r.push({ kind: 'info', label: 'Battery', sub: `${S.battery}% · about ${Math.round(S.battery / 4)} h left` });
        r.push({ kind: 'info', label: 'Address', sub: 'pocketos-a1b2.local' });
        r.push({ kind: 'info', label: 'Firmware', sub: S.firmware });
        r.push({ kind: 'info', label: 'Created by', sub: 'Leeko' });
        r.push({ kind: 'info', label: 'Home', sub: `${ents().length} devices, ${AREAS.length} rooms` });
        r.push({ kind: 'screen', id: 'wifi', label: 'Change Wi-Fi network', sub: 'Keeps your settings' });
        r.push({ kind: 'screen', id: 'reset', label: 'Reset PocketOS', sub: 'Erases everything on it' }); break;
      case 'companion': {
        const c = S.cfg, cap = s => s[0].toUpperCase() + s.slice(1);
        r.push({ kind: 'set', id: 'eye', label: 'Eye colour', sub: cap(c.eye) });
        r.push({ kind: 'set', id: 'anim', label: 'Animation', sub: cap(c.anim) });
        r.push({ kind: 'set', id: 'brightness', label: 'Brightness', sub: c.brightness + '%' });
        r.push({ kind: 'set', id: 'sleep', label: 'Sleep after', sub: c.sleep_s ? c.sleep_s + ' seconds' : 'Never' }); break;
      }
    }
    return r;
  }
  function listTitle(ref) {
    if (ref.list === 'opts') { const e = find(ref.arg); return e ? nm(e) : 'Device'; }
    return { rooms: 'Rooms', favs: 'Favourites', more: 'More', info: 'This PocketOS', companion: 'Companion', room: ref.arg, hidden: 'Hidden devices',
      cat: (S.cfg.tiles.find(t => t.key === ref.arg) || {}).label || ref.arg }[ref.list];
  }
  function listHTML(ref) {
    const r = rows(ref); ref._rows = r;
    const empty = ref.list === 'favs' ? 'No favourites yet.<br>Press and hold a device<br>to add it.' : ref.list === 'hidden' ? 'Nothing is hidden.' : 'Nothing here yet.<br>Add devices in Home Assistant,<br>then review them on the<br>settings website.';
    return `<div class="list">${title(listTitle(ref), true)}<div class="rows">${r.map((x, i) =>
      `<button class="row" data-row="${i}"><b>${esc(x.label)}</b>${x.sub ? `<small class="${x.on ? 'on' : ''}">${esc(x.sub)}</small>` : ''}</button>`).join('')}</div>
      ${r.length ? '' : `<p class="empty">${empty}</p>`}</div>`;
  }
  function highlight() {
    const box = $('.rows', layer); if (!box) return;
    const c = box.scrollTop + box.clientHeight / 2; let best = 1e9, bi = -1;
    const rs = $$('.row', box);
    rs.forEach((r, i) => { const d = Math.abs(r.offsetTop + r.offsetHeight / 2 - c); if (d < best) { best = d; bi = i; }
      const u = box.clientHeight / 466; r.style.opacity = clamp(1 - d / u / 200, 40 / 255, 1); });
    rs.forEach((r, i) => r.classList.toggle('sel', i === bi));
  }

  // ----- control (ui_control.c)
  const DIAL = { light: '#FFC46B', climate: '#FF7A50', media: '#E07BFF', fan: '#6AD3C0', cover: '#8FA8FF' };
  const R = 204, CX = 233, CY = 233, A0 = 135, SW = 270;
  const pt = a => [CX + R * Math.cos(a * Math.PI / 180), CY + R * Math.sin(a * Math.PI / 180)];
  const arcD = (a0, a1) => { const [x0, y0] = pt(a0), [x1, y1] = pt(a1); return `M${x0.toFixed(1)} ${y0.toFixed(1)}A${R} ${R} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1.toFixed(1)} ${y1.toFixed(1)}`; };
  const HUES = ['Red', 'Orange', 'Yellow', 'Lime', 'Green', 'Teal', 'Cyan', 'Sky', 'Blue', 'Purple', 'Pink', 'Rose'];
  const mixHex = (a, b, t) => '#' + [0, 2, 4].map(i => Math.round(parseInt(a.substr(i + 1, 2), 16) + (parseInt(b.substr(i + 1, 2), 16) - parseInt(a.substr(i + 1, 2), 16)) * clamp(t, 0, 1)).toString(16).padStart(2, '0')).join('');
  function dialRange(e, ref) {
    const m = ref.dmode || 0;
    if (m === 1) return { min: e.kmin, max: e.kmax, step: 50, value: ref.held != null ? ref.held : e.kelvin || (e.kmin + e.kmax) / 2,
      col: v => mixHex('#FFB15A', '#DDEBFF', (v - e.kmin) / (e.kmax - e.kmin)), text: v => Math.round(v / 50) * 50 + 'K', sub: 'Warmth · drag the ring' };
    if (m === 2) return { min: 0, max: 359, step: 5, value: ref.held != null ? ref.held : (e.sat >= 0 ? e.hue : 0),
      col: v => `hsl(${v} 100% 50%)`, text: v => HUES[Math.round(v / 30) % 12], sub: 'Colour · drag the ring' };
    return null;
  }
  function ctlHTML(ref) {
    const e = find(ref.id);
    if (!e || !visible(e)) return `<div class="ctl"><p class="empty">This device is no longer<br>in Home Assistant</p>${title('Back', true, true)}</div>`;
    const optsBtn = `<button class="rb optsb" style="--d:44" data-opts aria-label="Options for ${esc(nm(e))}">${icon('dots', 40)}</button>`;
    if (hasLevel(e)) {
      const col = DIAL[e.dom] || '#8FD3FF';
      const btn = (ic, a, size, on) => `<button class="rb${on ? ' on' : ''}" style="--d:${size}" data-act="${a}" aria-label="${a}">${icon(ic, 30)}</button>`;
      let row = '';
      if (e.dom === 'climate') row = btn('minus', 'down', 52) + btn('flame', 'toggle', 72, e.on) + btn('plus', 'up', 52);
      const modeBtn = (ic, m) => `<button class="rb${(ref.dmode || 0) === m ? ' on' : ''}" style="--d:52" data-mode="${m}" aria-label="${m === 1 ? 'Warmth' : 'Colour'}">${icon(ic, 30)}</button>`;
      if (e.dom === 'light') row = (e.warmth ? modeBtn('sun', 1) : '') + btn('power', 'toggle', 72, e.on) + (e.colour ? modeBtn('palette', 2) : '');
      if (e.dom === 'fan') row = btn('power', 'toggle', 72, e.on);
      if (e.dom === 'media') row = btn('prev', 'prev', 52) + btn(e.on ? 'pause' : 'play', 'toggle', 72, e.on) + btn('next', 'next', 52);
      if (e.dom === 'cover') row = btn('down', 'close', 52) + btn('stop', 'stop', 52) + btn('up', 'open', 52);
      return `<div class="ctl dial-ctl" style="--col:${col}" data-ro="${e.dom === 'sensor' ? 1 : 0}">
        <svg class="dial" viewBox="0 0 466 466"><path class="track" d="${arcD(A0, A0 + SW)}"/><path class="ind" d=""/><circle class="knob" r="16" cx="0" cy="0"/></svg>
        <div class="val"></div><div class="sub"></div><div class="btnrow">${row}</div>${title(nm(e), true, true)}${optsBtn}</div>`;
    }
    const ic = e.dom === 'lock' ? (e.on ? 'lock' : 'unlock') : 'power';
    return `<div class="ctl big-ctl"><button class="bigb${e.on && e.dom !== 'lock' ? ' on' : ''}" data-act="toggle" aria-label="${esc(e.name)}">${icon(ic, 84)}</button>
      <div class="biglbl"></div>${title(nm(e), true, true)}${optsBtn}</div>`;
  }
  function ctlUpdate(ref, liveValue) {
    const e = find(ref.id), el = $('.ctl', layer); if (!e || !el) return;
    const dr = $('.dial', el) && dialRange(e, ref);
    if (dr) {   // a light's warmth or colour
      const v = liveValue != null ? liveValue : dr.value, f = clamp((v - dr.min) / (dr.max - dr.min), 0, 1), a1 = A0 + SW * f;
      $('.ind', el).setAttribute('d', f > 0.001 ? arcD(A0, a1) : ''); $('.ind', el).style.opacity = 1;
      el.style.setProperty('--col', dr.col(v));
      const [kx, ky] = pt(a1); const k = $('.knob', el); k.setAttribute('cx', kx); k.setAttribute('cy', ky);
      $('.val', el).textContent = dr.text(v);
      $('.sub', el).textContent = e.pending ? 'Sending...' : dr.sub;
      $$('.rb[data-act=toggle]', el).forEach(b => b.classList.toggle('on', e.on));
      return;
    }
    if ($('.dial', el)) {
      el.style.setProperty('--col', DIAL[e.dom] || '#8FD3FF');
      const v = liveValue != null ? liveValue : (e.pendingLevel != null ? e.pendingLevel : e.level);
      const f = clamp((v - e.lmin) / (e.lmax - e.lmin), 0, 1), a1 = A0 + SW * f;
      $('.ind', el).setAttribute('d', f > 0.001 ? arcD(A0, a1) : '');
      const [kx, ky] = pt(a1); const k = $('.knob', el); k.setAttribute('cx', kx); k.setAttribute('cy', ky);
      k.style.display = e.dom === 'sensor' ? 'none' : '';
      const dim = ['light', 'fan', 'climate'].includes(e.dom) && !e.on && e.pendingLevel == null && liveValue == null;
      $('.ind', el).style.opacity = dim ? .35 : 1;
      $('.val', el).textContent = valueText(e, v, liveValue != null || e.pendingLevel != null);
      let sub;
      if (e.pending) sub = 'Sending...';
      else sub = { climate: (e.on ? 'Heating' : 'Heating off') + ` · now ${num(e.cur)}°`, light: e.on ? 'Brightness' : 'Off · drag the ring', fan: e.on ? 'Fan speed' : 'Off · drag the ring',
        media: `Volume · ${e.title}${e.on ? '' : ' (paused)'}`, cover: 'Position', sensor: `${e.area} · live` }[e.dom];
      $('.sub', el).textContent = sub;
      $$('.rb[data-act=toggle]', el).forEach(b => { b.classList.toggle('on', e.on); if (e.dom === 'media') b.innerHTML = icon(e.on ? 'pause' : 'play', 30); });
    } else {
      const b = $('.bigb', el);
      if (e.dom === 'lock') b.innerHTML = icon(e.on ? 'lock' : 'unlock', 84); else b.classList.toggle('on', e.on);
      $('.biglbl', el).textContent = e.pending ? (e.dom === 'lock' ? (e.on ? 'Unlocking...' : 'Locking...') : 'Sending...') : status(e);
    }
  }
  // drag the ring
  function dialDrag(ev) {
    const ref = top(), e = find(ref.id), svg = $('.dial', layer);
    if (!e || !svg || e.dom === 'sensor') return false;
    const r = svg.getBoundingClientRect(), s = r.width / 466;
    const x = (ev.clientX - r.left) / s - CX, y = (ev.clientY - r.top) / s - CY;
    const dist = Math.hypot(x, y); if (dist < 150) return false;   // only the ring, not the middle
    const dr = dialRange(e, ref), lo = dr ? dr.min : e.lmin, hi = dr ? dr.max : e.lmax, st = dr ? dr.step : e.lstep;
    const at = cev => {
      const xx = (cev.clientX - r.left) / s - CX, yy = (cev.clientY - r.top) / s - CY;
      let a = Math.atan2(yy, xx) * 180 / Math.PI; a = (a - A0 + 720) % 360;
      if (a > SW) a = a > SW + 45 ? 0 : SW;
      let v = lo + (hi - lo) * a / SW; v = Math.round(v / st) * st;
      return clamp(v, lo, hi);
    };
    dragging = true; let v = at(ev); ctlUpdate(ref, v);
    const mv = m => { v = at(m); ctlUpdate(ref, v); };
    const up = () => { dragging = false; removeEventListener('pointermove', mv); removeEventListener('pointerup', up); removeEventListener('pointercancel', up);
      if (dr) { ref.held = v; act(e.id, ref.dmode === 1 ? 'warmth' : 'hue', v); setTimeout(() => { ref.held = null; }, 1500); } else act(e.id, 'level', v); };
    addEventListener('pointermove', mv); addEventListener('pointerup', up); addEventListener('pointercancel', up);
    return true;
  }

  // ----- overlays (ui_misc.c, ui.c)
  function showOverlay(kind, html) { overlay = kind; over.innerHTML = html; over.className = 'os-over show ' + kind; }
  function hideOverlay() { overlay = null; over.className = 'os-over'; over.innerHTML = ''; }
  const qrHTML = () => `<div class="qr">${title('Settings', true)}<img src="assets/demo/qr.png" alt="Settings code"><p class="host">pocketos-a1b2.local</p><p class="qpin">PIN&nbsp; ${S.pin}</p><p class="hint">Scan on your phone, on home Wi-Fi</p></div>`;
  const VOICE = [
    { when: () => !find('light.floor_lamp').on, said: 'Turn on the floor lamp', reply: 'Turned on the light', run: () => act('light.floor_lamp', 'on') },
    { when: () => find('light.floor_lamp').on, said: 'Turn off the floor lamp', reply: 'Turned off the light', run: () => act('light.floor_lamp', 'off') },
    { when: () => true, said: 'Set the heating to 21 degrees', reply: 'Temperature set to 21°', run: () => { const t = find('climate.thermostat'); t.on = true; act(t.id, 'level', 21); } },
    { when: () => !find('lock.front').on, said: 'Lock the front door', reply: 'Locked the front door', run: () => act('lock.front', 'on') },
    { when: () => true, said: 'Play music in the kitchen', reply: 'Playing in the kitchen', run: () => act('media_player.kitchen', 'on') }
  ];
  let voiceN = 0;
  function voiceStart() { wake(); showOverlay('voice', `<div class="voice"><p class="vt">Listening...</p><div class="mic listening">${icon('mic', 84)}</div><p class="said"></p><p class="reply"></p></div>`); }
  function voiceStop() {
    if (overlay !== 'voice') return;
    let v; for (let i = 0; i < VOICE.length; i++) { const c = VOICE[(voiceN + i) % VOICE.length]; if (c.when()) { v = c; voiceN = (voiceN + i + 1) % VOICE.length; break; } }
    const o = $('.voice', over); $('.mic', o).classList.remove('listening'); $('.vt', o).textContent = 'Thinking...';
    setTimeout(() => {
      if (overlay !== 'voice') return;
      $('.vt', o).textContent = 'Done'; $('.said', o).textContent = `“${v.said}”`; $('.reply', o).textContent = v.reply; v.run();
      if (S.cfg.speak) try { const u = new SpeechSynthesisUtterance(v.reply); u.lang = 'en-GB'; u.volume = S.cfg.volume / 100; speechSynthesis.cancel(); speechSynthesis.speak(u); } catch (e) {}   // like the speaker on PocketOS
      setTimeout(() => { if (overlay === 'voice') hideOverlay(); }, 2600);
    }, 900);
  }
  function resetPrompt(on) {
    if (on) showOverlay('reset', `<div class="resetp"><p>${S.confirm === 'wifi' ? 'Set up Wi-Fi again?<br>Press the Boot button<br>to confirm' : 'Press the Boot button<br>to erase everything'}</p></div>`);
    else if (overlay === 'reset') hideOverlay();
  }

  // ----- sleep, power, boot
  function setAsleep(v) { asleep = v; root.classList.toggle('asleep', v); if (!v) lastTouch = Date.now(); hint(); }
  function wake() { if (power !== 'on') return; lastTouch = Date.now(); if (asleep) setAsleep(false); }
  function powerOff() { power = 'off'; hideOverlay(); root.classList.add('off'); root.classList.remove('asleep'); asleep = false; hint(); }
  function bootUp() {
    power = 'booting'; root.classList.remove('off'); stack = [{ k: 'home' }]; render();
    showOverlay('boot', `<div class="bootmark"><i></i><i></i></div>`);
    setTimeout(() => { hideOverlay(); power = 'on'; lastTouch = Date.now(); hint(); render(); }, reduce ? 300 : 2000);
  }
  setInterval(() => {
    if (power === 'on' && !asleep && S.cfg.sleep_s && !overlay && !dragging && Date.now() - lastTouch > S.cfg.sleep_s * 1000) setAsleep(true);
    if (S.resetUntil && Date.now() > S.resetUntil) { S.resetUntil = 0; resetPrompt(false); }
    if (top().k === 'home' && !asleep) { const c = $('.clock', layer); if (c) { const t = new Date(), h = t.getHours(), m = String(t.getMinutes()).padStart(2, '0'); c.textContent = S.cfg.clock_24h ? `${String(h).padStart(2, '0')}:${m}` : `${(h % 12) || 12}:${m}`; } homeFace(); }
  }, 1000);

  /* ---------- drawing ---------- */
  function render(anim, backwards) {
    const ref = top(); let html = '';
    if (ref.k === 'home') html = homeHTML(); else if (ref.k === 'menu') html = menuHTML(ref); else if (ref.k === 'list') html = listHTML(ref); else html = ctlHTML(ref);
    const keep = ref.k === 'list' && $('.rows', layer) && layer.dataset.ref === JSON.stringify([ref.list, ref.arg]) ? $('.rows', layer).scrollTop : null;
    layer.innerHTML = html;
    layer.dataset.ref = ref.k === 'list' ? JSON.stringify([ref.list, ref.arg]) : '';
    if (anim && !reduce) { layer.classList.remove('in', 'back'); void layer.offsetWidth; layer.classList.add('in'); if (backwards) layer.classList.add('back'); }
    if (ref.k === 'home') { lastMood = null; homeFace(true); }
    if (ref.k === 'list') { const box = $('.rows', layer); if (keep != null) box.scrollTop = keep; else if (ref.scroll) box.scrollTop = ref.scroll; box.addEventListener('scroll', () => { ref.scroll = box.scrollTop; highlight(); }, { passive: true }); highlight(); }
    if (ref.k === 'ctl') ctlUpdate(ref);
    scr.style.setProperty('--bright', (0.45 + 0.55 * S.cfg.brightness / 100).toFixed(2));
  }
  function changed() {
    if (dragging) return;
    const ref = top();
    if (ref.k === 'home') homeFace();
    else if (ref.k === 'ctl') { if (!$('.ctl', layer) || !find(ref.id) || !visible(find(ref.id))) render(); else ctlUpdate(ref); }
    else render();
    scr.style.setProperty('--bright', (0.45 + 0.55 * S.cfg.brightness / 100).toFixed(2));
  }

  /* ---------- touch: taps and swipes on the screen ---------- */
  let down = null;
  scr.addEventListener('pointerdown', ev => {
    if (power !== 'on') return;
    if (asleep) { setAsleep(false); down = null; ev.preventDefault(); return; }
    lastTouch = Date.now();
    if (top().k === 'ctl' && !overlay && !ev.target.closest('[data-opts],[data-back]') && dialDrag(ev)) { down = null; return; }
    down = { x: ev.clientX, y: ev.clientY, t: Date.now(), target: ev.target };
    // press and hold a device for its options, as on PocketOS
    const rowEl = !overlay && ev.target.closest('[data-row]'), ref = top();
    if (rowEl && ref._rows && ref._rows[+rowEl.dataset.row] && ref._rows[+rowEl.dataset.row].kind === 'e') {
      const d = down, id = ref._rows[+rowEl.dataset.row].id;
      d.longT = setTimeout(() => { if (down === d && !d.scrolled && !d.moved) { d.long = true; rowEl.classList.add('held'); openOptions(id); } }, 450);
    }
    // with a mouse, dragging a list scrolls it, as a finger would
    const box = ev.pointerType === 'mouse' && ev.target.closest('.rows');
    if (box) { down.box = box; down.top = box.scrollTop; }
  });
  scr.addEventListener('pointermove', ev => {
    if (down && Math.hypot(ev.clientX - down.x, ev.clientY - down.y) > 10) { down.moved = true; clearTimeout(down.longT); }
    if (!down || !down.box) return;
    const dy = ev.clientY - down.y, dx = ev.clientX - down.x;
    if (Math.abs(dy) > Math.abs(dx)) { down.box.style.scrollSnapType = 'none'; down.box.scrollTop = down.top - dy; down.scrolled = Math.abs(dy) > 6; }
  });
  scr.addEventListener('pointerup', ev => {
    if (!down) return;
    clearTimeout(down.longT);
    if (down.long) { down = null; return; }
    const dx = ev.clientX - down.x, dy = ev.clientY - down.y, ad = Math.max(Math.abs(dx), Math.abs(dy)), target = down.target;
    if (down.box) { const b = down.box; b.style.scrollSnapType = ''; if (down.scrolled) { down = null; return; } }
    down = null;
    const unit = scr.getBoundingClientRect().width / 466;
    if (ad > 40 * unit) return swipe(Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 'right' : 'left') : (dy > 0 ? 'down' : 'up'));
    if (ad < 12 * unit) tap(target);
  });
  function swipe(dir) {
    if (overlay === 'qr' && dir === 'right') return hideOverlay();
    if (overlay) return;
    const ref = top();
    if (ref.k === 'home') { if (dir === 'up' || dir === 'left') go({ k: 'menu', page: 0 }); return; }
    if (ref.k === 'menu') {
      const pages = Math.ceil(menuItems().length / 4);
      if (dir === 'left' && ref.page < pages - 1) { ref.page++; render(true); }
      else if (dir === 'right') { if (ref.page > 0) { ref.page--; render(true, true); } else back(); }
      else if (dir === 'down') back();
      return;
    }
    if (dir === 'right') back();
  }
  function openOptions(id) { go({ k: 'list', list: 'opts', arg: id }); }
  function option(id, what) {   // ui_list.c option()
    const c = S.cfg;
    if (what === 'fav') {
      const on = !c.favourites.includes(id);
      c.favourites = c.favourites.filter(x => x !== id); if (on) c.favourites.push(id);
      toast(on ? 'Added to favourites' : 'Removed from favourites', 1500); return changed();
    }
    if (what === 'up' || what === 'down') {
      const f = c.favourites, a = f.indexOf(id), b = a + (what === 'up' ? -1 : 1);
      if (a >= 0 && b >= 0 && b < f.length) [f[a], f[b]] = [f[b], f[a]];
      return changed();
    }
    if (what === 'rename') { showOverlay('qr', qrHTML()); toast('Rename it on the settings page', 2500); return; }
    if (what === 'hide' || what === 'show') {
      const set = new Set(c.hidden); what === 'hide' ? set.add(id) : set.delete(id); c.hidden = [...set].sort();
      stack.pop(); if (top().k === 'ctl' && top().id === id) stack.pop();
      render(true, true); toast(what === 'hide' ? 'Hidden. Find it under More' : 'Back on PocketOS', 2000);
    }
  }
  function tap(t) {
    if (overlay === 'qr') { if (t.closest('[data-back]')) hideOverlay(); return; }
    if (overlay === 'voice' || overlay === 'reset' || overlay === 'boot') return;
    if (t.closest('[data-back]')) return back();
    const ref = top();
    if (t.closest('[data-opts]') && ref.k === 'ctl') return openOptions(ref.id);
    if (ref.k === 'home' && t.closest('.orb')) { happyUntil = Date.now() + 1300; homeFace(); setTimeout(() => homeFace(), 1350); return; }
    const tile = t.closest('[data-tile]');
    if (tile) {
      const k = tile.dataset.tile;
      if (k === 'settings') return showOverlay('qr', qrHTML());
      if (k === 'more') return go({ k: 'list', list: 'more' });
      if (k === 'rooms') return go({ k: 'list', list: 'rooms' });
      if (k === 'favourites') return go({ k: 'list', list: 'favs' });
      return go({ k: 'list', list: 'cat', arg: k });
    }
    const row = t.closest('[data-row]');
    if (row && ref._rows) {
      const r = ref._rows[+row.dataset.row]; if (!r) return;
      if (r.kind === 'e') { const e = find(r.id); if (ref.list === 'hidden') return openOptions(r.id); if (e.dom === 'scene') return act(e.id); return go({ k: 'ctl', id: r.id }); }
      if (r.kind === 'opt') return option(ref.arg, r.id);
      if (r.kind === 'all') { ents().filter(e => visible(e) && catMatch(r.id, e) && (e.dom === 'lock' ? !e.on : e.on)).forEach(e => act(e.id, 'toggle')); return toast('Sent to every device', 1500); }
      if (r.kind === 'room') return go({ k: 'list', list: 'room', arg: r.id });
      if (r.kind === 'tile') return go({ k: 'list', list: r.id === 'rooms' ? 'rooms' : r.id === 'favourites' ? 'favs' : 'cat', arg: r.id });
      if (r.kind === 'screen') {
        if (r.id === 'settings') return showOverlay('qr', qrHTML());
        if (r.id === 'wifi' || r.id === 'reset') { S.confirm = r.id; S.resetUntil = Date.now() + 15000; return resetPrompt(true); }   // the Boot button confirms
        return go({ k: 'list', list: r.id });
      }
      if (r.kind === 'set') {   // cycle_setting
        const c = S.cfg, EY = ['blue', 'violet', 'white'], AN = ['calm', 'lively', 'off'], SL = [15, 30, 60, 0];
        if (r.id === 'eye') c.eye = EY[(EY.indexOf(c.eye) + 1) % 3];
        else if (r.id === 'anim') c.anim = AN[(AN.indexOf(c.anim) + 1) % 3];
        else if (r.id === 'brightness') c.brightness = c.brightness >= 100 ? 40 : c.brightness < 40 ? 40 : c.brightness >= 70 ? 100 : 70;
        else c.sleep_s = SL[(SL.indexOf(c.sleep_s) + 1) % 4];
        return changed();
      }
      return;
    }
    const mb = t.closest('[data-mode]');
    if (mb && ref.k === 'ctl') { const m = +mb.dataset.mode; ref.dmode = (ref.dmode || 0) === m ? 0 : m; ref.held = null; return render(); }
    const b = t.closest('[data-act]');
    if (b && ref.k === 'ctl') {
      const a = b.dataset.act, e = find(ref.id);
      if (a === 'prev' || a === 'next') return toast(a === 'next' ? 'Next track' : 'Previous track', 1200);
      if (a === 'stop') return toast('Stopped', 1200);
      act(e.id, a);
    }
  }
  // keyboard: arrows swipe, Escape goes back (for people who can't swipe)
  scr.addEventListener('keydown', ev => {
    const m = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' }[ev.key];
    if (m && !ev.target.closest('.rows')) { ev.preventDefault(); swipe(m); }
    if (ev.key === 'Escape') { ev.preventDefault(); overlay === 'qr' ? hideOverlay() : back(); }
  });
  scr.addEventListener('click', ev => { if (ev.detail === 0) { const t = ev.target.closest('button'); if (t) tap(t); } });   // Enter / Space

  /* ---------- the side buttons (esp/buttons.c, main.c, pmu.c timings) ---------- */
  function bootButton() {
    let downAt = 0, holding = false, holdT = 0, clickT = 0, clicks = 0;
    return {
      press() {
        if (power !== 'on') return;
        downAt = Date.now(); holding = false;
        holdT = setTimeout(() => { holding = true; clicks = 0; clearTimeout(clickT); voiceStart(); }, 600);
      },
      release() {
        if (!downAt) return; downAt = 0; clearTimeout(holdT);
        if (power !== 'on') return;
        if (holding) { holding = false; voiceStop(); return; }
        clicks++;
        if (clicks === 2) { clicks = 0; clearTimeout(clickT); wake(); hideOverlay(); home(); return; }
        clickT = setTimeout(() => {
          clicks = 0;
          if (S.resetUntil && Date.now() < S.resetUntil) {
            S.resetUntil = 0;
            if (S.confirm === 'wifi') { S.confirm = null; resetPrompt(false); return toast('On a real PocketOS, it now restarts into Wi-Fi setup', 3000); }
            doReset(); return;
          }
          if (asleep) return wake();
          if (overlay === 'qr') hideOverlay(); else if (!overlay) showOverlay('qr', qrHTML());
        }, 350);
      }
    };
  }
  function powerButton() {
    let downAt = 0, noticeT = 0, offT = 0, onT = 0, didLong = false;
    return {
      press() {
        downAt = Date.now(); didLong = false;
        if (power === 'off') { onT = setTimeout(() => { didLong = true; bootUp(); }, 1000); return; }
        if (power !== 'on') return;
        noticeT = setTimeout(() => { didLong = true; wake(); toast('Keep holding to switch off', 3000); }, 1500);
        offT = setTimeout(() => { toastEl.classList.remove('show'); powerOff(); }, 6000);
      },
      release() {
        if (!downAt) return; downAt = 0;
        clearTimeout(noticeT); clearTimeout(offT); clearTimeout(onT);
        if (didLong || power !== 'on') return;
        if (asleep) wake(); else setAsleep(true);
      }
    };
  }
  const keys = { boot: bootButton(), power: powerButton() };
  $$('[data-key]').forEach(b => {
    const k = keys[b.dataset.key];
    const start = ev => { ev.preventDefault(); b.classList.add('down'); try { b.setPointerCapture(ev.pointerId); } catch (e) {} k.press(); };
    const end = () => { if (!b.classList.contains('down')) return; b.classList.remove('down'); k.release(); };
    b.addEventListener('pointerdown', start); b.addEventListener('pointerup', end); b.addEventListener('pointercancel', end); b.addEventListener('lostpointercapture', end);
    b.addEventListener('keydown', ev => { if ((ev.key === ' ' || ev.key === 'Enter') && !ev.repeat) { ev.preventDefault(); b.classList.add('down'); k.press(); } });
    b.addEventListener('keyup', ev => { if (ev.key === ' ' || ev.key === 'Enter') end(); });
    b.addEventListener('contextmenu', ev => ev.preventDefault());
  });

  function doReset() {
    resetPrompt(false); fresh(); S.tokens = [TOKEN];
    try { localStorage.setItem('pocketos-token-a1b2', TOKEN); } catch (e) {}
    bootUp(); setTimeout(() => toast('Erased. The demo starts again with its sample home.', 3000), 2100);
    reloadSettings(2600);
  }

  /* ---------- a short hint under the device: what to try next ---------- */
  const hintEl = document.getElementById('demo-hint');
  function hint() {
    if (!hintEl) return;
    hintEl.textContent = power === 'off' ? 'PocketOS is off. Hold the Power button for one second to switch it on.'
      : asleep ? 'The screen is asleep. Tap it, or press the Power button, to wake it.'
      : 'Swipe up on the screen for the menu. Swipe right to go back.';
  }

  /* ---------- the settings page's view of the device (API v2, as mock_device.py) ---------- */
  const publicCfg = () => { const o = JSON.parse(JSON.stringify(S.cfg)); delete o.ha_token; o.ha_token_set = !!S.cfg.ha_token; return o; };
  const updInfo = () => ({ online: true, mode: S.cfg.updates, state: S.upd.state, progress: S.upd.progress, error: S.upd.error, ...(S.upd.checked ? { checked: S.upd.checked } : {}) });
  function validate(patch) {
    const s = JSON.parse(JSON.stringify(S.cfg));
    if (!patch || typeof patch !== 'object' || Array.isArray(patch)) return [null, 'Expected a JSON object'];
    const bytes = t => new TextEncoder().encode(t).length;
    for (const [k, v] of Object.entries(patch)) {
      if (k === 'schema') continue;
      const bad = m => [null, m, k];
      if (k === 'name') { if (typeof v !== 'string' || bytes(v) < 1 || bytes(v) > 24) return bad('Name must be 1 to 24 characters'); }
      else if (k === 'brightness') { if (!Number.isInteger(v) || v < 10 || v > 100) return bad('Brightness must be 10 to 100'); }
      else if (k === 'volume') { if (!Number.isInteger(v) || v < 10 || v > 100) return bad('Volume must be 10 to 100'); }
      else if (['clock_24h', 'auto_add', 'alerts', 'speak'].includes(k)) { if (typeof v !== 'boolean') return bad('Must be true or false'); }
      else if (k === 'date_format') { if (!['dmy', 'mdy'].includes(v)) return bad('Must be dmy or mdy'); }
      else if (k === 'sleep_s') { if (![15, 30, 60, 0].includes(v)) return bad('Sleep time must be 15, 30, 60 or 0'); }
      else if (k === 'eye') { if (!EYES[v]) return bad('Eye colour must be blue, violet or white'); }
      else if (k === 'anim') { if (!['calm', 'lively', 'off'].includes(v)) return bad('Animation must be calm, lively or off'); }
      else if (k === 'tz') { if (typeof v !== 'string' || v.length < 3 || v.length > 47 || !/^[A-Za-z0-9<>+\-:,./]+$/.test(v)) return bad('Time zone looks wrong'); }
      else if (k === 'updates') { if (!['off', 'notify', 'auto'].includes(v)) return bad('Updates must be off, notify or auto'); }
      else if (k === 'tiles') {
        const keysOk = TILE_KEYS.map(t => t[0]), seen = new Set();
        if (!Array.isArray(v) || v.length !== keysOk.length) return bad('Tiles list must include every tile once');
        for (const t of v) {
          if (!t || !keysOk.includes(t.key) || seen.has(t.key)) return bad('Unknown or repeated tile');
          if (typeof t.label !== 'string' || bytes(t.label) < 1 || bytes(t.label) > 12) return bad('Tile names must be 1 to 12 characters');
          if (typeof t.show !== 'boolean') return bad('Tile show must be true or false');
          seen.add(t.key);
        }
      }
      else if (k === 'names') {
        if (!Array.isArray(v) || v.length > 48) return bad('You can rename up to 48 devices');
        const seen = new Set();
        for (const x of v) {
          if (!x || typeof x.id !== 'string' || typeof x.name !== 'string') return bad('Contains an invalid device ID');
          if (seen.has(x.id)) return bad('A device is named twice');
          if (bytes(x.name) < 1 || bytes(x.name) > 32 || !x.name.trim()) return bad('Names can be up to 32 characters');
          seen.add(x.id);
        }
      }
      else if (['favourites', 'hidden', 'known'].includes(k)) { if (!Array.isArray(v) || v.length > 256 || v.some(x => typeof x !== 'string' || !x || x.length >= 64)) return bad('List of device ids expected'); }
      else return bad('Unknown setting');
      s[k] = k === 'tiles' ? v.map(t => ({ key: t.key, label: t.label, show: t.show })) : k === 'names' ? v.map(x => ({ id: x.id, name: x.name })) : v;
    }
    return [s];
  }
  const wait = ms => new Promise(r => setTimeout(r, ms));
  const R2 = (status, data) => ({ status, data });
  async function api(method, path, body, auth) {
    await wait(140 + Math.random() * 120);   // a little like a real Wi-Fi round trip
    if (Date.now() < S.downUntil) return { down: true };
    let j = null; if (typeof body === 'string') { try { j = JSON.parse(body); } catch (e) { j = undefined; } }
    const authed = auth && auth.startsWith('Bearer ') && S.tokens.includes(auth.slice(7));
    const c = S.cfg;
    if (method === 'GET') {
      if (path === '/info') {
        const list = ents();
        if (!authed) return R2(200, { api: 2, schema: 4, id: 'a1b2', name: c.name, firmware: S.firmware, mode: 'normal', host: 'pocketos-a1b2.local', paired: false });
        return R2(200, { api: 2, schema: 4, id: 'a1b2', name: c.name, firmware: S.firmware, mode: 'normal', paired: true, wifi: { ssid: 'HomeNet', rssi: -58, ip: '192.168.1.42' },
          host: 'pocketos-a1b2.local', battery: { percent: S.battery, charging: false, plugged: false, mv: 3500 + S.battery * 6 + 100, hours_left: Math.round(S.battery / 4) },
          ha: { status: S.ha, error: S.haError, devices: list.filter(visible).length, new: c.auto_add ? 0 : list.filter(e => !c.known.includes(e.id)).length },
          uptime_s: Math.floor((Date.now() - S.booted) / 1000), update: updInfo() });
      }
      if (path === '/wifi/scan') return R2(403, { error: 'Only available during first setup' });
      if (!authed) return R2(401, { error: 'Pair with the PIN on the device first' });
      if (path === '/config' || path === '/export') return R2(200, publicCfg());
      if (path === '/battery') return R2(200, { percent: S.battery, mv: 3500 + S.battery * 6 + 100, present: true, charging: false, full: false, plugged: false, now: Math.floor(Date.now() / 1000),
        last_full: S.hist[21 * 6][0], estimate: { known: true, pct_per_h: 4.0, hours_left: Math.round(S.battery / 4 * 10) / 10, full_h: 25.0, basis_h: 22.0 }, history: S.hist });
      if (path === '/entities') return R2(200, { areas: [...new Set(ents().map(e => e.area).filter(Boolean))].sort(), entities: ents().map(e => ({
        id: e.id, name: nm(e), ha_name: e.name, domain: DOMAIN[e.dom], area: e.area, status: status(e),
        controls: e.dom === 'light' && (e.warmth || e.colour) ? 'On, off, brightness' + (e.warmth && e.colour ? ', warmth and colour' : e.colour ? ' and colour' : ' and warmth') : CONTROLS[e.dom], fav: c.favourites.includes(e.id), hidden: c.hidden.includes(e.id),
        new: !c.auto_add && !c.known.includes(e.id) })) });
      return R2(404, { error: 'Not found' });
    }
    if (path === '/pair') {
      if (Date.now() < S.lockedUntil) return R2(429, { error: 'Too many wrong codes. Wait a minute and try again.' });
      if (!j || j.pin !== S.pin) { if (++S.bad >= 5) { S.bad = 0; S.lockedUntil = Date.now() + 60000; } return R2(401, { error: 'That code doesn’t match the one on PocketOS', field: 'pin' }); }
      S.bad = 0; const t = 'demo-' + Math.random().toString(36).slice(2, 12); S.tokens = [...S.tokens, t].slice(-4); S.pin = pin();
      if (overlay === 'qr') showOverlay('qr', qrHTML());
      return R2(200, { token: t });
    }
    if (path === '/wifi') return R2(403, { error: 'Only available during first setup' });
    if (!authed) return R2(401, { error: 'Pair with the PIN on the device first' });
    if (path === '/config') {
      if (j === undefined || j === null) return R2(400, { error: 'That wasn’t valid settings data' });
      const [s, err, field] = validate(j); if (!s) return R2(400, { error: err, ...(field ? { field } : {}) });
      S.cfg = s; changed(); return R2(200, publicCfg());
    }
    if (path === '/entities') {
      j = j || {};
      if (j.all_known === true) { c.known = [...new Set([...c.known, ...S.ents.map(e => e.id)])].sort(); changed(); return R2(200, { ok: true }); }
      if (typeof j.id !== 'string') return R2(400, { error: 'Missing device id', field: 'id' });
      if ('name' in j) {
        const e = find(j.id); if (!e) return R2(404, { error: 'That device isn’t in Home Assistant any more', field: 'id' });
        if (typeof j.name !== 'string') return R2(400, { error: 'Name must be text', field: 'name' });
        if (new TextEncoder().encode(j.name).length > 32) return R2(400, { error: 'Names can be up to 32 characters', field: 'name' });
        if (j.name && !j.name.trim()) return R2(400, { error: 'Names can’t be only spaces', field: 'name' });
        const rest = c.names.filter(n => n.id !== j.id);
        if (j.name && j.name !== e.name) { if (rest.length >= 48) return R2(400, { error: 'You can rename up to 48 devices', field: 'name' }); rest.push({ id: j.id, name: j.name }); }
        c.names = rest;
      }
      if (typeof j.move === 'number') { const f = c.favourites, a = f.indexOf(j.id), b = a + (j.move < 0 ? -1 : 1); if (a >= 0 && b >= 0 && b < f.length) [f[a], f[b]] = [f[b], f[a]]; }
      for (const [k, lst] of [['fav', 'favourites'], ['hidden', 'hidden'], ['known', 'known']]) if (typeof j[k] === 'boolean') {
        if (lst === 'favourites') { c.favourites = c.favourites.filter(x => x !== j.id); if (j[k]) c.favourites.push(j.id); continue; }   // order matters
        const set = new Set(c[lst]); j[k] ? set.add(j.id) : set.delete(j.id); c[lst] = [...set].sort();
      }
      changed(); return R2(200, { ok: true });
    }
    if (path === '/ha' || path === '/ha/test') {
      j = j || {};
      if (typeof j.url !== 'string' || !/^https?:\/\//.test(j.url)) return R2(400, { error: 'The address should look like http://homeassistant.local:8123', field: 'url' });
      const tok = j.token || c.ha_token;
      if (!tok) return R2(400, { error: 'Paste the token first.', field: 'url' });
      if (path === '/ha/test') { await wait(400); return R2(200, { ok: true, devices: S.ents.length, message: 'Home Assistant (demo home)' }); }
      c.ha_url = j.url; if (j.token) c.ha_token = j.token; S.ha = 'connecting'; changed();
      setTimeout(() => { S.ha = 'connected'; S.haError = ''; changed(); }, 1200);
      return R2(200, { ok: true });
    }
    if (path === '/ha/disconnect') { c.ha_token = ''; S.ha = 'not_set'; changed(); return R2(200, { ok: true }); }
    if (path === '/update/check') {
      if (['checking', 'downloading'].includes(S.upd.state)) return R2(409, { error: 'PocketOS is already checking or updating' });
      S.upd.state = 'checking'; S.upd.error = '';
      setTimeout(() => { S.upd.checked = Math.floor(Date.now() / 1000); S.upd.state = 'latest'; }, 1000);
      return R2(202, { ok: true });
    }
    if (path === '/update/install') return R2(400, { error: 'There’s no newer version to install' });
    if (path === '/restart') { S.downUntil = Date.now() + 3000; S.booted = Date.now() + 3000; hideOverlay(); bootUp(); return R2(202, { ok: true }); }
    if (path === '/wifi/forget') return R2(400, { error: 'Changing Wi-Fi isn’t available in the demo.' });
    if (path === '/unpair_all') { S.tokens = []; return R2(200, { ok: true }); }
    if (path === '/reset') { S.confirm = 'reset'; S.resetUntil = Date.now() + 15000; wake(); resetPrompt(true); return R2(202, { ok: true, confirm: 'Press the Boot button on PocketOS within 15 seconds' }); }
    return R2(404, { error: 'Not found' });
  }

  const frame = document.getElementById('demo-settings');
  function reloadSettings(ms) { if (frame) setTimeout(() => { try { frame.contentWindow.location.reload(); } catch (e) { frame.src = frame.src; } }, ms || 0); }
  window.PocketDemo = { api, token: () => S.tokens[0] || '' };
  try { localStorage.setItem('pocketos-token-a1b2', TOKEN); } catch (e) {}
  if (frame && !frame.getAttribute('src')) frame.src = frame.dataset.src;

  document.getElementById('demo-restart') && document.getElementById('demo-restart').addEventListener('click', () => {
    fresh(); try { localStorage.setItem('pocketos-token-a1b2', TOKEN); } catch (e) {}
    hideOverlay(); power = 'on'; root.classList.remove('off'); setAsleep(false); stack = [{ k: 'home' }]; render(true); reloadSettings();
  });

  // phones: switch between the device and the settings page
  $$('[data-pane]').forEach(b => b.addEventListener('click', () => {
    $$('[data-pane]').forEach(x => x.setAttribute('aria-selected', String(x === b)));
    document.querySelector('.demo').dataset.show = b.dataset.pane;
  }));

  render(); hint();
})();
