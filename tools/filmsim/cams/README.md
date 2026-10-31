# Kameras in EXCCD

Ein gemeinsamer Kern (`core/filmsim.c`, `core/filmsim_raw.h`) fuer alle Kameras.
Der Kern fragt nie den Kameranamen ab, sondern nur:

- was CHDK schon weiss: `CAM_SENSOR_BITS_PER_PIXEL` (10 oder 12), `cam_CFAPattern`,
  Sensorgroesse
- die EXCCD-Schalter im Block `EXCCD` am Ende von `platform/<kamera>/platform_camera.h`
  (Bedeutung und Standardwerte im Kopf von `core/filmsim.c`)

Pro Kamera gibt es hier einen Ordner mit allem, was nur diese Kamera betrifft:

| Datei | Inhalt |
|---|---|
| `notes.md` | Firmware, Start, Sensor, Tasten, offene Punkte |
| `kameratest.md` | Checkliste fuer den Test auf der Kamera |
| `PALREC.BIN` | Palettendump der Kamera (EXCCD-Optionen -> System -> Farbpalette speichern) |
| weitere | z. B. `palette_foto.json` (gemessene Farben fuer die Laschen) |

## Neue Kamera

1. **Branch** `<plattform>` von `main`. Er lebt nur bis zum Kameratest.
2. **Eintrag** in `tools/filmsim/cameras.txt` mit Status `test`.
3. **CHDK pruefen:** Die Kamera muss in CHDK 1.6.1 existieren
   (`platform/<plattform>/sub/<fw>/`). Der Kern kann 10 und 12 Bit sowie
   R G / G B und G B / R G. Fuer alles andere bricht der Build mit `#error` ab.
   Dann `core/filmsim_raw.h` erweitern und in `tools/filmsim/test_raw.c` mittesten.
4. **Block `EXCCD`** in `platform_camera.h` anlegen. Am Anfang reicht ein Kommentar:
   ohne Schalter gibt es Textkarten in CHDK-Farben und die WB-Gains der IXUS 60.
   Ausserdem die ALT-Taste (`CAM_ALT_BUTTON_*`) und den Menue-Cursor (weiss/schwarz) setzen.
5. **Ordner** `tools/filmsim/cams/<plattform>/` mit `notes.md` und `kameratest.md`
   (Vorlage: `ixus145_elph135/`).
6. **Bauen:** `tools/filmsim/build.sh <plattform>`
7. **Andere Kameras gegenpruefen:** `tools/filmsim/compare_build.sh main`
   muss fuer alle anderen Kameras "identisch" melden, ausser bei Aenderungen
   am Kern, die absichtlich alle betreffen.
8. **Kameratest.** Danach Palette, Laschen und Weissabgleich nachziehen
   (`FILMSIM_LABELS`, `FILMSIM_COLOR_*`, `FILMSIM_WB_R/_B`), Status auf
   `stabil`, Branch nach `main` mergen und loeschen.

Eine Versionsnummer gilt fuer alle Kameras. In den Release-Notizen steht pro
Kamera, ob sie auf dieser Version getestet wurde.

## Grenzen

- Die Download-Webseite (`tools/site/build_site.py`) kann bisher nur eine Kamera.
  Die CI bricht ab, wenn mehr als eine Kamera `stabil` ist.
- Die Laschen (`CHDK/FILMS/*.FLM`) und der Splash sind Palettenindizes der IXUS 60.
  Sie liegen in jedem ZIP, werden aber nur mit `FILMSIM_LABELS` benutzt.
  Laschen fuer eine zweite Palette brauchen eigene Dateien pro Kamera im ZIP.
