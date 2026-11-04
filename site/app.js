import { assemble } from "./zipjoin.js";
import { curveFiles, makeCurve, tonePreview, wbGains, WB_MAX } from "./curves.js";

const $ = (id) => document.getElementById(id);
const el = (tag, props = {}, ...kids) => {
  const e = Object.assign(document.createElement(tag), props);
  e.append(...kids);
  return e;
};
const SVG_NS = "http://www.w3.org/2000/svg";
const svg = (tag, attrs = {}) => {
  const e = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  return e;
};
const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;

const state = { site: null, config: null, camera: null, firmwareOk: true, selected: new Set(), knownLooks: new Set(),
  wb: new Map() };

// Spalten von FILMS.TXT (siehe CHDK/FILMS/EXAMPLE.TXT)
const MODES = { 0: "Canon colours", 1: "Vivid", 2: "Neutral", 3: "Black & white", 4: "Sepia", 5: "Positive film", 11: "Custom colour" };
const SLIDERS = ["Contrast", "Sharpness", "Saturation", "Red", "Green", "Blue", "Skin tone"];

async function loadJson(url) {
  const r = await fetch(url, { cache: "no-cache" });
  if (!r.ok) throw new Error(`${url}: HTTP ${r.status}`);
  return r.json();
}

async function loadBytes(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`${url}: HTTP ${r.status}`);
  return new Uint8Array(await r.arrayBuffer());
}

const labelSrc = (look) => `data/${state.camera.id}/looks/${look.id}.png`;
const mb = (bytes) => `${(bytes / 1e6).toFixed(1)} MB`;

// --- Hero: Filmauswahl wie auf dem Kameradisplay -------------------------

function renderHero() {
  const looks = state.camera.looks;
  $("facts").textContent = `${looks.length} looks · free · open source · ${state.site.version}`;

  const box = $("cam-labels");
  const imgs = looks.map((look) => el("img", { src: labelSrc(look), alt: "", width: 600, height: 440 }));
  box.append(...imgs);
  let i = 0;
  const show = (n) => {
    i = (n + looks.length) % looks.length;
    imgs.forEach((img, k) => img.classList.toggle("on", k === i));
    $("cam-name").textContent = looks[i].name;
    $("cam-count").textContent = `${i + 1}/${looks.length}`;
    $("cam-screen").style.setProperty("--accent", looks[i].accent);
  };
  show(0);

  let timer = null;
  const start = () => { if (!reducedMotion && !timer) timer = setInterval(() => show(i + 1), 4500); };
  const stop = () => { clearInterval(timer); timer = null; };
  $("cam-print").addEventListener("click", () => { stop(); show(i + 1); start(); });
  $("cam-screen").addEventListener("click", () => { stop(); show(i + 1); start(); });
  document.addEventListener("visibilitychange", () => (document.hidden ? stop() : start()));
  start();
}

// --- Schritt 1: Kamera ---------------------------------------------------

const CAM_KEY = "exccd-camera";

// Vorauswahl: ?camera=<id> (zum Verlinken), sonst die zuletzt gewaehlte, sonst die erste (stabile)
function initialCamera() {
  const cams = state.site.cameras;
  let want = new URLSearchParams(location.search).get("camera");
  try { want ??= localStorage.getItem(CAM_KEY); } catch { /* ohne Speicher */ }
  return cams.find((c) => c.id === want) ?? cams[0];
}

function renderCameras() {
  const box = $("cams");
  for (const cam of state.site.cameras) {
    const input = el("input", { type: "radio", name: "camera", value: cam.id, checked: cam === state.camera });
    input.addEventListener("change", () => setCamera(cam));
    const beta = cam.status !== "stabil";
    box.append(el("label", { className: "cam-opt" }, input,
      el("span", { className: "cam-opt-box" },
        el("span", { className: "cam-opt-name", textContent: cam.name.replace(/^Canon /, "") }),
        el("span", { className: "cam-opt-meta" },
          `Firmware ${cam.firmware}`,
          ...(beta ? [el("span", { className: "badge", textContent: "Beta" })] : [])))));
  }
}

