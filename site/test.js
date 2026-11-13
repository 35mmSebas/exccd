// Kameratest fuer Nutzer ohne Vorwissen: Testtafel am Bildschirm, Ja/Nein-Fragen,
// daraus ein fertiger Bericht fuer ein GitHub-Issue (oder per Mail, falls config.json "mail" hat)

const $ = (id) => document.getElementById(id);
const el = (tag, props = {}, ...kids) => {
  const e = Object.assign(document.createElement(tag), props);
  e.append(...kids);
  return e;
};

const OTHER = "other";
const ANSWERS = ["Yes", "No", "Not sure"];
// Reihenfolge und Wortlaut im Bericht
const QUESTIONS = {
  starts: "EXCCD starts",
  tab: "Look tab visible",
  redblue: "Red is red, blue is blue",
  grey: "Grey strip is grey",
  looks: "Looks change the photo",
  stable: "No freeze / switch-off",
};

const state = { site: null, config: { repo: "https://github.com/35mmSebas/exccd" }, cam: null, other: "", fw: "", answers: {} };

async function loadJson(url) {
  const r = await fetch(url, { cache: "no-cache" });
  if (!r.ok) throw new Error(`${url}: HTTP ${r.status}`);
  return r.json();
}

// --- Testtafel -------------------------------------------------------------
// Grosse Felder mit Namen: Rot/Blau vertauscht = Bayer-Muster falsch, Graustufen = Weissabgleich/Kurve,
// Haut/Himmel/Laub = Palette. Kein Feinkram, der auf Abstand und Moiré ankommt.

const PATCHES = [
  ["Red", "#d0141e"], ["Green", "#1e9b3c"], ["Blue", "#1e3cc8"], ["White", "#f2f2f2"],
  ["Cyan", "#00a5c8"], ["Magenta", "#c8288c"], ["Yellow", "#f0cd14"], ["Black", "#141414"],
  ["Skin", "#e6b496"], ["Skin", "#a0694b"], ["Sky", "#6e96c8"], ["Leaves", "#556e32"],
];
const GREYS = ["#141414", "#323232", "#505050", "#6e6e6e", "#8c8c8c", "#aaaaaa", "#c8c8c8", "#ebebeb"];

function chartSvg() {
  const NS = "http://www.w3.org/2000/svg";
  const s = document.createElementNS(NS, "svg");
  s.setAttribute("viewBox", "0 0 1600 900");
  s.setAttribute("preserveAspectRatio", "xMidYMid meet");
  const add = (tag, attrs, text) => {
    const e = document.createElementNS(NS, tag);
    for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    if (text) e.textContent = text;
    s.append(e);
  };
  add("rect", { x: 0, y: 0, width: 1600, height: 900, fill: "#5a5a5a" });
  const w = 360, h = 180, gx = 20, x0 = 50, y0 = 40;
  PATCHES.forEach(([name, fill], i) => {
    const x = x0 + (i % 4) * (w + gx), y = y0 + Math.floor(i / 4) * (h + gx);
    add("rect", { x, y, width: w, height: h, fill });
    const dark = ["White", "Yellow", "Skin"].includes(name) && fill !== "#a0694b";
    add("text", { x: x + 18, y: y + h - 18, fill: dark ? "#141414" : "#f2f2f2",
      "font-family": "Helvetica, Arial, sans-serif", "font-size": 30 }, name);
  });
  const gy = y0 + 3 * (h + gx) + 10, gw = (1600 - 2 * x0) / GREYS.length;
  GREYS.forEach((fill, i) => add("rect", { x: x0 + i * gw, y: gy, width: gw + 0.5, height: 170, fill }));
  add("text", { x: 1550, y: 885, fill: "#9a9a9a", "text-anchor": "end",
    "font-family": "Helvetica, Arial, sans-serif", "font-size": 22 }, "EXCCD test chart · top ↑");
  return s;
}

function openChart() {
  const box = $("t-chart");
  if (!box.firstChild) box.append(chartSvg());
  box.hidden = false;
  box.focus();
  box.requestFullscreen?.().catch(() => { /* Overlay reicht auch ohne Vollbild */ });
}

function closeChart() {
  const box = $("t-chart");
  box.hidden = true;
  if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
  $("t-chart-open").focus();
}

// --- Auswahl ----------------------------------------------------------------

function segOption(name, value, label, checked, onChange) {
  const input = el("input", { type: "radio", name, value, checked });
  input.addEventListener("change", onChange);
  return el("label", { className: "seg-opt" }, input, el("span", { textContent: label }));
}

function renderCameras() {
  const box = $("t-cams");
  const cams = [...state.site.cameras.filter((c) => c.status !== "stabil"), ...state.site.cameras.filter((c) => c.status === "stabil")];
  const want = new URLSearchParams(location.search).get("camera");
  state.cam = cams.find((c) => c.id === want) ?? cams[0] ?? null;
  const opt = (value, name, meta, checked) => {
    const input = el("input", { type: "radio", name: "t-camera", value, checked });
    input.addEventListener("change", () => setCamera(value));
    return el("label", { className: "cam-opt" }, input,
      el("span", { className: "cam-opt-box" }, el("span", { className: "cam-opt-name", textContent: name }),
        el("span", { className: "cam-opt-meta" }, ...meta)));
  };
  for (const c of cams) {
    const meta = c.status === "stabil" ? ["Tested"] : [el("span", { className: "badge", textContent: "Needs testers" })];
    box.append(opt(c.id, c.short ?? c.name.replace(/^Canon /, ""), meta, c === state.cam));
  }
  box.append(opt(OTHER, "Another Canon", ["Not supported yet, tell me anyway"], !state.cam));
  // Suchfeld: Karten ausblenden, die nicht passen ("Another Canon" und die gewaehlte bleiben)
  for (const [k, label] of [...box.querySelectorAll(".cam-opt")].entries()) {
    const c = cams[k];
    label.dataset.search = c ? `${c.name} ${c.short} ${c.id}`.toLowerCase() : "";
  }
  const search = $("t-cam-search");
  search.addEventListener("input", () => {
    const words = search.value.toLowerCase().split(" ").filter(Boolean);
    for (const label of box.querySelectorAll(".cam-opt")) {
      const t = label.dataset.search;
      label.hidden = !!t && !label.querySelector("input").checked && !words.every((w) => t.includes(w));
    }
  });
  renderFirmware();
}

