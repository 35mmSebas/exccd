# PowerShot A480

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.00b. EXCCD baut für **1.00b**.
- DryOS R31, DIGIC III, propset 2
- Sensor 12 Bit, R G / G B (0x02010100), 3720 x 2772, 10 MP – gleiche Werte und DryOS-Version wie die SX110. Sensorfamilie **B3** (10 MP CCD · 12 Bit RGGB · DIGIC III).
- Kein `PS.FI2`: Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- ALT-Taste: Print-Taste. Menü-Cursor weiß/schwarz wie bei den anderen EXCCD-Kameras.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- Sehr günstig, 2x AA, kein DISP-Knopf. Weißabgleich vorerst von der SX110 übernommen.

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
