#!/usr/bin/env python3
"""
Baut die Download-Webseite (GitHub Pages) aus einem fertigen Firmware-ZIP.

    python tools/site/build_site.py <exccd-...-full.zip> <ausgabe> [--version vX.Y.Z]

Die Seite setzt die ZIP fuer die SD-Karte im Browser zusammen, ohne Build pro
Download. Dafuer wird das Firmware-ZIP hier zerlegt:

    data/<kamera>/base.zip           alles ausser den Look-Dateien
    data/<kamera>/looks/<ID>.zip     je Look: Kurve + DR-Varianten, Lasche (.FLM)
    data/<kamera>/looks/<ID>.png     Laschen-Vorschau
    data/samples/<ID>-*.jpg          Beispielfotos aus site/samples (falls vorhanden)
    data/site.json                   Version, Kameras, Looks mit FILMS.TXT-Zeile

Die Looks kommen aus CHDK/FILMS/BEISPIEL.TXT (gleich der eingebauten Tabelle),
Art/Beschreibung/Akzentfarbe aus tools/filmsim/make_label_art.py.
"""

import argparse, glob, json, os, re, shutil, struct, sys, zipfile
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "filmsim"))
import make_label_art  # noqa: E402
import make_curves  # noqa: E402

# Bisher nur eine Kamera. Weitere Modelle bekommen hier einen Eintrag und ein
# eigenes Firmware-ZIP (siehe TODO Phase 5).
CAMERA = {
    "id": "ixus60_sd600-100d",
    "name": "Canon Digital IXUS 60 / PowerShot SD600",
    "firmware": "1.00d",
    "other_firmware": ["1.00a"],
}

FILMS_DIR = "CHDK/FILMS/"
CURVES_DIR = "CHDK/CURVES/"


def parse_films_txt(text):
    """Zeilen aus BEISPIEL.TXT, Spalten wie in core/filmsim_txt.c."""
    looks = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        cols = [c.strip() for c in line.split("|")]
        name, curve, label = cols[0], cols[9], cols[10]
        if label == "-":
            sys.exit("Look ohne Lasche wird von der Webseite nicht unterstuetzt: " + name)
        looks.append({"id": label, "name": name, "cols": cols,
                      "curve": None if curve == "-" else curve})
    return looks


def look_files(look):
    files = [FILMS_DIR + look["id"] + ".FLM"]
    if look["curve"]:
        stem, ext = os.path.splitext(look["curve"])
        files += [CURVES_DIR + stem + s + ext for s in ("", "1", "2")]
    return files


