# IXUS 132 / PowerShot ELPH 115 IS

Status: test (seit v1.9.23, Kameratest siehe `kameratest.md`)

CHDK startet per Autoboot (erster Kameratest 2026-11-01). Stolperstein: Die erste Karte
wurde von der Kamera nicht als gesperrt erkannt (Fotos trotz LOCK moeglich), deshalb kein
Start. Mit einer anderen SD-Karte lief es. Test: Mit LOCK und ohne CHDK darf kein Foto gehen.

Palette: `PALREC.BIN` (Typ 3, 256 Eintraege V, U, Y, Alpha). Fast nur Grau, Weiss und
Orange. EXCCD laedt 6 eigene Laschenfarben nach 0x5E..0x63 (`EXCCD_PAL_EXTRA` in
`platform_camera.h`, geladen in `lib.c`). Laschen: `EXCCD_CAM=ixus132_elph115
make_labels.py labels_src cams/ixus132_elph115/CHDK/FILMS`; der Makefile legt
`cams/<plattform>/CHDK/` ueber die gemeinsamen Dateien im ZIP.

Sensor der Testkamera: helle Spalte 2026 im Rohbild (JPEG x ~ 1989), auch ohne CHDK
und im Livebild. Korrektur ueber `A/CHDK/BADCOLS.TXT` (eine Spaltennummer pro Zeile,
`filmsim_badcols_apply` in core/filmsim.c). Die Datei gehoert zur einzelnen Kamera,
nicht ins ZIP. Spalten finden: Dunkelbild als DNG, Spaltenmedian gegen x-2/x+2.
Auf der Kamera bestaetigt (2026-11-01): Strich in JPEG und Wiedergabe weg, im Livebild bleibt er.

Offen: My Colors bleibt bei Postcard 100 auf "aus" (Propcase 187 pruefen, System -> Debug).

- Firmware nur 1.00b, DryOS R52, propset 6. Identische Firmware wie die IXUS 135 / ELPH 120
  (die hat zusaetzlich WLAN), ein Port fuer beide waere fast nur ein weiterer Eintrag
- Kein `PS.FI2`: Start nur per Autoboot (bootfaehige Karte, Schreibschutz auf LOCK)
- Sensor 12 Bit, G B / R G (`cam_CFAPattern` 0x01000201), 4704 x 3504, 16 MP,
  gleiche Werte wie die IXUS 145: der 12-Bit-Code aus v1.9.23 gilt unveraendert
  - CHDK schaltet `OPT_CURVES` bei 12 Bit ab, EXCCD rechnet die Kurven selbst
    (`filmsim_curve_apply`)
- Bildschirm 720 breit mit Seitenkorrektur, CHDK-Farben in die Palette geladen (`CAM_LOAD_CUSTOM_COLORS`)
- ALT-Taste: Video (keine Print- und keine Hilfe-Taste), Wiedergabe als Alternative im CHDK-Menue
- Keine Blende, nur ND-Filter

## Offen

- [ ] Kennt My Colors „Custom Color“? Sonst wirken nur die Kurven
- [ ] `PALREC.BIN` hier ablegen, Laschen bauen, `FILMSIM_LABELS` und `FILMSIM_COLOR_*` setzen
- [ ] Weissabgleich aus einer DNG von weissem Papier: `FILMSIM_WB_R/_B`
- [ ] Looks und Effekte auf dem 16-MP-Sensor pruefen
