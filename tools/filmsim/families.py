#!/usr/bin/env python3
"""
Sensorfamilien: welche Kameras denselben Sensor haben (Ziel 2.0).

Kennung = Buchstabe fuer den Sensor + Ziffer fuer die DIGIC-Generation, z. B. C4
(IXUS 132/145, A2400) oder B3/B4 (gleicher 10-MP-Sensor, DIGIC III bzw. 4).
DIGIC 4+ zaehlt als 4. Dazu ein sprechender Name fuer Menschen und Webseite.

Der Sensor wird aus dem CHDK-Port gelesen (platform/<p>/platform_camera.h):
gleiche Bittiefe, gleiches Bayer-Muster und gleiche RAW-Groesse = derselbe Sensor.
Buchstaben werden nie umvergeben; neue Sensoren bekommen den naechsten freien
(nach Z: AA, AB, ...). Stellt sich heraus, dass zwei Buchstaben derselbe Sensor
sind, bleiben beide stehen.

    python tools/filmsim/families.py            Tabelle aller Kameras aus cameras.txt
    python tools/filmsim/families.py --md       dieselbe Tabelle als Markdown
"""

import os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))

RGGB, GBRG = 0x02010100, 0x01000201
BAYER = {RGGB: "RGGB", GBRG: "GBRG"}

# (Bit, Bayer, RAW-Breite, RAW-Hoehe) -> (Buchstabe, Megapixel laut Canon, WB-Gains (R, B) oder None)
SENSORS = {
    (10, RGGB, 2888, 2136): ("A", 6, None),
    (12, RGGB, 3720, 2772): ("B", 10, (470, 441)),
    (12, GBRG, 4704, 3504): ("C", 16, None),
    (10, GBRG, 3336, 2480): ("D", 8, None),
    (12, RGGB, 3744, 2784): ("E", 10, None),
    (10, RGGB, 3152, 2340): ("F", 7, None),
    (12, RGGB, 4080, 3048): ("G", 12, None),
    (10, RGGB, 2672, 1968): ("H", 5, None),
    (12, GBRG, 4104, 3048): ("I", 12, None),
    (12, RGGB, 4416, 3296): ("J", 14, None),
    (12, RGGB, 5248, 3920): ("K", 20, None),
    (10, RGGB, 3736, 2772): ("L", 10, None),
    (10, RGGB, 4104, 3048): ("M", 12, None),
    (10, GBRG, 2392, 1752): ("N", 4, None),
    (10, RGGB, 2664, 1968): ("O", 5, None),
    (10, RGGB, 3344, 2484): ("P", 8, None),
    (12, GBRG, 3720, 2772): ("Q", 10, None),
    (12, GBRG, 4464, 3276): ("R", 14, None),
    (12, GBRG, 4480, 3348): ("S", 14, None),
    (10, GBRG, 3336, 2490): ("T", 8, None),
    (10, RGGB, 2144, 1560): ("U", 3, None),
    (10, RGGB, 2144, 1564): ("V", 3, None),
    (10, RGGB, 2400, 1766): ("W", 4, None),
    (10, RGGB, 3152, 2346): ("X", 7, None),
    (12, GBRG, 3728, 2778): ("Y", 10, None),
    (12, RGGB, 4704, 3504): ("Z", 16, None),
}

DIGIC = {"20": "2", "30": "3", "40": "4", "41": "4"}
DIGIC_NAME = {"2": "DIGIC II", "3": "DIGIC III", "4": "DIGIC 4"}


def _define(hdr, name):
    m = re.search(r"^\s*#define\s+%s\s+(\S+)" % name, hdr, re.M)
    return m.group(1) if m else None


def sensor(platform):
    """(Bit, Bayer, Breite, Hoehe) aus dem CHDK-Port."""
    hdr = open(os.path.join(ROOT, "platform", platform, "platform_camera.h"), encoding="utf-8").read()
    return (int(_define(hdr, "CAM_SENSOR_BITS_PER_PIXEL") or 10), int(_define(hdr, "cam_CFAPattern"), 16),
            int(_define(hdr, "CAM_RAW_ROWPIX")), int(_define(hdr, "CAM_RAW_ROWS")))


def digic(platform):
    for line in open(os.path.join(ROOT, "camera_info.csv"), encoding="utf-8"):
        f = line.strip().split(",")
        if f[0] == platform:
            return DIGIC[f[9]]
    raise KeyError(platform)


def family(platform):
    """{"id": "C4", "sensor": "C", "name": "16 MP CCD · 12 Bit GBRG · DIGIC 4", ...} oder None (neuer Sensor)."""
    key = sensor(platform)
    if key not in SENSORS:
        return None
    letter, mp, wb = SENSORS[key]
    d = digic(platform)
    bits, cfa, w, h = key
    return {"id": letter + d, "sensor": letter, "digic": d, "mp": mp, "wb": wb,
            "name": "%d MP CCD · %d Bit %s · %s" % (mp, bits, BAYER[cfa], DIGIC_NAME[d]),
            "raw": "%d×%d" % (w, h)}


def cameras():
    """(Plattform, Status, Kurzname) aus cameras.txt."""
    out = []
    for line in open(os.path.join(ROOT, "tools", "filmsim", "cameras.txt"), encoding="utf-8"):
        if line.strip() and not line.startswith("#"):
            plat, _subs, status, name = line.split(None, 3)
            out.append((plat, status, name.strip()))
    return out


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    groups = {}
    for plat, status, name in cameras():
        f = family(plat)
        fid = f["id"] if f else "?"
        groups.setdefault(fid, (f, []))[1].append(name + (" ✓" if status == "stabil" else ""))
    order = sorted(groups, key=lambda k: (len(k) > 2, k[:-1], k[-1:]))
    if "--md" in sys.argv:
        print("| Familie | Name | RAW | Anzahl | Kameras |\n|---|---|---|---|---|")
        for k in order:
            f, names = groups[k]
            print("| **%s** | %s | %s | %d | %s |" % (k, f["name"] if f else "neu", f["raw"] if f else "",
                                                     len(names), ", ".join(names)))
    else:
        for k in order:
            f, names = groups[k]
            print("%-3s %-38s %s" % (k, f["name"] if f else "neu", ", ".join(names)))
    print("\n%d Kameras, %d Familien" % (sum(len(v[1]) for v in groups.values()), len(groups)))


if __name__ == "__main__":
    main()
