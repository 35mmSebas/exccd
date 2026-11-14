# PowerShot A2400 IS

Status: test (seit v1.10.2, noch nie auf einer Kamera gelaufen). Erster Test geplant am 16.11.2026
an der Kamera eines Freundes, siehe `kameratest.md`.

- Firmware 1.00c, 1.00d und 1.00e, alle drei in CHDK 1.6.1. EXCCD baut alle drei (1.00e zuerst);
  die ZIPs unterscheiden sich nur in `DISKBOOT.BIN` (und den Textdateien). Auf der Webseite wählt
  man die Firmware, der Installer erkennt sie am Dateinamen.
- DIGIC 4, DryOS R50, **propset 5** (IXUS 132/145: propset 6). Alle Propcases, die EXCCD nutzt
  (My Colors 188, Custom 177–180/55/59/229, WB-Modus 272, Blitz bereit 212), gibt es in propset 5;
  der WB-Modus zählt wie bei 1/2/3/6.
- Kein `PS.FI2`: Start nur per Autoboot (bootfähige Karte, Schreibschutz auf LOCK). FAT32 geht (R47+).
- Sensor 12 Bit, G B / R G (`cam_CFAPattern` 0x01000201), 4704 x 3504, 16 MP: gleiche Werte wie
  IXUS 132/145, also vermutlich derselbe Sensor (Sensorfamilie C4). Der 12-Bit-Code gilt unverändert.
- Bildschirm 720 breit mit Seitenkorrektur, CHDK-Farben in die Palette geladen (`CAM_LOAD_CUSTOM_COLORS`).
- ALT-Taste: Hilfe [?] (EXCCD stellt sie nach vorn), Wiedergabe als Alternative im CHDK-Menü.
  Kein DISP, keine Löschtaste, kein manueller Fokus, keine Blende (nur ND-Filter).
- Akku NB-11L. LED für Firmware-Prüfung und CHDK-Fehler: Power-LED 0xC0220120.
  Die Firmware-Prüfung schaltet die A2400 nicht selbst aus (kein `PROBE_OFF_*`): Akku raus.
- `FILMSIM_DIAG` ist für den Test an (System → Debug, Propcases).

## Ungeprüft (nur auf der Kamera zu klären)

- [ ] Startet überhaupt (CHDK-Port der A2400 ist „release“, mit EXCCD aber nie gelaufen)
- [ ] OSD-Positionen (Batterie oben rechts, Filmname unten), Menü-Cursor
- [ ] Kennt My Colors „Custom Color“? Sonst wirken nur die Kurven
- [ ] Hilfe-Taste als ALT: kollidiert sie mit Canons Hilfe-Funktion?
- [ ] Bayer-Muster: kein grüner/magenta Stich über das ganze Bild
- [ ] Rechenzeit je Foto (16 MP, DIGIC 4)

## Nach dem Test

- [ ] `PALREC.BIN` hier ablegen, Laschen bauen, `FILMSIM_LABELS` und `FILMSIM_COLOR_*` setzen
- [ ] Weißabgleich aus einer DNG von weißem Papier: `FILMSIM_WB_R/_B`
- [ ] Helle/defekte Spalten im Dunkelbild prüfen (IXUS 132 hatte eine, `BADCOLS.TXT`)
- [ ] `FILMSIM_DIAG` wieder aus, bevor die Kamera `stabil` wird
