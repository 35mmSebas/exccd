#!/usr/bin/env python3
"""Neue Kamera als EXCCD-Beta (Status test) anlegen.

    python tools/filmsim/add_camera.py LISTE

LISTE: eine Kamera pro Zeile, `|`-getrennt, `#` = Kommentar:

    plattform | Kurzname | Name fuer die Webseite [| sub]

    a590 | PowerShot A590 IS | Canon PowerShot A590 IS
    g11  | PowerShot G11     | Canon PowerShot G11     | 100l

Ohne sub nimmt das Skript die neueste Firmware mit eigenem Code (nicht als Kopie
einer anderen Version in camera_list.csv). Fuer jede Kamera:

- platform/<p>/platform_camera.h: Menue-Cursor weiss/schwarz und EXCCD-Block
  (Sensorfamilie, Weissabgleich der Familie, FILMSIM_DIAG fuer den Test)
- tools/filmsim/cameras.txt, tools/installer/core.c, tools/site/build_site.py,
  .gitignore (PRIMARY.BIN-Platzhalter)
- tools/filmsim/cams/<p>/notes.md und kameratest.md

Alles, was sich nur an der Kamera pruefen laesst (Tasten, OSD, Palette, My Colors),
steht danach als ungeprueft in den Notizen. Bauen: tools/filmsim/build.sh <p>.
"""
import os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
VERSION_RE = re.compile(r'#define FILMSIM_VERSION "(.*)"')

# Sensorfamilien: (Bit, Bayer, Breite, Hoehe) -> Name, Beschreibung, WB-Gains (R, B) oder None
FAMILIES = {
    (10, 0x02010100, 2888, 2136): ("A", "wie IXUS 60", None),
    (12, 0x02010100, 3720, 2772): ("B", "wie SX110", (470, 441)),
    (12, 0x01000201, 4704, 3504): ("C", "wie IXUS 132/145", None),
    (10, 0x01000201, 3336, 2480): ("D", "wie A590/A720", None),
    (12, 0x02010100, 3744, 2784): ("E", "wie S90/S95/G11/G12", None),
}
CFA_NAME = {0x02010100: "R G / G B", 0x01000201: "G B / R G"}

WHITE = ("    #define CAM_DEFAULT_MENU_CURSOR_BG  IDX_COLOR_WHITE    // EXCCD: weisser Balken\n"
         "    #define CAM_DEFAULT_MENU_CURSOR_FG  IDX_COLOR_BLACK    // EXCCD: schwarze Schrift\n")
RED_RE = re.compile(r"[ \t]*#define CAM_DEFAULT_MENU_CURSOR_BG\s+IDX_COLOR_\w+[^\n]*\n"
                    r"[ \t]*#define CAM_DEFAULT_MENU_CURSOR_FG\s+IDX_COLOR_\w+[^\n]*\n")


def read(path):
    return open(os.path.join(ROOT, path), encoding="utf-8", newline="").read()


def write(path, text):
    open(os.path.join(ROOT, path), "w", encoding="utf-8", newline="").write(text)


def fmt(sub):
    return "%s.%s" % (sub[0], sub[1:])


def define(hdr, name):
    m = re.search(r"^\s*#define\s+%s\s+(\S+)" % name, hdr, re.M)
    return m.group(1) if m else None


def camera_info(p):
    rows = [l.strip().split(",") for l in open(os.path.join(ROOT, "camera_info.csv"))]
    head = rows[0]
    return [dict(zip(head, r)) for r in rows[1:] if r[0] == p]


def known_fw(p):
    out = []
    for l in open(os.path.join(ROOT, "camera_list.csv")):
        f = l.strip().split(",")
        if f[0] == p:
            out.append((f[1], f[3] if len(f) > 3 else ""))
    return out


