# PowerShot G11

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Am PC gebaut, Kameratest siehe `kameratest.md`.

- Firmware in CHDK: 1.00l, 1.00f, 1.00h, 1.00j, 1.00k. EXCCD baut für **1.00l, 1.00f, 1.00j, 1.00h, 1.00k**.
- DryOS R39, DIGIC 4, propset 3
- Sensor 12 Bit, R G / G B (0x02010100), 3744 x 2784, 10 MP, 1/1,7 Zoll. Sensorfamilie **E4** (10 MP CCD · 12 Bit RGGB · DIGIC 4).
- Kein `PS.FI2`: Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- ALT-Taste: Print-Taste. Menü-Cursor weiß/schwarz wie bei den anderen EXCCD-Kameras.
- Noch keine Laschen (Palette fehlt): Filmauswahl zeigt Textkarten.
- `FILMSIM_DIAG` ist für den Test an (Options → System → Debug zeigt Propcases).
- **Propset 3 kennt in CHDK keine My-Colors-Propcases.** EXCCD baut deshalb mit `FS_NO_MYCOLORS`: Looks wirken nur über Kurven und Rohdaten, Canons Kontrast/Sättigung/Farbregler werden nicht gesetzt, Schwarzweiß-Looks (Silver, Noir …) bleiben farbig.
- Im Test die Nummern suchen: Options → System → Debug zeigt die Propcases; im Canon-Menü My Colors umstellen und notieren, welche Nummer sich ändert (bei S95/G12 sind es 188 bzw. 177–180).
- Bildschirm 960 breit, Klappdisplay: Positionen ungeprüft.

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
