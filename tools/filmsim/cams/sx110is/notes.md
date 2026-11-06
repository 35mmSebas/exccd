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
- ALT-Taste 3 s halten schaltet aus (`FILMSIM_POWEROFF_HOLD_MS`), weil der Power-Schalter
  der Testkamera defekt ist. Einschalten dort ueber die Play-Taste.
  Auf der Kamera bestaetigt (05.11.2026)
- Echte Blende, kein ND-Filter
- Zwei AA-Zellen. Batterieanzeige rechnet mit 2,30-2,55 V (Werte von der SX100),
  fuer Alkaline vs. NiMH vermutlich ungenau

## Offen

- [ ] Weisse Punkte der Testkamera: mindestens 7 feste Pixel, in allen 13 JPEGs der Karte
  (05.11.2026, auch Farbe, auch 1/60 s), u. a. JPEG x1613 y412, x2540 y427, x2331 y448, x1637 y758,
  x2417 y1740, im Format 3456 x 1944. Das Dunkelbild-DNG (05.11.2026, 1 s, ISO 200) zeigt, warum
  `hotpix_fix` sie stehen laesst: Sie liegen als Gruppen von 2-3 Pixeln direkt nebeneinander,
  `hp_raised` hielt den hellen Nachbarn fuer einen echten Lichtpunkt. Seit `HP_STRONG` (05.11.2026)
  erkennt der Filter solche Gruppen: Am Dunkelbild bleibt von 24 Sensorpixeln > 600 einer uebrig.
  Abhilfe: `A/CHDK/badpixel.txt` (x,y je Zeile in RAW-Koordinaten, 3720 x 2772). Die Liste der
  Testkamera liegt in `badpixel-testkamera.txt`, gehoert zur einzelnen Kamera, nicht ins ZIP.
  Auf der Kamera bestaetigt (05.11.2026): die 7 Punkte sind weg. Pixel nach Staerke sortiert, die
  staerksten zuletzt, CHDK arbeitet die Liste von hinten ab, sonst fliesst ein noch defekter Pixel in
  einen anderen ein. RAW = JPEG + (120, 97), bei 16:9 + (120, 421)
- [ ] Violet unter Lampenlicht wird fast ganz blau (1009/1010), bei Tageslicht pruefen. Bayer-Stich
  ist auf den ersten Fotos nicht zu sehen, Daylight wirkt neutral
- My Colors: Custom Color wirkt (06.11.2026). Das FUNC-Menue zeigte bei Postcard 100 „aus“, die
  Fotos haben laut Canon-MakerNote aber Modus 9 (Custom) mit den Werten des Looks, Silver/Grit Modus 15
  (S/W). Pruefen am JPEG: MakerNote-Tag 0x1D = [.., .., Modus, R, G, B] (R/G/B + 2), Tag 0x01 Index
  13/14/15 = Kontrast, Saettigung, Schaerfe. Nicht das Menue zaehlt, sondern das Foto
- [ ] Laschen auf dem Display pruefen. Gebaut aus `PALREC.BIN` (16 Grundfarben, V U Y A) mit
  `EXCCD_CAM=sx110is make_labels.py labels_src cams/sx110is/CHDK/FILMS`; die Mischfarben sind
  gerechnet, nicht gemessen
- [ ] Weissabgleich gesetzt: `FILMSIM_WB_R/_B` 470/441 aus IMG_1029 (Papier, Tageslicht am Fenster,
  Roh R 306, G 562, B 326 ueber Schwarz). Bedeckter Himmel ergab 463/481, Canon AWB dort 445/519.
  Violet und Concrete mit diesen Werten auf der Kamera noch pruefen. Unter Lampenlicht meldet Canon
  R ~290, B ~910 (`camera_info.props.wb_adj`): feste Werte passen dort nicht, daher das Blau bei Violet
- [ ] Ember neu (07.11.2026): IMG_1045 war in Schatten und Mitten fast reines Rot (Farbton 2-10 Grad,
  Saettigung .87). Kurve zurueckgenommen, Ziel Schatten etwa 10 Grad, Mitten 20-25 Grad, Saettigung
  um .7. Nur am PC-Modell aus dem Off/Ember-Paar geprueft, auf der Kamera am selben Motiv pruefen,
  ebenso auf der IXUS 60 (gemeinsame Kurve)
- [ ] Batterie-Prozent mit vollen und leeren Zellen pruefen, ggf. `get_vbatt_min/max`
- [ ] Looks und Effekte auf dem 9-MP-Sensor pruefen
