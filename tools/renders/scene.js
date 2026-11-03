// Concept product renders for the PocketOS website. Run through render.py (Playwright).
import * as THREE from 'three';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';

const P = new URLSearchParams(location.search);
const SHOT = P.get('shot') || 'device';
const W = +(P.get('w') || 1600), H = +(P.get('h') || 1200);

const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, preserveDrawingBuffer: true });
renderer.setPixelRatio(1);
renderer.setSize(W, H);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.0;
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.VSMShadowMap;
renderer.setClearColor(0x000000, 0);
document.body.appendChild(renderer.domElement);

const scene = new THREE.Scene();

/* ---------- studio environment: dark room, soft boxes ---------- */
function studioEnv() {
  const env = new THREE.Scene();
  env.background = new THREE.Color(0x05060c);
  const panel = (w, h, c, i, pos, look) => {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ color: new THREE.Color(c).multiplyScalar(i), side: THREE.DoubleSide }));
    m.position.set(...pos); m.lookAt(...look); env.add(m);
  };
  panel(14, 8, 0xffffff, 3.2, [0, 12, 2], [0, 0, 0]);       // overhead soft box
  panel(3, 14, 0xffffff, 4.0, [-11, 4, 4], [0, 0, 0]);       // left strip
  panel(3, 14, 0xdfe8ff, 2.6, [11, 4, -2], [0, 0, 0]);       // right strip
  panel(16, 3, 0x8fb4ff, 1.4, [0, 3, -12], [0, 0, 0]);       // blue rim behind
  panel(12, 6, 0xffffff, 2.2, [0, 5, 12], [0, 0, 0]);        // front fill
  const floor = new THREE.Mesh(new THREE.PlaneGeometry(60, 60), new THREE.MeshBasicMaterial({ color: 0x0b0f22 }));
  floor.rotation.x = -Math.PI / 2; floor.position.y = -1; env.add(floor);
  const pm = new THREE.PMREMGenerator(renderer);
  return pm.fromScene(env, 0.035).texture;
}
scene.environment = studioEnv();

const key = new THREE.DirectionalLight(0xffffff, 2.2);
key.position.set(-4, 9, 5);
key.castShadow = true;
key.shadow.mapSize.set(4096, 4096);
Object.assign(key.shadow.camera, { left: -6, right: 6, top: 6, bottom: -6, near: 1, far: 30 });
key.shadow.radius = 14; key.shadow.blurSamples = 25; key.shadow.bias = -0.0004;
scene.add(key);
const rim = new THREE.DirectionalLight(0x9db8ff, 1.2); rim.position.set(5, 4, -6); scene.add(rim);

const floor = new THREE.Mesh(new THREE.PlaneGeometry(60, 60), new THREE.ShadowMaterial({ opacity: 0.62 }));
floor.rotation.x = -Math.PI / 2; floor.receiveShadow = true; scene.add(floor);

/* contact shadow: a soft dark pool right under an object, which shadow maps can't resolve */
const blobTex = (() => {
  const c = document.createElement('canvas'); c.width = c.height = 256;
  const g = c.getContext('2d'), r = g.createRadialGradient(128, 128, 0, 128, 128, 128);
  r.addColorStop(0, 'rgba(0,0,0,0.85)'); r.addColorStop(0.55, 'rgba(0,0,0,0.35)'); r.addColorStop(1, 'rgba(0,0,0,0)');
  g.fillStyle = r; g.fillRect(0, 0, 256, 256); return new THREE.CanvasTexture(c);
})();
function contact(x, z, sx, sz, o = 0.7, rot = 0) {
  const m = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), new THREE.MeshBasicMaterial({ map: blobTex, transparent: true, opacity: o, depthWrite: false }));
  m.rotation.x = -Math.PI / 2; m.rotation.z = rot; m.scale.set(sx, sz, 1); m.position.set(x, 0.002, z); scene.add(m); return m;
}