function setCamera(cam) {
  state.camera = cam;
  try { localStorage.setItem(CAM_KEY, cam.id); } catch { /* ohne Speicher */ }
  // Auswahl und WB-Verschiebung gelten je Look-ID weiter; neue Looks sind angewaehlt
  const known = new Set(state.knownLooks);
  for (const l of cam.looks) if (!known.has(l.id)) { state.selected.add(l.id); state.knownLooks.add(l.id); }
  renderCamera();
  renderLooks();
  update();
}

function renderCamera() {
  const cam = state.camera;
  $("beta-note").hidden = cam.status === "stabil";
  $("vers-link").href = `data/${cam.id}/vers.req`;
  $("fw-example").textContent = `Firmware Ver GM${cam.firmware.toUpperCase()}`;
  $("fw-note").textContent = "";
  state.firmwareOk = true;
  renderInstall();

  const fw = $("fw");
  fw.replaceChildren(fw.querySelector("legend"));
  const options = [[cam.firmware, true], ...cam.other_firmware.map((v) => [v, false]), ["Other", false]];
  for (const [v, ok] of options) {
    const input = el("input", { type: "radio", name: "fw", value: v, checked: ok });
    input.addEventListener("change", () => {
      state.firmwareOk = ok;
      $("fw-note").textContent = ok
        ? ""
        : v === "Other"
          ? `EXCCD needs firmware ${cam.firmware}. Other versions won't work.`
          : `Firmware ${v} isn't supported yet. Maybe later; for now EXCCD needs ${cam.firmware}.`;
      update();
    });
    fw.append(el("label", { className: "seg-opt" }, input, el("span", { textContent: v })));
  }
}

// Installationshinweise je Kamera (Boot-Regeln wie tools/installer/core.c)
function renderInstall() {
  const cam = state.camera;
  $("boot-note").textContent = cam.boot === "fat16"
    ? `The ${cam.short} starts EXCCD automatically only from cards up to 2 GB.`
    : `The ${cam.short} starts EXCCD automatically from cards up to 32 GB (FAT32); larger exFAT cards don't work.`;
  $("manual-firm-update").hidden = !cam.firm_update;
  $("manual-bootable").hidden = cam.firm_update;
}

// --- Schritt 2: Looks ----------------------------------------------------

function toneGraph(look, tone = look.tone) {
  const g = svg("svg", { viewBox: "0 0 100 100", class: "tone", role: "img",
    "aria-label": tone ? `Tone curve of ${look.name}` : `${look.name} uses no tone curve` });
  for (const p of [25, 50, 75]) {
    g.append(svg("line", { x1: p, y1: 0, x2: p, y2: 100, class: "grid" }), svg("line", { x1: 0, y1: p, x2: 100, y2: p, class: "grid" }));
  }
  g.append(svg("line", { x1: 0, y1: 100, x2: 100, y2: 0, class: tone ? "diag" : "diag solo" }));
  if (tone) {
    const mono = Number(look.cols[1]) === 3;
    for (const ch of mono ? ["g"] : ["r", "g", "b"]) {
      const pts = tone[ch];
      const d = pts.map((y, k) => `${((k / (pts.length - 1)) * 100).toFixed(1)},${(100 - y * 100).toFixed(1)}`).join(" ");
      g.append(svg("polyline", { points: d, class: `ch ${mono ? "mono" : ch}` }));
    }
  }
  return g;
}

function chips(look) {
  const mode = Number(look.cols[1]);
  const out = [el("span", { className: "chip mode", textContent: MODES[mode] ?? `Mode ${mode}` })];
  if (mode === 11) {
    SLIDERS.forEach((name, k) => {
      const v = Number(look.cols[2 + k]);
      if (v) out.push(el("span", { className: "chip", textContent: `${name} ${v > 0 ? "+" : "−"}${Math.abs(v)}` }));
    });
  }
  if (!look.tone) out.push(el("span", { className: "chip no-curve", textContent: "No curve" }));
  return el("div", { className: "chips" }, ...out);
}

