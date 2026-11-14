# Kameratest PowerShot A2400 IS – EXCCD v1.10.2

Erster Start von EXCCD auf dieser Kamera, bisher nur am PC gebaut. Es gibt **einen Besuch**, deshalb steht
hier alles, was man vorher vorbereiten muss. Dauer vor Ort: ca. 45 Minuten, Muss-Teil ca. 20 Minuten.

Die Kamera-Menüs von EXCCD sind englisch, die Namen stehen hier so, wie sie auf dem Display erscheinen.

Wenn etwas schiefgeht: **Akku raus, Karte raus** – die Kamera läuft danach wieder ganz normal.
CHDK verändert die Kamera nicht dauerhaft.

## Zu Hause vorbereiten (Sonntag)

- [ ] SD-Karte **bis 32 GB**. Mit dem Installer (`exccd-installer.exe` aus dem Münchenpaket) als Kamera
      „PowerShot A2400 IS“ und die ZIP `exccd-v1.10.2-a2400-100e-full.zip` auf die Karte schreiben.
- [ ] Ordner `A2400` aus dem Münchenpaket zusätzlich auf die Karte kopieren (Ersatzdateien für die anderen Firmwares).
- [ ] Leere Datei `CHDK/FILMTIME.CSV` auf der Karte anlegen (misst die Rechenzeit).
- [ ] Mitnehmen: Laptop **oder** Handy mit SD-Kartenleser (zum Tauschen einer Datei), Akku der A2400 voll laden lassen.

## Vor Ort: Firmware passend machen (5 Minuten)

1. Schreibschutz-Schieber der Karte auf **LOCK**, Karte rein, Kamera einschalten.
2. **Startet CHDK** (EXCCD-Startbild, unten der Filmname): Firmware ist 1.00e, weiter mit dem Test.
3. **Startet nur die normale Kamera, oder die Power-Lampe blinkt gleichmäßig:** andere Firmware.
   Akku raus, Karte in den Kartenleser und herausfinden, welche:
   - `A2400/FWPROBE.BIN` als `DISKBOOT.BIN` ins Hauptverzeichnis kopieren (alte überschreiben), Karte auf LOCK, einschalten.
   - Die Power-Lampe blinkt in Gruppen: **3× kurz = 1.00c, 4× kurz = 1.00d, 5× kurz = 1.00e.** Akku raus.
   - Passende Datei `A2400/DISKBOOT-100C.BIN` bzw. `-100D.BIN` als `DISKBOOT.BIN` ins Hauptverzeichnis kopieren. Fertig.

## Muss (das Wichtigste, ca. 20 Minuten)

- [ ] **Start:** CHDK startet, Menüs schwarz mit weißem Cursor. Richtig ist: nach dem Einschalten
      oben rechts Batterie + Prozent, unten der Filmname.
- [ ] **ALT-Taste ist die Hilfe-Taste [?].** Drücken öffnet die Filmauswahl. Richtig ist: Textkarten statt bunter
      Laschen (mit Absicht, die Palette fehlt noch). Blättern, mit FUNC/SET übernehmen.
- [ ] **OSD:** Batterie und Filmname sitzen sauber, verschwinden bei Halbdruck, MENU und FUNC/SET und kommen wieder.
- [ ] **My Colors:** Look *Postcard 100* wählen, Canon FUNC-Menü öffnen. Richtig ist: My Colors steht auf
      **Custom Color**. Steht dort „Aus“: Foto vom Display machen.
- [ ] **Farben (wichtigster Punkt):** Ein Motiv mit Himmel, Grün, Haut und etwas Weißem, bei Tageslicht,
      mit *Aus*, *Postcard 100*, *Silver 400*, *Violet 400*. Richtig ist: **kein grüner oder magenta Stich über
      das ganze Bild**, keine Streifen, keine Blockmuster.
- [ ] **Rechenzeit:** gefühlte Wartezeit nach dem Auslösen notieren (wird zusätzlich in `FILMTIME.CSV` gemessen).
- [ ] **Palette sichern:** In der Filmauswahl MENU → *Options* → *System* → *Save palette*.
      Richtig ist: auf der Karte liegt danach `CHDK/PALREC.BIN`.
- [ ] **Weißes Papier als DNG:** CHDK-Menü → RAW auf DNG, Look *Aus*, weißes Blatt im Tageslicht formatfüllend,
      ein Foto. Danach RAW wieder aus.

## Wenn Zeit ist

- [ ] Rezept C1 laden (Options → *Recipe*): Look, Effekte und Weißabgleich springen um
- [ ] Auto-ISO mit kürzester Zeit (Options → *Auto ISO min. shutter*): bei wenig Licht steigt die ISO statt der Belichtungszeit
- [ ] Effekte einzeln: Vignette, Datumsstempel, Filmkorn, Filmrahmen (sitzt der Rahmen an allen vier Kanten?)
- [ ] RAW entwickeln: ein DNG mit einem Look entwickeln (Options → *RAW develop*), läuft es durch?
- [ ] Ein Dunkelbild als DNG (Objektivdeckel drauf bzw. Hand davor, ISO 400) – für defekte Pixel und Spalten
- [ ] Options → *System* → *Debug*: Propcase 188 notieren, während My Colors auf Custom Color steht

## Mitnehmen (für mich)

- [ ] Ganzen Kartenordner `DCIM` (Testfotos, DNGs), `CHDK/PALREC.BIN`, `CHDK/FILMTIME.CSV`, `CHDK/FILMLOG.CSV`
- [ ] Welche Firmware die Kamera hat
- [ ] Display-Fotos von allem, was komisch aussah
- [ ] Rückmeldung als Textdatei in `rueckmeldung/` (z. B. „Punkt 3 ok, Punkt 4: My Colors aus“)
