# Kameratest v1.10.0-lab

Dieser Stand ist komplett am PC entstanden: Rohdaten-Code in PC-Tests (10 und 12 Bit), Looks
und Effekte mit `simulate_looks.py` und `sim_raw.c` beurteilt, alle Kameras gebaut.
**Auf einer Kamera lief davon noch nichts.** Reihenfolge: was am ehesten schiefgeht, zuerst.

Vorbereitung: leere Datei `A/CHDK/FILMTIME.CSV` anlegen, dann misst die Kamera die Zeiten.

## 1. Start und Auswahl

- [ ] Kamera startet, Filmauswahl (Print) zeigt 26 Looks plus Off
- [ ] Leiste zeigt die Position (`3/26`), Links/Rechts blaettert um 1, Hoch/Runter um 5, auch ueber das Ende hinaus
- [ ] Neue Laschen: Summer, Chrome, Bypass, Noir, Turquoise, Infra, Tintype, Cyano, Instant, Pastel (Farben, Lesbarkeit)
- [ ] Der vorher gewaehlte Look ist nach dem Update noch gewaehlt

## 2. Look-Speicher ueber den Namen

- [ ] Look waehlen, ausschalten, einschalten: gleicher Look
- [ ] `FILMS.TXT` mit anderer Reihenfolge auf die Karte: gleicher Look bleibt gewaehlt
- [ ] `FILMS.TXT` ohne diesen Look: faellt auf Off zurueck

## 3. Geschwindigkeit (neuer Codec)

Je 2 Fotos, dann `FILMTIME.CSV` ansehen. Vergleichswerte v1.9.27 (siehe Spalten):

| | Hotpixel | Kurve | Mix (Violet) |
|---|---|---|---|
| IXUS 60 | 1490-1560 ms | 1890-1900 ms | 5020 ms |
| SX110 | 2420-2810 ms | 1640-1750 ms | 9780-10390 ms |

- [ ] Violet 400: Spalte `Mix` schneller als oben? Bild unveraendert gegenueber v1.9.27?
- [ ] Hotpixel: schneller oder gleich?
- [ ] Ein Bild mit Double/Mix auf der Kamera: keine Streifen, keine verschobenen Farben (der Codec liest jetzt 16 Bit am Stueck)

## 4. Neue Looks

Motive: Laub und Himmel (Infra, Turquoise, Violet zum Vergleich), Haut (Summer, Pastel, Instant), Innenraum.

- [ ] Infra 200 und Turquoise 400 sind seit dem SX110-Test vom 07.11. neu abgestimmt (`tools/filmsim/fit_mix.py`,
  an DNG/JPEG-Paaren der SX110). Ein Bild davon ist noch nicht auf der Kamera entstanden. Pruefen:
  Infra: gruenes Laub rot, Herbstlaub orange, Himmel blau, Haut natuerlich. Turquoise: Laub und Warmes tuerkis, nicht kobaltblau
- [ ] Rauschen bei Infra/Turquoise: Die Matrizen verstaerken Farbrauschen bis 1,5-fach. Bei ISO 400 noch ok?
- [ ] IXUS 60 / IXUS 132: Die Matrizen sind an der SX110 gemessen. Weicht der Look stark ab, braucht es Werte je Kamera
- [ ] Cyano 25: durchgehend blau? Lichter nicht zu grell?
- [ ] Tintype 50: Canons Sepia-Modus plus harte Kurve, passt das zusammen?
- [ ] Noir 3200: nicht zu hart? Schwarz leicht grau gewollt
- [ ] Summer, Chrome, Bypass, Instant, Pastel: Hauttoene und Weissabgleich

## 5. Filmkorn (Optionen -> Effekte -> Filmkorn)

Der offene Punkt ist Canons Rauschfilter: Er laeuft nach EXCCD und kann feines Korn glaetten.

- [ ] Coarse bei ISO 100 auf einem Himmel: sichtbar, einfarbig, keine bunten Flecken, kein Muster
- [ ] Fine, Medium: jede Stufe erkennbar
- [ ] Auto: Noir 3200 deutlich koerniger als Postcard 100
- [ ] Hohe ISO: nicht zu viel zusammen mit dem Sensorrauschen
- [ ] Zeit in `FILMTIME.CSV`, Spalte `Effects`

## 6. Filmrahmen (Optionen -> Effekte -> Filmrahmen)