function setCamera(id) {
  state.cam = id === OTHER ? null : state.site.cameras.find((c) => c.id === id);
  $("t-other-box").hidden = !!state.cam;
  renderFirmware();
  update();
}

function renderFirmware() {
  const box = $("t-fw");
  box.querySelectorAll(".seg-opt").forEach((n) => n.remove());
  box.hidden = !state.cam;
  if (!state.cam) { state.fw = ""; return; }
  const opts = [...state.cam.firmwares.map((f) => f.version), "Something else", "Don't know"];
  state.fw = "Don't know";
  for (const v of opts) {
    box.append(segOption("t-fw", v, v, v === state.fw, () => { state.fw = v; update(); }));
  }
}

function renderQuestions() {
  for (const fs of document.querySelectorAll(".t-ask")) {
    const q = fs.dataset.q;
    for (const a of ANSWERS) {
      fs.append(segOption(`t-${q}`, a, a, false, () => { state.answers[q] = a; update(); }));
    }
  }
}

// --- Bericht -----------------------------------------------------------------

function cameraName() {
  return state.cam ? state.cam.name : `Not listed: ${state.other || "(model not given)"}`;
}

function report() {
  const notStarted = state.answers.starts === "No";
  const notes = $("t-notes").value.trim();
  // Nicht unterstuetzte Kamera: nichts zu testen, nur der Wunsch
  if (!state.cam) {
    return [`Camera: ${cameraName()}`, "", "I'd like EXCCD on this camera.", "", "Notes:", notes || "–", "",
      "_Sent with the camera test page._"].join("\n");
  }
  const lines = [
    `Camera: ${cameraName()}`,
    `Firmware: ${state.fw}`,
    `EXCCD version: ${state.site?.version ?? "?"}`,
    "",
    "| Check | Answer |",
    "|---|---|",
  ];
  for (const [q, text] of Object.entries(QUESTIONS)) {
    if (notStarted && q !== "starts") continue;
    lines.push(`| ${text} | ${state.answers[q] ?? "–"} |`);
  }
  lines.push("", "Notes:", notes || "–", "", "Photos: (drag them in here)", "", "_Sent with the camera test page._");
  return lines.join("\n");
}

function title() {
  if (!state.cam) return `Camera wish: ${state.other || "another camera"}`;
  return `Camera test: ${state.cam.short ?? state.cam.name}`;
}

function update() {
  const notStarted = state.answers.starts === "No";
  $("t-nostart").hidden = !notStarted;
  $("test-install").hidden = !state.cam;
  $("test-photos").hidden = notStarted || !state.cam;
  $("t-send-photos").hidden = !state.cam;
  $("t-send-num").textContent = state.cam ? "04" : "02";
  const text = report();
  $("t-report").textContent = text;
  const q = new URLSearchParams({ title: title(), body: text, labels: "camera" });
  $("t-github").href = `${state.config.repo}/issues/new?${q}`;
  const mail = state.config.mail;
  $("t-mail").hidden = !mail;
  if (mail) {
    $("t-mail").href = `mailto:${mail}?subject=${encodeURIComponent(title())}&body=${encodeURIComponent(text + "\n\n(Please attach your photos.)")}`;
  }
  $("t-copied").textContent = "";
}

async function copyReport() {
  const text = report();
  try {
    await navigator.clipboard.writeText(text);
  } catch {
    // Ohne Zwischenablage-Recht: Text markieren, dann reicht Strg+C
    const range = document.createRange();
    range.selectNodeContents($("t-report"));
    getSelection().removeAllRanges();
    getSelection().addRange(range);
    $("t-copied").textContent = "Selected – press Ctrl+C to copy.";
    return;
  }
  $("t-copied").textContent = "Copied.";
}

async function main() {
  try {
    const [site, config] = await Promise.all([loadJson("data/site.json"), loadJson("config.json")]);
    state.site = site;
    Object.assign(state.config, config);
  } catch (err) {
    $("t-report").textContent = `Could not load the site data (${err.message}).`;
    return;
  }
  renderCameras();
  renderQuestions();
  $("t-other-box").hidden = !!state.cam;
  $("t-other").addEventListener("input", (e) => { state.other = e.target.value.trim(); update(); });
  $("t-notes").addEventListener("input", update);
  $("t-copy").addEventListener("click", copyReport);
  $("t-chart-open").addEventListener("click", openChart);
  $("t-chart").addEventListener("click", closeChart);
  $("t-chart").addEventListener("keydown", (e) => { if (e.key === "Escape") closeChart(); });
  document.addEventListener("fullscreenchange", () => {
    if (!document.fullscreenElement && !$("t-chart").hidden) closeChart();
  });
  update();
}

main();
