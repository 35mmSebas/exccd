#!/usr/bin/env python3
"""
Wandelt Film-Laschen (PNG/JPG) in das Kameraformat .FLM fuer die
Filmsimulations-Auswahl der IXUS 60 (SD600) um.

  make_labels.py <quellordner> <zielordner> [vorschauordner]

Quelldateien heissen wie der Label-Schluessel in core/filmsim.c
(POSTCRD.png, SILVER.png, ...). Ergebnis: <zielordner>/POSTCRD.FLM usw.

Display-Fakten:
  * CHDK-Bitmap 360 x 240, Display physisch 4:3 -> ein Bitmap-Pixel ist
    320/360 = 0,889 so breit wie hoch. Quellbilder werden deshalb in
    quadratischen Pixeln eingepasst und danach horizontal um 1,125 gestreckt.
  * Feste Canon-Palette (Typ 1, 16 Eintraege). Genutzt werden die
    Farbindizes, die CHDK selbst im Aufnahmemodus verwendet.

Dateiformat .FLM:
  "FLM1"  u16 breite  u16 hoehe   (little endian)
  danach RLE-Paare (anzahl 1..255, farbindex), zeilenweise fortlaufend.
  Farbindex 0x00 = transparent (wird nicht gezeichnet).
"""
import os, sys, struct
from PIL import Image

# Zielbereich fuer die Lasche (Bitmap-Pixel) - Rest ist Namensleiste unten
AREA_W, AREA_H = 360, 196
PIXEL_ASPECT = 320 / 360
NCOLORS = 28                     # Hauptfarben je Lasche vor der Palettenzuordnung

# Palette der SD600 im Aufnahmemodus: 256 Indizes, aus einem Foto des
# CHDK-Palettenrasters gemessen (Index = Zeile*16 + Spalte). Normierung:
# Schwarz 0xFF abziehen, Farbstich ueber Weiss 0x11 neutralisieren, dann so
# skalieren, dass die hellsten Felder (98. Perzentil) 255 erreichen.
import json
_here = os.path.dirname(os.path.abspath(__file__))
def _load_palette(fn):
    raw = {int(k, 16): v for k, v in json.load(open(fn))["roh_rgb"].items()}
    blk, wht = raw[0xFF], raw[0x11]
    span = [wht[c] - blk[c] for c in range(3)]
    gain = [sum(span) / 3 / span[c] for c in range(3)]
    d = {i: [(v[c] - blk[c]) * gain[c] for c in range(3)] for i, v in raw.items()}
    peaks = sorted(max(v) for v in d.values())
    r = (len(peaks) - 1) * 0.98
    top = peaks[int(r)] + (peaks[min(int(r) + 1, len(peaks) - 1)] - peaks[int(r)]) * (r - int(r))
    return {i: tuple(min(max(c / top * 255, 0.0), 255.0) for c in v) for i, v in d.items()}
PALETTE = _load_palette(os.path.join(_here, "palette_sd600_foto.json"))

# Nur Indizes, die auf dem Display nachweislich deckend erscheinen: alle Farben
# der Laschen aus v1.8.2 plus die CHDK-Grundfarben. Andere Eintraege koennen
# (halb)transparent sein oder weichen stark vom Messwert ab (0xEE ist z.B.
# pink-rot, nicht gelb).
CANDIDATES = {
    0x11, 0x12, 0x15, 0x1B, 0x1F, 0x21, 0x26, 0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x2F,
    0x31, 0x32, 0x34, 0x36, 0x39, 0x3C, 0x3F, 0x44, 0x45, 0x46, 0x49, 0x4A, 0x51,
    0x53, 0x54, 0x56, 0x5A, 0x5B, 0x5C, 0x5E, 0x5F, 0x62, 0x64, 0x67, 0x69, 0x6F,
    0x76, 0x78, 0x7B, 0x7F, 0x83, 0x84, 0x87, 0x88, 0x89, 0x8E, 0x8F, 0x92, 0x94,
    0x98, 0x99, 0x9C, 0x9F, 0xA1, 0xA2, 0xA3, 0xA9, 0xAB, 0xAC, 0xB3, 0xB4, 0xB5,
    0xB7, 0xBB, 0xBF, 0xC4, 0xCD, 0xD1, 0xD6, 0xDC, 0xE3, 0xE5, 0xE6, 0xE7, 0xEB,
    0xEC, 0xF1, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF,
    # CHDK-Grundfarben
    0x22, 0x55, 0x25, 0xDF, 0xDD, 0x66, 0xCC,
}
# Deckkraft aus dem Palettendump der Kamera (System -> Farbpalette speichern,
# PALREC.BIN): 16 Grundfarben zu je A, Y, U, V. Ein Farbindex 0xXY mischt die
# Grundfarben X und Y, auch deren Alpha. Nur Indizes, bei denen beide Haelften
# voll deckend sind (A = 0xFF), kommen in Frage - sonst scheint das Sucherbild
# durch (z.B. 0xF4, 0x44, 0x89).
def _opaque(fn):
    d = open(fn, "rb").read()
    full = {k for k in range(16) if d[4 * k] == 0xFF}
    return {i for i in range(256) if (i >> 4) in full and (i & 15) in full}
