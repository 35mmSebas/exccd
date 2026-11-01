# IXUS 132 / PowerShot ELPH 115 IS

Status: test (seit v1.9.23, Kameratest siehe `kameratest.md`)

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