/* ---------- textures ---------- */
const loadImg = src => new Promise((ok, bad) => { const i = new Image(); i.onload = () => ok(i); i.onerror = bad; i.src = src; });
async function svgTex(src, w, pad = 0) {
  const img = await loadImg(src);
  const ar = img.naturalHeight / img.naturalWidth || 358 / 1069;
  const c = document.createElement('canvas'); c.width = w + pad * 2; c.height = Math.round(w * ar) + pad * 2;
  c.getContext('2d').drawImage(img, pad, pad, w, Math.round(w * ar));
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8; t.userData.ar = c.height / c.width; return t;
}
// the front glass: black bezel with the UI inset, as on the device (screen = 87% of the case)
async function screenTex(src) {
  const img = await loadImg(src);
  const S = 1024, c = document.createElement('canvas'); c.width = c.height = S;
  const g = c.getContext('2d'); g.fillStyle = '#000'; g.fillRect(0, 0, S, S);
  const r = S / 2 * (0.87 / 0.93);
  g.save(); g.beginPath(); g.arc(S / 2, S / 2, r, 0, Math.PI * 2); g.clip();
  g.drawImage(img, S / 2 - r, S / 2 - r, r * 2, r * 2); g.restore();
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8; return t;
}

/* ---------- materials ---------- */
const alu = new THREE.MeshPhysicalMaterial({ color: 0xd9dce3, metalness: 1, roughness: 0.3, clearcoat: 0.25, clearcoatRoughness: 0.2 });
const aluDark = new THREE.MeshPhysicalMaterial({ color: 0xb9bdc7, metalness: 1, roughness: 0.42 });
const portMat = new THREE.MeshStandardMaterial({ color: 0x050506, roughness: 0.6 });
const boxMat = new THREE.MeshPhysicalMaterial({ color: 0x0e0f14, roughness: 0.72, metalness: 0, sheen: 0.35, sheenRoughness: 0.8, sheenColor: 0x3a3f55 });
const boxIn = new THREE.MeshStandardMaterial({ color: 0x15161b, roughness: 0.9 });
const foam = new THREE.MeshStandardMaterial({ color: 0x18191e, roughness: 1 });
const foil = new THREE.MeshPhysicalMaterial({ color: 0xeef1f6, metalness: 1, roughness: 0.34, transparent: true, alphaTest: 0.4, polygonOffset: true, polygonOffsetFactor: -4 });
const cableMat = new THREE.MeshPhysicalMaterial({ color: 0xe9ebf1, roughness: 0.55, sheen: 0.6, sheenColor: 0xffffff });
const cardMat = new THREE.MeshPhysicalMaterial({ color: 0xf2f3f6, roughness: 0.8 });

const shadowAll = o => o.traverse(m => { if (m.isMesh) { m.castShadow = true; m.receiveShadow = true; } });

/* ---------- the device: lying flat, screen facing +Y, top of the UI towards -Z ---------- */
const R = 1, TH = 0.42;
async function device(screenSrc) {
  const g = new THREE.Group();
  const pts = [];
  const arcPts = (cx, cy, r, a0, a1, n) => { for (let i = 0; i <= n; i++) { const a = a0 + (a1 - a0) * i / n; pts.push(new THREE.Vector2(cx + r * Math.cos(a), cy + r * Math.sin(a))); } };
  pts.push(new THREE.Vector2(0, 0), new THREE.Vector2(0.86, 0));
  arcPts(0.9, 0.06, 0.06, -Math.PI / 2, 0, 6);                 // soft underside edge
  pts.push(new THREE.Vector2(0.97, 0.08));
  arcPts(0.92, 0.12, 0.08, -0.3, 0, 4);
  pts.push(new THREE.Vector2(R, 0.14), new THREE.Vector2(R, TH - 0.06));
  arcPts(R - 0.06, TH - 0.06, 0.06, 0, Math.PI / 2, 10);       // polished top edge
  pts.push(new THREE.Vector2(0.935, TH), new THREE.Vector2(0.932, TH - 0.006));
  const caseMesh = new THREE.Mesh(new THREE.LatheGeometry(pts, 192), alu);
  g.add(caseMesh);
  // a darker brushed ring on the base
  const base = new THREE.Mesh(new THREE.CircleGeometry(0.86, 96), aluDark);
  base.rotation.x = Math.PI / 2; base.position.y = 0.0005; g.add(base);
  // front glass with the UI
  const tex = await screenTex(screenSrc);
  const glass = new THREE.Mesh(new THREE.CircleGeometry(0.932, 128), new THREE.MeshPhysicalMaterial({
    color: 0x000000, emissive: 0xffffff, emissiveMap: tex, emissiveIntensity: 1.25, roughness: 0.06, metalness: 0, clearcoat: 1, clearcoatRoughness: 0.02, ior: 1.5 }));
  glass.rotation.x = -Math.PI / 2; glass.rotation.z = 0; glass.position.y = TH - 0.004; g.add(glass);
  // side buttons, where the website's drawing has them (CSS angles: 0° = right, clockwise)
  for (const a of [-34, 30]) {
    const rad = a * Math.PI / 180;
    const b = new THREE.Mesh(new RoundedBoxGeometry(0.07, 0.12, 0.25, 4, 0.03), alu);
    b.position.set((R + 0.012) * Math.cos(rad), TH * 0.55, (R + 0.012) * Math.sin(rad));
    b.rotation.y = -rad; g.add(b);
  }
  // USB-C port at the bottom of the rim
  const port = new THREE.Mesh(new RoundedBoxGeometry(0.03, 0.07, 0.24, 3, 0.03), portMat);
  port.position.set(0, TH * 0.5, R - 0.008); port.rotation.y = Math.PI / 2; g.add(port);
  shadowAll(g); glass.castShadow = false;
  return g;
}

