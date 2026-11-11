# Kameratest v1.10.0

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

- [ ] Thin: Rand auf allen vier Seiten gleich breit, kein Bildstreifen aussen (Negative und Filmrolle sind beim Merge nach main weggefallen)
- [ ] **Sitzt der Rahmen an allen vier Kanten?** Auf jeder Kamera pruefen, der Ausschnitt ist hier noch geschaetzt (IXUS 145: `To do` in `platform_camera.h`). Display-Foto oder JPEG-Ecke reicht
- [ ] Schwarzer Rand: Beeinflusst er Canons Weissabgleich oder die Belichtung? (Sollte nicht, er entsteht erst nach der Aufnahme.)
- [ ] Zusammen mit Date stamp: Datum sitzt ueber dem Filmrand
- [ ] DNG speichern: Das DNG bleibt ohne Rahmen und Korn (wird vorher geschrieben)

## 7. RAW entwickeln (Optionen -> RAW develop)

Vorher: Optionen -> RAW auf DNG, ein paar Fotos machen. Dann DNG waehlen, Look waehlen, einmal ausloesen.

- [ ] **Laeuft es durch?** Datei-Auswahl oeffnet sich, danach die Filmauswahl, nach SET das Menue `Develop: effects`, nach `Done - press shutter` zeigt die Statuszeile `Develop CRW_0012.DNG: Look` (mit `| FX`, wenn Effekte an sind)
- [ ] **Dateiauswahl:** startet im neuesten Kameraordner auf der neuesten DNG, JPEGs sind ausgeblendet. Rechts nach kurzem Stehen das Vorschaubild in Graustufen und das Aufnahmedatum. Erkennt man das Motiv? Bleibt das Blaettern fluessig (auch bei gehaltener Taste)? Erst kommt sofort das kleine Bild, kurz danach das schaerfere aus den Rohdaten: Wie lange dauert das, und ist das scharfe gerade (nicht verschoben, nicht verrauscht)?
- [ ] **Vollbild:** In der Dateiauswahl Rechts (oder Zoom): DNG bildschirmfuellend, unten Name, Zeit, Nummer. Links/Rechts blaettert (Ordner werden uebersprungen), SET entwickelt, Hoch/Runter/Zoom/MENU zurueck zur Liste (Cursor auf dem zuletzt gezeigten Bild, keine Reste vom Vollbild)
- [ ] **Belichtung und Farbtemperatur** (Effekt-Menue -> Weiter): -1 EV macht das JPEG eine Blende dunkler, +1 EV heller; waermer/kuehler verschiebt nach Gelb/Blau. Ausgefressene Stellen (Himmel, Lampen) bleiben beim Abdunkeln und Waermer-Machen neutral grau/weiss, ohne rosa oder gelben Fleck. Statuszeile zeigt `| -1 EV | warm +1`. Die naechste Entwicklung beginnt wieder bei 0
- [ ] **Toenung und Dynamik+** (gleiches Menue): Toenung gruener/magenta wirkt sichtbar, ohne die Helligkeit stark zu aendern (Test unter Leuchtstoffroehre). Dynamik+ Auto nimmt die Stufe der Aufnahme; fest +1/+2 hellt Schatten auf. Lichterrettung: Foto mit ausgefressenem Himmel, entwickeln mit -1 EV und Dynamik +1: Himmel bekommt Zeichnung, Schatten bleiben hell
- [ ] Effekt-Menue: startet mit den Effekten aus den Optionen. Dort z. B. Korn aendern, entwickeln: Korn wirkt im JPEG, in Optionen -> Effects steht danach der alte Wert, auch nach Aus/Ein
- [ ] **Fortschrittsbalken:** Nach dem Ausloesen erscheint mittig ein Kasten `Developing CRW_....DNG` mit Schritt, Prozent und Balken, der sichtbar waechst (Halation, Korn). Am Ende `Canon saves JPEG...`, danach verschwindet er. Falls nichts zu sehen ist: Canon verdeckt oder loescht die Bitmap waehrend der Verarbeitung
- [ ] Das neue JPEG zeigt das Motiv der DNG, nicht den Hilfsschuss, und mit dem gewaehlten Look. Die DNG ist unveraendert, es entsteht keine neue DNG
- [ ] **Weissabgleich, die wichtigste Pruefung:** Foto bei Tageslicht mit DNG und Look. Dieselbe DNG mit demselben Look einmal bei Tageslicht und einmal bei Kunstlicht entwickeln. Beide JPEGs sollen wie das Original aussehen. Falls nicht: Canon nutzt zur JPEG-Zeit andere Werte als `wb_adj` beim RAW-Hook, dann `filmsim_develop_begin` pruefen
- [ ] Hilfsschuss im Dunkeln und mit Blitz: Ergebnis gleich? Lange Belichtung kostet nur Zeit (Canons Dunkelbild)
- [ ] Danach ist wieder der vorher eingelegte Film aktiv
- [ ] Korn, Rahmen, Vignette, Datum wirken; der Datumsstempel zeigt den Aufnahmetag
- [ ] **Halation beim Entwickeln** (Tungsten 800, Nachtfoto mit Lichtern): Der Glow ist so rot wie beim direkt fotografierten Bild, auch wenn der Hilfsschuss abends unter warmem Licht entsteht. Ebenso Turquoise/Infra/Light Leak: Farben wie beim echten Foto
- [ ] Dynamik+: Foto mit DR+2 und DNG. Entwickeln bei Dynamik+ Off: gleiche Helligkeit wie das Original? Die DNG oeffnet sich weiter normal am PC
- [ ] Abbrechen: Optionen -> RAW develop nochmal. Meldung, Film zurueck, naechstes Foto ist normal
- [ ] Ablehnen: JPEG oder DNG einer anderen Kamera waehlen. Meldung, nichts passiert
- [ ] Kamera beim Hilfsschuss hochkant halten: Das JPEG uebernimmt die Ausrichtung (bekannt, siehe README). Digitalzoom beim Hilfsschuss: Ausschnitt? Stoert es, dann Hinweis oder Sperre ergaenzen
- [ ] Zeit in `FILMTIME.CSV`: Wartezeit vertretbar? Die WB-Umrechnung kommt vor `Hotpixel` und ist dort nicht mitgemessen