// --- WB-Verschiebung je Look (R/B), steckt in der Kurve -----------------

// S/W-Looks nicht: dort wirkte die Verschiebung wie ein Farbfilter
const canShift = (look) => look.recipe != null && Number(look.cols[1]) !== 3;
const shiftOf = (look) => state.wb.get(look.id) ?? [0, 0];
const isShifted = ([r, b]) => r !== 0 || b !== 0;
const signed = (v) => (v > 0 ? `+${v}` : v < 0 ? `−${-v}` : "0");
const shiftText = ([r, b]) => `R${signed(r)} B${signed(b)}`;
// Looks ohne Kurve (Daylight) bekommen eine eigene, max. 7 Zeichen
const curveStem = (look) => (look.curve ? look.curve.replace(/\.CV$/i, "") : look.id.slice(0, 7));

// Vorschau auf den Beispielfotos: dieselben Kanal-Faktoren, linear wie im RAW
function wbFilter(look) {
  const id = `wb-${look.id}`;
  let f = document.getElementById(id);
  if (!f) {
    f = svg("filter", { id, "color-interpolation-filters": "linearRGB" });
    f.append(svg("feColorMatrix", { type: "matrix" }));
    $("wb-defs").append(f);
  }
  const [r, g, , b] = wbGains(shiftOf(look));
  f.firstChild.setAttribute("values", `${r} 0 0 0 0  0 ${g} 0 0 0  0 0 ${b} 0 0  0 0 0 1 0`);
  return `url(#${id})`;
}

function applyShift(look, card) {
  const shift = shiftOf(look);
  const on = isShifted(shift);
  const filter = on ? wbFilter(look) : "";
  for (const img of card.querySelectorAll(".sample img")) img.style.filter = filter;
  const tone = on ? tonePreview(state.camera.curve, makeCurve(state.camera.curve, look.recipe, 1, shift)) : look.tone;
  card.querySelector(".tone").replaceWith(toneGraph(look, tone));
  card.classList.toggle("shifted", on);
  const noCurve = card.querySelector(".no-curve");
  if (noCurve) noCurve.hidden = on;
  update();
}

function wbPanel(look, card) {
  const sliders = [["R", "Red", "cyan", "red"], ["B", "Blue", "yellow", "blue"]].map(([key, name, lo, hi], idx) => {
    const out = el("output", { className: "wb-val", textContent: "0" });
    const input = el("input", { type: "range", min: -WB_MAX, max: WB_MAX, step: 1, value: 0,
      ariaLabel: `${name} shift for ${look.name}, minus is ${lo}, plus is ${hi}` });
    input.addEventListener("input", () => {
      const shift = [...shiftOf(look)];
      shift[idx] = Number(input.value);
      state.wb.set(look.id, shift);
      out.textContent = signed(shift[idx]);
      applyShift(look, card);
    });
    return { input, out, row: el("div", { className: `wb-row ${key.toLowerCase()}` },
      el("span", { className: "wb-key", textContent: key }), input, out) };
  });
  const reset = el("button", { type: "button", className: "link wb-reset", textContent: "Reset" });
  reset.addEventListener("click", () => {
    state.wb.delete(look.id);
    for (const s of sliders) { s.input.value = 0; s.out.textContent = "0"; }
    applyShift(look, card);
  });
  return el("div", { className: "wb" },
    el("div", { className: "wb-head" }, el("span", { textContent: "White balance shift" }), reset),
    ...sliders.map((s) => s.row));
}

