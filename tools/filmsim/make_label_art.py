#!/usr/bin/env python3
"""
Zeichnet die EXCCD-Laschen als PNG (Quelle fuer make_labels.py).

  make_label_art.py [zielordner]      (Standard: labels_src neben diesem Skript)

Swiss-Layout, fuer alle Looks gleich:
  * Kopfzeile: Wortmarke EXCCD links, Filmart rechts, darunter eine Haarlinie
  * Look-Name gross, darunter die ISO-Zahl in der Akzentfarbe
  * Motiv als weisse Line-Art rechts
  * zwei Woerter als Beschreibung mit kleinem Akzent-Quadrat
  * unten eine dezente Perforationslinie

Hintergrund ist tiefes Schwarz (0xFF), Linien und Kontur in Schiefer (0xFA), Schrift weiss, jeder Look hat genau eine
Akzentfarbe. Gezeichnet wird in 1200 x 880, auf dem Display sind das
301 x 196 Pixel (Faktor ~4,5). Linien sind deshalb mind. 9 px stark, damit
sie nach dem Verkleinern als 2-Pixel-Linie stehen bleiben.

Alle Farben stammen aus der Kamerapalette (make_labels.PALETTE).
Schrift: Arial aus dem Windows-Fontordner (nur zum Rendern, wird nicht
weitergegeben). Anderer Ordner ueber die Umgebungsvariable FONT_DIR.
"""
import math, os, sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

_here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _here)
from make_labels import PALETTE

W, H = 1200, 880
M = 64                                   # Rand links/rechts
FONT_DIR = os.environ.get("FONT_DIR", os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts"))
REGULAR = os.path.join(FONT_DIR, "arial.ttf")
BOLD = os.path.join(FONT_DIR, "arialbd.ttf")

BG, WHITE, GREY, RULE = 0xFF, 0x11, 0xA3, 0xFA   # nur deckende Farben
STROKE = 9                               # Line-Art-Strichstaerke
DW, DH = 301, 196                        # Groesse auf dem Display

def gx(px):
    """Display-Pixel -> Quellkoordinate, damit feine Elemente auf dem
    Pixelraster liegen und beim Verkleinern nicht zerfasern."""
    return px * W / DW

def gy(py):
    return py * H / DH

def rule(d, py, col):
    d.rectangle([M, gy(py), W - M, gy(py + 1) - 1], fill=col)

def C(i):
    """Kamerafarbe als RGB. 0x11 (CHDK-Weiss) als reines Weiss, sonst landet
    es bei der Zuordnung auf dem blaugrauen 0x1F."""
    return (255, 255, 255) if i == 0x11 else tuple(int(round(c)) for c in PALETTE[i])

def font(path, px):
    return ImageFont.truetype(path, px)

def text_w(d, text, f, track):
    return sum(d.textlength(ch, font=f) for ch in text) + track * (len(text) - 1)

def text_tracked(d, x, y, text, f, fill, track=0):
    """Text mit Sperrung; y ist die Oberkante der Versalien."""
    y -= f.getbbox("H")[1]
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + track

# ---------------------------------------------------------------------------
# Line-Art-Motive: (d, Mittelpunkt x, y, Groesse r, Farbe)
# ---------------------------------------------------------------------------
def _circle(d, cx, cy, r, c, w=STROKE):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=c, width=w)

def _poly(d, pts, c, close=True, w=STROKE):
    if close:
        pts = list(pts) + [pts[0], pts[1]]
    d.line(pts, fill=c, width=w, joint="curve")

def icon_sun(d, cx, cy, r, c):
    _circle(d, cx, cy, 0.42 * r, c)
    for k in range(12):
        a = k * math.pi / 6
        r0, r1 = 0.62 * r, (1.0 if k % 2 == 0 else 0.84) * r
        d.line([(cx + r0 * math.cos(a), cy + r0 * math.sin(a)),
                (cx + r1 * math.cos(a), cy + r1 * math.sin(a))], fill=c, width=STROKE)