def facts(p, sub):
    hdr = read("platform/%s/platform_camera.h" % p)
    if "// EXCCD (Bedeutung" in hdr:
        sys.exit("%s hat schon einen EXCCD-Block" % p)
    fws = known_fw(p)
    if not fws:
        sys.exit("%s steht nicht in camera_list.csv" % p)
    if not sub:
        own = [s for s, src in fws if not src and os.path.isdir(os.path.join(ROOT, "platform", p, "sub", s))]
        sub = sorted(own)[-1]
    if sub not in [s for s, _ in fws]:
        sys.exit("%s: Firmware %s unbekannt" % (p, sub))
    info = [i for i in camera_info(p) if i["FIRMWARE"] == sub][0]
    bits = int(define(hdr, "CAM_SENSOR_BITS_PER_PIXEL") or 10)
    cfa = int(define(hdr, "cam_CFAPattern"), 16)
    w = int(define(hdr, "CAM_RAW_ROWPIX"))
    h = int(define(hdr, "CAM_RAW_ROWS"))
    if cfa not in CFA_NAME:
        sys.exit("%s: Bayer-Muster 0x%08x kann core/filmsim_raw.h nicht" % (p, cfa))
    propset = int(define(hdr, "CAM_PROPSET"))
    names = re.search(r"CAM_ALT_BUTTON_NAMES\s+\{\s*\"([^\"]+)\"", hdr)
    fam = FAMILIES.get((bits, cfa, w, h))
    os_ = info["OS"]
    osver = ("DryOS R%s" % info["OSVER"]) if os_ == "dryos" else "VxWorks"
    digic = {"20": "DIGIC II", "30": "DIGIC III", "40": "DIGIC 4", "41": "DIGIC 4+"}.get(info["DIGIC"], "DIGIC ?")
    mp = round(w * h / 1e6)
    return dict(p=p, sub=sub, fws=fws, hdr=hdr, bits=bits, cfa=cfa, w=w, h=h, mp=mp, propset=propset,
                alt=(names.group(1) if names else "Print"), fam=fam, os=osver, digic=digic,
                fat32="CAM_DRYOS_2_3_R47" in hdr, cursor_red=bool(RED_RE.search(hdr)))


def block(c):
    sensor = "%d Bit %s, %d x %d (%d MP)" % (c["bits"], CFA_NAME[c["cfa"]], c["w"], c["h"], c["mp"])
    lines = ["// %s, %s, propset %d, %s" % (c["digic"], c["os"], c["propset"], sensor)]
    if c["fam"]:
        lines.append("// Sensorfamilie %s (%s)" % c["fam"][:2])
    else:
        lines.append("// Eigene Sensorfamilie (noch kein anderer Port mit diesen Werten)")
    lines.append("// Noch keine Laschen (Palette fehlt): Textkarten in CHDK-Farben.")
    if c["fam"] and c["fam"][2]:
        r, b = c["fam"][2]
        lines += ["// Weissabgleich vorerst von der Familie uebernommen.",
                  "#define FILMSIM_WB_R                %d" % r,
                  "#define FILMSIM_WB_B                %d" % b]
    else:
        lines.append("// Weissabgleich: Werte der IXUS 60 (Standard), nach dem Test aus einer DNG messen.")
    if c["propset"] in (3, 13):
        lines.append("// Propset %d kennt in CHDK keine My-Colors-Propcases: Looks nur ueber Kurven und Rohdaten." % c["propset"])
    lines.append("#define FILMSIM_DIAG                1")
    return ("\n//--------------------------------------------------\n"
            "// EXCCD (Bedeutung siehe Kopf von core/filmsim.c)\n" + "".join("    " + l + "\n" for l in lines))


def patch_header(c):
    s = c["hdr"]
    if c["cursor_red"]:
        s = RED_RE.sub(WHITE, s, count=1)
        tail = ""
    else:
        tail = "\n    #undef  CAM_DEFAULT_MENU_CURSOR_BG\n    #undef  CAM_DEFAULT_MENU_CURSOR_FG\n" + WHITE
    write("platform/%s/platform_camera.h" % c["p"], s.rstrip("\n") + "\n" + tail + block(c))


def patch_tables(c, short, name):
    fws = [fmt(s) for s, _ in c["fws"]]
    other = [f for f in fws if f != fmt(c["sub"])]

    ct = read("tools/filmsim/cameras.txt")
    if re.search(r"^%s\s" % re.escape(c["p"]), ct, re.M):
        sys.exit("%s steht schon in cameras.txt" % c["p"])
    write("tools/filmsim/cameras.txt", ct.rstrip("\n") + "\n%-16s %s  test    %s\n" % (c["p"], c["sub"], short))

    core = read("tools/installer/core.c")
    end = core.index("};\nconst int camera_count")
    row = '    { %-18s %-36s %d, 0, "%s", "%s" },\n' % ('"%s",' % c["p"], '"%s",' % short, c["fat32"],
                                                       fmt(c["sub"]), " ".join(fws))
    write("tools/installer/core.c", core[:end] + row + core[end:])

    site = read("tools/site/build_site.py")
    end = site.index("\n}\n", site.index("CAMERA_META = {")) + 1
    row = '    "%s": {"name": "%s",\n        "other_firmware": %s, "boot": "%s", "firm_update": False},\n' % (
        c["p"], name, "[" + ", ".join('"%s"' % o for o in other) + "]", "fat32" if c["fat32"] else "fat16")
    write("tools/site/build_site.py", site[:end] + row + site[end:])

    gi = read(".gitignore")
    line = "platform/%s/sub/%s/PRIMARY.BIN\n" % (c["p"], c["sub"])
    if line not in gi:
        anchor = [l for l in gi.splitlines(True) if l.endswith("/PRIMARY.BIN\n")][-1]
        i = gi.rindex(anchor) + len(anchor)
        write(".gitignore", gi[:i] + line + gi[i:])