/* ---------- packaging ---------- */
const BW = 3.0, BD = 3.7, BH = 1.05, WALL = 0.05;
function roundRect(w, d, r) {
  const s = new THREE.Shape(), x = -w / 2, y = -d / 2;
  s.moveTo(x + r, y); s.lineTo(x + w - r, y); s.quadraticCurveTo(x + w, y, x + w, y + r);
  s.lineTo(x + w, y + d - r); s.quadraticCurveTo(x + w, y + d, x + w - r, y + d);
  s.lineTo(x + r, y + d); s.quadraticCurveTo(x, y + d, x, y + d - r);
  s.lineTo(x, y + r); s.quadraticCurveTo(x, y, x + r, y); return s;
}
function holePath(w, d, r, cx = 0, cy = 0) { const s = roundRect(w, d, r); const p = new THREE.Path(s.getPoints(8).map(v => new THREE.Vector2(v.x + cx, v.y + cy)).reverse()); return p; }
// extrude a 2D shape (x, y) upwards (y in 3D), so shape y maps to -z
function upExtrude(shape, h, mat, bevel = 0.012) {
  const geo = new THREE.ExtrudeGeometry(shape, { depth: h - bevel * 2, bevelEnabled: bevel > 0, bevelThickness: bevel, bevelSize: bevel, bevelSegments: 3, curveSegments: 24 });
  geo.rotateX(-Math.PI / 2); geo.translate(0, bevel, 0);
  return new THREE.Mesh(geo, mat);
}
function tray(w, d, h, outer, inner) {   // open box: walls and floor
  const g = new THREE.Group();
  const ring = roundRect(w, d, 0.06); ring.holes.push(holePath(w - WALL * 2, d - WALL * 2, 0.03));
  g.add(upExtrude(ring, h, outer));
  const fl = upExtrude(roundRect(w - 0.02, d - 0.02, 0.05), WALL, inner, 0.004); g.add(fl);
  return g;
}
// the device recess: a circle with two finger notches, as one outline
const DEV_Z = -0.42, SLOT_Z = 1.18;
function recessPath() {
  const R0 = R + 0.035, d = R0 - 0.02, rl = 0.2, pts = [];
  for (let i = 0; i < 360; i++) {
    const t = i / 360 * Math.PI * 2; let r = R0;
    for (const s of [0, Math.PI]) { const u = t - s, sn = Math.sin(u); const k = rl * rl - d * d * sn * sn;
      if (k >= 0 && Math.cos(u) > 0) r = Math.max(r, d * Math.cos(u) + Math.sqrt(k)); }
    pts.push(new THREE.Vector2(r * Math.cos(t), -DEV_Z + r * Math.sin(t)));
  }
  return new THREE.Path(pts.reverse());
}
function insert(withHoles = true) {
  const g = new THREE.Group();
  const iw = BW - WALL * 2 - 0.01, id = BD - WALL * 2 - 0.01;
  const slab = upExtrude(roundRect(iw, id, 0.03), 0.62, foam, 0.004); g.add(slab);       // lower block
  const top = roundRect(iw, id, 0.03);
  top.holes.push(recessPath());
  top.holes.push(holePath(2.3, 0.62, 0.12, 0, -SLOT_Z));
  const upper = upExtrude(top, 0.36, foam, 0.02); upper.position.y = 0.62; g.add(upper);
  return g;
}
async function lid() {
  const g = new THREE.Group();
  const lw = BW + 0.06, ld = BD + 0.06, lh = 0.98;
  const ring = roundRect(lw, ld, 0.07); ring.holes.push(holePath(lw - WALL * 2, ld - WALL * 2, 0.04));
  const walls = upExtrude(ring, lh, boxMat, 0.016); g.add(walls);
  const top = upExtrude(roundRect(lw, ld, 0.07), WALL + 0.01, boxMat, 0.016); top.position.y = lh - WALL - 0.01; g.add(top);
  const inside = new THREE.Mesh(new THREE.PlaneGeometry(lw - 0.12, ld - 0.12), boxIn); inside.rotation.x = Math.PI / 2; inside.position.y = lh - WALL - 0.012; g.add(inside);
  // foil-stamped stacked logo, centred
  const t = await svgTex('tex/pocketos-logo-white.svg', 2048, 0);
  const mw = 1.55, logo = new THREE.Mesh(new THREE.PlaneGeometry(mw, mw * t.userData.ar), foil.clone());
  logo.material.alphaMap = t; logo.material.map = null;
  logo.rotation.x = -Math.PI / 2; logo.position.set(0, lh + 0.0015, -0.05); g.add(logo);
  shadowAll(g); logo.castShadow = false;
  return g;
}
async function base(withDevice, withCable) {
  const g = new THREE.Group();
  g.add(tray(BW, BD, BH, boxMat, boxIn));
  const ins = insert(); ins.position.y = WALL; g.add(ins);
  if (withDevice) { const d = await device('tex/home.png'); d.position.set(0, WALL + 0.62, DEV_Z); g.add(d); }
  if (withCable) { const c = await sleeve(); c.position.set(0, WALL + 0.62, SLOT_Z); g.add(c); }
  shadowAll(g); return g;
}