## 8. Sprache

- [ ] `german.lng`: Filmkorn, Filmrahmen, RAW-Entwicklung passen in die Menuezeile; `Fertig - Ausloeser druecken` und `Entwickle CRW_....DNG` passen in Menue und Kasten

## 9. v1.10.1: Fortschritt bei jeder Aufnahme, Reset

- [ ] Foto mit Look und mehreren Effekten (z. B. Halation + Mist + Korn): Kasten `Processing <Film>` erscheint sofort nach dem Ausloesen, der Balken laeuft durch, am Ende `Canon saves JPEG...`, danach verschwindet der Kasten
- [ ] Daylight 100 ohne Effekte: kein Kasten
- [ ] Doppelbelichtung (erstes und zweites Bild): Balken haengt nicht beim Schreiben/Lesen der DBL1.RAW
- [ ] Serienbild / schnelle Folge: Kasten stoert nicht, keine Reste auf dem Bildschirm
- [ ] Optionen -> Reset to defaults -> Yes: Zeit, Fokus, Zoom, Blitz, ND, Farbrauschen, S/W-Filter, Dynamik+, alle Effekte und DNG speichern stehen auf Standard; der Film bleibt; nach Aus/An weiterhin Standard. No aendert nichts
- [ ] `german.lng`: `Standard wiederherstellen` passt in die Menuezeile, der Fragetext in den Kasten

## 10. v1.10.1: Weissabgleich, WB-Shift, Rezepte

Auf IXUS 60 (10 Bit, curves.flt, propset 1) und SX110 (12 Bit, eigene Kurve, propset 2) pruefen.

