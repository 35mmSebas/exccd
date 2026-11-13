# PowerShot A2200

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Angelegt mit `tools/filmsim/add_camera.py`,
am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.00b, 1.00c, 1.00d. EXCCD baut für **1.00d**.
- DIGIC 4, DryOS R47, propset 4
- Sensor 12 Bit, R G / G B (0x02010100), 4416 x 3296, 15 MP. Sensorfamilie **J4** (14 MP CCD · 12 Bit RGGB · DIGIC 4).
- Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte bis 32 GB (FAT32).
- ALT-Taste: Playback (CHDK-Voreinstellung). Menü-Cursor weiß/schwarz.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- Weißabgleich und Looks sind noch Werte der IXUS 60.
- Propset 4: Das Weißabgleich-Menü von EXCCD bleibt aus (Canon zählt dort anders oder es ist nicht verifiziert).
- 1.00c nutzt in CHDK den Code von 1.00b.
- Installer und Webseite kennen bisher nur 1.00d (Aufgabe „mehrere-firmwares“).

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
