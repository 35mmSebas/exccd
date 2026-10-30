import { assemble } from "./zipjoin.js";

const $ = (id) => document.getElementById(id);
const el = (tag, props = {}, ...kids) => {
  const e = Object.assign(document.createElement(tag), props);
  e.append(...kids);
  return e;
};

const state = { site: null, config: null, camera: null, firmwareOk: true, selected: new Set(), amount: 0 };

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
          ? `EXCCD needs firmware ${cam.firmware}. Other versions do not work.`
          : `Firmware ${v} is not supported yet. It may come later – for now EXCCD needs ${cam.firmware}.`;
      update();
    });
    fw.append(el("label", { className: "fw-opt" }, input, el("span", { textContent: v })));
  }
}

function renderLooks() {
  const box = $("looks");
  for (const look of state.camera.looks) {
    const input = el("input", { type: "checkbox", checked: true, value: look.id });
    input.addEventListener("change", () => {
      input.checked ? state.selected.add(look.id) : state.selected.delete(look.id);
      update();
    });
    state.selected.add(look.id);

    const media = el("div", { className: "look-media" },
      el("img", { src: `data/${state.camera.id}/looks/${look.id}.png`, alt: `${look.name} label`, loading: "lazy", width: 600, height: 440 }));
    for (const src of look.samples) {
      media.append(el("img", { src, alt: `Sample photo, ${look.name}`, loading: "lazy", className: "sample" }));
    }

    const card = el("label", { className: "look" },
      media,
      el("div", { className: "look-text" },
        el("span", { className: "look-name", textContent: look.name }),
        el("span", { className: "look-kind", textContent: `${look.kind} · ${look.desc}` })),
      input,
      el("span", { className: "tick", ariaHidden: "true" }));
    card.style.setProperty("--accent", look.accent);
    box.append(card);
  }
  $("all").addEventListener("click", () => setAll(true));
  $("none").addEventListener("click", () => setAll(false));
}

function setAll(on) {
  for (const input of $("looks").querySelectorAll("input")) {
    input.checked = on;
    on ? state.selected.add(input.value) : state.selected.delete(input.value);
  }
  update();
}

function renderSupport() {
  const s = state.config.support;
  const box = $("pwyw");
  box.append(el("p", {},
    "EXCCD is free. If you like it, pay what you want – ",
    el("b", { textContent: `${s.suggested} ${s.currency}` }),
    " is a good amount. The money buys and ports more cameras."));
  if (!s.url) {
    box.append(el("p", { className: "muted small", textContent: "Payments are not open yet – download for free for now." }));
    return;
  }
  state.amount = s.suggested;
  const row = el("div", { className: "amounts", role: "radiogroup", ariaLabel: "Amount" });
  for (const a of s.amounts) {
    const input = el("input", { type: "radio", name: "amount", value: a, checked: a === s.suggested });
    input.addEventListener("change", () => { state.amount = a; update(); });
    row.append(el("label", { className: "amount" }, input, el("span", { textContent: `${a} ${s.currency}` })));
  }
  box.append(row);
}

function update() {
  const n = state.selected.size;
  $("look-count").textContent = `${n} of ${state.camera.looks.length} looks`;
  const btn = $("download");
  btn.disabled = n === 0 || !state.firmwareOk;
  $("dl-status").textContent = !state.firmwareOk ? "Not available for this firmware." : n === 0 ? "Pick at least one look." : "";
  const s = state.config.support;
  btn.textContent = s.url && state.amount > 0 ? `Pay ${state.amount} ${s.currency} & download` : "Download ZIP";
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
    const looks = cam.looks.filter((l) => state.selected.has(l.id));
    const [base, ...lookZips] = await Promise.all([
      loadBytes(`data/${cam.id}/base.zip`),
      ...looks.map((l) => loadBytes(`data/${cam.id}/looks/${l.id}.zip`)),
    ]);
    const zip = assemble(base, lookZips, looks, state.site.version);
    const name = `exccd-${state.site.version}-${cam.id}-${looks.length}looks.zip`;
    const url = URL.createObjectURL(new Blob([zip], { type: "application/zip" }));
    el("a", { href: url, download: name }).click();
    setTimeout(() => URL.revokeObjectURL(url), 60000);
    status.textContent = `${name} – ${(zip.length / 1e6).toFixed(1)} MB. Now see step 4.`;
  } catch (err) {
    status.textContent = `Download failed: ${err.message}`;
  } finally {
    btn.disabled = false;
  }
}

async function main() {
  try {
    [state.site, state.config] = await Promise.all([loadJson("data/site.json"), loadJson("config.json")]);
  } catch (err) {
    $("dl-status").textContent = `Could not load the site data (${err.message}).`;
    return;
  }
  state.camera = state.site.cameras[0];
  $("version").textContent = state.site.version;
  $("repo-link").href = state.config.repo;
  renderCamera();
  renderLooks();
  renderSupport();
  $("download").addEventListener("click", download);
  update();
}

main();
