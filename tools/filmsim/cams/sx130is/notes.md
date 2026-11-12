# PowerShot SX130 IS

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.01f, 1.01c, 1.01d. EXCCD baut für **1.01f** (eine Version je Kamera, siehe Aufgabe „mehrere-firmwares“).
- DryOS R45, DIGIC 4, propset 4
- Sensor 12 Bit, R G / G B (0x02010100), 4080 x 3048, 12 MP. Sensorfamilie eigene (12 MP, DIGIC 4).
- Kein `PS.FI2`: Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- ALT-Taste: Print-Taste. Menü-Cursor weiß/schwarz wie bei den anderen EXCCD-Kameras.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- Läuft mit 2x AA, 12-fach-Zoom.
- Propset 4: Das Weißabgleich-Menü von EXCCD bleibt aus (Canon zählt dort anders, in CHDK nicht verifiziert).
- Weißabgleich und Looks sind noch Werte der IXUS 60.

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
