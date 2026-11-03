# PowerShot SX110 IS

Status: test (seit v1.9.23, Kameratest siehe `kameratest.md`)

- Firmware nur 1.00b, DryOS (PLATFORMOSVER 31), DIGIC III, propset 2
- Kein `PS.FI2` im Build: Start nur per Autoboot (bootfaehige Karte, Schreibschutz auf LOCK)
- Sensor 12 Bit, R G / G B (`cam_CFAPattern` 0x02010100), 3720 x 2772 roh, 9 MP,
  Schwarzwert 128 (laut CHDK-Forum)
  - Erste Kamera mit 12 Bit **und** R G / G B (IXUS 60: 10 Bit R G / G B,
    IXUS 145/132: 12 Bit G B / R G). Der Kern deckt das ab (`FS_ROW0` = 0), auf der Kamera ungeprueft
  - CHDK schaltet `OPT_CURVES` bei 12 Bit ab, EXCCD rechnet die Kurven selbst
    (`filmsim_curve_apply`)
- ALT-Taste: Print (wie IXUS 60), Face als Alternative im CHDK-Menue
- Echte Blende, kein ND-Filter
- Zwei AA-Zellen. Batterieanzeige rechnet mit 2,30-2,55 V (Werte von der SX100),
  fuer Alkaline vs. NiMH vermutlich ungenau

## Offen

- [ ] Kennt My Colors „Custom Color“? Sonst wirken nur die Kurven
- [ ] `PALREC.BIN` hier ablegen, Laschen bauen, `FILMSIM_LABELS` und `FILMSIM_COLOR_*` setzen
- [ ] Weissabgleich aus einer DNG von weissem Papier: `FILMSIM_WB_R/_B`
- [ ] Batterie-Prozent mit vollen und leeren Zellen pruefen, ggf. `get_vbatt_min/max`
- [ ] Looks und Effekte auf dem 9-MP-Sensor pruefen
