#!/usr/bin/env python3
"""Startbildschirm 'EXCCD' als PNG (4:3, quadratische Pixel)."""
import sys
from PIL import Image, ImageDraw, ImageFont
W, H = 960, 720
BLACK = (18, 18, 18); BAND = (48, 42, 36); HOLE = (235, 228, 210)
CREAM = (240, 232, 205); ORANGE = (245, 140, 35); GREY = (150, 150, 150); WHITE = (255, 255, 255)
HEAVY = "/usr/share/fonts/opentype/noto/NotoSansCJK-Black.ttc"
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
SANS = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

im = Image.new("RGB", (W, H), BLACK)
d = ImageDraw.Draw(im)
d.fontmode = "1"          # ohne Kantenglaettung -> saubere Farben auf der Kamerapalette

def strip(y0, y1, edge_top):
    d.rectangle([0, y0, W, y1], fill=BAND)
    hy0, hy1 = (y0 + 18, y0 + 70) if edge_top else (y1 - 70, y1 - 18)
    for x in range(-20, W, 96):
        d.rounded_rectangle([x, hy0, x + 50, hy1], radius=9, fill=HOLE)
    f = ImageFont.truetype(MONO, 26)
    ty = y1 - 36 if edge_top else y0 + 8
    for i, x in enumerate(range(30, W, 240)):
        n = 36 - i
        d.text((x, ty), f"▸ {n}   ▸ {n}A", font=f, fill=ORANGE)

strip(0, 120, True)
strip(H - 120, H, False)

def ctext(y, s, font, fill, spacing=0):
    if spacing:
        widths = [d.textlength(c, font=font) for c in s]
        total = sum(widths) + spacing * (len(s) - 1)
        x = (W - total) / 2
        for c, w in zip(s, widths):
            d.text((x, y), c, font=font, fill=fill); x += w + spacing
    else:
        w = d.textlength(s, font=font)
        d.text(((W - w) / 2, y), s, font=font, fill=fill)

import os
GREEN = (0, 160, 80)
GROT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fonts", "ArchivoBlack-Regular.ttf")
title = "EXCCD"
size = 200
while True:
    ft = ImageFont.truetype(GROT, size)
    if d.textlength(title, font=ft) <= W - 90: break
    size -= 4
ctext(225, title, ft, GREEN)
ctext(440, "SOOC since 2006", ImageFont.truetype(GROT, 44), GREEN, spacing=4)
im.save(sys.argv[1])