def copy_entries(src, names, dst_path):
    """Eintraege komprimiert (deflate) in eine neue ZIP, Zeitstempel bleiben."""
    with zipfile.ZipFile(dst_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as dst:
        for name in names:
            info = src.getinfo(name)
            out = zipfile.ZipInfo(info.filename, info.date_time)
            out.external_attr = info.external_attr
            out.compress_type = zipfile.ZIP_STORED if info.is_dir() else zipfile.ZIP_DEFLATED
            dst.writestr(out, src.read(name))


def curve_preview(cv, steps=32):
    """Kurve fuer die Webseite: R/G/B im Anzeige-Raum (0..1, Gamma wie make_curves.py),
    damit sie wie eine normale Gradationskurve aussieht. cv = Inhalt der .CV-Datei."""
    vals = struct.unpack("<4096H", cv)
    black, white, gamma, wb = make_curves.BLACK, make_curves.WHITE, make_curves.GAMMA, make_curves.WB
    out = {}
    for key, ch in (("r", 0), ("g", 1), ("b", 3)):
        curve, pts = vals[ch * 1024:(ch + 1) * 1024], []
        for i in range(steps + 1):
            x = i / steps
            raw = min(white, int(round(black + x ** gamma * (white - black) / wb[ch])))
            lin = min(1.0, max(0.0, (curve[raw] - black) * wb[ch] / (white - black)))
            pts.append(round(lin ** (1 / gamma), 3))
        out[key] = pts
    return out


def label_meta():
    meta = {}
    for key, _name, iso, kind, desc, _icon, accent in make_label_art.LOOKS:
        r, g, b = make_label_art.C(accent)
        meta[key] = {"iso": iso, "kind": kind.title(), "desc": desc.capitalize(),
                     "accent": "#%02x%02x%02x" % (r, g, b)}
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fw_zip")
    ap.add_argument("out")
    ap.add_argument("--version", help="Release-Version, sonst aus core/filmsim.h")
    args = ap.parse_args()

    version = args.version
    if not version:
        h = open(os.path.join(ROOT, "core", "filmsim.h"), encoding="ascii").read()
        version = re.search(r'#define FILMSIM_VERSION "(.*)"', h).group(1)

    site_src = os.path.join(ROOT, "site")
    if os.path.exists(args.out):
        shutil.rmtree(args.out)
    shutil.copytree(site_src, args.out, ignore=shutil.ignore_patterns("samples", "README.md"))

    cam_dir = os.path.join(args.out, "data", CAMERA["id"])
    os.makedirs(os.path.join(cam_dir, "looks"))
    meta = label_meta()

    with zipfile.ZipFile(args.fw_zip) as fw:
        names = fw.namelist()
        looks = parse_films_txt(fw.read(FILMS_DIR + "BEISPIEL.TXT").decode("ascii"))

        look_owned = set()
        for look in looks:
            files = look_files(look)
            missing = [f for f in files if f not in names]
            if missing:
                sys.exit("%s: fehlt im Firmware-ZIP: %s" % (look["name"], ", ".join(missing)))
            look_owned.update(files)
            zpath = os.path.join(cam_dir, "looks", look["id"] + ".zip")
            copy_entries(fw, files, zpath)
            look["size"] = os.path.getsize(zpath)
            look["tone"] = curve_preview(fw.read(CURVES_DIR + look["curve"])) if look["curve"] else None

        # BEISPIEL.TXT nennt alle Looks; die Download-ZIP bekommt stattdessen
        # eine FILMS.TXT mit genau den gewaehlten Looks.
        base = [n for n in names if n not in look_owned and n != FILMS_DIR + "BEISPIEL.TXT"]
        copy_entries(fw, base, os.path.join(cam_dir, "base.zip"))
        base_size = os.path.getsize(os.path.join(cam_dir, "base.zip"))

        # Einzeln ladbar, um vorab die Firmware-Version der Kamera zu pruefen
        with open(os.path.join(args.out, "data", "vers.req"), "wb") as f:
            f.write(fw.read("vers.req"))

    samples_out = os.path.join(args.out, "data", "samples")
    for look in looks:
        m = meta.get(look["id"])
        if not m:
            sys.exit("Keine Laschen-Daten in make_label_art.py fuer " + look["id"])
        look.update(m)
        img = Image.open(os.path.join(ROOT, "tools", "filmsim", "labels_src", look["id"] + ".png"))
        img.convert("RGB").resize((600, 440), Image.LANCZOS).save(
            os.path.join(cam_dir, "looks", look["id"] + ".png"), optimize=True)
        look["samples"] = []
        for path in sorted(glob.glob(os.path.join(site_src, "samples", look["id"] + "-*.jpg"))):
            os.makedirs(samples_out, exist_ok=True)
            shutil.copy(path, samples_out)
            look["samples"].append("data/samples/" + os.path.basename(path))

    site = {"version": version, "cameras": [dict(CAMERA, base_size=base_size, looks=looks)]}
    with open(os.path.join(args.out, "data", "site.json"), "w", encoding="utf-8") as f:
        json.dump(site, f, indent=1)

    print("%s: %d Looks, Basis %d Dateien -> %s" % (version, len(looks), len(base), args.out))


if __name__ == "__main__":
    main()
