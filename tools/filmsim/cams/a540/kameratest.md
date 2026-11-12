# Kameratest PowerShot A540 – EXCCD v1.10.2

Erster Start von EXCCD auf dieser Kamera, bisher nur am PC gebaut. Dauer ca. 20 Minuten.
Die Kamera-Menüs von EXCCD sind englisch, die Namen stehen hier so, wie sie auf dem Display erscheinen.

Wenn etwas schiefgeht: **Akku raus, Karte raus** – die Kamera läuft danach wieder ganz normal.
CHDK verändert die Kamera nicht dauerhaft.

## Vorbereiten

- [ ] SD-Karte **bis 2 GB** (FAT16) – größere Karten startet diese Kamera nicht automatisch.
- [ ] Mit dem EXCCD-Installer zuerst **Firmware prüfen**: Die Kamera blinkt die Version, der Installer zeigt die Tabelle.
      Unterstützt wird **1.00b**.
- [ ] Mit dem Installer die Karte für „PowerShot A540“ schreiben, Schreibschutz-Schieber auf **LOCK**.
- [ ] Leere Datei `CHDK/FILMTIME.CSV` anlegen (misst die Rechenzeit).

## Muss

- [ ] **Start:** Kamera startet mit EXCCD. Richtig ist: oben rechts Batterie + Prozent, unten der Filmname.
- [ ] **Print-Taste = ALT:** öffnet die Filmauswahl. Richtig ist: Textkarten statt bunter Laschen (mit Absicht). Mit FUNC/SET übernehmen.
- [ ] **OSD:** Batterie und Filmname sitzen sauber, verschwinden bei Halbdruck, MENU und FUNC/SET und kommen wieder.
- [ ] **My Colors:** Look *Postcard 100*, Canon FUNC-Menü öffnen. Richtig ist: My Colors steht auf **Custom Color**.
- [ ] **Farben:** Motiv mit Himmel, Grün, Haut und etwas Weißem bei Tageslicht, mit *Off*, *Postcard 100*, *Silver 400*, *Violet 400*.
      Richtig ist: **kein grüner oder magenta Stich über das ganze Bild**, keine Streifen, keine Blockmuster.
- [ ] **Rechenzeit:** gefühlte Wartezeit nach dem Auslösen (zusätzlich in `FILMTIME.CSV`).
- [ ] **Palette sichern:** In der Filmauswahl MENU → *Options* → *System* → *Save palette*. Richtig ist: danach liegt `CHDK/PALREC.BIN` auf der Karte.
- [ ] **Weißes Papier als DNG:** CHDK-Menü → RAW auf DNG, Look *Off*, weißes Blatt im Tageslicht formatfüllend, ein Foto.

## Zurückschicken

Testfotos und DNGs aus `DCIM`, `CHDK/PALREC.BIN`, `CHDK/FILMTIME.CSV`, `CHDK/FILMLOG.CSV`, die Firmware-Version
und Display-Fotos von allem, was komisch aussah.
