#!/usr/bin/env python3
"""
Baut die Download-Webseite (GitHub Pages) aus einem fertigen Firmware-ZIP.

    python tools/site/build_site.py <exccd-...-full.zip>... <ausgabe> [--version vX.Y.Z]

Ein Firmware-ZIP je Kamera; welche Kamera, steht im Dateinamen
(<plattform>-<sub> wie in tools/filmsim/cameras.txt). Die Seite zeigt die
Kameras in einem Auswahlmenue, Kameras mit Status "test" als Beta.

Die Seite setzt die ZIP fuer die SD-Karte im Browser zusammen, ohne Build pro
Download. Dafuer wird jedes Firmware-ZIP hier zerlegt:

    data/<kamera>/base.zip           alles ausser den Look-Dateien
    data/<kamera>/looks/<ID>.zip     je Look: Kurve + DR-Varianten, Lasche (.FLM)
    data/<kamera>/looks/<ID>.png     Laschen-Vorschau
    data/<kamera>/ver.req, vers.req  zum Pruefen der Firmware-Version
    data/samples/<ID>-*.jpg          Beispielfotos aus site/samples (falls vorhanden),
                                     <ID>-sim-<n>.jpg = am PC simuliert, die Seite markiert sie
    data/site.json                   Version, Kameras, Looks mit FILMS.TXT-Zeile und
                                     Kurvenrezept (fuer die WB-Verschiebung, site/curves.js)

Die Looks kommen aus CHDK/FILMS/EXAMPLE.TXT (gleich der eingebauten Tabelle),
Art/Beschreibung/Akzentfarbe aus tools/filmsim/make_label_art.py.
"""

import argparse, glob, json, os, re, shutil, struct, sys, zipfile
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "filmsim"))
import make_label_art  # noqa: E402
import make_curves  # noqa: E402

CAMERAS_TXT = os.path.join(ROOT, "tools", "filmsim", "cameras.txt")

