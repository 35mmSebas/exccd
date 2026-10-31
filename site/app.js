import { assemble } from "./zipjoin.js";

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

const state = { site: null, config: null, camera: null, firmwareOk: true, selected: new Set(), amount: 0 };

// Spalten von FILMS.TXT (siehe CHDK/FILMS/BEISPIEL.TXT)
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
  const start = () => { if (!reducedMotion && !timer) timer = setInterval(() => show(i + 1), 2600); };
  const stop = () => { clearInterval(timer); timer = null; };
  $("cam-print").addEventListener("click", () => { stop(); show(i + 1); start(); });
  $("cam-screen").addEventListener("click", () => { stop(); show(i + 1); start(); });
  document.addEventListener("visibilitychange", () => (document.hidden ? stop() : start()));
  start();
}

// --- Schritt 1: Kamera ---------------------------------------------------

function renderCamera() {
  const cam = state.camera;
  $("camera-name").textContent = cam.name;
  const fw = $("fw");
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

// --- Schritt 2: Looks ----------------------------------------------------

function toneGraph(look) {
  const g = svg("svg", { viewBox: "0 0 100 100", class: "tone", role: "img",
    "aria-label": look.tone ? `Tone curve of ${look.name}` : `${look.name} uses no tone curve` });
  for (const p of [25, 50, 75]) {
    g.append(svg("line", { x1: p, y1: 0, x2: p, y2: 100, class: "grid" }), svg("line", { x1: 0, y1: p, x2: 100, y2: p, class: "grid" }));
  }
  g.append(svg("line", { x1: 0, y1: 100, x2: 100, y2: 0, class: look.tone ? "diag" : "diag solo" }));
  if (look.tone) {
    const mono = Number(look.cols[1]) === 3;
    for (const ch of mono ? ["g"] : ["r", "g", "b"]) {
      const pts = look.tone[ch];
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
  if (!look.tone) out.push(el("span", { className: "chip", textContent: "No curve" }));
  return el("div", { className: "chips" }, ...out);
}

function renderLooks() {
  const box = $("looks-grid");
  for (const look of state.camera.looks) {
    const input = el("input", { type: "checkbox", checked: true, value: look.id });
    input.addEventListener("change", () => {
      input.checked ? state.selected.add(look.id) : state.selected.delete(look.id);
      update();
    });
    state.selected.add(look.id);

    const media = el("div", { className: "look-media" },
      el("img", { src: labelSrc(look), alt: `${look.name} film label`, loading: "lazy", width: 600, height: 440 }));
    for (const src of look.samples) {
      media.append(el("img", { src, alt: `Sample photo, ${look.name}`, loading: "lazy", className: "sample" }));
    }

    const card = el("label", { className: "look" },
      media,
      el("div", { className: "look-body" },
        el("div", { className: "look-head" },
          el("div", {},
            el("span", { className: "look-name", textContent: look.name }),
            el("span", { className: "look-kind", textContent: `${look.kind} · ${look.desc}` })),
          el("span", { className: "tick", ariaHidden: "true" })),
        el("div", { className: "look-detail" }, toneGraph(look), chips(look))),
      input);
    card.dataset.kind = look.kind;
    card.style.setProperty("--accent", look.accent);
    box.append(card);
  }

  const kinds = ["All", ...new Set(state.camera.looks.map((l) => l.kind))];
  for (const kind of kinds) {
    const b = el("button", { type: "button", className: "filter", textContent: kind, ariaPressed: String(kind === "All") });
    b.addEventListener("click", () => {
      for (const other of $("filters").children) other.ariaPressed = String(other === b);
      for (const card of box.children) card.hidden = kind !== "All" && card.dataset.kind !== kind;
    });
    $("filters").append(b);
  }
  $("all").addEventListener("click", () => setAll(true));
  $("none").addEventListener("click", () => setAll(false));
}

function setAll(on) {
  for (const input of $("looks-grid").querySelectorAll("input")) {
    input.checked = on;
    on ? state.selected.add(input.value) : state.selected.delete(input.value);
  }
  update();
}

// --- Schritt 3: Download -------------------------------------------------

function renderSupport() {
  const s = state.config.support;
  const box = $("pwyw");
  box.append(el("p", {},
    "It's free. If you want to chip in, around ",
    el("b", { textContent: `${s.suggested} ${s.currency}` }),
    " is a nice amount. It goes towards the next camera."));
  if (!s.url) {
    box.append(el("p", { className: "muted small", textContent: "Donations aren't set up yet, so for now it's simply free." }));
    return;
  }
  state.amount = s.suggested;
  const row = el("div", { className: "seg amounts", role: "radiogroup", ariaLabel: "Amount" });
  for (const a of s.amounts) {
    const input = el("input", { type: "radio", name: "amount", value: a, checked: a === s.suggested });
    input.addEventListener("change", () => { state.amount = a; update(); });
    row.append(el("label", { className: "seg-opt" }, input, el("span", { textContent: `${a} ${s.currency}` })));
  }
  box.append(row);
}

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
      const li = el("li", { textContent: l.name });
      li.style.setProperty("--accent", l.accent);
      return li;
    })));

  const btn = $("download");
  btn.disabled = n === 0 || !state.firmwareOk;
  $("dl-status").textContent = !state.firmwareOk ? "Not available for this firmware." : n === 0 ? "Pick at least one look." : "";
  const s = state.config.support;
  btn.textContent = s.url && state.amount > 0 ? `Pay ${state.amount} ${s.currency} & download` : "Download ZIP";
  $("dock-text").textContent = n ? `${n} look${n === 1 ? "" : "s"} selected` : "No looks selected";
}

async function download() {
  const btn = $("download");
  const status = $("dl-status");
  const cam = state.camera;
  const s = state.config.support;
  // Zahlungsseite sofort im Klick oeffnen, sonst blockiert der Popup-Blocker
  if (s.url && state.amount > 0) window.open(s.url, "_blank", "noopener");
  btn.disabled = true;
  status.textContent = "Building ZIP …";
  try {
    const looks = selectedLooks();
    const [base, ...lookZips] = await Promise.all([
      loadBytes(`data/${cam.id}/base.zip`),
      ...looks.map((l) => loadBytes(`data/${cam.id}/looks/${l.id}.zip`)),
    ]);
    const zip = assemble(base, lookZips, looks, state.site.version);
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
  state.camera = state.site.cameras[0];
  $("version").textContent = state.site.version;
  for (const a of document.querySelectorAll(".repo-link")) a.href = state.config.repo;
  renderHero();
  renderCamera();
  renderLooks();
  renderSupport();
  $("download").addEventListener("click", download);
  watchDock();
  update();
}

main();