function renderLooks() {
  const box = $("looks-grid");
  box.replaceChildren();
  $("filters").replaceChildren();
  $("wb-defs").replaceChildren();
  state.camera.looks.forEach((look) => {
    const input = el("input", { type: "checkbox", checked: state.selected.has(look.id), value: look.id });
    input.addEventListener("change", () => {
      input.checked ? state.selected.add(look.id) : state.selected.delete(look.id);
      update();
    });

    // Beispielfotos zuerst, die Lasche dahinter
    const media = el("div", { className: "look-media" }, ...look.samples.map((src) =>
      el("div", { className: "sample" },
        el("img", { src, alt: `Sample photo, ${look.name}`, loading: "lazy" }))));
    media.append(el("img", { src: labelSrc(look), alt: `${look.name} film label`, loading: "lazy", width: 600, height: 440 }));

    // Nur die Auswahl ist ein <label>; die WB-Regler liegen daneben, sonst
    // wuerde jeder Klick darauf den Look ab- oder anwaehlen
    const card = el("div", { className: "look" });
    card.append(el("label", { className: "look-pick" },
      media,
      el("div", { className: "look-body" },
        el("div", { className: "look-head" },
          el("div", {},
            el("span", { className: "look-name", textContent: look.name }),
            el("span", { className: "look-kind", textContent: `${look.kind} · ${look.desc}` })),
          el("span", { className: "tick", ariaHidden: "true" })),
        el("div", { className: "look-detail" }, toneGraph(look), chips(look))),
      input));
    if (canShift(look)) card.append(wbPanel(look, card));
    card.dataset.kind = look.kind;
    card.style.setProperty("--accent", look.accent);
    box.append(card);
    if (isShifted(shiftOf(look))) restoreShift(look, card);
  });

  const kinds = ["All", ...new Set(state.camera.looks.map((l) => l.kind))];
  for (const kind of kinds) {
    const b = el("button", { type: "button", className: "filter", textContent: kind, ariaPressed: String(kind === "All") });
    b.addEventListener("click", () => {
      for (const other of $("filters").children) other.ariaPressed = String(other === b);
      for (const card of box.children) card.hidden = kind !== "All" && card.dataset.kind !== kind;
    });
    $("filters").append(b);
  }
}

// Nach einem Kamerawechsel: Regler und Vorschau auf die gespeicherte Verschiebung setzen
function restoreShift(look, card) {
  const shift = shiftOf(look);
  card.querySelectorAll(".wb-row").forEach((row, idx) => {
    row.querySelector("input").value = shift[idx];
    row.querySelector("output").textContent = signed(shift[idx]);
  });
  applyShift(look, card);
}

function setAll(on) {
  for (const input of $("looks-grid").querySelectorAll('input[type="checkbox"]')) {
    input.checked = on;
    on ? state.selected.add(input.value) : state.selected.delete(input.value);
  }
  update();
}

// --- Schritt 3: Download -------------------------------------------------

function selectedLooks() {
  return state.camera.looks.filter((l) => state.selected.has(l.id));
}

function update() {
  const looks = selectedLooks();
  const n = looks.length;
  const size = state.camera.base_size + looks.reduce((sum, l) => sum + l.size, 0);

  const summary = $("summary");
  summary.replaceChildren(
    el("div", { className: "summary-head" },
      el("span", { textContent: `${n} of ${state.camera.looks.length} looks` }),
      el("span", { className: "muted", textContent: n ? `about ${mb(size)}` : "" })),
    el("ul", { className: "picked" }, ...looks.map((l) => {
      const shift = shiftOf(l);
      const li = el("li", { textContent: isShifted(shift) ? `${l.name} · ${shiftText(shift)}` : l.name });
      li.style.setProperty("--accent", l.accent);
      return li;
    })));

  const btn = $("download");
  btn.disabled = n === 0 || !state.firmwareOk;
  $("dl-camera").textContent = state.camera.name;
  $("dl-status").textContent = !state.firmwareOk ? "Not available for this firmware." : n === 0 ? "Pick at least one look." : "";
  $("dock-text").textContent = n ? `${n} look${n === 1 ? "" : "s"} selected` : "No looks selected";
}