/* ---------- USB-C cable, coiled into a flat bundle with a paper band ---------- */
async function cable(len, wid) {
  const g = new THREE.Group(), tr = 0.026, loops = 4, pts = [];
  const a = len / 2 - wid / 2, rr = wid / 2;
  // stadium loops, each slightly offset so the turns sit side by side
  for (let L = 0; L < loops; L++) {
    const k = (L - (loops - 1) / 2) * tr * 1.9, rad = rr + k * 0.4;
    for (let i = 0; i < 64; i++) {
      const t = i / 64; let x, z;
      if (t < 0.25) { const u = t / 0.25; x = -a + 2 * a * u; z = -rad; }
      else if (t < 0.5) { const u = (t - 0.25) / 0.25 * Math.PI; x = a + rad * Math.sin(u); z = -rad * Math.cos(u); }
      else if (t < 0.75) { const u = (t - 0.5) / 0.25; x = a - 2 * a * u; z = rad; }
      else { const u = (t - 0.75) / 0.25 * Math.PI; x = -a - rad * Math.sin(u); z = rad * Math.cos(u); }
      pts.push(new THREE.Vector3(x, tr + Math.abs(k) * 0.4 + (L % 2) * tr * 0.9, z + k * 0.25));
    }
  }
  const top = tr * 2 * 2.4 + 0.02, y2 = top + tr * 0.2;
  // start and finish the coil under the band, so both leads come out from beneath it
  let i0 = 0, best = 1e9;
  for (let i = 0; i < 64; i++) { const q = pts[i], dd = (q.x - 0.1) ** 2 + (q.z + rr) ** 2; if (dd < best) { best = dd; i0 = i; } }
  const coil = pts.slice(i0, pts.length - (64 - i0) + 1);
  const f = coil[0], l = coil[coil.length - 1];
  const curve = new THREE.CatmullRomCurve3([
    new THREE.Vector3(0.36, y2, -0.065), new THREE.Vector3(0.16, y2, -0.065), new THREE.Vector3(0.1, y2 - tr, -rr * 0.5), new THREE.Vector3(0.04, f.y, f.z), 
    ...coil,
    new THREE.Vector3(l.x + 0.06, l.y + tr, l.z * 0.5), new THREE.Vector3(0.16, y2, 0.065), new THREE.Vector3(0.36, y2, 0.065)], false, 'catmullrom', 0.3);
  g.add(new THREE.Mesh(new THREE.TubeGeometry(curve, 1000, tr, 12, false), cableMat));
  const plug = (z) => {
    const pg = new THREE.Group();
    const hous = new THREE.Mesh(new RoundedBoxGeometry(0.26, 0.07, 0.1, 4, 0.03), cableMat); pg.add(hous);
    const metal = new THREE.Mesh(new RoundedBoxGeometry(0.11, 0.032, 0.08, 3, 0.015), alu); metal.position.x = 0.17; pg.add(metal);
    pg.position.set(0.47, y2, z); g.add(pg);
  };
  plug(-0.065); plug(0.065);
  // paper band
  const band = new THREE.Mesh(new RoundedBoxGeometry(0.34, tr * 2 * 2.4 + 0.02, wid + tr * 4 + 0.06, 3, 0.012), boxMat);
  band.position.set(0.1, tr * 2.3, 0); g.add(band);
  const t = await svgTex('tex/pocketos-mark-white.svg', 512, 0);
  const m = new THREE.Mesh(new THREE.PlaneGeometry(0.16, 0.16 * t.userData.ar), foil.clone()); m.material.alphaMap = t;
  m.rotation.x = -Math.PI / 2; m.position.set(0.1, tr * 2.3 + (tr * 2 * 2.4 + 0.02) / 2 + 0.001, 0); g.add(m);
  shadowAll(g); m.castShadow = false;
  return g;
}

