import { assemble } from "./zipjoin.js";
import { renderShowroom } from "./showroom.js";

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

const state = { site: null, config: null, camera: null, fw: null, firmwareOk: true, selected: new Set(), knownLooks: new Set() };

// Spalten von FILMS.TXT (siehe CHDK/FILMS/EXAMPLE.TXT)
const MODES = { 0: "Canon colours", 1: "Vivid", 2: "Neutral", 3: "Black & white", 4: "Sepia", 5: "Positive film", 11: "Custom colour" };
const SLIDERS = ["Contrast", "Sharpness", "Saturation", "Red", "Green", "Blue", "Skin tone"];

// app.js und test.js teilen sich die Antwort: site.json ist gross, nur einmal laden
const jsonCache = (globalThis.exccdJson ??= new Map());
function loadJson(url) {
  if (!jsonCache.has(url)) {
    jsonCache.set(url, fetch(url, { cache: "no-cache" }).then((r) => {
      if (!r.ok) throw new Error(`${url}: HTTP ${r.status}`);
      return r.json();
    }));
  }
  return jsonCache.get(url);
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
  $("facts").textContent = `${looks.length} looks · free · open source · ${state.site.version} beta`;

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
  // bis v1.10.2 hiess die Kamera "<plattform>-<sub>"
  want = want?.replace(/-\d{3}[a-z]$/, "");
  const cam = cams.find((c) => c.id === want);
  // Ohne Wahl zeigt die Installationsanleitung die Regeln fuer alle Modelle
  state.chosen = !!cam;
  return cam ?? cams[0];
}

const camLabel = (cam) => cam.name.replace(/^Canon /, "");

// Suchfeld ueber der Kameraliste: "ixus 132", "a2400", "sd600" ...
function filterCameras(query) {
  const words = query.toLowerCase().split(/\s+/).filter(Boolean);
  let shown = 0;
  for (const opt of $("cams").querySelectorAll(".cam-opt")) {
    const text = opt.dataset.search;
    const hit = words.every((w) => text.includes(w)) || opt.querySelector("input").checked;
    opt.hidden = !hit;
    shown += hit;
  }
  $("cam-search-count").textContent = words.length ? `${shown} of ${state.site.cameras.length} cameras` : "";
}

function renderCameras() {
  const box = $("cams");
  for (const cam of state.site.cameras) {
    const input = el("input", { type: "radio", name: "camera", value: cam.id, checked: cam === state.camera });
    input.addEventListener("change", () => setCamera(cam));
    const beta = cam.status !== "stabil";
    const fws = cam.firmwares.map((f) => f.version);
    const label = el("label", { className: "cam-opt" }, input,
      el("span", { className: "cam-opt-box" },
        el("span", { className: "cam-opt-name", textContent: camLabel(cam) }),
        el("span", { className: "cam-opt-meta" },
          fws.length === 1 ? `Firmware ${fws[0]}` : `${fws.length} firmware versions`,
          ...(cam.family ? [el("span", { textContent: `Family ${cam.family.id}`, title: cam.family.name })] : []),
          ...(beta ? [el("span", { className: "badge", textContent: "Beta" })] : []))));
    label.dataset.search = `${camLabel(cam)} ${cam.short} ${cam.id}`.toLowerCase();
    box.append(label);
  }
  const search = $("cam-search");
  search.addEventListener("input", () => filterCameras(search.value));
}

function setCamera(cam) {
  state.camera = cam;
  state.chosen = true;
  try { localStorage.setItem(CAM_KEY, cam.id); } catch { /* ohne Speicher */ }
  const radio = $("cams").querySelector(`input[value="${cam.id}"]`);
  if (radio) radio.checked = true;
  // Die Auswahl gilt je Look-ID weiter; neue Looks sind angewaehlt
  const known = new Set(state.knownLooks);
  for (const l of cam.looks) if (!known.has(l.id)) { state.selected.add(l.id); state.knownLooks.add(l.id); }
  renderCamera();
  renderLooks();
  update();
}

// "1.00c, 1.00d or 1.00e"
function fwList(cam) {
  const v = cam.firmwares.map((f) => f.version);
  return v.length === 1 ? v[0] : `${v.slice(0, -1).join(", ")} or ${v.at(-1)}`;
}

// Beta-Hinweis: Kamera im Test, oder stabile Kamera mit einer noch nicht getesteten Firmware
function renderBeta() {
  const cam = state.camera;
  $("beta-note").hidden = cam.status === "stabil" && (!state.fw || state.fw.tested);
}

