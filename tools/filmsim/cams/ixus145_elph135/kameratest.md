# Kameratest IXUS 145 / ELPH 135 – EXCCD v1.9.23

Datei: `exccd-v1.9.23-ixus145_elph135-100c-full.zip`

Es ist der erste Start von EXCCD auf dieser Kamera. Bisher ist nur der Build am PC geprüft.

## Vorher (5 Minuten)

- [ ] **Firmware muss 1.00c sein** (CHDK gibt es nur für diese Version). Leere Datei `vers.req` auf die Karte
      legen und im Wiedergabemodus FUNC/SET gedrückt halten, dann eine zweite Taste drücken. Die 145 hat kein DISP,
      die genaue Kombination steht nicht in den CHDK-Notizen, also durchprobieren (Hilfe, Hoch). Ist die Firmware
      anders, startet CHDK einfach nicht: Akku raus, und die Kamera läuft wieder normal.
- [ ] SD-Karte bis 32 GB, FAT32. Ist sie größer, im Kamera-Menü formatieren und am PC als FAT32 neu formatieren.
- [ ] Karte bootfähig machen (wie bei der IXUS 60, z. B. mit STICK oder EOScard). Danach die ZIP entpacken.
      Es gibt **kein `PS.FIR`** für die 145, Start nur per Autoboot.
- [ ] Schreibschutz-Schieber der Karte auf **LOCK** stellen. CHDK schreibt trotzdem.

## Start und Bedienung

- [ ] Kamera startet mit CHDK, EXCCD-Menüs sind schwarz mit weißem Cursor
- [ ] **ALT-Taste ist die Hilfe-Taste [?]**, nicht Print. Wiedergabe-Taste funktioniert normal.
      Lange auf [?] drücken → Canon-Hilfe wie gewohnt.
- [ ] Filmauswahl: Statt der Laschen erscheinen **Textkarten** (mit Absicht, die Laschen brauchen erst die Palette der 145, siehe unten).
      Blättern, mit FUNC/SET übernehmen
- [ ] Oben rechts nur Batterie + Prozent, sitzt sauber. Verschwindet bei Halbdruck, MENU und FUNC.SET
- [ ] Filmname unten wird angezeigt

## My Colors

- [ ] Look **Postcard 100** wählen → Canon FUNC-Menü öffnen: Steht My Colors auf **Custom Color**?
      Zeigt Custom Color Werte (Kontrast, Schärfe, Sättigung, R/G/B, Haut)?
      **Wichtig:** Kennt die 145 kein Custom Color, wirken nur die Kurven, dann bitte melden.
- [ ] Silver 400 → My Colors = S/W

## Fotos (wichtigster Teil)

Ein Motiv mit Himmel, Grün, Haut und einer weißen Fläche, Stativ oder aufgestützt, Tageslicht:

- [ ] **Aus** (Referenz), Daylight 100, Postcard 100, Journal 400, Drugstore 200, Silver 400, Portrait 400
- [ ] Violet 400 (Grün wird violett) und Concrete 100 (Kanalmischung, Mix 1/2)
- [ ] Prüfen: **Kein Farbstich über das ganze Bild** (grün/magenta = falsche Bayer-Anordnung, dann melden!)
- [ ] Prüfen: keine Streifen, kein Rauschen in Blöcken, keine schwarzen Punkte
- [ ] Ein Bild mit Dynamik+ +1
- [ ] Effekte einzeln: Vignette stark, Datumsstempel (orange, unten rechts), Light Leak, Black Mist 1/4, Halation
- [ ] Farbrauschen-Filter Stark, ein Bild bei wenig Licht
- [ ] Doppelbelichtung (2 Bilder; die Kamera schreibt dabei 25 MB auf die Karte, das dauert)
- [ ] Gefühlte Wartezeit nach dem Auslösen notieren (16 MP statt 6 MP)

## Für mich zum Nacharbeiten mitbringen

- [ ] **Palette:** Im Aufnahmemodus Optionen (MENU in der Filmauswahl) → System → *Farbpalette speichern*
      → `A/CHDK/PALREC.BIN`. Damit baue ich die richtigen Laschen für die 145.
- [ ] **Weißabgleich:** CHDK RAW/DNG einschalten, Look *Aus*, weißes Papier im Tageslicht formatfüllend
      → die DNG-Datei. Daraus kalibriere ich die Kanalmischung und die Effekte (bisher Werte der IXUS 60).
- [ ] Alle Testfotos plus `A/CHDK/FILMLOG.CSV`
- [ ] Display-Fotos, wenn etwas komisch aussieht

## Was im Code passiert ist (zur Einordnung)

- Die IXUS 145 hat einen 12-Bit-Sensor, die IXUS 60 10 Bit. Das CHDK-Kurvenmodul kann nur 10 Bit,
  deshalb hat EXCCD jetzt eigene 12-Bit-Tonkurven (dieselben `.CV`-Dateien, fein interpoliert).
- Kanalmischung, Effekte, Doppelbelichtung und Farbrauschen-Filter laufen jetzt mit 10 und 12 Bit
  und mit beiden Bayer-Anordnungen (IXUS 145: G B / R G).
- Die Looks sind auf den Sensor und die Bildverarbeitung der IXUS 60 abgestimmt. Auf der 145 werden sie
  ähnlich, aber nicht gleich aussehen. Die Feinabstimmung kommt nach dem Test.