def icon_stamp(d, cx, cy, r, c):
    # Umriss mit Zackenrand: Viertelkreis-Buchten entlang aller Kanten
    w, h = 1.45 * r, 1.85 * r
    x0, y0 = cx - w / 2, cy - h / 2
    pts = []
    def edge(ax, ay, bx, by, n):
        nx, ny = (by - ay), -(bx - ax)               # Normale nach innen
        ln = math.hypot(nx, ny); nx, ny = nx / ln, ny / ln
        for i in range(n):
            for j in range(9):
                t = (i + j / 8) / n
                s = math.sin(j / 8 * math.pi) * 0.06 * r
                pts.append((ax + (bx - ax) * t + nx * s, ay + (by - ay) * t + ny * s))
    edge(x0, y0, x0 + w, y0, 7)
    edge(x0 + w, y0, x0 + w, y0 + h, 9)
    edge(x0 + w, y0 + h, x0, y0 + h, 7)
    edge(x0, y0 + h, x0, y0, 9)
    _poly(d, pts, c)
    # Bildfeld mit Sonne und Bergen
    m = 0.2 * r
    ix0, iy0, ix1, iy1 = x0 + m, y0 + m, x0 + w - m, y0 + h - 0.5 * r
    d.rectangle([ix0, iy0, ix1, iy1], outline=c, width=STROKE)
    _circle(d, ix1 - 0.28 * r, iy0 + 0.28 * r, 0.11 * r, c)
    d.line([(ix0, iy1 - 0.12 * r), (ix0 + 0.35 * r, iy1 - 0.55 * r), (ix0 + 0.62 * r, iy1 - 0.28 * r),
            (ix0 + 0.78 * r, iy1 - 0.42 * r), (ix1, iy1 - 0.1 * r)], fill=c, width=STROKE, joint="curve")
    # Wertangabe unten als zwei Linien
    d.line([(ix0, y0 + h - 0.32 * r), (ix0 + 0.5 * r, y0 + h - 0.32 * r)], fill=c, width=STROKE)
    d.line([(ix1 - 0.25 * r, y0 + h - 0.32 * r), (ix1, y0 + h - 0.32 * r)], fill=c, width=STROKE)

def icon_heart(d, cx, cy, r, c):
    # klassische Herzkurve x = 16 sin^3 t, y = 13 cos t - 5 cos 2t - 2 cos 3t - cos 4t
    k = 0.82 * r / 16
    pts = [(cx + k * 16 * math.sin(t) ** 3,
            cy - 0.08 * r - k * (13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)))
           for t in (i * 2 * math.pi / 240 for i in range(240))]
    _poly(d, pts, c)

def icon_newspaper(d, cx, cy, r, c):
    w, h = 1.5 * r, 1.9 * r
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    d.rectangle([x0, y0, x1, y1], outline=c, width=STROKE)
    m = 0.16 * r
    # Titelzeile (dick) und Doppellinie
    d.rectangle([x0 + m, y0 + m, x1 - m, y0 + m + 0.16 * r], fill=c)
    d.line([(x0 + m, y0 + m + 0.3 * r), (x1 - m, y0 + m + 0.3 * r)], fill=c, width=STROKE - 3)
    # Foto als Rahmen, Textzeilen daneben und darunter
    py0 = y0 + m + 0.46 * r
    d.rectangle([x0 + m, py0, cx - 0.05 * r, py0 + 0.52 * r], outline=c, width=STROKE)
    for k in range(4):
        y = py0 + 0.03 * r + k * 0.155 * r
        d.line([(cx + 0.1 * r, y), (x1 - m - (0.15 * r if k == 3 else 0), y)], fill=c, width=STROKE - 3)
    for k in range(3):
        y = py0 + 0.72 * r + k * 0.155 * r
        d.line([(x0 + m, y), (x1 - m - (0.35 * r if k == 2 else 0), y)], fill=c, width=STROKE - 3)

