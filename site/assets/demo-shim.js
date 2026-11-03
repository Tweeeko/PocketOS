// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Loaded into the demo copy of the settings page (demo-settings.html, made by site/build.sh).
// The page is the device's own, unchanged; this routes its requests to the demo PocketOS on the
// parent page instead of a real one, so everything it shows and saves is pretend.
(function () {
  'use strict';
  var D = null;
  try { D = window.parent && window.parent !== window && window.parent.PocketDemo; } catch (e) {}
  if (!D) { document.addEventListener('DOMContentLoaded', function () { document.body.innerHTML = '<p style="font:16px system-ui;color:#ccc;padding:24px">This is part of the PocketOS demo. <a style="color:#7fe3ff" href="demo.html">Open the demo</a>.</p>'; }); return; }
  var real = window.fetch.bind(window);
  window.fetch = function (url, opt) {
    opt = opt || {};
    var u = String(url);
    if (u.indexOf('/api/v2') !== 0) return real(url, opt);
    var h = opt.headers || {}, auth = h.Authorization || h.authorization || '';
    return D.api(opt.method || 'GET', u.slice(7).split('?')[0], opt.body, auth).then(function (r) {
      if (opt.signal && opt.signal.aborted) throw new DOMException('Aborted', 'AbortError');
      if (r.down) throw new TypeError('Failed to fetch');
      return new Response(JSON.stringify(r.data), { status: r.status, headers: { 'Content-Type': 'application/json' } });
    });
  };
  // Installing from a file needs a real device
  var open = XMLHttpRequest.prototype.open;
  XMLHttpRequest.prototype.open = function (m, u) {
    if (String(u).indexOf('/api/v2') === 0) arguments[1] = '/demo-unavailable';
    return open.apply(this, arguments);
  };
})();