def docs(c, short):
    version = VERSION_RE.search(read("core/filmsim.h")).group(1)
    fws = [fmt(s) for s, _ in c["fws"]]
    copies = ["%s nutzt in CHDK den Code von %s" % (fmt(s), fmt(src)) for s, src in c["fws"] if src]
    other = [f for f in fws if f != fmt(c["sub"])]
    card = ("SD-Karte bis 32 GB (FAT32)" if c["fat32"] else
            "SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch")
    sensor = "%d Bit, %s (0x%08x), %d x %d, %d MP" % (c["bits"], CFA_NAME[c["cfa"]], c["cfa"], c["w"], c["h"], c["mp"])
    fam = ("%s (%s)" % c["fam"][:2]) if c["fam"] else "eigene (noch kein anderer Port mit diesen Werten)"
    extra = []
    if c["fam"] and c["fam"][2]:
        extra.append("Weißabgleich vorerst von der Sensorfamilie übernommen.")
    else:
        extra.append("Weißabgleich und Looks sind noch Werte der IXUS 60.")
    if c["propset"] in (3, 13):
        extra.append("**Propset %d kennt in CHDK keine My-Colors-Propcases:** Looks wirken nur über Kurven und Rohdaten, "
                     "Schwarzweiß-Looks bleiben farbig. Im Test die Nummern mit Options → System → Debug suchen." % c["propset"])
    if c["propset"] not in (1, 2, 3, 5, 6):
        extra.append("Propset %d: Das Weißabgleich-Menü von EXCCD bleibt aus (Canon zählt dort anders oder es ist nicht verifiziert)." % c["propset"])
    if copies:
        extra.append("; ".join(copies) + ".")
    if other:
        extra.append("Installer und Webseite kennen bisher nur %s (Aufgabe „mehrere-firmwares“)." % fmt(c["sub"]))
    d = os.path.join(ROOT, "tools", "filmsim", "cams", c["p"])
    os.makedirs(d, exist_ok=True)
    notes = (
        "# %s\n\n" % short +
        "Status: test (seit %s, noch nie auf einer Kamera gelaufen). Angelegt mit `tools/filmsim/add_camera.py`,\n" % version +
        "am PC gebaut, Kameratest siehe `kameratest.md`.\n\n" +
        "- Firmware in CHDK: %s. EXCCD baut für **%s**.\n" % (", ".join(fws), fmt(c["sub"])) +
        "- %s, %s, propset %d\n" % (c["digic"], c["os"], c["propset"]) +
        "- Sensor %s. Sensorfamilie %s.\n" % (sensor, fam) +
        "- Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). %s.\n" % card +
        "- ALT-Taste: %s (CHDK-Voreinstellung). Menü-Cursor weiß/schwarz.\n" % c["alt"] +
        "- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.\n" +
        "- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).\n" +
        "".join("- %s\n" % e for e in extra) +
        "\n## Ungeprüft (nur auf der Kamera zu klären)\n\n"
        "- [ ] Startet überhaupt mit EXCCD\n"
        "- [ ] ALT-Taste sinnvoll (kollidiert sie mit einer Canon-Funktion?)\n"
        "- [ ] OSD-Positionen (Batterie oben rechts, Filmname unten)\n"
        "- [ ] My Colors „Custom Color“ wird gesetzt\n"
        "- [ ] Kein grüner/magenta Stich über das ganze Bild (Bayer-Muster)\n"
        "- [ ] Rechenzeit je Foto\n"
        "\n## Nach dem Test\n\n"
        "- [ ] `PALREC.BIN` hier ablegen, Laschen bauen, `FILMSIM_LABELS` und `FILMSIM_COLOR_*` setzen\n"
        "- [ ] Weißabgleich aus einer DNG von weißem Papier: `FILMSIM_WB_R/_B`\n"
        "- [ ] `FILMSIM_DIAG` wieder aus, bevor die Kamera `stabil` wird\n")
    other_txt = (" (%s kennt CHDK auch, EXCCD baut sie noch nicht – bitte trotzdem melden.)" % ", ".join(other)) if other else ""
    mycolors = ("- [ ] **My Colors suchen:** Options → *System* → *Debug* öffnen, im Canon-FUNC-Menü My Colors zwischen Aus "
                "und Custom Color umschalten. Display-Foto von der Debug-Seite vorher und nachher.\n"
                if c["propset"] in (3, 13) else
                "- [ ] **My Colors:** Look *Postcard 100*, Canon FUNC-Menü öffnen. Richtig ist: My Colors steht auf **Custom Color**.\n")
    test = (
        "# Kameratest %s – EXCCD %s\n\n" % (short, version) +
        "Erster Start von EXCCD auf dieser Kamera, bisher nur am PC gebaut. Dauer ca. 20 Minuten.\n"
        "Die Kamera-Menüs von EXCCD sind englisch, die Namen stehen hier so, wie sie auf dem Display erscheinen.\n\n"
        "Wenn etwas schiefgeht: **Akku raus, Karte raus** – die Kamera läuft danach wieder ganz normal.\n"
        "CHDK verändert die Kamera nicht dauerhaft.\n\n"
        "## Vorbereiten\n\n" +
        "- [ ] %s.\n" % card +
        "- [ ] Mit dem EXCCD-Installer zuerst **Firmware prüfen**: Die Kamera blinkt die Version, der Installer zeigt die Tabelle.\n"
        "      Unterstützt wird **%s**.%s\n" % (fmt(c["sub"]), other_txt) +
        "- [ ] Mit dem Installer die Karte für „%s“ schreiben, Schreibschutz-Schieber auf **LOCK**.\n" % short +
        "- [ ] Leere Datei `CHDK/FILMTIME.CSV` anlegen (misst die Rechenzeit).\n\n"
        "## Muss\n\n"
        "- [ ] **Start:** Kamera startet mit EXCCD. Richtig ist: oben rechts Batterie + Prozent, unten der Filmname.\n" +
        "- [ ] **%s-Taste = ALT:** öffnet die Filmauswahl. Richtig ist: Textkarten statt bunter Laschen (mit Absicht). "
        "Mit FUNC/SET übernehmen.\n" % c["alt"] +
        "- [ ] **OSD:** Batterie und Filmname sitzen sauber, verschwinden bei Halbdruck, MENU und FUNC/SET und kommen wieder.\n" +
        mycolors +
        "- [ ] **Farben:** Motiv mit Himmel, Grün, Haut und etwas Weißem bei Tageslicht, mit *Off*, *Postcard 100*, *Silver 400*, *Violet 400*.\n"
        "      Richtig ist: **kein grüner oder magenta Stich über das ganze Bild**, keine Streifen, keine Blockmuster.\n"
        "- [ ] **Rechenzeit:** gefühlte Wartezeit nach dem Auslösen (zusätzlich in `FILMTIME.CSV`).\n"
        "- [ ] **Palette sichern:** In der Filmauswahl MENU → *Options* → *System* → *Save palette*. Richtig ist: danach liegt `CHDK/PALREC.BIN` auf der Karte.\n"
        "- [ ] **Weißes Papier als DNG:** CHDK-Menü → RAW auf DNG, Look *Off*, weißes Blatt im Tageslicht formatfüllend, ein Foto.\n\n"
        "## Zurückschicken\n\n"
        "Testfotos und DNGs aus `DCIM`, `CHDK/PALREC.BIN`, `CHDK/FILMTIME.CSV`, `CHDK/FILMLOG.CSV`, die Firmware-Version\n"
        "und Display-Fotos von allem, was komisch aussah.\n")
    open(os.path.join(d, "notes.md"), "w", encoding="utf-8").write(notes)
    open(os.path.join(d, "kameratest.md"), "w", encoding="utf-8").write(test)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    for line in open(sys.argv[1], encoding="utf-8"):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        f = [x.strip() for x in line.split("|")]
        p, short, name = f[:3]
        c = facts(p, f[3] if len(f) > 3 and f[3] else None)
        patch_header(c)
        patch_tables(c, short, name)
        docs(c, short)
        print("%-16s %s  %s  Familie %s" % (p, c["sub"], short, c["fam"][0] if c["fam"] else "neu"))


if __name__ == "__main__":
    main()