def icon_softbox(d, cx, cy, r, c):
    # Softbox als Trapez, Front, Stativ und gestrichelter Lichtkegel
    bx = cx - 0.55 * r
    _poly(d, [(bx - 0.45 * r, cy - 0.45 * r), (bx, cy - 0.8 * r), (bx, cy + 0.3 * r), (bx - 0.45 * r, cy - 0.05 * r)], c)
    d.line([(bx + 0.14 * r, cy - 0.8 * r), (bx + 0.14 * r, cy + 0.3 * r)], fill=c, width=STROKE)
    sx = bx - 0.18 * r
    d.line([(sx, cy + 0.12 * r), (sx, cy + 0.95 * r)], fill=c, width=STROKE)
    d.line([(sx - 0.32 * r, cy + 0.95 * r), (sx, cy + 0.62 * r), (sx + 0.32 * r, cy + 0.95 * r)], fill=c, width=STROKE, joint="curve")
    ox, oy = bx + 0.3 * r, cy - 0.25 * r
    for a in (-0.32, 0.0, 0.32):
        for s in range(3):
            t0, t1 = 0.12 + s * 0.38, 0.32 + s * 0.38
            d.line([(ox + t0 * 1.15 * r * math.cos(a), oy + t0 * 1.15 * r * math.sin(a)),
                    (ox + t1 * 1.15 * r * math.cos(a), oy + t1 * 1.15 * r * math.sin(a))], fill=c, width=STROKE)

def icon_camera(d, cx, cy, r, c):
    w, h = 1.9 * r, 1.12 * r
    x0, y0 = cx - w / 2, cy - h / 2 + 0.08 * r
    d.rounded_rectangle([x0, y0, x0 + w, y0 + h], radius=0.14 * r, outline=c, width=STROKE)
    d.rectangle([x0 + w - 0.45 * r, y0 - 0.16 * r, x0 + w - 0.16 * r, y0], outline=c, width=STROKE)   # Ausloeser
    d.rectangle([x0 + 0.16 * r, y0 + 0.16 * r, x0 + 0.56 * r, y0 + 0.38 * r], outline=c, width=STROKE)  # Blitz
    d.rectangle([x0 + w - 0.48 * r, y0 + 0.16 * r, x0 + w - 0.18 * r, y0 + 0.34 * r], outline=c, width=STROKE)  # Sucher
    lx, ly = cx + 0.06 * r, y0 + 0.56 * h
    _circle(d, lx, ly, 0.34 * r, c)
    _circle(d, lx, ly, 0.17 * r, c)

def icon_reel(d, cx, cy, r, c):
    rr = 0.82 * r
    ccx = cx - 0.12 * r
    _circle(d, ccx, cy, rr, c)
    for k in range(5):
        a = -math.pi / 2 + k * 2 * math.pi / 5
        _circle(d, ccx + 0.47 * rr * math.cos(a), cy + 0.47 * rr * math.sin(a), 0.19 * rr, c)
    _circle(d, ccx, cy, 0.06 * rr, c)
    # abgewickelter Filmstreifen als zwei Linien
    for off in (0.0, 0.22 * r):
        d.line([(ccx, cy + rr - off), (cx + 1.08 * r, cy + rr - off)], fill=c, width=STROKE)

def icon_moon(d, cx, cy, r, c):
    # Sichel = Kreis minus versetzter Kreis; Kontur aus der Maske (Dilatation - Erosion)
    m = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(m)
    R, ox, oy, R2 = 0.8 * r, 0.38 * r, -0.28 * r, 0.7 * r
    md.ellipse([cx - R, cy - R, cx + R, cy + R], fill=255)
    md.ellipse([cx + ox - R2, cy + oy - R2, cx + ox + R2, cy + oy + R2], fill=0)
    edge = ImageChops.subtract(m.filter(ImageFilter.MaxFilter(STROKE)), m.filter(ImageFilter.MinFilter(STROKE)))
    d.bitmap((0, 0), edge, fill=c)
    # zwei feine Sterne
    for sx, sy, s in ((0.62, -0.7, 0.15), (0.95, 0.05, 0.1)):
        x, y = cx + sx * r, cy + sy * r
        d.line([(x - s * r, y), (x + s * r, y)], fill=c, width=STROKE - 2)
        d.line([(x, y - s * r), (x, y + s * r)], fill=c, width=STROKE - 2)

