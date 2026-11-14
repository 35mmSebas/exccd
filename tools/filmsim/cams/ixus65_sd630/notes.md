# IXUS 65 / PowerShot SD630

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.00a. EXCCD baut für **1.00a**.
- VxWorks, DIGIC II, propset 1
- Sensor 10 Bit, R G / G B (`cam_CFAPattern` 0x02010100), 2888 x 2136, 6 MP – gleiche RAW-Größe wie die IXUS 60. Sensorfamilie **A2** (6 MP CCD · 10 Bit RGGB · DIGIC II).
- Kein `PS.FI2`: Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- ALT-Taste: Print-Taste. Menü-Cursor weiß/schwarz wie bei den anderen EXCCD-Kameras.
- Laschen der IXUS 60 (Annahme gleiche Palette).
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- Laschen und Palettenfarben von der IXUS 60 übernommen (Annahme: gleiche Canon-Palette). Stimmen die Laschenfarben nicht: `PALREC.BIN`.
- Weißabgleich: Werte der IXUS 60 (vermutlich gleicher Sensor).
- Fortschrittsbalken beim Entwickeln: Das OSD-Freischalten (`FILMSIM_OSD_UNMUTE`) gibt es nur für die IXUS 60; hier kann der Balken unsichtbar bleiben.

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