async function download() {
  const btn = $("download");
  const status = $("dl-status");
  const cam = state.camera;
  btn.disabled = true;
  status.textContent = "Building ZIP …";
  try {
    const looks = selectedLooks();
    const [base, ...lookZips] = await Promise.all([
      loadBytes(`data/${cam.id}/base.zip`),
      ...looks.map((l) => loadBytes(`data/${cam.id}/looks/${l.id}.zip`)),
    ]);
    // Looks mit WB-Verschiebung: Kurven neu rechnen, ersetzen die aus der Look-ZIP
    const extra = [];
    const rows = looks.map((l) => {
      const shift = shiftOf(l);
      if (!canShift(l) || !isShifted(shift)) return l;
      const stem = curveStem(l);
      extra.push(...curveFiles(cam.curve, stem, l.recipe, shift));
      const cols = [...l.cols];
      cols[9] = `${stem}.CV`;
      return { ...l, cols, wb: shift };
    });
    const zip = assemble(base, lookZips, rows, state.site.version, new Date(), extra);
    const name = `exccd-${state.site.version}-${cam.id}-${looks.length}looks.zip`;
    const url = URL.createObjectURL(new Blob([zip], { type: "application/zip" }));
    el("a", { href: url, download: name }).click();
    setTimeout(() => URL.revokeObjectURL(url), 60000);
    status.textContent = `${name} – ${mb(zip.length)}.`;
  } catch (err) {
    status.textContent = `Download failed: ${err.message}`;
  } finally {
    btn.disabled = false;
  }
}

// Leiste unten: zwischen Look-Auswahl und Download sichtbar
function watchDock() {
  const dock = $("dock");
  const seen = { looks: false, download: false };
  const io = new IntersectionObserver((entries) => {
    for (const e of entries) seen[e.target.id === "looks-step" ? "looks" : "download"] = e.isIntersecting;
    dock.hidden = !seen.looks || seen.download;
  }, { rootMargin: "0px 0px -30% 0px" });
  io.observe($("looks-step"));
  io.observe($("download-step"));
}

// --- Menue: je Menuepunkt eine Ansicht, die Startseite zeigt nur die Kamera

function route() {
  const target = location.hash.length > 1 ? document.getElementById(decodeURIComponent(location.hash.slice(1))) : null;
  const view = target?.closest("[data-view]") ?? $("home");
  const switched = view.hidden;
  for (const v of document.querySelectorAll("[data-view]")) v.hidden = v !== view;
  for (const a of document.querySelectorAll(".nav-main")) {
    if (a.getAttribute("href") === `#${view.id}`) a.setAttribute("aria-current", "page");
    else a.removeAttribute("aria-current");
  }
  document.body.dataset.page = view.id;
  // Beim Wechsel der Ansicht springen statt scrollen
  const behavior = switched || reducedMotion ? "instant" : "smooth";
  if (!target || target === view) window.scrollTo({ top: 0, behavior });
  else target.scrollIntoView({ behavior });
}

async function main() {
  route();
  addEventListener("hashchange", route);

  try {
    [state.site, state.config] = await Promise.all([loadJson("data/site.json"), loadJson("config.json")]);
  } catch (err) {
    $("dl-status").textContent = `Could not load the site data (${err.message}).`;
    return;
  }
  state.camera = initialCamera();
  for (const l of state.camera.looks) { state.selected.add(l.id); state.knownLooks.add(l.id); }
  $("version").textContent = state.site.version;
  for (const a of document.querySelectorAll(".repo-link")) a.href = a.href.endsWith("/issues") ? `${state.config.repo}/issues` : state.config.repo;
  renderHero();
  renderCameras();
  renderCamera();
  renderLooks();
  $("all").addEventListener("click", () => setAll(true));
  $("none").addEventListener("click", () => setAll(false));
  $("download").addEventListener("click", download);
  watchDock();
  update();
}

main();
