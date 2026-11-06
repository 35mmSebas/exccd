#!/usr/bin/env python3
"""
Fuehrt die echte core/filmsim.c am PC aus, mit nachgebildeter Kamera (test_filmsim_host.c):
Filmauswahl, Rolle, Ueberraschungsfilm, Look-Speicher, Statuszeile und die komplette
RAW-Kette mit den Sensordaten jeder Kamera.

    python tools/filmsim/test_filmsim.py [arbeitsordner]

Laeuft nur unter Windows, nicht in der CI: Die Nachbildung braucht long = 32 Bit (MinGW).
Braucht gcc im PATH (toolchain/env.sh). filmsim.c ist fuer 32 Bit geschrieben: Menue-Texte
stehen als (int)"Text" in Tabellen. Das geht auf dem PC (64 Bit) nicht, darum ersetzt dieses
Skript sie vor dem Uebersetzen durch Nummern, die lang_str() wieder aufloest.
"""
import os, re, subprocess, sys, tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

# Kamera, Sub, Bit-Tiefe; aus cameras.txt, damit neue Kameras automatisch mitlaufen
def cameras():
    for line in open(os.path.join(ROOT, "tools", "filmsim", "cameras.txt"), encoding="utf-8"):
        if line.strip() and not line.startswith("#"):
            p, sub = line.split()[:2]
            yield p, sub


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate(out):
    src = open(os.path.join(ROOT, "core", "filmsim.c"), encoding="latin-1").read()
    lits = []
    def rep(m):
        lits.append(m.group(1))
        return str(100000 + len(lits) - 1)
    src = re.sub(r'\(int\)"([^"]*)"', rep, src)
    with open(os.path.join(out, "filmsim_host.c"), "w", encoding="latin-1") as f:
        f.write('#line 1 "core/filmsim.c"\n' + src)
    with open(os.path.join(out, "lang_host.h"), "w", encoding="latin-1") as f:
        f.write("static const char *host_lits[] = {" + ",".join(c_str(x) for x in lits) + ",0};\n")
        f.write("static const struct { int id; const char *s; } host_lang[] = {\n")
        for line in open(os.path.join(ROOT, "CHDK", "LANG", "english.lng"), encoding="latin-1"):
            m = re.match(r'\s*(\d+)\s+"(.*)"\s*$', line)
            if m and int(m.group(1)) >= 676:
                f.write('{%s,"%s"},\n' % (m.group(1), m.group(2)))
        f.write("{0,0}};\n")


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else tempfile.mkdtemp(prefix="exccd-test")
    os.makedirs(out, exist_ok=True)
    generate(out)
    fails = 0
    for plat, sub in cameras():
        exe = os.path.join(out, "t_%s.exe" % plat)
        cmd = ["gcc", "-w", "-O1", "-g", "-fno-strict-aliasing", "-I" + out,
               "-I" + os.path.join(ROOT, "include"), "-I" + os.path.join(ROOT, "include", "std"),
               "-I" + os.path.join(ROOT, "core"), "-I" + os.path.join(ROOT, "modules"),
               "-I" + os.path.join(ROOT, "platform", plat),
               "-DCAMERA_%s=1" % plat, '-DPLATFORM="%s"' % plat, '-DPLATFORMSUB="%s"' % sub,
               "-DPLATFORMID=0", "-DLOW_LEVEL", "-DHOST_TEST",
               "-DMEMISOSIZE=0x30000", "-DMEMISOSTART=0x10000", "-DMEMBASEADDR=0x1900",
               "-DROMBASEADDR=0xff810000", "-DMAXRAMADDR=0x1ffffff", "-DCAM_DIGIC=2",
               '-DHDK_VERSION="host"', '-DBUILD_NUMBER="0"', '-DBUILD_SVNREV="0"',
               os.path.join(out, "filmsim_host.c"), os.path.join(ROOT, "core", "filmsim_txt.c"),
               os.path.join(ROOT, "tools", "filmsim", "test_filmsim_host.c"),
               "-o", exe]
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        if r.returncode:
            print(plat, "Uebersetzen fehlgeschlagen:\n", r.stdout[-6000:])
            fails += 1
            continue
        for scenario in [("names", str(v)) for v in range(6)] + [("picker",), ("roll",), ("surprise",), ("raw",)]:
            r = subprocess.run([exe, scenario[0], ROOT] + list(scenario[1:]), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, encoding="ascii", errors="replace", timeout=600)
            tail = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "(keine Ausgabe)"
            print("%-16s %-9s %s" % (plat, " ".join(scenario), tail))
            if r.returncode:
                print(r.stdout[-3000:].encode("ascii", "replace").decode())
                print("  Rueckgabewert", r.returncode)
                fails += 1
    print("FEHLGESCHLAGEN" if fails else "OK", "(%d)" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
