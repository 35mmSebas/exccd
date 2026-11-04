# Looks – Übersicht

Stand: v1.9.24 (Quellen: `sims_builtin[]` in `core/filmsim.c`, `CHDK/FILMS/BEISPIEL.TXT`,
`RECIPES` in `tools/filmsim/make_curves.py`). Bei Änderungen bitte hier mitziehen.

## 1. Kamera-Einstellungen (FILMS.TXT)

My Colors: 0 = Aus, 3 = S/W, 11 = Custom Color. Die Regler Kon … Haut (−2..+2) wirken
**nur bei Modus 11**. Mix: 0 = keine, 1 = Lila-Effekt (Farbtausch), 2 = Entsättigt.

| # | Look          | My Colors | Kon | Schärfe | Sätt. | R  | G  | B  | Haut | Kurve      | Mix | Lasche  |
|---|---------------|-----------|----:|--------:|------:|---:|---:|---:|-----:|------------|----:|---------|
| 0 | Aus           | Aus       |  0  |   0     |   0   | 0  | 0  | 0  |  0   | –          | 0   | AUS     |
| 1 | Daylight 100  | Aus       |  0  |   0     |   0   | 0  | 0  | 0  |  0   | –          | 0   | DAYLITE |
| 2 | Postcard 100  | Custom    |  0  |  +1     |  +2   | +1 | +1 | +1 |  0   | POSTCRD.CV | 0   | POSTCRD |
| 3 | Blush 100     | Custom    | −1  |   0     |  +1   | 0  | 0  | 0  | +1   | BLUSH.CV   | 0   | BLUSH   |
| 4 | Journal 400   | Custom    |  0  |   0     |  −1   | 0  | −1 | 0  |  0   | JOURNAL.CV | 0   | JOURNAL |
| 5 | Studio 160    | Custom    | −1  |  −1     |  −1   | 0  | 0  | 0  | +1   | STUDIO.CV  | 0   | STUDIO  |
| 6 | Drugstore 200 | Custom    |  0  |   0     |  −1   | 0  | +1 | −1 |  0   | DRUGSTR.CV | 0   | DRUGSTR |
| 7 | Scene 500     | Custom    | −2  |  −1     |  −2   | 0  | 0  | 0  |  0   | SCENE.CV   | 0   | SCENE   |
| 8 | Silver 400    | S/W       |  –  |   –     |   –   | –  | –  | –  |  –   | SILVER.CV  | 0   | SILVER  |
| 9 | Portrait 400  | Custom    | −1  |  −1     |   0   | 0  | 0  | −1 | +1   | PORTRT.CV  | 0   | PORTRT  |
|10 | Tungsten 800  | Aus       |  –  |   –     |   –   | –  | –  | –  |  –   | TUNGSTN.CV | 0   | TUNGSTN |
|11 | Expired 200   | Custom    | −1  |  −1     |  −1   | 0  | 0  | 0  |  0   | EXPIRED.CV | 0   | EXPIRED |
|12 | Cross 100     | Custom    |  0  |  +1     |  +2   | 0  | 0  | 0  |  0   | CROSS.CV   | 0   | CROSS   |
|13 | Violet 400    | Custom    |  0  |   0     |  +1   | 0  | 0  | 0  |  0   | VIOLET.CV  | 1   | VIOLET  |
|14 | Concrete 100  | Custom    |  0  |  +1     |  −1   | 0  | 0  | 0  |  0   | CONCRET.CV | 2   | CONCRET |
|15 | Ember 200     | Custom    |  0  |   0     |   0   | 0  | 0  | 0  |  0   | EMBER.CV   | 0   | EMBER   |
|16 | Grit 400      | S/W       |  –  |   –     |   –   | –  | –  | –  |  –   | GRIT.CV    | 0   | GRIT    |

## 2. Tonkurven (make_curves.py)

Parametrische Rezepte: `contrast` = S-Kurve (0,3 = deutlich, <0 flacher), `lift` =
Schwarzwert anheben (matt), `shoulder` = weiches Abrollen der Lichter. Farbverschiebung
als (R, G, B), ±0,03 = deutlich. Stützpunkt-Looks sind je Kanal frei gezeichnet.

| Look         | Typ        | Kontrast | Lift | Schulter | Schatten (R,G,B)        | Lichter (R,G,B)         | Charakter |
|--------------|------------|---------:|-----:|---------:|-------------------------|-------------------------|-----------|
| Postcard     | Rezept     |  0,20    | 0,03 | 0,05     | 0 / 0 / +0,010          | +0,010 / 0 / −0,005     | knackig, satt, offene Schatten |
| Blush        | Rezept     |  0,10    | 0,01 | 0,15     | 0 / 0 / +0,005          | +0,008 / +0,003 / 0     | weich, rosige Lichter |
| Journal      | Stützpkt.  | leicht S | –    | –        | neutral                 | neutral                 | dokumentarisch, gedämpft (gemessen) |
| Studio       | Stützpkt.  | flach    | –    | –        | offen                   | leicht abgesenkt        | flach, offene Schatten |
| Drugstore    | Stützpkt.  | hart     | –    | –        | cyan                    | warm                    | Consumer-Film-Look |
| Scene        | Rezept     | −0,12    | 0,04 | 0,25     | −0,012 / +0,004 / +0,010| +0,006 / +0,002 / −0,004| sehr flach, kühle Schatten |
| Silver (S/W) | Rezept     |  0,26    | 0,00 | 0,08     | Gain 1,10 / 1,0 / 0,92  |                         | S/W, leichter Gelbfilter |
| Portrait     | Rezept     |  0,05    | 0,02 | 0,20     | +0,004 / +0,002 / −0,002| +0,012 / +0,004 / −0,006| weich, warme Hauttöne |
| Tungsten     | Stützpkt.  | mittel   | –    | –        | Teal                    | warme Spitzlichter      | kühl, Kunstlicht-Film |
| Expired      | Rezept     | −0,05    | 0,09 | 0,20     | +0,028 / −0,015 / +0,018| +0,005 / +0,022 / −0,028| stark matt, magenta Schatten, gelbgrüne Lichter |
| Cross        | Stützpkt.  | hart     | –    | –        | blau                    | gelbgrün                | Crossentwicklung |
| Violet       | Rezept     |  0,15    | 0,02 | 0,10     | +0,006 / −0,004 / +0,010| +0,006 / 0 / +0,004     | + Mix 1: Lila-Farbtausch |
| Concrete     | Rezept     |  0,28    | 0,02 | 0,10     | 0 / 0 / +0,004          | +0,004 / +0,002 / 0     | + Mix 2: entsättigt, hart |
| Ember        | Stützpkt.  | mittel   | –    | –        | warm                    | pfirsich                | kupfern-warm, Blau gedämpft |
| Grit (S/W)   | Rezept     |  0,38    | 0,00 | 0,04     | Gain 1,20 / 1,0 / 0,80  |                         | S/W, hart, Orangefilter |

Daylight 100 und Aus haben keine Filmkurve (nur Dynamik+-Kurven, falls aktiv).

## 3. Notizen / offene Punkte

| Look | Datum | Notiz |
|------|-------|-------|
| Postcard, Journal, Drugstore | v1.9.21 | Schatten geöffnet, Canon-Kontrast +1 entfernt |
| Journal | v1.9.24 | Kurve gegen X-E2 Classic Chrome gemessen |
| Ember | v1.9.24 | deutlich zurückgenommen (Bild war komplett rot) |
