# IXUS 130 / PowerShot SD1400 IS

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Angelegt mit `tools/filmsim/add_camera.py`,
am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.00a, 1.00c. EXCCD baut für **1.00c**.
- DIGIC 4, DryOS R43, propset 3
- Sensor 12 Bit, R G / G B (0x02010100), 4416 x 3296, 15 MP. Sensorfamilie eigene (noch kein anderer Port mit diesen Werten).
- Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- ALT-Taste: Print (CHDK-Voreinstellung). Menü-Cursor weiß/schwarz.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- Weißabgleich und Looks sind noch Werte der IXUS 60.
- **Propset 3 kennt in CHDK keine My-Colors-Propcases:** Looks wirken nur über Kurven und Rohdaten, Schwarzweiß-Looks bleiben farbig. Im Test die Nummern mit Options → System → Debug suchen.
- Installer und Webseite kennen bisher nur 1.00c (Aufgabe „mehrere-firmwares“).

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