- [ ] Thin: Rand auf allen vier Seiten gleich breit, kein Bildstreifen aussen
- [ ] Negative: Perforation oben und unten, Filmname und Nummer in Bernstein, Bild etwa 3:2
- [ ] **Sitzt der Rahmen an allen vier Kanten?** Auf jeder Kamera pruefen, der Ausschnitt ist hier noch geschaetzt (IXUS 145: `To do` in `platform_camera.h`). Display-Foto oder JPEG-Ecke reicht
- [ ] Schwarzer Rand: Beeinflusst er Canons Weissabgleich oder die Belichtung? (Sollte nicht, er entsteht erst nach der Aufnahme.)
- [ ] Zusammen mit Date stamp: Datum sitzt ueber dem Filmrand
- [ ] DNG speichern: Das DNG bleibt ohne Rahmen und Korn (wird vorher geschrieben)

## 7. Filmrolle und Ueberraschungsfilm (Optionen)

- [ ] Filmrolle 12: Statuszeile zaehlt `1/12`, `2/12` ...; bei `12` Anzeige "roll done"
- [ ] Rolle angefangen: Filmauswahl laesst blaettern, SET auf einen anderen Film zeigt "Roll loaded: n left" und wechselt nicht
- [ ] Rolle voll: Wechsel moeglich, naechstes Foto zaehlt wieder `1/12`
- [ ] Ausschalten mitten in der Rolle: Zaehler bleibt
- [ ] Filmrolle auf Off: Sperre weg
- [ ] Mit Negative: Nummer im Filmrand passt zum Zaehler
- [ ] Ueberraschungsfilm ohne Rolle: nach jedem Foto ein anderer Look, Statuszeile zeigt ihn
- [ ] Ueberraschungsfilm mit Rolle: neuer Look erst, wenn die Rolle voll ist
- [ ] Serienbild: Zaehler stimmt, keine Haenger

## 8. RAW entwickeln (Optionen -> RAW develop)

Vorher: Optionen -> RAW auf DNG, ein paar Fotos machen. Dann DNG waehlen, Look waehlen, einmal ausloesen.

- [ ] **Laeuft es durch?** Datei-Auswahl oeffnet sich, danach die Filmauswahl, Statuszeile zeigt `Develop CRW_0012.DNG: Look`
- [ ] Das neue JPEG zeigt das Motiv der DNG, nicht den Hilfsschuss, und mit dem gewaehlten Look. Die DNG ist unveraendert, es entsteht keine neue DNG
- [ ] **Weissabgleich, die wichtigste Pruefung:** Foto bei Tageslicht mit DNG und Look. Dieselbe DNG mit demselben Look einmal bei Tageslicht und einmal bei Kunstlicht entwickeln. Beide JPEGs sollen wie das Original aussehen. Falls nicht: Canon nutzt zur JPEG-Zeit andere Werte als `wb_adj` beim RAW-Hook, dann `filmsim_develop_begin` pruefen
- [ ] Hilfsschuss im Dunkeln und mit Blitz: Ergebnis gleich? Lange Belichtung kostet nur Zeit (Canons Dunkelbild)
- [ ] Danach ist wieder der vorher eingelegte Film aktiv, Filmrolle hat nicht gezaehlt, auch bei angefangener Rolle ging die Auswahl
- [ ] Korn, Rahmen, Vignette, Datum wirken; der Datumsstempel zeigt den Aufnahmetag
- [ ] Dynamik+: Foto mit DR+2 und DNG. Entwickeln bei Dynamik+ Off: gleiche Helligkeit wie das Original? Die DNG oeffnet sich weiter normal am PC
- [ ] Abbrechen: Optionen -> RAW develop nochmal. Meldung, Film zurueck, naechstes Foto ist normal
- [ ] Ablehnen: JPEG oder DNG einer anderen Kamera waehlen. Meldung, nichts passiert
- [ ] Kamera beim Hilfsschuss hochkant halten: Das JPEG uebernimmt die Ausrichtung (bekannt, siehe README). Digitalzoom beim Hilfsschuss: Ausschnitt? Stoert es, dann Hinweis oder Sperre ergaenzen
- [ ] Zeit in `FILMTIME.CSV`: Wartezeit vertretbar? Die WB-Umrechnung kommt vor `Hotpixel` und ist dort nicht mitgemessen

## 9. Sprache

- [ ] `german.lng`: Filmkorn, Filmrahmen, Filmrolle, Ueberraschungsfilm, RAW-Entwicklung passen in die Menuezeile