def icon_portrait(d, cx, cy, r, c):
    # Kopf und Schultern in einem ovalen Bilderrahmen
    d.ellipse([cx - 0.72 * r, cy - 0.95 * r, cx + 0.72 * r, cy + 0.95 * r], outline=c, width=STROKE)
    _circle(d, cx, cy - 0.28 * r, 0.27 * r, c)
    d.arc([cx - 0.5 * r, cy + 0.08 * r, cx + 0.5 * r, cy + 1.0 * r], 180, 360, fill=c, width=STROKE)

def icon_bulb(d, cx, cy, r, c):
    # Gluehbirne mit Wendel und Fassung
    by = cy - 0.25 * r
    d.arc([cx - 0.55 * r, by - 0.55 * r, cx + 0.55 * r, by + 0.55 * r], 140, 400, fill=c, width=STROKE)
    x0, x1 = cx + 0.55 * r * math.cos(math.radians(140)), cx + 0.55 * r * math.cos(math.radians(40))
    y0 = by + 0.55 * r * math.sin(math.radians(40))
    d.line([(x0, y0), (cx - 0.22 * r, cy + 0.5 * r)], fill=c, width=STROKE)
    d.line([(x1, y0), (cx + 0.22 * r, cy + 0.5 * r)], fill=c, width=STROKE)
    for k in range(3):
        y = cy + 0.5 * r + k * 0.16 * r
        d.line([(cx - 0.22 * r, y), (cx + 0.22 * r, y)], fill=c, width=STROKE)
    d.line([(cx - 0.12 * r, cy + 0.32 * r), (cx - 0.12 * r, by), (cx - 0.04 * r, by - 0.12 * r),
            (cx + 0.04 * r, by), (cx + 0.12 * r, by - 0.12 * r), (cx + 0.12 * r, cy + 0.32 * r)],
           fill=c, width=STROKE - 3, joint="curve")

def icon_hourglass(d, cx, cy, r, c):
    w, h = 0.62 * r, 0.85 * r
    for y in (cy - h, cy + h):
        d.line([(cx - w - 0.12 * r, y), (cx + w + 0.12 * r, y)], fill=c, width=STROKE)
    _poly(d, [(cx - w, cy - h), (cx + w, cy - h), (cx + 0.08 * r, cy), (cx + w, cy + h),
              (cx - w, cy + h), (cx - 0.08 * r, cy)], c)
    # Rest Sand oben, Haeufchen unten
    d.line([(cx - 0.3 * r, cy - 0.42 * r), (cx + 0.3 * r, cy - 0.42 * r)], fill=c, width=STROKE)
    _poly(d, [(cx - 0.42 * r, cy + 0.72 * r), (cx, cy + 0.42 * r), (cx + 0.42 * r, cy + 0.72 * r)], c, close=False)

def icon_cross(d, cx, cy, r, c):
    # zwei gekreuzte Pfeile
    def arrow(x0, y0, x1, y1):
        d.line([(x0, y0), (x1, y1)], fill=c, width=STROKE)
        a = math.atan2(y1 - y0, x1 - x0)
        for da in (2.6, -2.6):
            d.line([(x1, y1), (x1 + 0.3 * r * math.cos(a + da), y1 + 0.3 * r * math.sin(a + da))],
                   fill=c, width=STROKE)
    k = 0.72 * r
    arrow(cx - k, cy + k, cx + k, cy - k)
    arrow(cx - k, cy - k, cx + k, cy + k)

def icon_flower(d, cx, cy, r, c):
    # sechs Bluetenblaetter um eine Mitte
    _circle(d, cx, cy, 0.2 * r, c)
    for k in range(6):
        a = k * math.pi / 3
        pts = []
        for i in range(41):
            t = i / 40 * math.pi
            u, v = 0.27 * r + 0.62 * r * (1 - math.cos(t)) / 2, 0.24 * r * math.sin(t)
            pts.append((u, v))
        pts += [(u, -v) for u, v in reversed(pts)]
        _poly(d, [(cx + u * math.cos(a) - v * math.sin(a), cy + u * math.sin(a) + v * math.cos(a))
                  for u, v in pts], c)

