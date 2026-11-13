# IXUS 85 IS / PowerShot SD770 IS

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.00a, 1.00b. EXCCD baut für **1.00a** (eine Version je Kamera, siehe Aufgabe „mehrere-firmwares“).
- DryOS R23, DIGIC III, propset 2
- Sensor 12 Bit, R G / G B (0x02010100), 3720 x 2772, 10 MP – gleiche RAW-Größe wie die SX110. Sensorfamilie **B3** (10 MP CCD · 12 Bit RGGB · DIGIC III).
- Kein `PS.FI2`: Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- ALT-Taste: Print-Taste. Menü-Cursor weiß/schwarz wie bei den anderen EXCCD-Kameras.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- Weißabgleich vorerst von der SX110 übernommen (gemessen an weißem Papier), Laschen fehlen noch (Palette).
- 1.00b nutzt in CHDK denselben Code wie 1.00a, Installer und Seite kennen bisher nur 1.00a.

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
