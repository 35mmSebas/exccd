#!/usr/bin/env python3
"""
Bilder fuer den Showroom der Webseite (site/showroom/, Szenen in scenes.txt).

    showroom.py import <karte> [--dry-run]
        Holt in der Kamera entwickelte JPEGs von der Karte (oder einem kopierten
        Kartenordner mit DCIM/ und CHDK/FILMLOG.CSV). FILMLOG.CSV nennt je Foto
        den Film und, beim Entwickeln, die Quell-DNG; die DNG bestimmt die Szene,
        der Film den Look. Ergebnis: site/showroom/<szene>/<LABEL>.jpg, ein
        simuliertes <LABEL>-sim.jpg derselben Szene wird dabei geloescht.
        Ein Daylight-100-Foto mit der Quell-DNG als Namen (IMG_1307.JPG zu
        CRW_1307.DNG) wird als DAYLITE.jpg uebernommen.

    showroom.py sim <szene> <daylight.jpg> LABEL [...]
        Platzhalter: DAYLITE.jpg aus dem Kamera-JPEG (Daylight 100), dazu je
        Look eine PC-Simulation <LABEL>-sim.jpg (tools/filmsim/simulate_looks.py).

Bilder werden gedreht (Exif), auf 1600 px lange Seite verkleinert und ohne
Exif gespeichert.
"""

import argparse, csv, glob, os, re, sys, time
import numpy as np
from PIL import Image, ImageOps

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SHOWROOM = os.path.join(ROOT, "site", "showroom")
LONG_SIDE = 1600
QUALITY = 86
sys.path.insert(0, os.path.join(ROOT, "tools", "filmsim"))


def read_scenes():
    """scenes.txt: id | camera | source | title"""
    scenes = []
    for line in open(os.path.join(SHOWROOM, "scenes.txt"), encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        sid, cam, src, title = [c.strip() for c in line.split("|", 3)]
        scenes.append({"id": sid, "camera": cam, "source": src.upper(), "title": title})
    return scenes


def looks_by_name():
    """'Postcard 100' -> ('POSTCRD', My-Colors-Modus, Mix) aus EXAMPLE.TXT"""
    out = {}
    for line in open(os.path.join(ROOT, "CHDK", "FILMS", "EXAMPLE.TXT"), encoding="ascii"):
        if line.strip() and not line.startswith("#"):
            c = [x.strip() for x in line.split("|")]
            mix = int(c[11]) if len(c) > 11 and c[11] else 0
            out[c[0].lower()] = (c[10], int(c[1]), mix)
    return out


MC_BW = 3           # My Colors Schwarzweiss (EXAMPLE.TXT, Spalte Mode)
MIX_CYANO = 5       # Spalte Mix: Blauton
CYANO_MIN_BLUE = 10 # Mittelwert B - R; echte Cyano-Bilder 27..58, falsche -45..-11
SCENE_TYPES = {1: "Landschaft", 2: "Portraet", 3: "Nacht"}


def check_shot(path, mode, mix=0):
    """Grund, warum das Foto nicht auf die Seite darf, sonst None.
    Die Exif-Daten eines entwickelten JPEGs stammen vom Hilfsschuss. Lief der in einem
    Canon-Motivprogramm (Portraet, Landschaft ...), ignoriert die Kamera My Colors:
    S/W-Looks kommen farbig heraus, Custom Color fehlt (15.11.2026, IMG_1388-1446).
    Cyano muss blau sein: Firmware bis v1.10.2 mischte beim Entwickeln mit dem Weissabgleich
    des Hilfsschusses, unter Kunstlicht kam Cyano gelb heraus (IMG_1468-1546). Cyano steht
    dabei fuer alle Mix-Looks; Violet, Concrete & Co. lassen sich nicht so sicher pruefen."""
    im = Image.open(path)
    scene = im.getexif().get_ifd(0x8769).get(0xA406, 0)
    if scene:
        return "Hilfsschuss im Motivprogramm %s - My Colors wirkt dort nicht, im P- oder M-Modus neu entwickeln" \
               % SCENE_TYPES.get(scene, scene)
    if mode != MC_BW and mix != MIX_CYANO:
        return None
    im.draft("RGB", (320, 240))
    rgb = im.convert("RGB")
    if mode == MC_BW:
        sat = float(np.asarray(rgb.convert("HSV"))[..., 1].mean())
        if sat > 3:
            return "Schwarzweiss-Look ist farbig (Saettigung %.0f)" % sat
    if mix == MIX_CYANO:
        r, _, b = np.asarray(rgb, dtype=np.float64).reshape(-1, 3).mean(0)
        if b - r < CYANO_MIN_BLUE:
            return "Cyano ist nicht blau (Mittel R %.0f, B %.0f) - Weissabgleich beim Entwickeln, " \
                   "mit Firmware ab dist/v1.10.2-absicherung neu entwickeln; Mix-Looks der Szene pruefen" % (r, b)
    return None


def prepare(path, orient=None):
    """orient: Exif-Lage des Originals. Der Hilfsschuss beim Entwickeln traegt die Lage der
    Kamera beim Entwickeln, nicht die der Aufnahme; hochkant fotografierte Szenen liegen sonst quer."""
    im = Image.open(path)
    if orient and orient != 1 and im.getexif().get(0x112, 1) == 1:
        im.getexif()[0x112] = orient    # exif_transpose liest dieselbe (gecachte) Exif
    im = ImageOps.exif_transpose(im).convert("RGB")
    im.thumbnail((LONG_SIDE, LONG_SIDE), Image.LANCZOS)
    return im


def save(im, scene, name):
    folder = os.path.join(SHOWROOM, scene)
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name)
    # Unter Windows haelt ab und zu ein anderes Programm (Virenscanner, Indexer) die eben
    # geschriebene Datei kurz offen, dann scheitert das Ueberschreiben mit Errno 22
    for attempt in range(5):
        try:
            im.save(path, quality=QUALITY, optimize=True, progressive=True)
            return path
        except OSError:
            if attempt == 4:
                raise
            time.sleep(0.5)