def icon_city(d, cx, cy, r, c):
    # Hochhaeuser mit Fensterreihen auf einer Grundlinie
    base = cy + 0.85 * r
    d.line([(cx - 1.05 * r, base), (cx + 1.05 * r, base)], fill=c, width=STROKE)
    for x0, x1, top in ((-0.9, -0.4, -0.2), (-0.4, 0.15, -0.85), (0.15, 0.85, -0.45)):
        d.rectangle([cx + x0 * r, cy + top * r, cx + x1 * r, base], outline=c, width=STROKE)
        y = cy + (top + 0.25) * r
        while y < base - 0.2 * r:
            d.line([(cx + (x0 + 0.13) * r, y), (cx + (x1 - 0.13) * r, y)], fill=c, width=STROKE - 4)
            y += 0.22 * r
    d.line([(cx - 0.12 * r, cy - 0.85 * r), (cx - 0.12 * r, cy - 1.05 * r)], fill=c, width=STROKE)

def icon_flame(d, cx, cy, r, c):
    # Tropfenform (Spitze oben), innen eine kleinere
    def flame(s, oy):
        pts = [(cx + s * 0.75 * r * math.sin(t) * math.sin(t / 2) ** 1.5, cy + oy * r - s * 0.95 * r * math.cos(t))
               for t in (i * 2 * math.pi / 160 for i in range(160))]
        _poly(d, pts, c)
    flame(1.0, 0.0)
    flame(0.42, 0.52)

def icon_canister(d, cx, cy, r, c):
    # Filmpatrone mit herausstehender Zunge
    w, h = 0.95 * r, 1.45 * r
    x0, y0 = cx - 0.75 * r, cy - h / 2
    d.rounded_rectangle([x0, y0, x0 + w, y0 + h], radius=0.12 * r, outline=c, width=STROKE)
    d.rectangle([x0 + 0.3 * r, y0 - 0.22 * r, x0 + w - 0.3 * r, y0], outline=c, width=STROKE)
    d.line([(x0 + w, y0 + 0.3 * r), (cx + 0.95 * r, y0 + 0.3 * r), (cx + 0.95 * r, y0 + h - 0.3 * r),
            (x0 + w, y0 + h - 0.3 * r)], fill=c, width=STROKE, joint="curve")
    for k in range(3):
        x = x0 + w + 0.2 * r + k * 0.3 * r
        d.rectangle([x, y0 + 0.42 * r, x + 0.12 * r, y0 + 0.56 * r], fill=c)

