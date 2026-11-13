# IXUS 850 IS / PowerShot SD800 IS

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Angelegt mit `tools/filmsim/add_camera.py`,
am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.00e, 1.00d. EXCCD baut für **1.00e**.
- DIGIC III, VxWorks, propset 1
- Sensor 10 Bit, R G / G B (0x02010100), 3152 x 2340, 7 MP. Sensorfamilie **F3** (7 MP CCD · 10 Bit RGGB · DIGIC III).
- Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- ALT-Taste: Print (CHDK-Voreinstellung). Menü-Cursor weiß/schwarz.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- Weißabgleich und Looks sind noch Werte der IXUS 60.
- 1.00d nutzt in CHDK den Code von 1.00e.
- Installer und Webseite kennen bisher nur 1.00e (Aufgabe „mehrere-firmwares“).

## Ungeprüft (nur auf der Kamera zu klären)

- [ ] Startet überhaupt mit EXCCD
- [ ] ALT-Taste sinnvoll (kollidiert sie mit einer Canon-Funktion?)
- [ ] OSD-Positionen (Batterie oben rechts, Filmname unten)
- [ ] My Colors „Custom Color“ wird gesetzt
- [ ] Kein grüner/magenta Stich über das ganze Bild (Bayer-Muster)
- [ ] Rechenzeit je Foto

## Nach dem Test

- [ ] `PALREC.BIN` hier ablegen, Laschen bauen, `FILMSIM_LABELS` und `FILMSIM_COLOR_*` setzen
- [ ] Weißabgleich aus einer DNG von weißem Papier: `FILMSIM_WB_R/_B`
- [ ] `FILMSIM_DIAG` wieder aus, bevor die Kamera `stabil` wird
