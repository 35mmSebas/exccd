// Showroom: ein Foto, viele Looks. Szenen aus site.json (tools/site/build_site.py,
// Bilder in site/showroom/), Vergleich mit Daylight 100 per Schieber.

const $ = (id) => document.getElementById(id);
const el = (tag, props = {}, ...kids) => {
  const e = Object.assign(document.createElement(tag), props);
  e.append(...kids);
  return e;
};

const BASE = "DAYLITE";
const KEY = "exccd-showroom";

export function renderShowroom(site) {
  const scenes = site.showroom ?? [];
  if (!scenes.length) {
    $("sr-empty").hidden = false;
    for (const e of document.querySelectorAll("#compare .sr-scenes, #compare .sr-stage, #compare .sr-bar")) e.hidden = true;
    return;
  }

  // Look-Daten (Name, Art, Farbe) aus allen Kameras, je Lasche
  const meta = new Map();
  for (const cam of site.cameras) for (const l of cam.looks) if (!meta.has(l.id)) meta.set(l.id, l);

  let saved = {};
  try { saved = JSON.parse(localStorage.getItem(KEY)) ?? {}; } catch { /* ohne Speicher */ }
  const state = {
    scene: scenes.find((s) => s.id === saved.scene) ?? scenes[0],
    look: saved.look ?? "CHROME",
    split: 50,
  };
  const store = () => {
    try { localStorage.setItem(KEY, JSON.stringify({ scene: state.scene.id, look: state.look })); } catch { /* egal */ }
  };

  const frame = $("sr-frame");
  const base = $("sr-base");
  const top = $("sr-look");
  const split = $("sr-split");
  const compare = $("sr-compare");

  // Szenen als kleine Bilder
  const sceneButtons = scenes.map((s) => {
    const daylight = s.shots.find((x) => x.look === BASE);
    const b = el("button", { type: "button", className: "sr-scene", title: `${s.title} · ${s.camera}` },
      el("img", { src: daylight.src, alt: "", loading: "lazy" }),
      el("span", { textContent: s.title }));
    b.addEventListener("click", () => { state.scene = s; renderScene(); });
    return b;
  });
  $("sr-scenes").append(...sceneButtons);

  const preload = (scene) => { for (const s of scene.shots) new Image().src = s.src; };

  function renderScene() {
    const scene = state.scene;
    sceneButtons.forEach((b, k) => b.setAttribute("aria-pressed", String(scenes[k] === scene)));
    const daylight = scene.shots.find((x) => x.look === BASE);
    base.src = daylight.src;
    base.alt = `${scene.title}, Daylight 100`;
    frame.style.setProperty("--ar", `${daylight.w} / ${daylight.h}`);
    frame.style.setProperty("--wh", String(daylight.w / daylight.h));
    frame.classList.toggle("portrait", daylight.h > daylight.w);

    const box = $("sr-looks");
    box.replaceChildren(...scene.shots.map((shot) => {
      const l = meta.get(shot.look);
      const b = el("button", { type: "button", className: "filter sr-pick", value: shot.look },
        el("span", { className: "sr-dot", ariaHidden: "true" }), l?.name ?? shot.look);
      b.style.setProperty("--accent", l?.accent ?? "#888");
      if (shot.sim) b.title = "simulated";
      b.addEventListener("click", () => { state.look = shot.look; renderLook(); });
      return b;
    }));
    if (!scene.shots.some((s) => s.look === state.look)) {
      state.look = scene.shots.find((s) => s.look !== BASE)?.look ?? BASE;
    }
    const sims = scene.shots.filter((s) => s.sim).length;
    $("sr-note").textContent = sims
      ? `${sims === scene.shots.length - 1 ? "All looks" : `${sims} of ${scene.shots.length - 1} looks`} in this scene are still previews rendered on a PC from the Daylight 100 photo, marked “simulated”. I'll replace them with photos developed on the camera.`
      : "";
    renderLook();
    preload(scene);
  }

  function renderLook() {
    const scene = state.scene;
    const shot = scene.shots.find((s) => s.look === state.look);
    const l = meta.get(shot.look);
    const isBase = shot.look === BASE;
    for (const b of $("sr-looks").children) b.setAttribute("aria-pressed", String(b.value === shot.look));

    top.src = shot.src;
    top.alt = `${scene.title}, ${l?.name ?? shot.look}`;
    const comparing = compare.checked && !isBase;
    frame.classList.toggle("comparing", comparing);
    split.hidden = !comparing;
    compare.disabled = isBase;
    $("sr-side-look").textContent = l?.name ?? shot.look;
    $("sr-sim").hidden = !shot.sim;
    frame.style.setProperty("--accent", l?.accent ?? "#888");

    $("sr-name").textContent = l?.name ?? shot.look;
    $("sr-desc").textContent = l ? `${l.kind} · ${l.desc}` : "";
    $("sr-meta").textContent = `${scene.camera} · ${shot.sim ? "simulated on a PC" : isBase ? "straight from the camera" : "developed on the camera from the DNG"}`;
    store();
  }

  const setSplit = (v) => {
    state.split = Math.min(100, Math.max(0, v));
    frame.style.setProperty("--split", `${state.split}%`);
    split.value = state.split;
  };
  split.addEventListener("input", () => setSplit(Number(split.value)));
  compare.addEventListener("change", renderLook);

  // Ziehen direkt auf dem Bild, auch auf Touch: die Position folgt dem Zeiger
  let dragging = false;
  const fromEvent = (e) => {
    const r = frame.getBoundingClientRect();
    setSplit(((e.clientX - r.left) / r.width) * 100);
  };
  frame.addEventListener("pointerdown", (e) => {
    if (!frame.classList.contains("comparing")) return;
    dragging = true;
    frame.setPointerCapture(e.pointerId);
    fromEvent(e);
  });
  frame.addEventListener("pointermove", (e) => { if (dragging) fromEvent(e); });
  frame.addEventListener("pointerup", () => { dragging = false; });
  frame.addEventListener("pointercancel", () => { dragging = false; });

  // Pfeiltasten in der Look-Leiste: naechster / voriger Look
  $("sr-looks").addEventListener("keydown", (e) => {
    if (e.key !== "ArrowRight" && e.key !== "ArrowLeft") return;
    const shots = state.scene.shots;
    const i = shots.findIndex((s) => s.look === state.look);
    const next = shots[(i + (e.key === "ArrowRight" ? 1 : -1) + shots.length) % shots.length];
    state.look = next.look;
    renderLook();
    [...$("sr-looks").children].find((b) => b.value === next.look)?.focus();
    e.preventDefault();
  });

  setSplit(50);
  renderScene();
}