- [ ] Optionen -> White balance: Wechsel auf Tageslicht/Kunstlicht. Canon-Symbol oben aendert sich (sofort oder nach Halbdruck)? Im FUNC-Menue steht derselbe Wert? Das JPEG ist entsprechend waermer/kuehler
- [ ] Danach im FUNC-Menue umstellen: EXCCD stellt nicht zurueck
- [ ] Optionen -> WB shift oeffnet das Raster (SET und links/rechts): Pfeile bewegen das weisse Feld, rechts = R+, oben = B+, Wert oben rechts stimmt; SET uebernimmt, MENU bricht ab, Zoom setzt auf 0; danach steht das Menue wieder auf WB shift. Auf IXUS 132/145 (480 breit?) mittig und nicht abgeschnitten
- [ ] Langes Halten von SET beim Oeffnen schliesst das Raster nicht sofort wieder
- [ ] Neue Menuestruktur: Optionen passen ohne Scrollen auf den Schirm (Recipe, Look: <Film>, Trennlinie, Zeit/Fokus/Zoom/Blitz/ND/Doppelbelichtung, Trennlinie, RAW, Reset, System, Back); Trennlinien werden uebersprungen
- [ ] "Look: <Film>" zeigt den eingelegten Film, auch nach Filmwechsel per Print und nach Rezept laden
- [ ] Doppelbelichtung steht in den Optionen, nicht mehr unter Effects; RAW -> Save RAW und RAW develop funktionieren wie vorher
- [ ] Filmauswahl: unten steht grau "SET OK   MENU Options", Lasche und Leiste ueberlappen nicht (IXUS 132/145 auch)
- [ ] WB shift R+9 mit Summer 200: Foto deutlich roeter als mit 0; B+9 deutlich blauer; Lichter (Himmel) fressen nicht frueher aus
- [ ] WB shift mit Daylight 100 (keine Filmkurve): wirkt trotzdem (neutrale Kurve)
- [ ] WB shift mit Film Aus: wirkt nicht, Statuszeile zeigt keinen Shift
- [ ] WB shift + Dynamik+ 2: Shift und Aufhellung wirken zusammen
- [ ] Statuszeile zeigt `| R+2 B-1`
- [ ] Optionen -> Look -> Save as recipe -> C1 (empty): `A/CHDK/FILMS/RECIPES.TXT` entsteht, Inhalt passt (film, wb, shift, Effekte), Slot heisst wie der Film
- [ ] Zweites Rezept C2 mit anderem Film, anderem WB und Effekten speichern, dann Optionen -> Recipe zwischen C1 und C2 wechseln: Film, Canon-WB, Shift, Dynamik+ und Effekte springen mit
- [ ] Nach dem Laden etwas aendern (z. B. Korn): Menue und Statuszeile zeigen `C1*`; zurueckstellen -> `*` verschwindet
- [ ] Belegten Platz speichern: Rueckfrage `Overwrite C1 ...?`, No aendert nichts
- [ ] RECIPES.TXT am PC umbenennen/aendern (z. B. `wb = keep`, falscher Schluessel): Kamera nimmt die Aenderung, Fehler stehen in `RECIPES.LOG`
- [ ] Rezept mit einem Film, der nicht auf der Karte ist: Film bleibt, Rest wird geladen, Anzeige mit `*`
- [ ] Aus/An: geladenes Rezept, Shift und WB bleiben
- [ ] Reset to defaults: Shift 0, kein Rezept aktiv; RECIPES.TXT bleibt
- [ ] `german.lng`: `Rezept speichern`, `WB-Shift Rot`, `Leuchtstoff H`, `C1 (leer)` passen in die Menuezeile

## 11. v1.10.2: Summer 200 ohne Gelbstich

- [ ] Grauer Himmel / graue Wand mit Summer 200 bleibt grau (nur leicht cremig), nicht gelb
- [ ] Haut, Laub, Holz bleiben warm; Vergleich jeweils gegen Daylight 100 (Paare fuer fit_mix.py)

## 12. v1.10.2: Auto-ISO mit kuerzester Zeit

Canon-ISO im FUNC-Menue auf **Auto**, Blitz aus. Optionen -> `Auto ISO min. shutter` = 1/60, `Auto ISO max.` = 800.
- [ ] Statuszeile zeigt `| A-ISO 1/60`
- [ ] Dunkler Raum: Halbdruck zeigt 1/60 (oder kuerzer) statt 1/8; Foto ist richtig belichtet, EXIF ISO > 100
- [ ] Sehr dunkel: ISO bleibt bei 800, Zeit wird laenger als 1/60 (kein Absturz, Bild nicht zu dunkel)
- [ ] Draussen hell: EXIF zeigt Grund-ISO (80/100), Bild normal belichtet
- [ ] `Auto` (1/f): Weitwinkel ca. 1/35, Tele ca. 1/105 (IXUS 60); SX110 Tele -> sehr kurze Zeit, ISO steigt bis max.
- [ ] Feste Canon-ISO (z. B. 200): Funktion wirkt nicht, Zeit wie von Canon
- [ ] `Shutter speed` fest gesetzt: Auto-ISO wirkt nicht
- [ ] Blitz loest aus: Auto-ISO wirkt nicht
- [ ] Mit Dynamik+ 1: Bild weiterhin 1 EV knapper belichtet (Kurve hellt auf), Zeit trotzdem >= 1/60
- [ ] `Auto ISO max.` bietet auf der IXUS 60 nur 200/400/800 an
- [ ] RAW (DNG) zeigt dieselbe ISO wie das JPEG
- [ ] `german.lng`: `Auto-ISO min. Zeit` passt in die Menuezeile