function renderCamera() {
  const cam = state.camera;
  // Nur eine Firmware: gleich gewaehlt. Mehrere: erst waehlen, die Dateien sind je Version verschieden.
  state.fw = cam.firmwares.length === 1 ? cam.firmwares[0] : null;
  state.firmwareOk = !!state.fw;
  renderBeta();
  $("ver-link").href = `data/${cam.id}/ver.req`;
  $("vers-link").href = `data/${cam.id}/vers.req`;
  $("fw-example").textContent = `Firmware Ver GM${cam.firmware.toUpperCase()}`;
  $("fw-note").textContent = state.fw ? "" : "Pick your camera's firmware version, the download is different for each.";
  renderInstall();

  const fw = $("fw");
  fw.replaceChildren(fw.querySelector("legend"));
  const options = [...cam.firmwares.map((f) => [f.version, f]), ...cam.other_firmware.map((v) => [v, null]), ["Other", null]];
  for (const [v, f] of options) {
    const input = el("input", { type: "radio", name: "fw", value: v, checked: !!f && f === state.fw });
    input.addEventListener("change", () => {
      state.fw = f;
      state.firmwareOk = !!f;
      renderBeta();
      $("fw-note").textContent = f
        ? ""
        : v === "Other"
          ? `EXCCD needs firmware ${fwList(cam)}. Other versions won't work.`
          : `Firmware ${v} isn't supported yet. Maybe later; for now EXCCD needs ${fwList(cam)}.`;
      update();
    });
    fw.append(el("label", { className: "seg-opt" }, input, el("span", { textContent: v })));
  }
}

// Kamerawahl direkt in der Installationsanleitung
function renderInstallSelect() {
  const sel = $("install-cam");
  const cams = [...state.site.cameras].sort((a, b) =>
    camLabel(a).localeCompare(camLabel(b), "en", { numeric: true }));
  sel.append(...cams.map((c) => el("option", { value: c.id, textContent: camLabel(c) })));
  sel.addEventListener("change", () => {
    const cam = state.site.cameras.find((c) => c.id === sel.value);
    if (cam) setCamera(cam);
    else { state.chosen = false; renderInstall(); }
  });
}