/* a round coil, as a cable comes out of the box: flat spiral, an inner lead crossing under a tie */
async function coil(r0 = 0.36, turns = 5) {
  const g = new THREE.Group(), tr = 0.026, step = tr * 2.08, pts = [];
  const tie = 0;                                   // the tie sits at angle 0 (towards +x)
  // inner lead: from the plug, over the coil under the tie, into the centre
  const rOut = r0 + turns * step;
  const yTop = tr * 2.6;
  pts.push(new THREE.Vector3(rOut + 0.32, tr, 0.07), new THREE.Vector3(rOut + 0.12, tr, 0.07), new THREE.Vector3(rOut - 0.02, yTop, 0.06),
           new THREE.Vector3((r0 + rOut) / 2, yTop + 0.01, 0.05), new THREE.Vector3(r0 + 0.02, yTop * 0.7, 0.04), new THREE.Vector3(r0 - 0.06, tr * 1.2, 0.0));
  const N = turns * 96;
  for (let i = 0; i <= N; i++) {
    const t = -i / 96 * Math.PI * 2 - 0.25, r = r0 + (i / 96) * step;
    pts.push(new THREE.Vector3(r * Math.cos(t), tr, r * Math.sin(t)));
  }
  const last = pts[pts.length - 1];
  pts.push(new THREE.Vector3(rOut + 0.12, tr, -0.07), new THREE.Vector3(rOut + 0.32, tr, -0.07));
  const curve = new THREE.CatmullRomCurve3(pts, false, 'catmullrom', 0.4);
  g.add(new THREE.Mesh(new THREE.TubeGeometry(curve, 2400, tr, 12, false), cableMat));
  for (const z of [0.07, -0.07]) {
    const pg = new THREE.Group();
    pg.add(new THREE.Mesh(new RoundedBoxGeometry(0.26, 0.07, 0.1, 4, 0.03), cableMat));
    const metal = new THREE.Mesh(new RoundedBoxGeometry(0.11, 0.032, 0.08, 3, 0.015), alu); metal.position.x = 0.17; pg.add(metal);
    pg.position.set(rOut + 0.45, 0.038, z); g.add(pg);
  }
  const band = new THREE.Mesh(new RoundedBoxGeometry(rOut - r0 + 0.18, yTop + tr + 0.02, 0.2, 3, 0.012), boxMat);
  band.position.set((r0 + rOut) / 2, (yTop + tr + 0.02) / 2, 0.02); g.add(band);
  const t = await svgTex('tex/pocketos-mark-white.svg', 512, 0);
  const m = new THREE.Mesh(new THREE.PlaneGeometry(0.12, 0.12 * t.userData.ar), foil.clone()); m.material.alphaMap = t;
  m.rotation.x = -Math.PI / 2; m.position.set((r0 + rOut) / 2, yTop + tr + 0.021, 0.02); g.add(m);
  shadowAll(g); m.castShadow = false;
  return g;
}
/* the accessory compartment holds a closed sleeve, so nothing unconfirmed is shown inside it */
async function sleeve() {
  const g = new THREE.Group(), w = 2.18, d = 0.5, h = 0.2;
  const b = new THREE.Mesh(new RoundedBoxGeometry(w, h, d, 3, 0.02), boxMat); b.position.y = h / 2; g.add(b);
  const tab = new THREE.Mesh(new RoundedBoxGeometry(0.34, 0.012, 0.16, 2, 0.005), boxIn); tab.position.set(0, h + 0.004, -d / 2 + 0.02); g.add(tab);
  const t = await svgTex('tex/pocketos-logo-horizontal-white.svg', 1024, 0);
  const m = new THREE.Mesh(new THREE.PlaneGeometry(0.62, 0.62 * t.userData.ar), foil.clone()); m.material.alphaMap = t;
  m.rotation.x = -Math.PI / 2; m.position.set(0, h + 0.0015, 0.04); g.add(m);
  shadowAll(g); m.castShadow = false; return g;
}

