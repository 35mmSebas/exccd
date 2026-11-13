# PowerShot A570 IS

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.01a, 1.00e. EXCCD baut für **1.01a** (eine Version je Kamera, siehe Aufgabe „mehrere-firmwares“).
- VxWorks, DIGIC III, propset 2
- Sensor 10 Bit, R G / G B (0x02010100), 3152 x 2340, 7 MP – gleiche RAW-Größe wie IXUS 70/75. Sensorfamilie **F3** (7 MP CCD · 10 Bit RGGB · DIGIC III).
- Kein `PS.FI2`: Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- ALT-Taste: Print-Taste. Menü-Cursor weiß/schwarz wie bei den anderen EXCCD-Kameras.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- Läuft mit 2x AA, optischer Sucher. Weißabgleich und Looks sind noch Werte der IXUS 60.

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