// Installationshinweise je Kamera (Boot-Regeln wie tools/installer/core.c).
// Ohne gewaehlte Kamera: allgemeine Hinweise fuer alle Modelle.
function renderInstall() {
  const cam = state.camera;
  const chosen = state.chosen;
  $("install-cam").value = chosen ? cam.id : "";
  for (const e of document.querySelectorAll(".if-cam")) e.hidden = !chosen;
  for (const e of document.querySelectorAll(".if-generic")) e.hidden = chosen;
  for (const e of document.querySelectorAll(".cam-short")) e.textContent = cam.short;
  for (const e of document.querySelectorAll(".cam-fw")) e.textContent = fwList(cam);
  for (const e of document.querySelectorAll(".cam-alt")) e.textContent = cam.alt_key ?? "Print";
  if (!chosen) {
    $("card-need").innerHTML = "an SD card: <b>2 GB or less</b> for most models; newer ones (about 2011 on, like "
      + "the IXUS 132 or A2400) also take cards up to 32 GB. Choose your camera above to see which.";
    $("trouble-size").textContent = "Is the card too big? Most models only start EXCCD from cards up to 2 GB, "
      + "newer ones from cards up to 32 GB. SDXC cards (over 32 GB) don't work on any model yet.";
    $("faq-card").textContent = "on most models that card can be at most 2 GB (32 GB on newer ones)";
    $("manual-firm-update").hidden = false;
    $("manual-bootable").hidden = false;
    return;
  }
  const fat16 = cam.boot === "fat16";
  $("faq-card").textContent = fat16
    ? `on the ${cam.short} that card can be at most 2 GB`
    : `on the ${cam.short} that card can be up to 32 GB`;
  $("card-need").textContent = fat16
    ? "an SD card of 2 GB or less (bigger cards won't start EXCCD on this camera)"
    : "an SD card of 32 GB or less (SD or SDHC; bigger SDXC cards don't work yet)";
  $("trouble-size").textContent = fat16
    ? `Is the card bigger than 2 GB? The ${cam.short} only starts EXCCD on its own from cards up to 2 GB.`
      + (cam.firm_update ? " On a bigger card you can still start it with MENU → Firm Update in playback mode." : "")
    : `Is the card bigger than 32 GB? The ${cam.short} can't start EXCCD from SDXC (exFAT) cards.`;
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

// Karussell der Look-Karte: mit der Maus darueber laufen die Beispielfotos durch,
// beim Verlassen zurueck zum ersten; per Touch wischen (Scroll-Snap). Punkte zeigen,
// wo man ist, der letzte steht fuer die Lasche
function carousel(wrap, media, count) {
  const slides = media.children.length;
  const dots = el("div", { className: "shot-dots", ariaHidden: "true" },
    ...Array.from({ length: slides }, (_, k) => el("span", { className: k === count ? "dot label" : "dot" })));
  wrap.append(dots);
  let timer = 0, i = 0;
  const go = (n) => { i = n; media.scrollTo({ left: n * media.clientWidth, behavior: reducedMotion ? "instant" : "smooth" }); };
  const mark = () => {
    const k = Math.round(media.scrollLeft / media.clientWidth);
    [...dots.children].forEach((d, j) => d.classList.toggle("on", j === k));
  };
  media.addEventListener("scroll", mark, { passive: true });
  mark();
  const hover = matchMedia("(hover: hover)");
  wrap.addEventListener("mouseenter", () => {
    if (!hover.matches) return;
    clearInterval(timer);
    for (const img of media.querySelectorAll("img")) img.loading = "eager";  // die naechsten Fotos vorladen
    timer = setInterval(() => go((i + 1) % count), 1300);
  });
  wrap.addEventListener("mouseleave", () => { clearInterval(timer); if (hover.matches) go(0); });
}

function renderLooks() {
  const box = $("looks-grid");
  box.replaceChildren();
  $("filters").replaceChildren();
  state.camera.looks.forEach((look) => {
    const input = el("input", { type: "checkbox", checked: state.selected.has(look.id), value: look.id });
    input.addEventListener("change", () => {
      input.checked ? state.selected.add(look.id) : state.selected.delete(look.id);
      update();
    });

    // Beispielfotos (dieselben wie im Showroom) zuerst, die Lasche dahinter
    // Simulierte Beispielfotos offen als solche kennzeichnen
    const media = el("div", { className: "look-media" }, ...look.samples.map((s) =>
      el("div", { className: "sample" },
        el("img", { src: s.src, alt: `${s.sim ? "Simulated sample" : "Sample photo"}, ${look.name}: ${s.title}`, loading: "lazy" }),
        el("span", { className: "shot-title", textContent: s.title }),
        ...(s.sim ? [el("span", { className: "sim-tag", textContent: "simulated", title: "Rendered on a PC from another photo, not shot on the camera yet" })] : []))));
    media.append(el("img", { src: labelSrc(look), alt: `${look.name} film label`, loading: "lazy", width: 600, height: 440 }));
    const shots = el("div", { className: "look-shots" }, media);
    if (look.samples.length > 1) carousel(shots, media, look.samples.length);

    const card = el("div", { className: "look" });
    card.append(el("label", { className: "look-pick" },
      shots,
      el("div", { className: "look-body" },
        el("div", { className: "look-head" },
          el("div", {},
            el("span", { className: "look-name", textContent: look.name }),
            el("span", { className: "look-kind", textContent: `${look.kind} · ${look.desc}` })),
          el("span", { className: "tick", ariaHidden: "true" })),
        el("div", { className: "look-detail" }, toneGraph(look), chips(look))),
      input));
    card.dataset.kind = look.kind;
    card.style.setProperty("--accent", look.accent);
    box.append(card);
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
  const size = (state.fw ?? state.camera.firmwares[0]).base_size + looks.reduce((sum, l) => sum + l.size, 0);

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
  $("dl-camera").textContent = state.camera.name + (state.fw ? `, firmware ${state.fw.version}` : "");
  $("dl-status").textContent = !state.fw
    ? ($("fw").querySelector("input:checked") ? "Not available for this firmware." : "Pick your firmware version first (step 01).")
    : n === 0 ? "Pick at least one look." : "";
  $("dock-text").textContent = n ? `${n} look${n === 1 ? "" : "s"} selected` : "No looks selected";
}

async function download() {
  const btn = $("download");
  const status = $("dl-status");
  const cam = state.camera;
  const fw = state.fw;
  if (!fw) return;
  btn.disabled = true;
  status.textContent = "Building ZIP …";
  try {
    const looks = selectedLooks();
    const [base, ...lookZips] = await Promise.all([
      loadBytes(`data/${fw.dir}/base.zip`),
      ...looks.map((l) => loadBytes(`data/${cam.id}/looks/${l.id}.zip`)),
    ]);
    const zip = assemble(base, lookZips, looks, state.site.version, new Date());
    // Plattform und sub im Namen: daran erkennt der Installer Kamera und Firmware
    const name = `exccd-${state.site.version}-${cam.id}-${fw.sub}-${looks.length}looks.zip`;
    const url = URL.createObjectURL(new Blob([zip], { type: "application/zip" }));
    el("a", { href: url, download: name }).click();
    setTimeout(() => URL.revokeObjectURL(url), 60000);
    status.textContent = `${name} – ${mb(zip.length)}.`;
    track({ path: `download/${cam.id}-${fw.sub}`, title: `ZIP ${cam.name} ${fw.version}`, event: true });
    for (const l of looks) track({ path: `look/${l.id}`, title: `Look ${l.id}`, event: true });
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

// --- Zaehlung (GoatCounter, ohne Cookies). count.js laedt async: bis dahin puffern.
// Fehlt das Script (Blocker, offline), passiert einfach nichts.

const trackQueue = [];
function track(hit) {
  const gc = window.goatcounter;
  if (gc?.count) gc.count(hit);
  else trackQueue.push(hit);
}
document.querySelector("script[data-goatcounter]")?.addEventListener("load", () => {
  for (const hit of trackQueue.splice(0)) window.goatcounter?.count?.(hit);
});

// --- Menue: je Menuepunkt eine Ansicht, die Startseite zeigt nur die Kamera

let trackedView = null;
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
  // Je Ansicht ein Seitenaufruf (/, /compare, /install …), Spruenge innerhalb zaehlen nicht
  if (view.id !== trackedView) {
    trackedView = view.id;
    track({ path: view.id === "home" ? "/" : `/${view.id}`, title: view.id });
  }
  // Beim Wechsel der Ansicht springen statt scrollen
  const behavior = switched || reducedMotion ? "instant" : "smooth";
  if (!target || target === view) window.scrollTo({ top: 0, behavior });
  else target.scrollIntoView({ behavior });
}

// Dropdowns: mit Maus per Hover/Fokus (CSS); per Touch oeffnet das Tippen auf den Menuepunkt
// die Ansicht und zeigt zugleich die Unterpunkte, Tippen daneben schliesst sie
function initNav() {
  const items = [...document.querySelectorAll(".nav-item")];
  const hover = matchMedia("(hover: hover)");
  const closeAll = (except) => { for (const i of items) if (i !== except) i.classList.remove("open"); };
  for (const item of items) {
    const sub = item.querySelector(".nav-sub");
    if (!sub) continue;
    item.classList.add("has-sub");
    item.querySelector(".nav-main").addEventListener("click", () => {
      if (hover.matches) return;
      closeAll(item);
      item.classList.toggle("open");
    });
    sub.addEventListener("click", (e) => {
      if (!e.target.closest("a")) return;
      item.classList.remove("open");
      // Nach der Wahl zuklappen, obwohl Maus/Fokus noch im Dropdown sind
      item.classList.add("closed");
      e.target.blur();
      item.addEventListener("mouseleave", () => item.classList.remove("closed"), { once: true });
      if (!hover.matches) item.classList.remove("closed");
    });
  }
  document.addEventListener("click", (e) => { if (!e.target.closest(".nav-item")) closeAll(); });
  document.addEventListener("keydown", (e) => {
    if (e.key !== "Escape") return;
    closeAll();
    const item = document.activeElement?.closest(".nav-item");
    if (item) { item.classList.add("closed"); item.querySelector(".nav-main").focus();
      item.addEventListener("focusout", () => item.classList.remove("closed"), { once: true }); }
  });
}

async function main() {
  initNav();
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
  $("version").textContent = `${state.site.version} · beta`;
  // Repo-Links zeigen auf das Repo aus config.json, Unterpfade (issues, Dateien) bleiben
  for (const a of document.querySelectorAll(".repo-link")) {
    a.href = a.getAttribute("href").replace(/^https:\/\/github\.com\/[^/]+\/[^/]+/, state.config.repo);
  }
  renderHero();
  renderShowroom(state.site);
  renderCameras();
  renderInstallSelect();
  renderCamera();
  renderLooks();
  for (const a of document.querySelectorAll('a[href="exccd-installer.exe"]')) {
    a.addEventListener("click", () => track({ path: "download/installer", title: "Installer for Windows", event: true }));
  }
  $("all").addEventListener("click", () => setAll(true));
  $("none").addEventListener("click", () => setAll(false));
  $("download").addEventListener("click", () => {
    // Wer herunterlaedt, hat die Kamera bestaetigt: Anleitung zeigt ab jetzt deren Details
    if (!state.chosen) {
      state.chosen = true;
      try { localStorage.setItem(CAM_KEY, state.camera.id); } catch { /* ohne Speicher */ }
      renderInstall();
    }
    download();
  });
  watchDock();
  update();
}

main();