/* ---------- quick start card ---------- */
async function card() {
  const g = new THREE.Group(), w = 1.45, d = 1.0;
  const c = document.createElement('canvas'); c.width = 1450; c.height = 1000;
  const x = c.getContext('2d'); x.fillStyle = '#f3f4f7'; x.fillRect(0, 0, 1450, 1000);
  const logo = await loadImg('tex/pocketos-logo-horizontal-white.svg');
  // draw the logo dark by tinting the white artwork
  const tmp = document.createElement('canvas'); tmp.width = 700; tmp.height = Math.round(700 * logo.naturalHeight / logo.naturalWidth);
  const tx = tmp.getContext('2d'); tx.drawImage(logo, 0, 0, tmp.width, tmp.height); tx.globalCompositeOperation = 'source-in'; tx.fillStyle = '#0d1124'; tx.fillRect(0, 0, tmp.width, tmp.height);
  x.drawImage(tmp, 110, 120);
  x.fillStyle = '#0d1124'; x.font = '600 96px sans-serif'; x.fillText('Quick start', 110, 440);
  x.fillStyle = '#4a5068'; x.font = '400 54px sans-serif';
  ['1  Charge and switch on', '2  Scan the code on screen', '3  Connect Home Assistant'].forEach((s, i) => x.fillText(s, 110, 590 + i * 100));
    const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8;
  const body = new THREE.Mesh(new RoundedBoxGeometry(w, 0.012, d, 2, 0.005), cardMat); body.position.y = 0.006; g.add(body);
  const face = new THREE.Mesh(new THREE.PlaneGeometry(w - 0.002, d - 0.002), new THREE.MeshPhysicalMaterial({ map: t, roughness: 0.75 }));
  face.rotation.x = -Math.PI / 2; face.position.y = 0.0125; g.add(face);
  shadowAll(g); return g;
}

/* ---------- shots ---------- */
const camera = new THREE.PerspectiveCamera(30, W / H, 0.1, 100);
function look(pos, target, fov = 30) { camera.fov = fov; camera.position.set(...pos); camera.lookAt(...target); camera.updateProjectionMatrix(); }