# Was die Seite je Kamera zusaetzlich zu cameras.txt braucht. Neue Kameras
# bekommen hier einen Eintrag.
#   other_firmware: bekannte Versionen, die (noch) nicht unterstuetzt werden
#   boot: Dateisystem fuer den Autostart (fat16 = nur Karten bis 2 GB),
#         gleiche Regeln wie tools/installer/core.c
#   firm_update: Start per MENU -> Firm Update moeglich (PS.FIR im ZIP)
CAMERA_META = {
    "ixus60_sd600": {"name": "Canon Digital IXUS 60 / PowerShot SD600",
                     "other_firmware": ["1.00a"], "boot": "fat16", "firm_update": True},
    "ixus132_elph115": {"name": "Canon IXUS 132 / PowerShot ELPH 115 IS",
                        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "ixus145_elph135": {"name": "Canon IXUS 145 / PowerShot ELPH 135",
                        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "sx110is": {"name": "Canon PowerShot SX110 IS",
                "other_firmware": [], "boot": "fat16", "firm_update": False},
    # 1.00c/1.00d baut CHDK auch; Installer und Seite kennen bisher nur eine Version je Kamera
    "a2400": {"name": "Canon PowerShot A2400 IS",
              "other_firmware": ["1.00c", "1.00d"], "boot": "fat32", "firm_update": False},
    "ixus65_sd630": {"name": "Canon Digital IXUS 65 / PowerShot SD630",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "ixus135_elph120": {"name": "Canon IXUS 135 / PowerShot ELPH 120 IS",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "ixus140_elph130": {"name": "Canon IXUS 140 / PowerShot ELPH 130 IS",
        "other_firmware": ["1.00b"], "boot": "fat32", "firm_update": False},
    "ixus150_elph140": {"name": "Canon IXUS 150 / PowerShot ELPH 140 IS",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "ixus70_sd1000": {"name": "Canon Digital IXUS 70 / PowerShot SD1000",
        "other_firmware": ["1.00c", "1.01b", "1.01a"], "boot": "fat16", "firm_update": False},
    "ixus75_sd750": {"name": "Canon Digital IXUS 75 / PowerShot SD750",
        "other_firmware": ["1.00b", "1.01a", "1.01b"], "boot": "fat16", "firm_update": False},
    "ixus85_sd770": {"name": "Canon Digital IXUS 85 IS / PowerShot SD770 IS",
        "other_firmware": ["1.00b"], "boot": "fat16", "firm_update": False},
    "ixus90_sd790": {"name": "Canon Digital IXUS 90 IS / PowerShot SD790 IS",
        "other_firmware": ["1.00c", "1.00e"], "boot": "fat16", "firm_update": False},
    "a590": {"name": "Canon PowerShot A590 IS",
        "other_firmware": ["1.00e"], "boot": "fat16", "firm_update": False},
    "a570": {"name": "Canon PowerShot A570 IS",
        "other_firmware": ["1.00e"], "boot": "fat16", "firm_update": False},
    "a720": {"name": "Canon PowerShot A720 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a1000": {"name": "Canon PowerShot A1000 IS",
        "other_firmware": ["1.00a"], "boot": "fat16", "firm_update": False},
    "a2000": {"name": "Canon PowerShot A2000 IS",
        "other_firmware": ["1.00a", "1.00b"], "boot": "fat16", "firm_update": False},
    "a480": {"name": "Canon PowerShot A480",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "s95": {"name": "Canon PowerShot S95",
        "other_firmware": ["1.00e", "1.00h", "1.00i"], "boot": "fat16", "firm_update": False},
    "g12": {"name": "Canon PowerShot G12",
        "other_firmware": ["1.00c", "1.00e", "1.00f"], "boot": "fat16", "firm_update": False},
    "sx130is": {"name": "Canon PowerShot SX130 IS",
        "other_firmware": ["1.01c", "1.01d"], "boot": "fat16", "firm_update": False},
    "g11": {"name": "Canon PowerShot G11",
        "other_firmware": ["1.00f", "1.00j", "1.00h", "1.00k"], "boot": "fat16", "firm_update": False},
    "ixus80_sd1100": {"name": "Canon Digital IXUS 80 IS / PowerShot SD1100 IS",
        "other_firmware": ["1.00c", "1.01b"], "boot": "fat16", "firm_update": False},
    "ixus95_sd1200": {"name": "Canon Digital IXUS 95 IS / PowerShot SD1200 IS",
        "other_firmware": ["1.00b"], "boot": "fat16", "firm_update": False},
    "ixus100_sd780": {"name": "Canon Digital IXUS 100 IS / PowerShot SD780 IS",
        "other_firmware": ["1.00b"], "boot": "fat16", "firm_update": False},
    "ixus105_sd1300": {"name": "Canon IXUS 105 / PowerShot SD1300 IS",
        "other_firmware": ["1.00b", "1.00c"], "boot": "fat16", "firm_update": False},
    "ixus120_sd940": {"name": "Canon IXUS 120 IS / PowerShot SD940 IS",
        "other_firmware": ["1.00e", "1.01a", "1.02c", "1.03b"], "boot": "fat16", "firm_update": False},
    "ixus130_sd1400": {"name": "Canon IXUS 130 / PowerShot SD1400 IS",
        "other_firmware": ["1.00a"], "boot": "fat16", "firm_update": False},
    "ixus200_sd980": {"name": "Canon IXUS 200 IS / PowerShot SD980 IS",
        "other_firmware": ["1.00c", "1.01c"], "boot": "fat16", "firm_update": False},
    "ixus860_sd870": {"name": "Canon Digital IXUS 860 IS / PowerShot SD870 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "ixus870_sd880": {"name": "Canon Digital IXUS 870 IS / PowerShot SD880 IS",
        "other_firmware": ["1.00e", "1.01a"], "boot": "fat16", "firm_update": False},
    "ixus960_sd950": {"name": "Canon Digital IXUS 960 IS / PowerShot SD950 IS",
        "other_firmware": ["1.00c"], "boot": "fat16", "firm_update": False},
    "ixus970_sd890": {"name": "Canon Digital IXUS 970 IS / PowerShot SD890 IS",
        "other_firmware": ["1.00b", "1.00c", "1.00d"], "boot": "fat16", "firm_update": False},
    "ixus980_sd990": {"name": "Canon Digital IXUS 980 IS / PowerShot SD990 IS",
        "other_firmware": ["1.00e"], "boot": "fat16", "firm_update": False},
    "ixus950_sd850": {"name": "Canon Digital IXUS 950 IS / PowerShot SD850 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "ixus850_sd800": {"name": "Canon Digital IXUS 850 IS / PowerShot SD800 IS",
        "other_firmware": ["1.00d"], "boot": "fat16", "firm_update": False},
    "ixus900_sd900": {"name": "Canon Digital IXUS 900 Ti / PowerShot SD900",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "ixus800_sd700": {"name": "Canon Digital IXUS 800 IS / PowerShot SD700 IS",
        "other_firmware": ["1.00b", "1.01a"], "boot": "fat16", "firm_update": False},
    "ixus55_sd450": {"name": "Canon Digital IXUS 55 / PowerShot SD450",
        "other_firmware": ["1.00b", "1.00c"], "boot": "fat16", "firm_update": False},
    "ixus750_sd550": {"name": "Canon Digital IXUS 750 / PowerShot SD550",
        "other_firmware": ["1.00f", "1.00g"], "boot": "fat16", "firm_update": False},
    "ixus700_sd500": {"name": "Canon Digital IXUS 700 / PowerShot SD500",
        "other_firmware": ["1.01a"], "boot": "fat16", "firm_update": False},
    "ixus50_sd400": {"name": "Canon Digital IXUS 50 / PowerShot SD400",
        "other_firmware": ["1.01a"], "boot": "fat16", "firm_update": False},
    "ixus160_elph160": {"name": "Canon IXUS 160 / PowerShot ELPH 160",
        "other_firmware": ["1.00b"], "boot": "fat32", "firm_update": False},
    "ixus175_elph180": {"name": "Canon IXUS 175 / PowerShot ELPH 180",
        "other_firmware": ["1.00b", "1.00a"], "boot": "fat32", "firm_update": False},
    "a460": {"name": "Canon PowerShot A460",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a530": {"name": "Canon PowerShot A530",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a540": {"name": "Canon PowerShot A540",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a550": {"name": "Canon PowerShot A550",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a560": {"name": "Canon PowerShot A560",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a580": {"name": "Canon PowerShot A580",
        "other_firmware": ["1.00c"], "boot": "fat16", "firm_update": False},
    "a610": {"name": "Canon PowerShot A610",
        "other_firmware": ["1.00e", "1.00d"], "boot": "fat16", "firm_update": False},
    "a620": {"name": "Canon PowerShot A620",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a630": {"name": "Canon PowerShot A630",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a640": {"name": "Canon PowerShot A640",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a650": {"name": "Canon PowerShot A650 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a700": {"name": "Canon PowerShot A700",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a710": {"name": "Canon PowerShot A710 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a800": {"name": "Canon PowerShot A800",
        "other_firmware": ["1.00b"], "boot": "fat32", "firm_update": False},
    "a810": {"name": "Canon PowerShot A810",
        "other_firmware": ["1.00b", "1.00d"], "boot": "fat32", "firm_update": False},
    "a1100": {"name": "Canon PowerShot A1100 IS",
        "other_firmware": ["1.00b"], "boot": "fat16", "firm_update": False},
    "a1200": {"name": "Canon PowerShot A1200",
        "other_firmware": ["1.00b", "1.00a"], "boot": "fat32", "firm_update": False},
    "a1300": {"name": "Canon PowerShot A1300",
        "other_firmware": ["1.00b", "1.00d"], "boot": "fat32", "firm_update": False},
    "a1400": {"name": "Canon PowerShot A1400",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "a2200": {"name": "Canon PowerShot A2200",
        "other_firmware": ["1.00b", "1.00c"], "boot": "fat32", "firm_update": False},
    "a2300": {"name": "Canon PowerShot A2300",
        "other_firmware": ["1.00c", "1.00e"], "boot": "fat32", "firm_update": False},
    "a2500": {"name": "Canon PowerShot A2500",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "a2600": {"name": "Canon PowerShot A2600",
        "other_firmware": ["1.00d"], "boot": "fat32", "firm_update": False},
    "a3000": {"name": "Canon PowerShot A3000 IS",
        "other_firmware": ["1.00b", "1.00c"], "boot": "fat16", "firm_update": False},
    "a3100": {"name": "Canon PowerShot A3100 IS",
        "other_firmware": ["1.00a", "1.00b"], "boot": "fat16", "firm_update": False},
    "a3200": {"name": "Canon PowerShot A3200 IS",
        "other_firmware": ["1.00a", "1.00c"], "boot": "fat32", "firm_update": False},
    "a3300": {"name": "Canon PowerShot A3300 IS",
        "other_firmware": ["1.00a", "1.00c"], "boot": "fat32", "firm_update": False},
    "a3400": {"name": "Canon PowerShot A3400 IS",
        "other_firmware": ["1.00f", "1.01a"], "boot": "fat32", "firm_update": False},
    "a4000": {"name": "Canon PowerShot A4000 IS",
        "other_firmware": ["1.00c", "1.01a", "1.02a"], "boot": "fat32", "firm_update": False},
    "a490": {"name": "Canon PowerShot A490",
        "other_firmware": ["1.00d", "1.00e"], "boot": "fat16", "firm_update": False},
    "a495": {"name": "Canon PowerShot A495",
        "other_firmware": ["1.00d", "1.00e"], "boot": "fat16", "firm_update": False},
    "sx100is": {"name": "Canon PowerShot SX100 IS",
        "other_firmware": ["1.00b"], "boot": "fat16", "firm_update": False},
    "sx10": {"name": "Canon PowerShot SX10 IS",
        "other_firmware": ["1.00c", "1.01a", "1.01b", "1.02b"], "boot": "fat16", "firm_update": False},
    "sx20": {"name": "Canon PowerShot SX20 IS",
        "other_firmware": ["1.00f", "1.02b", "1.02c"], "boot": "fat16", "firm_update": False},
    "sx30": {"name": "Canon PowerShot SX30 IS",
        "other_firmware": ["1.00e", "1.00h", "1.00l", "1.00m", "1.00n"], "boot": "fat16", "firm_update": False},
    "sx120is": {"name": "Canon PowerShot SX120 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "sx150is": {"name": "Canon PowerShot SX150 IS",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "sx160is": {"name": "Canon PowerShot SX160 IS",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "sx200is": {"name": "Canon PowerShot SX200 IS",
        "other_firmware": ["1.00d"], "boot": "fat16", "firm_update": False},
    "sx400is": {"name": "Canon PowerShot SX400 IS",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "s3is": {"name": "Canon PowerShot S3 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "s5is": {"name": "Canon PowerShot S5 IS",
        "other_firmware": ["1.01a"], "boot": "fat16", "firm_update": False},
    "s90": {"name": "Canon PowerShot S90",
        "other_firmware": ["1.00c", "1.01a", "1.01b"], "boot": "fat16", "firm_update": False},
    "g7": {"name": "Canon PowerShot G7",
        "other_firmware": ["1.00e", "1.00g", "1.00i"], "boot": "fat16", "firm_update": False},
    "g9": {"name": "Canon PowerShot G9",
        "other_firmware": ["1.00d", "1.00g", "1.00f", "1.00h"], "boot": "fat16", "firm_update": False},
    "g10": {"name": "Canon PowerShot G10",
        "other_firmware": ["1.02a", "1.03b"], "boot": "fat16", "firm_update": False},
    "d10": {"name": "Canon PowerShot D10",
        "other_firmware": ["1.00b"], "boot": "fat16", "firm_update": False},
    "a2100": {"name": "Canon PowerShot A2100 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a410": {"name": "Canon PowerShot A410",
        "other_firmware": ["1.00e"], "boot": "fat16", "firm_update": False},
    "a420": {"name": "Canon PowerShot A420",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a430": {"name": "Canon PowerShot A430",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a450": {"name": "Canon PowerShot A450",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "a470": {"name": "Canon PowerShot A470",
        "other_firmware": ["1.00e", "1.01b", "1.01a"], "boot": "fat16", "firm_update": False},
    "ixus110_sd960": {"name": "Canon Digital IXUS 110 IS / PowerShot SD960 IS",
        "other_firmware": ["1.00b", "1.01d", "1.01g"], "boot": "fat16", "firm_update": False},
    "ixus185_elph185": {"name": "Canon IXUS 185 / PowerShot ELPH 185",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "ixus30_sd200": {"name": "Canon Digital IXUS 30 / PowerShot SD200",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "ixus40_sd300": {"name": "Canon Digital IXUS 40 / PowerShot SD300",
        "other_firmware": ["1.00j"], "boot": "fat16", "firm_update": False},
    "ixusizoom_sd30": {"name": "Canon Digital IXUS i zoom / PowerShot SD30",
        "other_firmware": ["1.00g", "1.00e", "1.00f"], "boot": "fat16", "firm_update": False},
    "s2is": {"name": "Canon PowerShot S2 IS",
        "other_firmware": ["1.00e", "1.00f", "1.00i"], "boot": "fat16", "firm_update": False},
    "s80": {"name": "Canon PowerShot S80",
        "other_firmware": ["1.00g"], "boot": "fat16", "firm_update": False},
    "sx170is": {"name": "Canon PowerShot SX170 IS",
        "other_firmware": ["1.00a"], "boot": "fat32", "firm_update": False},
    "sx210is": {"name": "Canon PowerShot SX210 IS",
        "other_firmware": [], "boot": "fat16", "firm_update": False},
    "sx410is": {"name": "Canon PowerShot SX410 IS",
        "other_firmware": ["1.00b"], "boot": "fat32", "firm_update": False},
    "d30": {"name": "Canon PowerShot D30",
        "other_firmware": [], "boot": "fat32", "firm_update": False},
    "tx1": {"name": "Canon PowerShot TX1",
        "other_firmware": ["1.00g"], "boot": "fat16", "firm_update": False},
}

FILMS_DIR = "CHDK/FILMS/"
CURVES_DIR = "CHDK/CURVES/"


def parse_films_txt(text):
    """Zeilen aus EXAMPLE.TXT, Spalten wie in core/filmsim_txt.c."""
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


def read_cameras():
    """cameras.txt: plattform, sub, status, name (Reihenfolge bleibt)."""
    cams = []
    for line in open(CAMERAS_TXT, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        plat, sub, status, name = line.split(None, 3)
        cams.append({"platform": plat, "sub": sub, "status": status, "short": name})
    return cams


def match_camera(cams, zip_path):
    base = os.path.basename(zip_path)
    hits = [c for c in cams if "%s-%s" % (c["platform"], c["sub"]) in base]
    if len(hits) != 1:
        sys.exit("%s: keine eindeutige Kamera aus cameras.txt im Dateinamen" % base)
    return hits[0]


def build_camera(cam, fw_zip, out, site_src, meta):
    """Ein Firmware-ZIP zerlegen, liefert den Eintrag fuer site.json."""
    plat, sub = cam["platform"], cam["sub"]
    extra = CAMERA_META.get(plat)
    if not extra:
        sys.exit("Keine Seiten-Daten in CAMERA_META fuer " + plat)
    cam_id = "%s-%s" % (plat, sub)
    cam_dir = os.path.join(out, "data", cam_id)
    os.makedirs(os.path.join(cam_dir, "looks"))

    with zipfile.ZipFile(fw_zip) as fw:
        names = fw.namelist()
        if extra["firm_update"] != ("PS.FIR" in names):
            sys.exit("%s: firm_update in CAMERA_META passt nicht zu PS.FIR im ZIP" % cam_id)
        looks = parse_films_txt(fw.read(FILMS_DIR + "EXAMPLE.TXT").decode("ascii"))

        look_owned = set()
        for look in looks:
            files = look_files(look)
            missing = [f for f in files if f not in names]
            if missing:
                sys.exit("%s, %s: fehlt im Firmware-ZIP: %s" % (cam_id, look["name"], ", ".join(missing)))
            look_owned.update(files)
            zpath = os.path.join(cam_dir, "looks", look["id"] + ".zip")
            copy_entries(fw, files, zpath)
            look["size"] = os.path.getsize(zpath)
            look["tone"] = curve_preview(fw.read(CURVES_DIR + look["curve"])) if look["curve"] else None
            # Rezept, damit die Seite die Kurve mit WB-Verschiebung neu rechnen kann;
            # Looks ohne Kurve bekommen dafuer eine neutrale, eigene Kurven ohne Rezept keine
            stem = os.path.splitext(look["curve"])[0] if look["curve"] else None
            look["recipe"] = make_curves.RECIPES.get(stem) if stem else {}

        # EXAMPLE.TXT nennt alle Looks; die Download-ZIP bekommt stattdessen
        # eine FILMS.TXT mit genau den gewaehlten Looks.
        base = [n for n in names if n not in look_owned and n != FILMS_DIR + "EXAMPLE.TXT"]
        copy_entries(fw, base, os.path.join(cam_dir, "base.zip"))
        base_size = os.path.getsize(os.path.join(cam_dir, "base.zip"))

        # Einzeln ladbar, um vorab die Firmware-Version der Kamera zu pruefen.
        # Aeltere Kameras lesen VER.REQ, neuere VERS.REQ (wie tools/installer)
        req = fw.read("vers.req")
        for name in ("ver.req", "vers.req"):
            with open(os.path.join(cam_dir, name), "wb") as f:
                f.write(req)

    samples_out = os.path.join(out, "data", "samples")
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

    curve = {"black": make_curves.BLACK, "white": make_curves.WHITE,
             "gamma": make_curves.GAMMA, "wb": list(make_curves.WB)}
    print("%s: %d Looks, Basis %d Dateien" % (cam_id, len(looks), len(base)))
    return {"id": cam_id, "name": extra["name"], "short": cam["short"], "status": cam["status"],
            "firmware": "%s.%s" % (sub[0], sub[1:]), "other_firmware": extra["other_firmware"],
            "boot": extra["boot"], "firm_update": extra["firm_update"],
            "base_size": base_size, "curve": curve, "looks": looks}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fw_zip", nargs="+", help="Firmware-ZIPs, eines je Kamera")
    ap.add_argument("out")
    ap.add_argument("--version", help="Release-Version, sonst aus core/filmsim.h")
    args = ap.parse_args()

    version = args.version
    if not version:
        h = open(os.path.join(ROOT, "core", "filmsim.h"), encoding="ascii").read()
        version = re.search(r'#define FILMSIM_VERSION "(.*)"', h).group(1)

    cams = read_cameras()
    picked = [(match_camera(cams, z), z) for z in args.fw_zip]
    plats = [c["platform"] for c, _ in picked]
    if len(set(plats)) != len(plats):
        sys.exit("Je Kamera nur ein Firmware-ZIP")
    # Reihenfolge wie cameras.txt, stabile Kameras zuerst (die erste ist die Vorauswahl)
    picked.sort(key=lambda cz: (cz[0]["status"] != "stabil", cams.index(cz[0])))

    site_src = os.path.join(ROOT, "site")
    # Nur den Inhalt leeren: unter Windows laesst sich ein Ordner, in dem gerade
    # ein Webserver oder eine Shell steht, nicht loeschen
    if os.path.exists(args.out):
        for name in os.listdir(args.out):
            path = os.path.join(args.out, name)
            shutil.rmtree(path) if os.path.isdir(path) else os.remove(path)
    shutil.copytree(site_src, args.out, ignore=shutil.ignore_patterns("samples", "README.md"), dirs_exist_ok=True)

    meta = label_meta()
    site = {"version": version,
            "cameras": [build_camera(c, z, args.out, site_src, meta) for c, z in picked]}
    with open(os.path.join(args.out, "data", "site.json"), "w", encoding="utf-8") as f:
        json.dump(site, f, indent=1)

    print("%s: %d Kamera(s) -> %s" % (version, len(site["cameras"]), args.out))


if __name__ == "__main__":
    main()
