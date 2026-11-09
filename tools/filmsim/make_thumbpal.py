"""Graustufen fuer das DNG-Vorschaubild in der Dateiauswahl (RAW entwickeln).

Waehlt aus der Kamerapalette (make_labels.PALETTE / CANDIDATES, nur deckende Eintraege)
moeglichst neutrale Grautoene, sortiert nach Helligkeit, und gibt die Zeile fuer den
EXCCD-Block in platform/<kamera>/platform_camera.h aus:

    #define FILMSIM_THUMB_GREYS  { Index, Helligkeit 0..255, ... }

Aufruf:  EXCCD_CAM=sx110is python tools/filmsim/make_thumbpal.py
Ohne Eintrag nimmt die Firmware die CHDK-Farben Schwarz, Grau, Weiss.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_labels as ml          # noqa: E402  (liest EXCCD_CAM)

MAX_LEVELS = 8
MAX_CHROMA = 12.0                 # a*/b*-Abstand, ab dem ein Grau als bunt gilt


def lab(rgb):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(c) for c in rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.9505
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.089
    f = lambda t: t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    return 116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z))


def main():
    greys = [(0.0, 0.0, 0xFF)]                # Schwarz (EXCCD-Hintergrund) zuerst
    for i in sorted(ml.CANDIDATES):
        L, a, b = lab(ml.PALETTE[i])
        c = (a * a + b * b) ** 0.5
        if c <= MAX_CHROMA:
            greys.append((L, c, i))
    if getattr(ml, "GREYS", None):            # IXUS 60: feste neutrale Stufen
        greys += [(L, 0.0, i) for i, L in ml.GREYS]
    # Gleichmaessig ueber L* verteilt: je Ziel die neutralste Farbe in der Naehe
    picked = []
    lo, hi = min(g[0] for g in greys), max(g[0] for g in greys)
    for k in range(MAX_LEVELS):
        t = lo + (hi - lo) * k / (MAX_LEVELS - 1)
        best = min(greys, key=lambda g: abs(g[0] - t) * 2 + g[1])
        if best[2] not in [p[2] for p in picked]:
            picked.append(best)
    picked.sort()
    # Helligkeit als 8-Bit-Wert auf der Skala des Vorschaubilds (sRGB-artig, L* -> 0..255)
    items = ", ".join("0x%02X,%d" % (i, round(L * 2.55)) for L, _, i in picked)
    print("    #define FILMSIM_THUMB_GREYS         { %s }" % items)
    for L, c, i in picked:
        print("    // 0x%02X  L*=%5.1f  Buntheit %4.1f" % (i, L, c), file=sys.stderr)


if __name__ == "__main__":
    main()