async function build() {
  switch (SHOT) {
    case 'device': {   // on its own, standing, facing the camera
      const d = await device('tex/home.png');
      d.rotation.x = Math.PI / 2; d.position.set(0, R + 0.0, -TH / 2);
      const s = new THREE.Group(); s.add(d); s.rotation.y = -0.32; scene.add(s);
      contact(0, 0, 1.4, 0.55, 0.75, 0.32);
      key.position.set(-3, 8, 7);
      look([0.85, 1.45, 6.4], [0.05, 1.0, 0], 26); break;
    }
    case 'angle': {    // lying flat, three-quarter view showing the rim and buttons
      const d = await device('tex/menu.png'); d.rotation.y = -0.5; scene.add(d);
      contact(0, 0, 2.5, 2.5, 0.8);
      look([-2.6, 3.4, 4.6], [0.15, 0.15, 0.1], 30); break;
    }
    case 'box': {      // closed box with the device beside it
      const b = await base(false, false), l = await lid();
      l.position.y = BH - 0.98 + 0.12; const box = new THREE.Group(); box.add(b, l); box.rotation.y = 0.42; box.position.set(-0.9, 0, -0.3); scene.add(box);
      contact(-0.9, -0.3, 4.4, 5.0, 0.6, 0.42);
      const d = await device('tex/home.png'); d.rotation.x = Math.PI / 2 - 0.12; d.position.set(0, R * Math.cos(0.12), -TH / 2);
      const s = new THREE.Group(); s.add(d); s.rotation.y = -0.5; s.position.set(1.85, 0, 1.25); scene.add(s);
      contact(1.85, 1.25, 1.4, 0.6, 0.75, 0.5);
      look([2.0, 4.6, 8.4], [0.3, 0.7, 0.1], 30); break;
    }
    case 'open': {     // lid off and leaning behind, device and cable in their places
      const b = await base(true, true); scene.add(b);
      contact(0, 0, 4.0, 4.8, 0.55);
      const l = await lid(); const lg = new THREE.Group(); l.position.z = -(BD + 0.06) / 2; l.position.y = -0.98;
      lg.add(l); lg.rotation.x = Math.PI / 2 - 0.18; lg.position.set(0.35, 0, -BD / 2 - 1.15); lg.rotation.z = 0.0; scene.add(lg);
      // lid sits on its edge: rotate it so the top faces the camera
      look([2.6, 6.6, 6.9], [0.1, 0.6, -0.55], 32); break;
    }
    case 'inside': {   // the empty insert: device cradle and accessory compartment
      const b = await base(false, false); scene.add(b);
      contact(0, 0, 4.0, 4.8, 0.55);
      look([1.4, 9.2, 4.3], [0, 0.5, -0.05], 32); break;
    }
    case 'contents': { // flat lay of everything in the box
      const d = await device('tex/home.png'); d.position.set(-1.25, 0, -0.35); scene.add(d); contact(-1.25, -0.35, 2.4, 2.4, 0.7);
      const c = await coil(); c.rotation.y = Math.PI / 2; c.position.set(1.35, 0, -0.55); scene.add(c); contact(1.35, -0.45, 1.6, 1.9, 0.5);
      const k = await card(); k.rotation.y = 0.12; k.position.set(-0.15, 0, 1.55); scene.add(k); contact(0.0, 1.6, 1.7, 1.2, 0.35);
      key.position.set(-2, 10, 3);
      look([0, 9.0, 2.0], [0, 0, 0.45], 30); break;
    }
    case 'size': {     // front and side views, for the size drawing (marks for the labels go to render.py)
      const front = await device('tex/home.png'); front.scale.y = 15 / 11.55;          // the case is about 15 mm deep
      front.rotation.x = Math.PI / 2; front.position.set(-1.25, R, -TH / 2); scene.add(front);
      const side = await device('tex/home.png'); side.scale.y = 15 / 11.55;
      side.rotation.x = Math.PI / 2; side.position.set(0, R, -TH * 15 / 11.55 / 2);
      const sg = new THREE.Group(); sg.add(side); sg.rotation.y = -Math.PI / 2; sg.position.set(1.55, 0, 0); scene.add(sg);
      contact(-1.25, 0, 1.6, 0.45, 0.6); contact(1.55, 0, 0.6, 0.45, 0.6);
      key.position.set(-2, 7, 9);
      look([0.0, 1.05, 16], [0.0, 0.85, 0], 15);
      const pt = (x, y, z) => { const v = new THREE.Vector3(x, y, z).project(camera); return [(v.x + 1) / 2 * W, (1 - v.y) / 2 * H]; };
      const d = TH * 15 / 11.55;
      window.__marks = { w: [pt(-1.25 - R, 0, 0), pt(-1.25 + R, 0, 0)], d: [pt(1.55 - d / 2, 0, 0), pt(1.55 + d / 2, 0, 0)], top: pt(0, 2 * R, 0)[1] };
      break;
    }
    case 'scale': {    // beside a phone, for scale (a plain, unbranded phone)
      const phone = new THREE.Group();
      const body = new THREE.Mesh(new RoundedBoxGeometry(2.6, 0.29, 5.35, 6, 0.36), new THREE.MeshPhysicalMaterial({ color: 0x2a2d35, metalness: 0.8, roughness: 0.35 }));
      body.position.y = 0.145; phone.add(body);
      const wall = (() => { const c = document.createElement('canvas'); c.width = 256; c.height = 512; const g = c.getContext('2d');
        const gr = g.createLinearGradient(0, 0, 256, 512); gr.addColorStop(0, '#1d2a5a'); gr.addColorStop(0.55, '#3a2a6e'); gr.addColorStop(1, '#0d1430');
        g.fillStyle = gr; g.fillRect(0, 0, 256, 512); const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t; })();
      const scr = new THREE.Mesh(new RoundedBoxGeometry(2.48, 0.01, 5.23, 6, 0.3), new THREE.MeshPhysicalMaterial({ color: 0x000000, emissive: 0xffffff, emissiveMap: wall, emissiveIntensity: 0.55, roughness: 0.05, clearcoat: 1, clearcoatRoughness: 0.02 }));
      scr.position.y = 0.292; phone.add(scr);
      shadowAll(phone); phone.rotation.y = 0.18; phone.position.set(-1.35, 0, 0.1); scene.add(phone);
      contact(-1.35, 0.1, 3.4, 6.2, 0.55, 0.18);
      const d = await device('tex/home.png'); d.rotation.y = -0.3; d.position.set(1.6, 0, 0.35); scene.add(d);
      contact(1.6, 0.35, 2.4, 2.4, 0.75);
      key.position.set(-3, 10, 4);
      look([0.3, 9.6, 5.2], [0.1, 0, 0.25], 34); break;
    }
    case 'home': {     // on a wooden bedside table, at night
      floor.visible = false;
      const wood = (() => {
        const c = document.createElement('canvas'); c.width = 2048; c.height = 1024; const g = c.getContext('2d');
        g.fillStyle = '#5a3a24'; g.fillRect(0, 0, 2048, 1024);
        let seed = 7; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
        for (let i = 0; i < 260; i++) {
          const y = rnd() * 1024, w = 0.6 + rnd() * 2.6, a = 0.05 + rnd() * 0.12;
          g.strokeStyle = rnd() > 0.5 ? `rgba(30,16,8,${a})` : `rgba(150,100,60,${a})`; g.lineWidth = w; g.beginPath();
          for (let x = 0; x <= 2048; x += 32) g.lineTo(x, y + Math.sin(x / 260 + i) * 6 + Math.sin(x / 47 + i * 3) * 1.5);
          g.stroke();
        }
        const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8; t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(2, 2); return t;
      })();
      const table = new THREE.Mesh(new THREE.PlaneGeometry(30, 30), new THREE.MeshPhysicalMaterial({ map: wood, roughness: 0.55, clearcoat: 0.35, clearcoatRoughness: 0.4 }));
      table.rotation.x = -Math.PI / 2; table.receiveShadow = true; scene.add(table);
      // a mug and a book beside it
      const mugPts = []; for (let i = 0; i <= 12; i++) mugPts.push(new THREE.Vector2(0.62 + (i === 0 ? -0.62 : 0) - (i > 10 ? (i - 10) * 0.02 : 0), i * 0.13));
      const mugMat = new THREE.MeshPhysicalMaterial({ color: 0xe8e2d6, roughness: 0.3, clearcoat: 0.6 });
      const mug = new THREE.Mesh(new THREE.LatheGeometry([new THREE.Vector2(0, 0), new THREE.Vector2(0.62, 0), new THREE.Vector2(0.64, 1.5), new THREE.Vector2(0.56, 1.5), new THREE.Vector2(0.54, 0.1), new THREE.Vector2(0, 0.1)], 64), mugMat);
      const handle = new THREE.Mesh(new THREE.TorusGeometry(0.3, 0.07, 16, 32, Math.PI * 1.2), mugMat); handle.position.set(0.66, 0.78, 0); handle.rotation.z = -Math.PI * 0.6;
      const mg = new THREE.Group(); mg.add(mug, handle); shadowAll(mg); mg.position.set(-2.5, 0, -1.9); mg.rotation.y = 0.9; scene.add(mg);
      const book = new THREE.Mesh(new RoundedBoxGeometry(2.6, 0.32, 3.6, 2, 0.03), new THREE.MeshPhysicalMaterial({ color: 0x2b3a5c, roughness: 0.7 }));
      book.position.set(2.7, 0.16, -1.2); book.rotation.y = -0.35; shadowAll(book); scene.add(book);
      const d = await device('tex/home.png');
      d.rotation.x = Math.PI / 2 - 0.12; d.position.set(0, R * Math.cos(0.12), -TH / 2);
      const s = new THREE.Group(); s.add(d); s.rotation.y = -0.22; s.position.set(0.1, 0, 0.6); scene.add(s);
      contact(0.1, 0.6, 1.5, 0.6, 0.8, 0.22);
      key.color.set(0xffe2c0); key.intensity = 1.6; key.position.set(-5, 7, 3);
      rim.intensity = 1.5;
      renderer.toneMappingExposure = 0.85;
      look([0.6, 2.2, 7.4], [0.1, 0.95, 0], 30); break;
    }
  }
}
await build();
renderer.render(scene, camera);
window.__done = true;
