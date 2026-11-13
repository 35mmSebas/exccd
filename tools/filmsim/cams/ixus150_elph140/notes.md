# IXUS 150 / PowerShot ELPH 140 IS

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.00c. EXCCD baut für **1.00c**.
- DryOS R54, DIGIC 4+, propset 6
- Sensor 12 Bit, G B / R G (0x01000201), 4704 x 3504, 16 MP. Sensorfamilie **C4** (16 MP CCD · 12 Bit GBRG · DIGIC 4).
- Kein `PS.FI2`: Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte bis 32 GB (FAT32).
- ALT-Taste: Hilfe-Taste [?]. Menü-Cursor weiß/schwarz wie bei den anderen EXCCD-Kameras.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- CHDK stellte die Wiedergabe-Taste als ALT ein; EXCCD nimmt wie bei der IXUS 145 die Hilfe-Taste, damit Wiedergabe frei bleibt.

## Ungeprüft (nur auf der Kamera zu klären)

- [ ] Startet überhaupt mit EXCCD
- [ ] OSD-Positionen (Batterie oben rechts, Filmname unten)
- [ ] My Colors „Custom Color“ wird gesetzt
- [ ] Kein grüner/magenta Stich über das ganze Bild (Bayer-Muster)
- [ ] Rechenzeit je Foto

## Nach dem Test

- [ ] `PALREC.BIN` hier ablegen, Laschen bauen, `FILMSIM_LABELS` und `FILMSIM_COLOR_*` setzen
- [ ] Weißabgleich aus einer DNG von weißem Papier: `FILMSIM_WB_R/_B`
- [ ] `FILMSIM_DIAG` wieder aus, bevor die Kamera `stabil` wird
