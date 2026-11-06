# Kameratest PowerShot SX110 IS – EXCCD v1.9.27

Datei: `dist/v1.9.27/exccd-v1.9.27-sx110is-100b-labels-full.zip`

Bisher auf der Kamera bestätigt: CHDK startet, Ausschalten per ALT-Taste. Alles andere ist nur am PC geprüft.

Die Testkamera hat einen defekten Power-Schalter: **Einschalten mit der Play-Taste, Ausschalten mit Print 3 s halten.**
Der Zoom ist laut, funktioniert aber. Das liegt an der Kamera, nicht an EXCCD.

## Vorher (5 Minuten)

- [ ] **Firmware muss 1.00b sein** (CHDK gibt es nur für diese Version). Leere Datei `vers.req` auf die Karte
      legen, im Wiedergabemodus FUNC/SET gedrückt halten und dann DISP drücken. Ist die Firmware
      anders, startet CHDK einfach nicht: Batterien raus, und die Kamera läuft wieder normal.
- [ ] SD-Karte bis 32 GB, FAT32. Ist sie größer, im Kamera-Menü formatieren und am PC als FAT32 neu formatieren.
- [ ] Karte bootfähig machen (wie bei der IXUS 60, z. B. mit STICK oder EOScard). Danach die ZIP entpacken.
- [ ] Schreibschutz-Schieber der Karte auf **LOCK** stellen. CHDK schreibt trotzdem.
      Es gibt **kein `PS.FI2`** für die SX110, Start nur per Autoboot.
- [ ] Frische AA-Zellen einlegen und notieren, welche (Alkaline oder NiMH).

## Start und Bedienung

- [ ] Kamera startet mit CHDK, EXCCD-Menüs sind schwarz mit weißem Cursor
- [ ] **ALT-Taste ist die Print-Taste** (wie bei der IXUS 60).
      Klappt das schlecht: CHDK-Menü → Einstellungen → ALT-Taste auf Face.
- [ ] Filmauswahl: **Laschen** erscheinen, Farben deckend und nicht durchscheinend (gerechnet aus der Palette, noch nie auf dem Display gesehen).
      Blättern, mit FUNC/SET übernehmen
- [ ] Oben rechts nur Batterie + Prozent, sitzt sauber. Verschwindet bei Halbdruck, MENU und FUNC.SET.
      **Prozentwert notieren** (bei frischen Zellen sollte er nahe 100 sein)
- [ ] Filmname unten wird angezeigt
- [ ] Moduswahlrad: EXCCD in P, Av und M ausprobieren

## My Colors

- [x] Custom Color und S/W wirken laut Canon-Daten im JPEG (06.11.2026). Das FUNC-Menü zeigt trotzdem „aus“, das ist nur die Anzeige.

## Fotos (wichtigster Teil)

Ein Motiv mit Himmel, Grün, Haut und einer weißen Fläche, Stativ oder aufgestützt, Tageslicht:

- [ ] **Aus** (Referenz), Daylight 100, Postcard 100, Journal 400, Drugstore 200, Silver 400, Portrait 400
- [ ] Violet 400 (Grün wird violett) und Concrete 100 (Kanalmischung, Mix 1/2)
- [ ] Prüfen: **Kein Farbstich über das ganze Bild** (grün/magenta = falsche Bayer-Anordnung, dann melden!)
      Die SX110 ist die erste Kamera mit 12 Bit und R G / G B, genau das ist hier neu.
- [ ] Prüfen: keine Streifen, kein Rauschen in Blöcken, keine schwarzen Punkte
- [ ] Ein Bild mit Dynamik+ +1
- [ ] Effekte einzeln: Vignette stark, Datumsstempel (orange, unten rechts), Light Leak, Black Mist 1/4, Halation
- [ ] Farbrauschen-Filter Stark, ein Bild bei wenig Licht
- [ ] Doppelbelichtung (2 Bilder; die Kamera schreibt dabei rund 15 MB auf die Karte, das dauert)
- [ ] Gefühlte Wartezeit nach dem Auslösen notieren (9 MP statt 6 MP)

## Weiße Punkte in Schwarzweiß-Bildern

Feste defekte Pixel der Testkamera, in allen Fotos an denselben Stellen (05.11.2026).

- [ ] **Zwei Dunkelbilder als DNG:** Speichern als DNG einschalten, Look *Aus*, Moduswahlrad M, ISO 400, 1 s
      (länger nicht, sonst zieht Canon selbst ein Dunkelbild ab). Dunkler Raum, schwarzer Stoff vor dem Objektiv.
      Daraus entsteht eine Liste der defekten Pixel dieser Kamera.

## Für mich zum Nacharbeiten mitbringen

- [ ] **Weißabgleich:** CHDK RAW/DNG einschalten, Look *Aus*, weißes Papier im Tageslicht formatfüllend
      → die DNG-Datei. Daraus kalibriere ich die Kanalmischung und die Effekte (bisher Werte der IXUS 60).
- [ ] Alle Testfotos plus `A/CHDK/FILMLOG.CSV`
- [ ] `A/CHDK/FILMTIME.CSV`: Vor dem Test eine leere Datei mit diesem Namen auf die Karte legen, dann steht darin die Rechenzeit je Foto in ms
- [ ] Batterie-Prozent mit frischen und mit fast leeren Zellen
- [ ] Display-Fotos, wenn etwas komisch aussieht

## Was im Code passiert ist (zur Einordnung)

- Die SX110 hat einen 12-Bit-Sensor, die IXUS 60 10 Bit. Das CHDK-Kurvenmodul kann nur 10 Bit,
  deshalb rechnet EXCCD die Tonkurven selbst (dieselben `.CV`-Dateien, fein interpoliert).
- Die Bayer-Anordnung ist R G / G B wie bei der IXUS 60, aber in 12 Bit. Diese Kombination
  lief bisher auf keiner Kamera.
- Die Looks sind auf den Sensor und die Bildverarbeitung der IXUS 60 abgestimmt. Auf der SX110 werden sie
  ähnlich, aber nicht gleich aussehen. Die Feinabstimmung kommt nach dem Test.