def cmd_sim(args):
    import simulate_looks as sl
    rows, mix = sl.table(), sl.mix_matrices()
    base = prepare(args.photo)
    print("->", save(base, args.scene, "DAYLITE.jpg"))
    arr = np.asarray(base, dtype=np.float64) / 255
    for label in args.labels:
        if label not in rows:
            sys.exit("Unbekannter Look: " + label)
        if rows[label]["mix"] in (3, 4):
            print("   %s: Simulation zeigt Mix 3/4 falsch, uebersprungen" % label)
            continue
        if os.path.exists(os.path.join(SHOWROOM, args.scene, label + ".jpg")):
            print("   %s: echtes Foto vorhanden, keine Simulation" % label)
            continue
        print("->", save(sl.to_image(sl.simulate(arr, rows[label], mix)), args.scene, label + "-sim.jpg"))


def find_jpeg(card, rel):
    """'102CANON/IMG_7503.JPG' unter <karte>/DCIM oder direkt unter <karte>"""
    for base in (os.path.join(card, "DCIM"), card):
        p = os.path.join(base, *rel.split("/"))
        if os.path.exists(p):
            return p
        p = os.path.join(base, os.path.basename(rel))
        if os.path.exists(p):
            return p
    return None


def cmd_import(args):
    log = os.path.join(args.card, "CHDK", "FILMLOG.CSV")
    if not os.path.exists(log):
        sys.exit("Keine CHDK/FILMLOG.CSV unter " + args.card)
    scenes = read_scenes()
    by_source = {s["source"]: s for s in scenes}
    by_number = {re.sub(r"\D", "", s["source"]): s for s in scenes}
    names = looks_by_name()

    picks = {}      # (szene, label) -> jpeg, spaetere Zeilen gewinnen
    with open(log, encoding="latin-1", newline="") as f:
        for row in csv.reader(f, delimiter=";"):
            if len(row) < 2 or row[0] == "Datei":
                continue
            jpg, film = row[0].strip(), row[1].strip()
            label, mode, mix = names.get(film.lower(), (None, 0, 0))
            src = row[2].strip().upper() if len(row) > 2 and row[2].strip() else None
            if src:
                scene = by_source.get(src)
            elif label == "DAYLITE":
                # das Original selbst: gleiche Nummer wie die DNG der Szene
                scene = by_number.get(re.sub(r"\D", "", os.path.basename(jpg)))
            else:
                scene = None
            if not scene:
                continue
            if not label:
                print("   %s: Film %r nicht in EXAMPLE.TXT, uebersprungen" % (jpg, film))
                continue
            if len(row) > 3 and row[3].strip():
                print("   %s: %s - trotzdem uebernommen, Bild pruefen" % (jpg, row[3].strip()))
            picks[(scene["id"], label)] = (jpg, mode, mix)

    if not picks:
        print("Nichts gefunden. Stimmen die Quell-DNGs in site/showroom/scenes.txt?")
        return
    # Szene -> Exif-Lage des Original-JPEGs (gleiche Nummer wie die DNG). Der Hilfsschuss traegt
    # die Lage beim Entwickeln; ohne Original liegen hochkant fotografierte Szenen quer.
    orient = {}
    dirs = [os.path.join(args.card, "DCIM", "*")] + ([args.originals] if args.originals else [])
    for s in scenes:
        num = re.sub(r"\D", "", s["source"])
        for d in dirs:
            for f in glob.glob(os.path.join(d, "IMG_%s.JPG" % num)):
                orient[s["id"]] = Image.open(f).getexif().get(0x112, 1)
    for sid in sorted({sc for sc, _ in picks} - set(orient)):
        print("   WARNUNG %s: Original-JPEG (IMG_%s.JPG) weder auf der Karte noch in --originals, "
              "Lage unbekannt - hochkant fotografierte Szenen liegen dann quer"
              % (sid, re.sub(r"\D", "", next(s["source"] for s in scenes if s["id"] == sid))))
    rejected = []
    for (scene, label), (rel, mode, mix) in sorted(picks.items()):
        path = find_jpeg(args.card, rel)
        if not path:
            print("   fehlt auf der Karte: %s (%s, %s)" % (rel, scene, label))
            continue
        why = check_shot(path, mode, mix)
        if why:
            rejected.append("   %s (%s, %s): %s" % (rel, scene, label, why))
            continue
        if args.dry_run:
            print("   %s -> %s/%s.jpg" % (rel, scene, label))
            continue
        print("->", save(prepare(path, orient.get(scene)), scene, label + ".jpg"), "aus", rel)
        sim = os.path.join(SHOWROOM, scene, label + "-sim.jpg")
        if os.path.exists(sim):
            os.remove(sim)
    if rejected:
        print("NICHT uebernommen (das bisherige Bild der Seite bleibt):")
        print("\n".join(rejected))
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("import")
    p.add_argument("card")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--originals", help="Ordner mit den Original-JPEGs (IMG_<nr>.JPG), falls nicht mehr auf der Karte")
    p.set_defaults(fn=cmd_import)
    p = sub.add_parser("sim")
    p.add_argument("scene")
    p.add_argument("photo")
    p.add_argument("labels", nargs="+")
    p.set_defaults(fn=cmd_sim)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