OPAQUE = _opaque(os.path.join(_here, "palette_sd600_rec.bin"))
CANDIDATES &= OPAQUE

# Die Palette hat keine neutralen hellen Grautoene (Grau wirkt blaugrau).
# Unbunte Motivfarben deshalb fest auf diese drei Stufen legen.
GREYS = ((0xFA, 28), (0x3F, 48), (0x1F, 66))     # (Index, Helligkeit L*)

def _lab(rgb):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(c) for c in rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.9505
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.089
    f = lambda t: t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    return (116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z)))

_PAL_LAB = {}
def nearest(rgb, pal):
    if id(pal) not in _PAL_LAB:
        _PAL_LAB[id(pal)] = {i: _lab(c) for i, c in pal.items()}
    L = _lab(rgb)
    chroma = (L[1] ** 2 + L[2] ** 2) ** 0.5
    plab = _PAL_LAB[id(pal)]
    if chroma < 8 and 22 < L[0] < 88:
        return min(GREYS, key=lambda g: abs(g[1] - L[0]))[0]
    best, bd = None, 1e18
    for idx, (l2, a2, b2) in plab.items():
        if idx not in CANDIDATES: continue
        # Farbton wichtiger als Helligkeit: flache Grafik bleibt erkennbar
        d = (L[0] - l2) ** 2 + 2.5 * ((L[1] - a2) ** 2 + (L[2] - b2) ** 2)
        if d < bd:
            best, bd = idx, d
    return best

# Farbkorrekturen je Lasche: {Schluessel: {Kamera-Index: Ersatz-Index}}
# (die EXCCD-Laschen aus make_label_art.py brauchen keine)
OVERRIDES = {}

def convert(src, pal, area_h=None):
    im = Image.open(src).convert("RGBA")
    bg = Image.new("RGBA", im.size, (0, 0, 0, 255))
    im = Image.alpha_composite(bg, im).convert("RGB")
    # in quadratischen Pixeln einpassen
    area_w_sq = AREA_W * PIXEL_ASPECT
    s = min(area_w_sq / im.width, (area_h or AREA_H) / im.height)
    w_sq, h = im.width * s, int(round(im.height * s))
    w = int(round(w_sq / PIXEL_ASPECT))           # horizontal strecken
    # Startbildschirm: harte Kanten (kein Mischen zu Zwischenfarben)
    im = im.resize((w, h), Image.BOX if area_h else Image.LANCZOS)
    # erst auf wenige Hauptfarben reduzieren (saubere Flaechen statt Rauschen),
    # dann jede Hauptfarbe auf die naechste Kamerafarbe abbilden
    q = im.quantize(colors=NCOLORS, method=Image.Quantize.MAXCOVERAGE, dither=Image.Dither.NONE)
    qpal = q.getpalette()
    cmap = {i: nearest(tuple(qpal[3*i:3*i+3]), pal) for i in range(NCOLORS)}
    px = q.load()
    idx = [[cmap[px[x, y]] for x in range(w)] for y in range(h)]
    return w, h, idx

def encode(w, h, idx):
    out = bytearray(b"FLM1" + struct.pack("<HH", w, h))
    flat = [v for row in idx for v in row]
    i = 0
    while i < len(flat):
        c = flat[i]; n = 1
        while i + n < len(flat) and flat[i + n] == c and n < 255:
            n += 1
        out += bytes((n, c))
        i += n
    return bytes(out)

def preview(w, h, idx, pal):
    """So sieht es auf dem Display aus (inkl. Pixel-Seitenverhaeltnis)."""
    im = Image.new("RGB", (360, 240), (0, 0, 0))
    px = im.load()
    ox, oy = (360 - w) // 2, (4 if h <= AREA_H else 0)
    for y in range(h):
        for x in range(w):
            px[ox + x, oy + y] = tuple(int(round(c)) for c in pal.get(idx[y][x], (0, 0, 0)))
    return im.resize((320, 240), Image.LANCZOS)

if __name__ == "__main__":
    srcdir, dstdir = sys.argv[1], sys.argv[2]
    prevdir = sys.argv[3] if len(sys.argv) > 3 else None
    os.makedirs(dstdir, exist_ok=True)
    if prevdir: os.makedirs(prevdir, exist_ok=True)
    for f in sorted(os.listdir(srcdir)):
        key, ext = os.path.splitext(f)
        if ext.lower() not in (".png", ".jpg", ".jpeg"):
            continue
        key = key.upper()[:8]
        w, h, idx = convert(os.path.join(srcdir, f), PALETTE, 240 if key == "SPLASH" else None)
        ov = OVERRIDES.get(key, {})
        if ov:
            idx = [[ov.get(v, v) for v in row] for row in idx]
        data = encode(w, h, idx)
        open(os.path.join(dstdir, key + ".FLM"), "wb").write(data)
        if prevdir:
            preview(w, h, idx, PALETTE).save(os.path.join(prevdir, key + ".png"))
        print(f"{key}.FLM  {w}x{h}  {len(data)} Bytes")
