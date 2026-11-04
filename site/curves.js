// Tonkurven (*.CV) im Browser, 1:1 nach tools/filmsim/make_curves.py, plus
// Weissabgleich-Verschiebung. Ohne Verschiebung entstehen dieselben Bytes wie
// mit Python (CI-Test: tools/site/test_curves.mjs). Laeuft auch unter Node.
//
// p = Kurvenparameter der Kamera aus site.json: { black, white, gamma, wb: [R, G, G, B] }
// r = Rezept aus make_curves.RECIPES (contrast, lift, shoulder, shadow, highlight, gain, points)

// Faktor je Stufe der WB-Verschiebung (R/B -9..+9). Vorlaeufig, noch nicht
// gegen Referenzaufnahmen kalibriert.
export const WB_STEP = 1.03;
export const WB_MAX = 9;

// Python round(): bei .5 zur geraden Zahl
function pyRound(x) {
  const f = Math.floor(x), d = x - f;
  if (d > 0.5) return f + 1;
  if (d < 0.5) return f;
  return f % 2 === 0 ? f : f + 1;
}

function pchip(pts) {
  const xs = [0.0, ...pts.map((p) => p[0]), 1.0];
  const ys = [0.0, ...pts.map((p) => p[1]), 1.0];
  const n = xs.length;
  const h = [], d = [];
  for (let i = 0; i < n - 1; i++) h.push(xs[i + 1] - xs[i]);
  for (let i = 0; i < n - 1; i++) d.push((ys[i + 1] - ys[i]) / h[i]);
  const m = new Array(n).fill(0.0);
  for (let i = 1; i < n - 1; i++) {
    if (d[i - 1] * d[i] > 0) {
      const w1 = 2 * h[i] + h[i - 1], w2 = h[i] + 2 * h[i - 1];
      m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i]);
    }
  }
  const edge = (h0, h1, d0, d1) => {
    const e = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if (e * d0 <= 0) return 0.0;
    if (d0 * d1 <= 0 && Math.abs(e) > Math.abs(3 * d0)) return 3 * d0;
    return e;
  };
  m[0] = edge(h[0], h[1], d[0], d[1]);
  m[n - 1] = edge(h[n - 2], h[n - 3], d[n - 2], d[n - 3]);
  return (t) => {
    t = Math.min(Math.max(t, 0.0), 1.0);
    let i = 0;
    while (i < n - 2 && t > xs[i + 1]) i++;
    const s = (t - xs[i]) / h[i];
    const h00 = (1 + 2 * s) * (1 - s) ** 2, h10 = s * (1 - s) ** 2;
    const h01 = s * s * (3 - 2 * s), h11 = s * s * (s - 1);
    return h00 * ys[i] + h10 * h[i] * m[i] + h01 * ys[i + 1] + h11 * h[i] * m[i + 1];
  };
}

function tone(t, r) {
  const k = r.contrast ?? 0.0;
  let y = t + k * (t - 0.5) * t * (1 - t) * 4;
  const s = r.shoulder ?? 0.0;
  if (s) y = y - s * (y ** 3) * (1 - y) * 2;
  const lift = r.lift ?? 0.0;
  return lift + (1 - lift) * y;
}

function drGain(lin, G) {
  if (G <= 1) return lin;
  return lin * G / (1 + (G - 1) * lin);
}

// Kanal-Faktoren (R, G, G, B) der Verschiebung, nie ueber 1: angehoben wird
// relativ, indem die anderen Kanaele sinken, damit keine Lichter frueher ausfressen
export function wbGains([r, b] = [0, 0]) {
  const kr = WB_STEP ** r, kb = WB_STEP ** b;
  const top = Math.max(kr, 1, kb);
  return [kr / top, 1 / top, 1 / top, kb / top];
}

function channelCurve(p, ch, r, G, k) {
  const rgb = [0, 1, 1, 2][ch];
  const g = p.wb[ch];
  const sh = (r.shadow ?? [0, 0, 0])[rgb];
  const hi = (r.highlight ?? [0, 0, 0])[rgb];
  const gain = (r.gain ?? [1, 1, 1])[rgb];
  const look = r.points ? pchip(r.points["RGB"[rgb]]) : null;
  const span = p.white - p.black;
  const out = new Array(1024);
  for (let v = 0; v < 1024; v++) {
    if (v <= p.black) { out[v] = v; continue; }
    const x = (v - p.black) / span;
    const lin = x * g * gain;
    if (lin >= 1.0) { out[v] = v; continue; }
    const t = drGain(lin, G) ** (1 / p.gamma);
    let y = look ? look(t) : tone(t, r);
    y += sh * 6.75 * t * (1 - t) ** 2;
    y += hi * 6.75 * t * t * (1 - t);
    y = Math.min(Math.max(y, 0.0), 1.0);
    const ylin = (y ** p.gamma) / (g * gain);
    const o = p.black + ylin * span;
    out[v] = pyRound(Math.min(Math.max(o, 0), p.white));
  }
  // Verschiebung ueber den ganzen Bereich, auch oberhalb des Clipping-Punkts,
  // sonst gaebe es dort einen Sprung
  if (k !== 1) {
    for (let v = p.black + 1; v < 1024; v++) out[v] = pyRound(p.black + (out[v] - p.black) * k);
  }
  for (let i = 1; i < 1024; i++) if (out[i] < out[i - 1]) out[i] = out[i - 1];
  return out;
}

// Eine .CV-Datei: 4 x 1024 unsigned short little-endian (R, G, G, B)
export function makeCurve(p, recipe, G = 1, shift = [0, 0]) {
  const k = wbGains(shift);
  const bytes = new Uint8Array(8192);
  const dv = new DataView(bytes.buffer);
  for (let ch = 0; ch < 4; ch++) {
    const c = channelCurve(p, ch, recipe, G, k[ch]);
    if (ch === 0 && c[0] > 1023) throw new Error("curve0[0] > 1023");
    c.forEach((v, i) => dv.setUint16((ch * 1024 + i) * 2, v, true));
  }
  return bytes;
}

// Kurve + Dynamik+-Varianten wie make_curves.py: <stem>.CV, <stem>1.CV (-1 EV), <stem>2.CV (-2 EV)
export function curveFiles(p, stem, recipe, shift) {
  return [["", 1], ["1", 2.0], ["2", 4.0]].map(([s, G]) =>
    ({ name: `CHDK/CURVES/${stem}${s}.CV`, bytes: makeCurve(p, recipe, G, shift) }));
}

// Vorschau wie build_site.py curve_preview(): R/G/B im Anzeige-Raum
export function tonePreview(p, cv, steps = 32) {
  const dv = new DataView(cv.buffer, cv.byteOffset, cv.byteLength);
  const out = {};
  for (const [key, ch] of [["r", 0], ["g", 1], ["b", 3]]) {
    const pts = [];
    for (let i = 0; i <= steps; i++) {
      const x = i / steps;
      const raw = Math.min(p.white, pyRound(p.black + x ** p.gamma * (p.white - p.black) / p.wb[ch]));
      const v = dv.getUint16((ch * 1024 + raw) * 2, true);
      const lin = Math.min(1.0, Math.max(0.0, (v - p.black) * p.wb[ch] / (p.white - p.black)));
      pts.push(Math.round(lin ** (1 / p.gamma) * 1000) / 1000);
    }
    out[key] = pts;
  }
  return out;
}
