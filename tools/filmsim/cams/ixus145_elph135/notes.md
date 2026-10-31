# IXUS 145 / PowerShot ELPH 135

Status: test (seit v1.9.23, Kameratest siehe `kameratest.md`)

- Firmware nur 1.00c, DryOS, DIGIC 4+, propset 6
- Kein `PS.FIR`: Start nur per Autoboot (bootfaehige Karte, Schreibschutz auf LOCK)
- Sensor 12 Bit, G B / R G (`cam_CFAPattern` 0x01000201), 16 MP
  - CHDK schaltet `OPT_CURVES` bei 12 Bit ab, EXCCD rechnet die Kurven selbst
    (`filmsim_curve_apply`)
- Bildschirm 720x360 mit Seitenkorrektur, CHDK-Farben ab 0xD0 (`CAM_LOAD_CUSTOM_COLORS`)
- ALT-Taste: Hilfe [?] (keine Print-Taste)

## Offen

- [ ] Kennt My Colors „Custom Color“? Sonst wirken nur die Kurven
- [ ] `PALREC.BIN` hier ablegen, Laschen bauen, `FILMSIM_LABELS` und `FILMSIM_COLOR_*` setzen
- [ ] Weissabgleich aus einer DNG von weissem Papier: `FILMSIM_WB_R/_B`
- [ ] Looks und Effekte auf dem 16-MP-Sensor pruefen
