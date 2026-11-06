#!/usr/bin/env python3
"""
Rechnet Looks am PC nach, um sie vor dem Kameratest zu beurteilen.

  simulate_looks.py sheet <foto> <ziel.jpg> [LABEL ...]    Kontaktabzug
  simulate_looks.py sample <foto> <ordner> LABEL [...]     einzelne Bilder (1200 px)

<foto> ist ein Bild mit Daylight 100 (Canons neutrales Bild). Kurven kommen aus
make_curves.RECIPES, die Tabelle aus CHDK/FILMS/EXAMPLE.TXT, die Matrizen aus
core/filmsim_raw.h. Die Regler von Custom Color und Canons Tonkurve sind nur
genaehert (Staerken an echten Fotos geprueft): Das Ergebnis zeigt die Richtung,
es ersetzt kein Foto von der Kamera.
"""
import os, re, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import make_curves as mc  # noqa: E402

G = mc.GAMMA
K_CON, K_SAT, K_HUE = 0.10, 0.16, 0.18
LUMA = np.array([0.299, 0.587, 0.114])


def mix_matrices():
    """{1: Violet, ...} aus filmsim_raw.h, damit nichts auseinanderlaeuft."""
    src = open(os.path.join(ROOT, "core", "filmsim_raw.h"), encoding="ascii").read()
    names = {1: "violet", 2: "concrete", 3: "turquoise", 4: "infra", 5: "cyano"}
    out = {}
    for k, n in names.items():
        m = re.search(r"mix_%s\[3\]\[3\] = \{(.*?)\};" % n, src, re.S)
        body = re.sub(r"//.*", "", m.group(1))
        out[k] = np.array([int(x) for x in re.findall(r"-?\d+", body)]).reshape(3, 3) / 256
    return out


def table():
    rows = {}
    for line in open(os.path.join(ROOT, "CHDK", "FILMS", "EXAMPLE.TXT"), encoding="ascii"):
        if line.strip() and not line.startswith("#"):
            c = [x.strip() for x in line.split("|")]
            rows[c[10]] = dict(name=c[0], mode=int(c[1]), con=int(c[2]), sat=int(c[4]), r=int(c[5]),
                               g=int(c[6]), b=int(c[7]), skin=int(c[8]),
                               curve=None if c[9] == "-" else c[9][:-3], mix=int(c[11]))
    return rows


def look_lut(r):
    """Je Kanal: Anzeige-Wert 0..1 -> Anzeige-Wert nach der Kurve (wie channel_curve)."""
    pts = r.get("points")
    xs = np.linspace(0, 1, 1024)
    out = []
    for ch in range(3):
        gain = r.get("gain", (1, 1, 1))[ch]
        sh = r.get("shadow", (0, 0, 0))[ch]
        hi = r.get("highlight", (0, 0, 0))[ch]
        f = mc.pchip(pts["RGB"[ch]]) if pts else None
        ys = []
        for x in xs:
            lin = x ** G * gain
            if lin >= 1:
                ys.append(x)
                continue
            t = lin ** (1 / G)
            y = f(t) if f else mc.tone(t, r)
            y += sh * 6.75 * t * (1 - t) ** 2 + hi * 6.75 * t * t * (1 - t)
            y = min(max(y, 0), 1)
            ys.append((y ** G / gain) ** (1 / G))
        out.append(np.maximum.accumulate(np.array(ys)))
    return xs, out


def hue_weight(img, center):
    mx, mn = img.max(2), img.min(2)
    c = mx - mn + 1e-6
    r, g, b = img[..., 0], img[..., 1], img[..., 2]
    h = np.where(mx == r, ((g - b) / c) % 6, np.where(mx == g, (b - r) / c + 2, (r - g) / c + 4)) * 60
    d = np.abs((h - center + 180) % 360 - 180)
    return np.clip(np.cos(np.radians(d)), 0, 1) ** 2 * np.clip(c * 3, 0, 1)


def simulate(img, look, mix):
    x = img.copy()
    if look["curve"]:
        xs, luts = look_lut(mc.RECIPES[look["curve"]])
        x = np.stack([np.interp(x[..., c], xs, luts[c]) for c in range(3)], 2)
    if look["mix"]:
        x = np.clip(x ** G @ mix[look["mix"]].T, 0, 1) ** (1 / G)
    if look["mode"] == 3:                     # S/W
        return np.repeat((x @ LUMA)[..., None], 3, 2)
    if look["mode"] == 4:                     # Sepia, genaehert
        return np.clip((x @ LUMA)[..., None] * np.array([1.08, 0.97, 0.78]), 0, 1)
    if look["mode"] == 11:
        x = x + look["con"] * K_CON * (x - 0.5) * x * (1 - x) * 4
        y = (x @ LUMA)[..., None]
        s = 1 + look["sat"] * K_SAT
        for key, hue in (("r", 0), ("g", 120), ("b", 220), ("skin", 25)):
            if look[key]:
                s = s + look[key] * K_HUE * hue_weight(x, hue)[..., None]
        x = y + s * (x - y)
    return np.clip(x, 0, 1)


def load(path, width):
    im = Image.open(path).convert("RGB")
    im = im.resize((width, round(width * im.height / im.width)), Image.LANCZOS)
    return np.asarray(im, dtype=np.float64) / 255


def to_image(x):
    return Image.fromarray((x * 255 + 0.5).astype(np.uint8))


def sheet(photo, dst, ids):
    rows, mix = table(), mix_matrices()
    ids = ids or list(rows)
    base = load(photo, 400)
    h, w = base.shape[:2]
    cols = 4
    nrows = (len(ids) + cols - 1) // cols
    im = Image.new("RGB", (cols * (w + 8) + 8, nrows * (h + 26) + 8), (24, 24, 24))
    d = ImageDraw.Draw(im)
    for i, k in enumerate(ids):
        x0, y0 = 8 + (i % cols) * (w + 8), 8 + (i // cols) * (h + 26)
        im.paste(to_image(simulate(base, rows[k], mix)), (x0, y0))
        d.text((x0 + 2, y0 + h + 5), rows[k]["name"], fill=(235, 235, 235))
    im.save(dst, quality=90)


def samples(photo, dst, ids):
    rows, mix = table(), mix_matrices()
    base = load(photo, 1200)
    os.makedirs(dst, exist_ok=True)
    for k in ids:
        path = os.path.join(dst, k + "-1.jpg")
        to_image(simulate(base, rows[k], mix)).save(path, quality=88, optimize=True)
        print("->", path)


if __name__ == "__main__":
    if sys.argv[1] == "sheet":
        sheet(sys.argv[2], sys.argv[3], sys.argv[4:])
    else:
        samples(sys.argv[2], sys.argv[3], sys.argv[4:])