# ---------------------------------------------------------------------------
# Looks: Schluessel (8.3), Name, ISO, Filmart, Beschreibung, Motiv, Akzentfarbe
# ---------------------------------------------------------------------------
LOOKS = [
    ("DAYLITE", "DAYLIGHT",  "100", "COLOR SLIDE",   "NATURAL BALANCE",    icon_sun,       0x62),
    ("POSTCRD", "POSTCARD",  "100", "COLOR SLIDE",   "RICH SATURATION",    icon_stamp,     0xDD),
    ("BLUSH",   "BLUSH",     "100", "COLOR SLIDE",   "SOFT PORTRAIT",      icon_heart,     0x32),
    ("JOURNAL", "JOURNAL",   "400", "COLOR SLIDE",   "DOCUMENTARY CHROMA", icon_newspaper, 0xF6),
    ("STUDIO",  "STUDIO",    "160", "COLOR NEG",     "NEUTRAL SHADOW",     icon_softbox,   0x1B),
    ("DRUGSTR", "DRUGSTORE", "200", "COLOR NEG",     "RETRO NEGATIVE",     icon_camera,    0x5A),
    ("SCENE",   "SCENE",     "500", "CINEMA",        "CINEMATIC FLAT",     icon_reel,      0x2E),
    ("SILVER",  "SILVER",    "400", "BLACK & WHITE", "FINE MONO",          icon_moon,      WHITE),
    ("PORTRT",  "PORTRAIT",  "400", "COLOR NEG",     "WARM SKIN",          icon_portrait,  0x6F),
    ("TUNGSTN", "TUNGSTEN",  "800", "CINEMA",        "NEON NIGHTS",        icon_bulb,      0xDF),
    ("EXPIRED", "EXPIRED",   "200", "COLOR NEG",     "FADED MEMORY",       icon_hourglass, 0x5E),
    ("CROSS",   "CROSS",     "100", "COLOR SLIDE",   "CROSS PROCESSED",    icon_cross,     0x66),
    ("VIOLET",  "VIOLET",    "400", "COLOR NEG",     "PURPLE FOLIAGE",     icon_flower,    0xE3),
    ("CONCRET", "CONCRETE",  "100", "COLOR NEG",     "MUTED URBAN",        icon_city,      0xF3),
    ("EMBER",   "EMBER",     "200", "COLOR NEG",     "RED SCALE",          icon_flame,     0x22),
    ("GRIT",    "GRIT",      "400", "BLACK & WHITE", "PUSHED GRAIN",       icon_canister,  0xE6),
]

def name_size(d):
    """Eine Schriftgroesse fuer alle Namen, passend fuer den laengsten."""
    size = 150
    while any(text_w(d, n, font(BOLD, size), -2) > 700 for _, n, *_ in LOOKS):
        size -= 2
    return size

def desc_size(d):
    """Eine Schriftgroesse fuer alle Beschreibungen, links vom Motiv."""
    size = 42
    while any(text_w(d, l[4], font(REGULAR, size), 9) > 720 - 56 for l in LOOKS):
        size -= 1
    return size

def draw_label(name, iso, kind, desc, icon, accent, nsize, dsize):
    im = Image.new("RGB", (W, H), C(0xFF))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, W - 1, H - 1], radius=40, fill=C(BG), outline=C(RULE), width=round(gx(1)))
    acc = C(accent)

    # Kopfzeile
    f = font(BOLD, 46)
    text_tracked(d, M, 60, "EXCCD", f, C(WHITE), 10)
    f = font(REGULAR, 40)
    text_tracked(d, W - M - text_w(d, kind, f, 6), 63, kind, f, C(GREY), 6)
    rule(d, 34, C(RULE))

    # Name und ISO
    f = font(BOLD, nsize)
    text_tracked(d, M - 4, 220, name, f, C(WHITE), -2)
    f = font(BOLD, 96)
    text_tracked(d, M, 400, iso, f, acc, 0)
    x = M + text_w(d, iso, f, 0) + 22
    text_tracked(d, x, 400, "ISO", font(REGULAR, 36), C(GREY), 4)

    # Motiv
    icon(d, 955, 405, 150, C(WHITE))

    # Beschreibung mit Akzent-Quadrat
    d.rectangle([M, 600, M + 26, 626], fill=acc)
    text_tracked(d, M + 56, 597 + (42 - dsize) // 2, desc, font(REGULAR, dsize), C(WHITE), 9)

    # Perforationslinie, Loecher auf ganzen Display-Pixeln
    rule(d, 156, C(RULE))
    n, pw = 16, 8
    l, r = round(M * DW / W), round((W - M) * DW / W)
    step = (r - l - pw) // (n - 1)
    l += (r - l - pw - step * (n - 1)) // 2
    for k in range(n):
        x = l + k * step
        d.rectangle([gx(x), gy(166), gx(x + pw) - 1, gy(177) - 1], fill=C(RULE))
    return im

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(_here, "labels_src")
    os.makedirs(out, exist_ok=True)
    d = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    nsize, dsize = name_size(d), desc_size(d)
    for key, name, iso, kind, desc, icon, accent in LOOKS:
        draw_label(name, iso, kind, desc, icon, accent, nsize, dsize).save(os.path.join(out, key + ".png"))
        print("->", key + ".png")
