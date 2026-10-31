/*
 * Filmsimulationen fuer CHDK - IXUS 60 / PowerShot SD600
 *
 * Eine Simulation besteht aus:
 *   1. Canon "My Colors"-Modus (Custom Color, Schwarzweiss, Sepia ...)
 *   2. bei Custom Color: 7 Regler (-2..+2) Kontrast, Schaerfe, Saettigung,
 *      Rot, Gruen, Blau, Hautton
 *   3. optional einer RAW-Tonkurve (A/CHDK/CURVES/<name>.CV), die curves.flt
 *      vor der JPEG-Erzeugung auf die Rohdaten anwendet.
 *
 * filmsim_tick() wird aus der CHDK-Hauptschleife aufgerufen und stellt im
 * Aufnahmemodus sicher, dass die Canon-Einstellungen zur gewaehlten
 * Simulation passen (auch nach dem Einschalten oder wenn jemand im
 * Canon-Menue My Colors aendert).
 */

#include "platform.h"
#include "stdlib.h"
#include "camera_info.h"
#include "conf.h"
#include "properties.h"
#include "shooting.h"
#include "gui_draw.h"
#include "modules.h"
#include "curves.h"
#include "gui.h"
#include "gui_menu.h"
#include "keyboard.h"
#include "filmsim.h"
#include "filmsim_txt.h"

//-------------------------------------------------------------------
// Rohdaten-Zugriff fuer 10- und 12-Bit-Sensoren
//   Alle RAW-Funktionen rechnen auf der 10-Bit-Skala der IXUS 60
//   (Schwarz 31, Weiss 1023). Bei 12 Bit wird beim Lesen durch 4 geteilt
//   (127 -> 31, 4095 -> 1023); beim Schreiben bleiben die unteren zwei Bit
//   des Originalpixels erhalten - unveraenderte Pixel sind damit bitgleich,
//   veraenderte behalten ihr feines Sensorrauschen als natuerlichen Dither.
//   FS_GRP = Bytes je 8 Pixel.
//
//   FS_ROW0: erste Zeile mit R G (die Funktionen erwarten R G / G B).
//   IXUS 60: R G / G B ab Zeile 0, IXUS 145: G B / R G -> ab Zeile 1.
//-------------------------------------------------------------------
#if CAM_SENSOR_BITS_PER_PIXEL == 10 || CAM_SENSOR_BITS_PER_PIXEL == 12
#define FS_RAW 1
#define FS_GRP CAM_SENSOR_BITS_PER_PIXEL

#if cam_CFAPattern == 0x02010100        // Red Green Green Blue
#define FS_ROW0 0
#elif cam_CFAPattern == 0x01000201      // Green Blue Red Green
#define FS_ROW0 1
#else
#error "Filmsim: Bayer-Anordnung nicht unterstuetzt (cam_CFAPattern)"
#endif
#endif

#ifndef PROPCASE_MY_COLORS
    #error "Filmsimulation braucht PROPCASE_MY_COLORS (propset)"
#endif

// Eingebaute Tabelle, falls A/CHDK/FILMS/FILMS.TXT fehlt oder unbrauchbar ist
static const filmsim_t sims_builtin[] = {
    // name              mode        con shp sat  R   G   B  skin  curve           label      mix
    { "Aus",             MC_OFF,      0,  0,  0,  0,  0,  0,  0,  0,              0,          0 },
    { "Daylight 100",    MC_OFF,      0,  0,  0,  0,  0,  0,  0,  0,              "DAYLITE",  0 },
    { "Postcard 100",    MC_CUSTOM,   0,  1,  2,  1,  1,  1,  0,  "POSTCRD.CV",   "POSTCRD",  0 },
    { "Blush 100",       MC_CUSTOM,  -1,  0,  1,  0,  0,  0,  1,  "BLUSH.CV",     "BLUSH",    0 },
    { "Journal 400",     MC_CUSTOM,   0,  0, -1, -1, -1,  0,  0,  "JOURNAL.CV",   "JOURNAL",  0 },
    { "Studio 160",      MC_CUSTOM,  -1, -1, -1,  0,  0,  0,  1,  "STUDIO.CV",    "STUDIO",   0 },
    { "Drugstore 200",   MC_CUSTOM,   0,  0, -1,  0,  1, -1,  0,  "DRUGSTR.CV",   "DRUGSTR",  0 },
    { "Scene 500",       MC_CUSTOM,  -2, -1, -2,  0,  0,  0,  0,  "SCENE.CV",     "SCENE",    0 },
    { "Silver 400",      MC_BW,       0,  0,  0,  0,  0,  0,  0,  "SILVER.CV",    "SILVER",   0 },
    { "Portrait 400",    MC_CUSTOM,  -1, -1,  0,  0,  0, -1,  1,  "PORTRT.CV",    "PORTRT",   0 },
    { "Tungsten 800",    MC_OFF,      0,  0,  0,  0,  0,  0,  0,  "TUNGSTN.CV",   "TUNGSTN",  0 },
    { "Expired 200",     MC_CUSTOM,  -1, -1, -1,  0,  0,  0,  0,  "EXPIRED.CV",   "EXPIRED",  0 },
    { "Cross 100",       MC_CUSTOM,   0,  1,  2,  0,  0,  0,  0,  "CROSS.CV",     "CROSS",    0 },
    { "Violet 400",      MC_CUSTOM,   0,  0,  1,  0,  0,  0,  0,  "VIOLET.CV",    "VIOLET",   1 },
    { "Concrete 100",    MC_CUSTOM,   0,  1, -1,  0,  0,  0,  0,  "CONCRET.CV",   "CONCRET",  2 },
    { "Ember 200",       MC_CUSTOM,   0,  0,  1,  0,  0,  0,  0,  "EMBER.CV",     "EMBER",    0 },
    { "Grit 400",        MC_BW,       0,  0,  0,  0,  0,  0,  0,  "GRIT.CV",      "GRIT",     0 },
};
#define NUM_BUILTIN ((int)(sizeof(sims_builtin)/sizeof(sims_builtin[0])))

// Aktive Tabelle: eingebaut oder aus FILMS.TXT (Eintrag 0 ist immer "Aus")
static const filmsim_t *sims = sims_builtin;
static int num_sims = NUM_BUILTIN;
#define NUM_SIMS num_sims

#include "gui_osd.h"

// Nur im Foto-Aufnahmemodus (nicht Video, nicht Wiedergabe)
int filmsim_photo_mode(void)
{
    return camera_info.state.mode_rec && !camera_info.state.mode_video && !camera_info.state.mode_play;
}

// Darf die Print-Taste die Filmauswahl oeffnen?
// Nicht bei offenem Canon-Menue / FUNC-Menue (andere Bildschirmpalette)
static volatile int photo_since = 0;   // seit wann ununterbrochen Fotomodus (Tick)

int filmsim_alt_allowed(void)
{
    if (!filmsim_photo_mode()) return 0;
    // direkt nach dem Einschalten meldet die Kamera kurzzeitig falsche
    // Zustaende -> erst oeffnen, wenn seit >0,7 s stabil Fotomodus
    // und die Kamera schon >3 s laeuft
    int now = get_tick_count();
    if (now < 3000 || !photo_since || now - photo_since < 700) return 0;
    if (canon_menu_active != (int)&canon_menu_active - 4) return 0;
    if (canon_shoot_menu_active != 0) return 0;
    return 1;
}

//-------------------------------------------------------------------
// Gesamtes CHDK-OSD (Statuszeile, Gitter, Batterie ...) ausblenden, solange
// der Ausloeser halb gedrueckt oder das Canon-MENU / FUNC.SET-Menue offen ist.
//
// FILMSIM_OSD_KEYTRACK 1: Rueckfallebene, falls canon_shoot_menu_active auf
// der IXUS 60 nicht zuverlaessig ist - Menues ueber die Tasten verfolgen
// (wie Binaer-Patch v1.9.4).
//-------------------------------------------------------------------
#ifndef FILMSIM_OSD_KEYTRACK
#define FILMSIM_OSD_KEYTRACK 0
#endif

#if FILMSIM_OSD_KEYTRACK
static int canon_menus_open(void)
{
    static int func_open = 0, menu_open = 0, set_prev = 0, menu_prev = 0;
    if (!camera_info.state.mode_rec) {
        func_open = menu_open = 0;
        return 0;
    }
    int m = kbd_is_key_pressed(KEY_MENU) != 0;
    if (m && !menu_prev) { menu_open ^= 1; func_open = 0; }
    menu_prev = m;
    int s = kbd_is_key_pressed(KEY_SET) != 0;
    if (s && !set_prev && !menu_open) func_open ^= 1;  // im MENU ist SET normale Auswahl
    set_prev = s;
    if (camera_info.state.is_shutter_half_press) func_open = menu_open = 0;
    return func_open || menu_open;
}
#else
static int canon_menus_open(void)
{
    return canon_menu_active != (int)&canon_menu_active - 4 || canon_shoot_menu_active != 0;
}
#endif

// Canon zeigt seine Aufnahme-Symbole nach dem Schliessen von FUNC.SET/MENU,
// nach dem Einschalten bzw. Wechsel in den Aufnahmemodus und nach den
// Ringtasten (Blitz, Makro, Bildfolge ...) noch einige Sekunden
// -> so lange noch nichts zeichnen
#ifndef FILMSIM_MENU_HOLD_MS
#define FILMSIM_MENU_HOLD_MS 6000
#endif
// nach dem Loslassen des halb gedrueckten Ausloesers
#ifndef FILMSIM_HALF_HOLD_MS
#define FILMSIM_HALF_HOLD_MS 2000
#endif

int filmsim_osd_hidden(void)
{
    static int was_hidden = 0, was_menus = 0, was_rec = 0, was_half = 0;
    static int hold_until = 0;
    int now = get_tick_count();
    int menus = canon_menus_open();
    int rec = camera_info.state.mode_rec;
    int half = camera_info.state.is_shutter_half_press;
    int ring = rec && (kbd_is_key_pressed(KEY_UP) || kbd_is_key_pressed(KEY_DOWN)
                    || kbd_is_key_pressed(KEY_LEFT) || kbd_is_key_pressed(KEY_RIGHT));
    if ((was_menus && !menus) || (rec && !was_rec) || ring)
        hold_until = now + FILMSIM_MENU_HOLD_MS;
    else if (was_half && !half && hold_until - now < FILMSIM_HALF_HOLD_MS)
        hold_until = now + FILMSIM_HALF_HOLD_MS;
    was_menus = menus;
    was_rec = rec;
    was_half = half;
    int hidden = menus || half || hold_until - now > 0;
    if (hidden && !was_hidden) gui_set_need_restore();   // Reste wegwischen
    was_hidden = hidden;
    return hidden;
}

//-------------------------------------------------------------------
// Filmtabelle aus A/CHDK/FILMS/FILMS.TXT (einmalig nach dem Start).
// Fehlt die Datei oder enthaelt sie keinen gueltigen Look, bleibt die
// eingebaute Tabelle aktiv. Fehlerhafte Zeilen -> A/CHDK/FILMS/FILMS.LOG
//-------------------------------------------------------------------
#define FILMS_TXT   "A/CHDK/FILMS/FILMS.TXT"
#define FILMS_LOG   "A/CHDK/FILMS/FILMS.LOG"
#define FILMS_LOG_SIZE 1024

static filmsim_t films_file[1 + FILMS_TXT_MAX];
static films_txt_str_t films_str[FILMS_TXT_MAX];

static void films_load(void)
{
    static int done = 0;
    struct stat st;
    char *buf = 0, *log = 0;
    int n = 0, len = -1;
    if (done) return;
    done = 1;

    if (stat(FILMS_TXT, &st) != 0) {
        if (stat(FILMS_LOG, &st) == 0) remove(FILMS_LOG);   // veraltetes Protokoll
        return;
    }
    log = malloc(FILMS_LOG_SIZE);
    if (!log) return;
    log[0] = 0;
    if (st.st_size > 0 && st.st_size <= FILMS_TXT_FILE_MAX)
        buf = malloc(st.st_size + 1);
    if (buf) {
        int fd = open(FILMS_TXT, O_RDONLY, 0777);
        if (fd >= 0) {
            len = read(fd, buf, st.st_size);
            close(fd);
        }
        if (len > 0)
            n = films_txt_parse(buf, len, films_file + 1, films_str, FILMS_TXT_MAX, log, FILMS_LOG_SIZE - 80);
        free(buf);
    } else {
        strcpy(log, "Datei leer oder groesser als 16 KB\r\n");
    }

    if (n > 0) {
        films_file[0] = sims_builtin[0];    // "Aus"
        sims = films_file;
        num_sims = n + 1;
    } else {
        strcat(log, "Keine gueltigen Looks - eingebaute Tabelle aktiv\r\n");
    }

    if (log[0]) {
        char hdr[48];
        sprintf(hdr, "FILMS.TXT: %d Looks geladen\r\n", n);
        int fd = open(FILMS_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd >= 0) {
            write(fd, hdr, strlen(hdr));
            write(fd, log, strlen(log));
            close(fd);
        }
    } else if (stat(FILMS_LOG, &st) == 0) {
        remove(FILMS_LOG);
    }
    free(log);
}

static const filmsim_t* current(void)
{
    int i = conf.film_sim;
    if (i < 0 || i >= NUM_SIMS) i = 0;
    return &sims[i];
}

const char* filmsim_name(void)
{
    return current()->name;
}

static int curve_file_ok = 0;     // Diagnose: Kurvendatei gefunden?
static int set_count = 0;         // Diagnose: wie oft Propcases gesetzt

// Canon uebernimmt neue Farbwerte im Sucherbild erst beim naechsten
// Halbdruck -> bis dahin Hinweis anzeigen
static int preview_pending = 0;

// Canon-Propcases setzen
static void set_canon_props(const filmsim_t *s)
{
    set_count++;
    shooting_set_prop(PROPCASE_MY_COLORS, s->mycolors);
    if (s->mycolors == MC_CUSTOM) {
        shooting_set_prop(PROPCASE_CUSTOM_CONTRAST,   s->contrast);
        shooting_set_prop(PROPCASE_CUSTOM_SHARPNESS,  s->sharpness);
        shooting_set_prop(PROPCASE_CUSTOM_SATURATION, s->saturation);
        shooting_set_prop(PROPCASE_CUSTOM_RED,        s->red);
        shooting_set_prop(PROPCASE_CUSTOM_GREEN,      s->green);
        shooting_set_prop(PROPCASE_CUSTOM_BLUE,       s->blue);
        shooting_set_prop(PROPCASE_CUSTOM_SKIN_TONE,  s->skin);
    }
}

static int canon_props_match(const filmsim_t *s)
{
    if (shooting_get_prop(PROPCASE_MY_COLORS) != s->mycolors) return 0;
    if (s->mycolors != MC_CUSTOM) return 1;
    return shooting_get_prop(PROPCASE_CUSTOM_CONTRAST)   == s->contrast
        && shooting_get_prop(PROPCASE_CUSTOM_SHARPNESS)  == s->sharpness
        && shooting_get_prop(PROPCASE_CUSTOM_SATURATION) == s->saturation
        && shooting_get_prop(PROPCASE_CUSTOM_RED)        == s->red
        && shooting_get_prop(PROPCASE_CUSTOM_GREEN)      == s->green
        && shooting_get_prop(PROPCASE_CUSTOM_BLUE)       == s->blue
        && shooting_get_prop(PROPCASE_CUSTOM_SKIN_TONE)  == s->skin;
}

//-------------------------------------------------------------------
// Eigene Tonkurven fuer 12-Bit-Sensoren
//   curves.flt kann nur 10 Bit (CHDK schaltet OPT_CURVES bei 12 Bit ab).
//   Die .CV-Dateien bleiben dieselben (4 x 1024 Werte, 10 Bit: R, G der
//   R-Zeile, G der B-Zeile, B). Beim Laden entsteht daraus eine Tabelle
//   mit 4 x 4096 Eintraegen, linear zwischen den 10-Bit-Stuetzstellen
//   interpoliert (Schwarz 127 / Weiss 4095 liegen exakt auf 31 / 1023).
//   Das Anwenden ist dann nur noch ein Tabellenzugriff je Pixel.
//-------------------------------------------------------------------
#if defined(FS_RAW) && CAM_SENSOR_BITS_PER_PIXEL == 12
#define FS_OWN_CURVES 1
#define CV_N 1024
static unsigned short *curve_lut = 0;

static void curve_lut_free(void)
{
    if (curve_lut) free(curve_lut);
    curve_lut = 0;
}

static int curve_lut_load(const char *fn)
{
    unsigned short *cv = malloc(4 * CV_N * sizeof(unsigned short));
    int ok = 0, c, v;
    curve_lut_free();
    if (!cv) return 0;
    int fd = open(fn, O_RDONLY, 0777);
    if (fd >= 0) {
        ok = read(fd, cv, 4 * CV_N * sizeof(unsigned short)) == 4 * CV_N * (int)sizeof(unsigned short);
        close(fd);
    }
    if (ok) curve_lut = malloc(4 * 4096 * sizeof(unsigned short));
    if (curve_lut) {
        for (c = 0; c < 4; c++) {
            const unsigned short *k = cv + c * CV_N;
            unsigned short *t = curve_lut + c * 4096;
            for (v = 0; v < 4096; v++) {
                int i = v >> 2, f = v & 3;
                int k0 = k[i] > 1023 ? 1023 : k[i];
                // letzte Stuetzstelle: Steigung fortsetzen (4093..4095)
                int k1 = (i < CV_N - 1) ? (k[i + 1] > 1023 ? 1023 : k[i + 1]) : 2 * k0 - (k[i - 1] > 1023 ? 1023 : k[i - 1]);
                int o = k0 * (4 - f) + k1 * f;          // = 4 * Interpolation
                t[v] = v ? (o < 1 ? 1 : (o > 4095 ? 4095 : o)) : 0;   // 0 = Defekt-Markierung
            }
        }
    }
    free(cv);
    return curve_lut != 0;
}

// Aus raw.c, an der Stelle von curves.flt
__attribute__((optimize("O2"))) void filmsim_curve_apply(char *raw)
{
    if (!curve_lut || !raw) return;
    int rows = camera_sensor.raw_rows, quads = camera_sensor.raw_rowpix / 4;
    int rowlen = camera_sensor.raw_rowlen, y, g;
    for (y = 0; y < rows; y++) {
        // R-Zeile: Kurve 0 (gerade Spalten, R) / 1 (G), B-Zeile: 2 (G) / 3 (B)
        const unsigned short *ce = curve_lut + (((y ^ FS_ROW0) & 1) << 1) * 4096;
        const unsigned short *co = ce + 4096;
        unsigned char *a = (unsigned char *)raw + y * rowlen;
        for (g = 0; g < quads; g++, a += 6) {
            int w0 = ce[(a[1] << 4) | (a[0] >> 4)];
            int w1 = co[((a[0] & 0x0f) << 8) | a[3]];
            int w2 = ce[(a[2] << 4) | (a[5] >> 4)];
            int w3 = co[((a[5] & 0x0f) << 8) | a[4]];
            a[1] = w0 >> 4;
            a[0] = (w0 << 4) | (w1 >> 8);
            a[3] = w1;
            a[2] = w2 >> 4;
            a[5] = (w2 << 4) | (w3 >> 8);
            a[4] = w3;
        }
    }
}
#endif

// Kurve laden bzw. abschalten
static void set_curve(const filmsim_t *s)
{
#if defined(OPT_CURVES) || defined(FS_OWN_CURVES)
    int dr = (conf.film_dr >= 1 && conf.film_dr <= 2) ? conf.film_dr : 0;
    if (s->curve || dr) {
        struct stat st;
        strcpy(conf.curve_file, CURVE_DIR "/");
        if (!s->curve) {
            strcat(conf.curve_file, dr == 1 ? "DR1.CV" : "DR2.CV");    // nur Dynamik+
        } else if (dr) {
            // POSTCRD.CV -> POSTCRD1.CV / POSTCRD2.CV
            char *e = conf.curve_file + strlen(conf.curve_file);
            strcat(conf.curve_file, s->curve);
            char *dot = strrchr(e, '.');
            if (dot) *dot = 0;
            strcat(conf.curve_file, dr == 1 ? "1.CV" : "2.CV");
        } else {
            strcat(conf.curve_file, s->curve);
        }
        curve_file_ok = (stat(conf.curve_file, &st) == 0) ? 1 : -1;
        conf.curve_enable = 1;
    } else {
        conf.curve_enable = 0;
        curve_file_ok = 0;
    }
#ifdef FS_OWN_CURVES
    if (!conf.curve_enable || !curve_lut_load(conf.curve_file)) {
        curve_lut_free();
        if (conf.curve_enable) curve_file_ok = -1;
    }
    conf.curve_enable = 0;           // curves.flt gibt es hier nicht
#else
    libcurves->curve_init_mode();
#endif
#else
    (void)s;
#endif
}

// Simulation vollstaendig anwenden (bei Menueauswahl / Start)
void filmsim_apply(void)
{
    const filmsim_t *s = current();
    if (filmsim_photo_mode())
        set_canon_props(s);
    set_curve(s);
}

// Wird ca. alle 20 ms aus der Hauptschleife aufgerufen
void filmsim_tick(void)
{
    static int cnt = 0;
    static int curve_done = 0;

    if (camera_info.state.is_shutter_half_press)
        preview_pending = 0;         // Canon hat die Werte jetzt uebernommen

    if (filmsim_photo_mode()) {
        if (!photo_since) photo_since = get_tick_count() | 1;
    } else {
        photo_since = 0;
    }

    // Filmauswahl / Optionen offen, aber Modus-Schalter auf Video oder
    // Wiedergabe gestellt -> sofort schliessen (andere Farbpalette)
    static int close_sent = 0;
    if (camera_info.state.gui_mode > GUI_MODE_NONE && !filmsim_photo_mode()) {
        if (!close_sent) {
            close_sent = 1;
            extern void filmsim_label_release(void);
            filmsim_label_release();
            exit_alt();
        }
    } else {
        close_sent = 0;
    }

    if (++cnt < 25) return;          // ~ alle 0,5 s pruefen
    cnt = 0;

    if (!curve_done) {               // einmalig nach dem Start Kurve laden
        films_load();
        // Index aus aelterer Version / kuerzerer FILMS.TXT -> "Aus"
        if (conf.film_sim < 0 || conf.film_sim >= NUM_SIMS) conf.film_sim = 0;
        set_curve(current());
        curve_done = 1;
    }

    if (!filmsim_photo_mode()) return;
    if (!camera_info.state.gui_mode_none) return;   // nicht in Menues eingreifen
    if (camera_info.state.state_shooting_progress == SHOOTING_PROGRESS_PROCESSING) return;

    // Altlast aus v0.9: von der Koernung erzwungene ISO 800 zuruecknehmen
    if (conf.film_iso_prev >= 0) {
        extern short shooting_get_canon_iso_mode();
        if (shooting_get_canon_iso_mode() == 800)
            shooting_set_iso_mode(conf.film_iso_prev ? conf.film_iso_prev : 0);
        conf.film_iso_prev = -1;
        conf.film_grain = 0;
    }
    if (conf.film_sim == 0) return;  // "Aus": Canon-Farben nicht anfassen

    const filmsim_t *s = current();
    if (!canon_props_match(s))
        set_canon_props(s);
}

// Film auswaehlen und sofort anwenden
void filmsim_set(int v)
{
    if (v < 0 || v >= NUM_SIMS) return;
    int was_off = (conf.film_sim == 0);
    if (v != conf.film_sim) preview_pending = 1;
    conf.film_sim = v;
    if (v == 0) {
        // zurueck auf Canon-Standard
        if (!was_off && filmsim_photo_mode())
            shooting_set_prop(PROPCASE_MY_COLORS, MC_OFF);
        set_curve(current());
    } else {
        filmsim_apply();
    }
}

//-------------------------------------------------------------------
// Filmauswahl im Stil eines Filmschachtel-Fensters
//   Print-Taste  -> Auswahl oeffnet sich mit dem aktiven Film
//   links/rechts oder hoch/runter -> blaettern
//   FUNC/SET     -> uebernehmen und schliessen
//   MENU         -> Optionen (RAW, System)
//   Print        -> abbrechen
//-------------------------------------------------------------------
extern CMenu root_menu;     // "Optionen" (core/gui.c)
extern gui_handler menuGuiHandler;

#define LABEL_TOP   4
#define LABEL_H     196
#define BAR_Y       (LABEL_TOP + LABEL_H + 10)
#define LABEL_W     301                 // Breite der EXCCD-Laschen

// EXCCD-Farben wie in den Laschen (Kamerapalette, Aufnahmemodus, alle deckend).
// Laschen (.FLM) und diese Farben sind Indizes der IXUS-60-Palette. Andere
// Kameras zeigen die Textkarte in CHDK-Farben, bis es Laschen fuer ihre
// Palette gibt (Optionen -> System -> Farbpalette speichern).
#ifdef CAMERA_ixus60_sd600
#define FS_FLM      1
#define EX_BG       0xFF
#define EX_GREY     0xA3
#define EX_RULE     0xFA
#else
#define EX_BG       COLOR_BLACK
#define EX_GREY     COLOR_GREY
#define EX_RULE     COLOR_GREY_DK
#endif

static int sel = 0;                 // gerade angezeigter Film
static int need_draw = 0;           // 1 = alles, 2 = nur Leiste
static unsigned char *label_buf = 0;
static int label_len = 0;
static int label_for = -1;          // fuer welchen Film label_buf gilt

static void label_free(void)
{
    if (label_buf) free(label_buf);
    label_buf = 0; label_len = 0; label_for = -1;
}

static void label_load(int i)
{
    char fn[40];
    struct stat st;
    label_free();
    label_for = i;
#ifndef FS_FLM
    return;
#endif
    if (!sims[i].label) return;
    strcpy(fn, "A/CHDK/FILMS/");
    strcat(fn, sims[i].label);
    strcat(fn, ".FLM");
    if (stat(fn, &st) != 0 || st.st_size < 8 || st.st_size > 120000) return;
    label_buf = malloc(st.st_size);
    if (!label_buf) return;
    int fd = open(fn, O_RDONLY, 0777);
    if (fd < 0) { label_free(); label_for = i; return; }
    label_len = read(fd, label_buf, st.st_size);
    close(fd);
    if (label_len < 8 || memcmp(label_buf, "FLM1", 4) != 0) { label_free(); label_for = i; }
}

// RLE-Lasche zeichnen; 0 = keine Datei
static int label_draw(void)
{
    if (!label_buf) return 0;
    int w = label_buf[4] | (label_buf[5] << 8);
    int h = label_buf[6] | (label_buf[7] << 8);
    if (w <= 0 || h <= 0 || w > camera_screen.width || h > LABEL_H) return 0;
    int ox = (camera_screen.width - w) / 2;
    int oy = LABEL_TOP + (LABEL_H - h) / 2;
    int x = 0, y = 0, p = 8;
    while (p + 1 < label_len && y < h) {
        int n = label_buf[p], c = label_buf[p + 1];
        p += 2;
        while (n > 0 && y < h) {
            int run = w - x;
            if (run > n) run = n;
            if (c) draw_hline(ox + x, oy + y, run, c);
            x += run; n -= run;
            if (x >= w) { x = 0; y++; }
        }
    }
    return 1;
}

// Ersatz, wenn es keine Lasche gibt: Textkarte im Aufbau der Laschen
static void card_draw(int i)
{
    const char *n = sims[i].name;
    int len = strlen(n);
    int x0 = (camera_screen.width - LABEL_W) / 2, y0 = LABEL_TOP, m = 16, k;
    draw_rectangle(x0, y0, x0 + LABEL_W - 1, y0 + LABEL_H - 1, MAKE_COLOR(EX_BG, EX_RULE),
                   RECT_BORDER1|DRAW_FILLED|RECT_ROUND_CORNERS);
    draw_string(x0 + m, y0 + 12, "EXCCD", MAKE_COLOR(EX_BG, COLOR_WHITE));
    draw_hline(x0 + m, y0 + 34, LABEL_W - 2 * m, EX_RULE);
    int sx = (len * FONT_WIDTH * 2 <= LABEL_W - 2 * m) ? 2 : 1;
    draw_string_scaled(x0 + m, y0 + 60, n, MAKE_COLOR(EX_BG, COLOR_WHITE), sx, 2);
    // Perforationslinie
    draw_hline(x0 + m, y0 + 156, LABEL_W - 2 * m, EX_RULE);
    for (k = 0; k < 16; k++)
        draw_rectangle(x0 + m + 3 + k * 17, y0 + 166, x0 + m + 3 + k * 17 + 7, y0 + 176,
                       MAKE_COLOR(EX_RULE, EX_RULE), RECT_BORDER0|DRAW_FILLED);
}

// Leiste unter der Lasche: Pfeile grau, Name weiss, aktiver Film mit Quadrat
static void bar_draw(void)
{
    const char *n = sims[sel].name;
    int len = strlen(n);
    int x0 = (camera_screen.width - LABEL_W) / 2, x1 = x0 + LABEL_W - FONT_WIDTH;
    int x = (camera_screen.width - len * FONT_WIDTH) / 2;
    draw_rectangle(0, LABEL_TOP + LABEL_H, camera_screen.width - 1, camera_screen.height - 1,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
    draw_string(x0, BAR_Y, "<", MAKE_COLOR(COLOR_BLACK, EX_GREY));
    draw_string(x1, BAR_Y, ">", MAKE_COLOR(COLOR_BLACK, EX_GREY));
    if (sel == conf.film_sim) {
        x += 6;
        draw_rectangle(x - 12, BAR_Y + 5, x - 7, BAR_Y + 10, MAKE_COLOR(COLOR_WHITE, COLOR_WHITE),
                       RECT_BORDER0|DRAW_FILLED);
    }
    draw_string(x, BAR_Y, n, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
}

static void film_gui_draw(int force)
{
    if (force) need_draw = 1;
    if (!need_draw) return;
    if (need_draw == 1) {
        draw_rectangle(0, 0, camera_screen.width - 1, camera_screen.height - 1,
                       MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
        if (label_for != sel) label_load(sel);
        if (!label_draw()) card_draw(sel);
    }
    bar_draw();
    need_draw = 0;
}

void filmsim_label_release(void)
{
    label_free();
}

static void film_gui_close(void)
{
    label_free();
    // erst schliessen, wenn die Taste losgelassen ist - sonst bekommt
    // die Canon-Firmware das Loslassen von FUNC/SET mit und oeffnet ihr Menue
    exit_alt_on_release();
}

static int film_gui_kbd(void)
{
    switch (kbd_get_autoclicked_key() | get_jogdial_direction())
    {
    case KEY_LEFT: case KEY_UP: case JOGDIAL_LEFT:
        sel = (sel > 0) ? sel - 1 : NUM_SIMS - 1;
        need_draw = 1;
        break;
    case KEY_RIGHT: case KEY_DOWN: case JOGDIAL_RIGHT:
        sel = (sel < NUM_SIMS - 1) ? sel + 1 : 0;
        need_draw = 1;
        break;
    case KEY_SET:
        filmsim_set(sel);
        conf_save();
        film_gui_close();
        break;
    }
    return 0;
}

static void film_gui_menu_btn(void)
{
    label_free();
    gui_menu_init(&root_menu);
    gui_set_mode(&menuGuiHandler);
}

static gui_handler filmGuiHandler = { GUI_MODE_MODULE, film_gui_draw, film_gui_kbd, film_gui_menu_btn, 0, 0 };

void filmsim_open_menu(void)
{
    films_load();
    sel = conf.film_sim;
    if (sel < 0 || sel >= NUM_SIMS) sel = 0;
    need_draw = 1;
    gui_set_mode(&filmGuiHandler);
}

// Menue-Eintrag "Filmsimulation" (MENUITEM_ENUM)
const char* filmsim_menu_enum(int change, __attribute__ ((unused))int arg)
{
    films_load();
    if (change) {
        int v = conf.film_sim + change;
        if (v < 0) v = NUM_SIMS - 1;
        if (v >= NUM_SIMS) v = 0;
        filmsim_set(v);
    }
    return filmsim_name();
}

// Kleine Anzeige des aktiven Films im Sucherbild
void filmsim_draw_osd(void)
{
    static char buf[96];
    static int was_pending = 0;
    // Modus gewechselt -> alten Filmnamen einmal wegwischen
    static int was_photo = -1;
    int is_photo = filmsim_photo_mode();
    if (was_photo == 1 && !is_photo) gui_set_need_restore();
    was_photo = is_photo;
    if (!is_photo) return;
    // Halbdruck (Canon zeigt unten Zeit/Blende), MENU, FUNC.SET -> ausblenden
    if (filmsim_osd_hidden()) return;
    if (was_pending && !preview_pending)
        gui_set_need_restore();      // laengeren Hinweis einmal wegwischen
    was_pending = preview_pending;
    if (conf.film_sim == 0 && !preview_pending && !conf.tv_override_enabled
        && !conf.film_double && !conf.subj_dist_override_koef
        && !conf.film_mist && !conf.film_halation && !conf.film_vignette
        && !conf.film_leak && !conf.film_datestamp && !conf.film_dr && !conf.film_cnr) return;
    strcpy(buf, filmsim_name());
    if (preview_pending) {
        strcat(buf, " - Ausloeser antippen");
    } else {
        extern const char* filmsim_tv_str(void);
        const char *tv = filmsim_tv_str();
        if (tv) { strcat(buf, " | "); strcat(buf, tv); }
        extern const char* filmsim_focus_str(void);
        const char *fo = filmsim_focus_str();
        if (fo) { strcat(buf, " | "); strcat(buf, fo); }
        if (conf.film_double) strcat(buf, conf.film_double == 1 ? " | 2x 1/2" : " | 2x 2/2");
        if (conf.film_dr == 1) strcat(buf, " | DR+1");
        if (conf.film_dr == 2) strcat(buf, " | DR+2");
        if (conf.film_cnr) strcat(buf, " | NR");
        extern int filmsim_effects_active(void);
        if (filmsim_effects_active()) strcat(buf, " | FX");
        extern const char* filmsim_bwfilter_str(void);
        const char *bf = filmsim_bwfilter_str();
        if (bf) { strcat(buf, " | "); strcat(buf, bf); }
    }
    const char *n = buf;
    int len = strlen(n);
    static int last_len = 0;
    if (len < last_len) gui_set_need_restore();   // kuerzerer Text: Reste wegwischen
    last_len = len;
    int x = (camera_screen.width - len * FONT_WIDTH) / 2;
    if (x < 0) x = 0;
    int y = camera_screen.height - FONT_HEIGHT - 4;   // unten, weg von den Canon-Symbolen oben
    draw_string(x, y, n, user_color(conf.osd_color));
}


//-------------------------------------------------------------------
// RAW-Kanalmischung (Spezialfilme)
//   Wird im RAW-Hook nach den Tonkurven aufgerufen, vor der JPEG-Erzeugung.
//   Je 2x2-Bayer-Block (R G / G B) werden die Kanaele - auf typischen
//   Tageslicht-Weissabgleich normiert - per Matrix neu gemischt.
//   Zeilensummen = 1, damit Grau grau bleibt.
//-------------------------------------------------------------------
#ifdef FS_RAW
// Faktoren *256. Lomochrome Purple: Gruen -> Violett, Blau -> Gruen/Tuerkis,
// Gelb -> Pink
static const short mix_lomo[3][3] = {
    { 128, 128,   0 },   // R' = 0.5 R + 0.5 G
    {  51,  77, 128 },   // G' = 0.2 R + 0.3 G + 0.5 B
    {  51, 205,   0 },   // B' = 0.2 R + 0.8 G
};
// LomoChrome Metropolis: entsaettigt, Blau/Gruen stark gedaempft, Haut bleibt
static const short mix_metro[3][3] = {
    { 192,  51,  13 },
    {  64, 154,  38 },
    {  51, 102, 103 },
};
// Farbfilter fuer Schwarzweiss: alle drei Kanaele = gewichtete Helligkeit
static const short mix_bw[4][3][3] = {
    {{0}},                                                          // kein Filter
    {{102,128,26},{102,128,26},{102,128,26}},                       // Gelb   .40 .50 .10
    {{141,102,13},{141,102,13},{141,102,13}},                       // Orange .55 .40 .05
    {{192, 64, 0},{192, 64, 0},{192, 64, 0}},                       // Rot    .75 .25 .00
};
// WB-Normierung (Rohwert * f / 256), Tageslicht-Gains des Sensors.
// IXUS 60: R 1.9, B 1.45. Andere Kameras: FILMSIM_WB_R/_B in platform_camera.h
#ifdef FILMSIM_WB_R
#define WB_R FILMSIM_WB_R
#define WB_B FILMSIM_WB_B
#else
#define WB_R 486
#define WB_B 371
#endif
#define BLK  31

#if CAM_SENSOR_BITS_PER_PIXEL == 12
// 4 Pixel in 6 Byte (Anordnung wie get_raw_pixel/set_raw_pixel in raw.c)
static inline __attribute__((always_inline)) void dec4_12(const unsigned char *a, int *v)
{
    v[0] = ((a[1] << 4) | (a[0] >> 4)) >> 2;
    v[1] = (((a[0] & 0x0f) << 8) | a[3]) >> 2;
    v[2] = ((a[2] << 4) | (a[5] >> 4)) >> 2;
    v[3] = (((a[5] & 0x0f) << 8) | a[4]) >> 2;
}
static inline __attribute__((always_inline)) void enc4_12(unsigned char *a, const int *v)
{
    // untere zwei Bit aus dem Original uebernehmen (0 bleibt 0: Defekt-Markierung)
    int w0 = v[0] ? (v[0] << 2) | ((a[0] >> 4) & 3) : 0;
    int w1 = v[1] ? (v[1] << 2) | (a[3] & 3) : 0;
    int w2 = v[2] ? (v[2] << 2) | ((a[5] >> 4) & 3) : 0;
    int w3 = v[3] ? (v[3] << 2) | (a[4] & 3) : 0;
    a[1] = w0 >> 4;
    a[0] = (w0 << 4) | ((w1 >> 8) & 0x0f);
    a[3] = w1;
    a[2] = w2 >> 4;
    a[5] = (w2 << 4) | ((w3 >> 8) & 0x0f);
    a[4] = w3;
}
static inline __attribute__((always_inline)) void dec8(const unsigned char *a, int *v)
{
    dec4_12(a, v); dec4_12(a + 6, v + 4);
}
static inline __attribute__((always_inline)) void enc8(unsigned char *a, const int *v)
{
    enc4_12(a, v); enc4_12(a + 6, v + 4);
}
#else
static inline __attribute__((always_inline)) void dec8(const unsigned char *a, int *v)
{
    v[0] = (a[1] << 2) | (a[0] >> 6);
    v[1] = ((a[0] & 0x3f) << 4) | (a[3] >> 4);
    v[2] = ((a[3] & 0x0f) << 6) | (a[2] >> 2);
    v[3] = ((a[2] & 0x03) << 8) | a[5];
    v[4] = (a[4] << 2) | (a[7] >> 6);
    v[5] = ((a[7] & 0x3f) << 4) | (a[6] >> 4);
    v[6] = ((a[6] & 0x0f) << 6) | (a[9] >> 2);
    v[7] = ((a[9] & 0x03) << 8) | a[8];
}
static inline __attribute__((always_inline)) void enc8(unsigned char *a, const int *v)
{
    a[1] = v[0] >> 2;
    a[0] = (v[0] << 6) | (v[1] >> 4);
    a[3] = (v[1] << 4) | (v[2] >> 6);
    a[2] = (v[2] << 2) | (v[3] >> 8);
    a[5] = v[3];
    a[4] = v[4] >> 2;
    a[7] = (v[4] << 6) | (v[5] >> 4);
    a[6] = (v[5] << 4) | (v[6] >> 6);
    a[9] = (v[6] << 2) | (v[7] >> 8);
    a[8] = v[7];
}
#endif
static inline __attribute__((always_inline)) int clip10(int x) { return x < 0 ? 0 : (x > 1023 ? 1023 : x); }

// Rueckrechnung des Weissabgleichs: (n << 8) / WB als Tabelle statt Division.
// Die alten ARM-Prozessoren haben keinen Divisionsbefehl - jede Division
// kostet dort Dutzende Takte. Die Tabelle enthaelt exakt dieselben Werte
// wie die Division (gleiche Formel), das Ergebnis ist bitgleich.
#define WBDIV_MIN  (-1024)
#define WBDIV_MAX  4095
static short wbdiv_r[WBDIV_MAX - WBDIV_MIN + 1];
static short wbdiv_b[WBDIV_MAX - WBDIV_MIN + 1];
static int wbdiv_ready = 0;
static void wbdiv_init(void)
{
    int n;
    if (wbdiv_ready) return;
    for (n = WBDIV_MIN; n <= WBDIV_MAX; n++) {
        wbdiv_r[n - WBDIV_MIN] = (short)((n << 8) / WB_R);
        wbdiv_b[n - WBDIV_MIN] = (short)((n << 8) / WB_B);
    }
    wbdiv_ready = 1;
}
static inline __attribute__((always_inline)) int div_wb_r(int n)
{
    return (n >= WBDIV_MIN && n <= WBDIV_MAX) ? wbdiv_r[n - WBDIV_MIN] : ((n << 8) / WB_R);
}
static inline __attribute__((always_inline)) int div_wb_b(int n)
{
    return (n >= WBDIV_MIN && n <= WBDIV_MAX) ? wbdiv_b[n - WBDIV_MIN] : ((n << 8) / WB_B);
}

__attribute__((optimize("O2"))) void filmsim_raw_apply(char *raw)
{
    const filmsim_t *s = current();
    if (!raw || !filmsim_photo_mode()) return;
    const short (*m)[3];
    if (s->mix == 1)      m = mix_lomo;
    else if (s->mix == 2) m = mix_metro;
    else if (s->mycolors == MC_BW && conf.film_bwfilter > 0 && conf.film_bwfilter < 4)
                          m = mix_bw[conf.film_bwfilter];
    else return;
    int rows = (camera_sensor.raw_rows - FS_ROW0) & ~1;
    int groups = camera_sensor.raw_rowpix / 8;
    int rowlen = camera_sensor.raw_rowlen;
    int y, g, k;
    wbdiv_init();
    for (y = 0; y < rows; y += 2) {
        unsigned char *a0 = (unsigned char *)raw + (y + FS_ROW0) * rowlen;   // R G R G
        unsigned char *a1 = a0 + rowlen;                         // G B G B
        for (g = 0; g < groups; g++, a0 += FS_GRP, a1 += FS_GRP) {
            int p[8], q[8];
            dec8(a0, p); dec8(a1, q);
            for (k = 0; k < 8; k += 2) {
                // Canon markiert defekte Sensorpixel in den Rohdaten mit 0 und
                // ersetzt sie erst bei der JPEG-Erzeugung. Wuerden wir den
                // Block mischen, waere die Markierung weg -> schwarzer Punkt.
                // Solche Bloecke bleiben deshalb unveraendert.
                if (!p[k] || !p[k+1] || !q[k] || !q[k+1]) continue;
                // auf Weissabgleich normieren (Schwarzwert abziehen)
                int r  = ((p[k]   - BLK) * WB_R) >> 8;
                int g1 =   p[k+1] - BLK;
                int g2 =   q[k]   - BLK;
                int b  = ((q[k+1] - BLK) * WB_B) >> 8;
                int gg = (g1 + g2) >> 1;
                int nr = (m[0][0] * r + m[0][1] * gg + m[0][2] * b) >> 8;
                int ng = (m[1][0] * r + m[1][1] * gg + m[1][2] * b) >> 8;
                int nb = (m[2][0] * r + m[2][1] * gg + m[2][2] * b) >> 8;
                int dg = ng - gg;                                // Gruen-Struktur erhalten
                p[k]   = clip10(div_wb_r(nr) + BLK);
                p[k+1] = clip10(g1 + dg + BLK);
                q[k]   = clip10(g2 + dg + BLK);
                q[k+1] = clip10(div_wb_b(nb) + BLK);
                // keine neuen "Defekt"-Markierungen erzeugen
                if (!p[k]) p[k] = 1;
                if (!p[k+1]) p[k+1] = 1;
                if (!q[k]) q[k] = 1;
                if (!q[k+1]) q[k+1] = 1;
            }
            enc8(a0, p); enc8(a1, q);
        }
    }
}
#else
void filmsim_raw_apply(__attribute__ ((unused))char *raw) {}
#endif

//-------------------------------------------------------------------
// Startbildschirm (A/CHDK/FILMS/SPLASH.FLM)
//   Wird einmal in den Speicher geladen und von dort gezeichnet - schnell
//   genug, dass Neuzeichnen (wenn Canon beim Start den Schirm loescht)
//   kaum flackert. Es gibt KEIN CHDK-Logo mehr als Ersatz.
//-------------------------------------------------------------------
static unsigned char *splash_buf = 0;
static int splash_len = 0;

static void splash_load(void)
{
    const char *fn = "A/CHDK/FILMS/SPLASH.FLM";
    struct stat st;
#ifndef FS_FLM
    return;
#endif
    if (splash_buf) return;
    if (stat(fn, &st) != 0 || st.st_size < 8 || st.st_size > 120000) return;
    unsigned char *b = malloc(st.st_size);
    if (!b) return;
    int fd = open(fn, O_RDONLY, 0777);
    if (fd < 0) { free(b); return; }
    int len = read(fd, b, st.st_size);
    close(fd);
    if (len < 8 || memcmp(b, "FLM1", 4) != 0) { free(b); return; }
    splash_buf = b; splash_len = len;
}

void filmsim_splash_free(void)
{
    if (splash_buf) free(splash_buf);
    splash_buf = 0; splash_len = 0;
}

void filmsim_splash_init(void)
{
    splash_load();
}

// Rueckgabe immer 1: CHDK-Logo nie zeichnen
int filmsim_draw_splash(void)
{
    splash_load();                       // falls die Karte beim Start noch nicht bereit war
    if (!splash_buf) return 1;
    unsigned char *b = splash_buf;
    int len = splash_len;
    int w = b[4] | (b[5] << 8);
    int h = b[6] | (b[7] << 8);
    if (w <= 0 || h <= 0 || w > camera_screen.width || h > camera_screen.height) return 1;
    int ox = (camera_screen.width - w) / 2;
    int oy = (camera_screen.height - h) / 2;
    int x = 0, y = 0, p = 8;
    while (p + 1 < len && y < h) {
        int n = b[p], c = b[p + 1];
        p += 2;
        while (n > 0 && y < h) {
            int run = w - x;
            if (run > n) run = n;
            if (c) draw_hline(ox + x, oy + y, run, c);
            x += run; n -= run;
            if (x >= w) { x = 0; y++; }
        }
    }
    return 1;
}

//-------------------------------------------------------------------
// Filmlog: haelt zu jedem Foto fest, welcher Film aktiv war
//   A/CHDK/FILMLOG.CSV   z.B.  "100CANON/IMG_0123.JPG;Postcard 100"
//   (VxWorks kann nicht anhaengen -> oeffnen, ans Ende springen, schreiben)
//-------------------------------------------------------------------
#include "file_counter.h"

void filmsim_log_shot(void)
{
    char dir[32], line[80];
    const char *fn = "A/CHDK/FILMLOG.CSV";
    struct stat st;
    int is_new = (stat(fn, &st) != 0);
    get_target_dir_name(dir);                     // "A/DCIM/100CANON"
    const char *d = strrchr(dir, '/');
    d = d ? d + 1 : dir;
    sprintf(line, "%s/IMG_%04d.JPG;%s\r\n", d, (int)get_target_file_num(), filmsim_name());
    int fd = open(fn, O_RDWR | O_CREAT, 0777);
    if (fd < 0) return;
    if (is_new) {
        const char *hdr = "Datei;Film\r\n";
        write(fd, hdr, strlen(hdr));
    } else {
        lseek(fd, 0, 2);                          // SEEK_END
    }
    write(fd, line, strlen(line));
    close(fd);
}

//-------------------------------------------------------------------
// Doppelbelichtung
//   Bild 1: Rohdaten (vor Kurven/Mischung) nach A/CHDK/DBL1.RAW schreiben,
//           Bild 1 wird ausserdem normal als JPEG gespeichert.
//   Bild 2: DBL1.RAW stueckweise einlesen und mit den neuen Rohdaten
//           verrechnen, danach laufen Filmkurve/-mischung wie gewohnt und
//           Canon erzeugt das JPEG aus dem gemischten Bild.
//   Mischung wie zwei Belichtungen auf einem Negativ: Summe * 5/8
//   (zwei normal belichtete Bilder ergeben so keine voellig ueberstrahlte
//   Summe, Lichter des einen Bildes zeichnen sich im Dunkel des anderen ab).
//-------------------------------------------------------------------
#include "cachebit.h"
#define DBL_FILE  "A/CHDK/DBL1.RAW"
#if CAM_SENSOR_BITS_PER_PIXEL == 12
#define DBL_CHUNK 60000                     // Vielfaches von 12 Byte (8 Pixel)
#else
#define DBL_CHUNK 64000                     // Vielfaches von 10 Byte (8 Pixel)
#endif

#ifdef FS_RAW
__attribute__((optimize("O2"))) static void dbl_merge(char *raw, int fd)
{
    unsigned char *buf = malloc(DBL_CHUNK);
    if (!buf) return;
    unsigned char *dst = (unsigned char *)raw;
    int total = camera_sensor.raw_size, done = 0;
    while (done < total) {
        int want = total - done;
        if (want > DBL_CHUNK) want = DBL_CHUNK;
        int got = read(fd, buf, want);
        if (got <= 0) break;
        got -= got % FS_GRP;
        int i, k;
        for (i = 0; i < got; i += FS_GRP) {
            int a[8], b[8];
            dec8(dst + done + i, a);
            dec8(buf + i, b);
            for (k = 0; k < 8; k++) {
                if (!a[k]) continue;                    // Defekt-Markierung erhalten
                if (!b[k]) b[k] = a[k];                 // Defekt im 1. Bild: nur 2. Bild nutzen
                int v = BLK + (((a[k] - BLK) + (b[k] - BLK)) * 5 >> 3);
                a[k] = clip10(v) ? clip10(v) : 1;      // nie versehentlich 0 erzeugen
            }
            enc8(dst + done + i, a);
        }
        done += got;
    }
    free(buf);
}
#endif

void filmsim_double_hook(char *raw)
{
#ifdef FS_RAW
    if (!conf.film_double || !raw || !filmsim_photo_mode()) return;
    if (conf.film_double == 1) {
        // erstes Bild sichern
        int fd = open(DBL_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd < 0) return;
        int w = write(fd, ADR_TO_UNCACHED(raw), camera_sensor.raw_size);
        close(fd);
        if (w == (int)camera_sensor.raw_size) {
            conf.film_double = 2;
            conf_save();
        }
    } else {
        // zweites Bild: mit dem gesicherten ersten verrechnen
        int fd = open(DBL_FILE, O_RDONLY, 0777);
        if (fd >= 0) {
            dbl_merge(raw, fd);
            close(fd);
        }
        remove(DBL_FILE);
        conf.film_double = 1;                    // bereit fuer das naechste Paar
        conf_save();
    }
#else
    (void)raw;
#endif
}

// Menue "Doppelbelichtung: Aus / An"
const char* filmsim_double_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *modes[] = { "Aus", "An" };
    if (change) {
        if (conf.film_double) { conf.film_double = 0; remove(DBL_FILE); }
        else conf.film_double = 1;
    }
    return modes[conf.film_double ? 1 : 0];
}

// Menue "Farbfilter (S/W)": wirkt nur bei Schwarzweissfilmen
const char* filmsim_bwfilter_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *modes[] = { "Aus", "Gelb", "Orange", "Rot" };
    if (change) conf.film_bwfilter = (conf.film_bwfilter + change + 4) % 4;
    return modes[conf.film_bwfilter % 4];
}

const char* filmsim_bwfilter_str(void)
{
    static const char *osd[] = { 0, "Gelbfilter", "Orangefilter", "Rotfilter" };
    if (current()->mycolors != MC_BW) return 0;
    return osd[conf.film_bwfilter % 4];
}

//===================================================================
// Kreativeffekte (Optionen -> Effekte)
//   Black Mist, Halation, Vignette, Light Leak, Datumsstempel
//   Laufen im RAW-Hook nach Filmkurve und Farbmischung, vor dem JPEG.
//   Leuchteffekte nutzen ein stark verkleinertes, weichgezeichnetes
//   Abbild der hellen Bildteile (wenig Speicher, ein Lese- und ein
//   Schreibdurchgang ueber die Rohdaten).
//===================================================================
#include "raw.h"
#include "time.h"

#ifdef FS_RAW
// Zellgroessen in Pixeln, mit der Sensorbreite wachsend: gleiche Wirkung
// im Bild und hoechstens 256 Zellen je Richtung
#if CAM_RAW_ROWPIX > 4096
#define MIST_BS 48                   // Black Mist
#define HAL_BS  24                   // Halation
#else
#define MIST_BS 32
#define HAL_BS  16
#endif

// Weichzeichnen eines kleinen Gitters (Kastenfilter, mehrfach = annaehernd Gauss)
static int blur_tmp[256];
static int glow_rm[3][256], glow_rh[256];

static void grid_blur(unsigned short *g, int w, int h, int r, int passes)
{
    int *tmp = blur_tmp;
    int x, y, p, n = 2 * r + 1;
    if (w > 256 || h > 256) return;
    for (p = 0; p < passes; p++) {
        for (y = 0; y < h; y++) {                     // horizontal
            unsigned short *row = g + y * w;
            for (x = 0; x < w; x++) {
                int s = 0, k;
                for (k = -r; k <= r; k++) {
                    int xx = x + k; if (xx < 0) xx = 0; if (xx >= w) xx = w - 1;
                    s += row[xx];
                }
                tmp[x] = s / n;
            }
            for (x = 0; x < w; x++) row[x] = tmp[x];
        }
        for (x = 0; x < w; x++) {                     // vertikal
            for (y = 0; y < h; y++) {
                int s = 0, k;
                for (k = -r; k <= r; k++) {
                    int yy = y + k; if (yy < 0) yy = 0; if (yy >= h) yy = h - 1;
                    s += g[yy * w + x];
                }
                tmp[y] = s / n;
            }
            for (y = 0; y < h; y++) g[y * w + x] = tmp[y];
        }
    }
}

// Interpolationstabelle: fuer jede Position Zellindex und Anteil (0..255)
static void make_interp(short *idx, unsigned char *frac, int n, int bs, int cells)
{
    int i;
    for (i = 0; i < n; i++) {
        int f = ((i * 256 + 128) / bs) - 128;         // Zellmitte-basiert, *256
        int c = f >> 8;
        int fr = f & 255;
        if (f < 0) { c = 0; fr = 0; }
        if (c >= cells - 1) { c = cells - 1; fr = 0; }
        idx[i] = c; frac[i] = fr;
    }
}

__attribute__((optimize("O2"))) static void effects_glow_and_tone(char *raw)
{
    int do_mist = conf.film_mist > 0, do_hal = conf.film_halation > 0;
    int do_vig = conf.film_vignette > 0, do_leak = conf.film_leak > 0;
    if (!do_mist && !do_hal && !do_vig && !do_leak) return;

    int rows = camera_sensor.raw_rows & ~1;
    int rowpix = camera_sensor.raw_rowpix;
    int groups = rowpix / 8;
    int rowlen = camera_sensor.raw_rowlen;
    int mw = rowpix / MIST_BS, mh = rows / MIST_BS;
    int hw = rowpix / HAL_BS,  hh = rows / HAL_BS;
    if (mw > 256 || hw > 256 || mh > 256 || hh > 256) return;

    unsigned short *gm = 0, *gh = 0;                  // Gitter: Mist (3 Kanaele), Halation
    int *acc = 0;
    short *ix_m = 0, *ix_h = 0;
    unsigned char *fx_m = 0, *fx_h = 0, *leak_x = 0;
    unsigned short *vig_x = 0;
    int x, y, k;

    if (do_mist) gm = malloc(3 * mw * mh * sizeof(unsigned short));
    if (do_hal)  gh = malloc(hw * hh * sizeof(unsigned short));
    acc = malloc((3 * 256 + 256) * sizeof(int));
    ix_m = malloc(rowpix * sizeof(short)); fx_m = malloc(rowpix);
    ix_h = malloc(rowpix * sizeof(short)); fx_h = malloc(rowpix);
    leak_x = malloc(rowpix);
    vig_x = malloc(rowpix * sizeof(unsigned short));
    if ((do_mist && !gm) || (do_hal && !gh) || !acc || !ix_m || !fx_m || !ix_h || !fx_h || !leak_x || !vig_x)
        goto out;

    // Schwellen (Rohwerte vor Weissabgleich, daher R/B niedriger)
    const int thr_m[4] = { BLK + 180, BLK + 340, BLK + 340, BLK + 235 };
    const int thr_h = BLK + 680;

    //---------------- 1. Durchgang: Lichter sammeln ----------------
    if (do_mist || do_hal) {
        int *am = acc, *ah = acc + 3 * 256;
        memset(acc, 0, (3 * 256 + 256) * sizeof(int));
        for (y = 0; y < rows; y++) {
            unsigned char *a = (unsigned char *)raw + y * rowlen;
            int odd = (y ^ FS_ROW0) & 1;
            for (k = 0; k < groups; k++, a += FS_GRP) {
                int v[8], j;
                dec8(a, v);
                for (j = 0; j < 8; j++) {
                    int xx = k * 8 + j;
                    int c = (odd << 1) | (xx & 1);   // 0 R, 1/2 G, 3 B
                    int val = v[j];
                    if (do_mist) {
                        int e = val - thr_m[c];
                        int cx = xx / MIST_BS;
                        if (e > 0 && cx < mw) am[(c == 3 ? 2 : (c ? 1 : 0)) * 256 + cx] += e;
                    }
                    if (do_hal && (c == 1 || c == 2)) {
                        int e = val - thr_h;
                        int cx = xx / HAL_BS;
                        if (e > 0 && cx < hw) ah[cx] += e;
                    }
                }
            }
            if (do_mist && ((y + 1) % MIST_BS) == 0 && (y / MIST_BS) < mh) {
                int cy = y / MIST_BS, ch;
                for (ch = 0; ch < 3; ch++)
                    for (x = 0; x < mw; x++) {
                        int cnt = (ch == 1) ? (MIST_BS * MIST_BS / 2) : (MIST_BS * MIST_BS / 4);
                        int val = am[ch * 256 + x] / cnt;
                        gm[(ch * mh + cy) * mw + x] = val > 65535 ? 65535 : val;
                        am[ch * 256 + x] = 0;
                    }
            }
            if (do_hal && ((y + 1) % HAL_BS) == 0 && (y / HAL_BS) < hh) {
                int cy = y / HAL_BS;
                for (x = 0; x < hw; x++) {
                    int val = ah[x] / (HAL_BS * HAL_BS / 2);
                    gh[cy * hw + x] = val > 65535 ? 65535 : val;
                    ah[x] = 0;
                }
            }
        }
        if (do_mist) { int ch; for (ch = 0; ch < 3; ch++) grid_blur(gm + ch * mw * mh, mw, mh, 2, 3); }
        if (do_hal) grid_blur(gh, hw, hh, 1, 3);
    }

    //---------------- Tabellen fuer den 2. Durchgang ----------------
    make_interp(ix_m, fx_m, rowpix, MIST_BS, mw);
    make_interp(ix_h, fx_h, rowpix, HAL_BS, hw);

    // Bildmitte / Bildbereich (sichtbarer JPEG-Ausschnitt)
    int jx = camera_sensor.jpeg.x, jy = camera_sensor.jpeg.y;
    int jw = camera_sensor.jpeg.width, jh = camera_sensor.jpeg.height;
    if (jw <= 0 || jh <= 0) { jx = 0; jy = 0; jw = rowpix; jh = rows; }
    int cxm = jx + jw / 2, cym = jy + jh / 2;
    int norm = ((jw / 2) * (jw / 2) + (jh / 2) * (jh / 2)) / 4096;   // Ecke = 4096
    if (norm < 1) norm = 1;
    for (x = 0; x < rowpix; x++) {
        int dx = x - cxm;
        int t = (dx * dx) / norm; if (t > 8191) t = 8191;
        vig_x[x] = t;
    }
    const int vig_k = conf.film_vignette >= 2 ? 2300 : 1300;            // Abdunklung in der Ecke (/4096)

    // Light Leak: warmer Streifen vom linken oder rechten Rand, Lage zufaellig
    unsigned int rnd = (unsigned int)get_tick_count() * 2654435761u;
    int leak_right = rnd & 1;
    int leak_yc = jy + jh / 5 + (int)((rnd >> 8) % (unsigned)(jh * 3 / 5));
    int leak_hh = jh / 3;
    int leak_w = jw / 3;
    for (x = 0; x < rowpix; x++) {
        int d = leak_right ? (jx + jw - x) : (x - jx);
        int t = (d <= 0) ? 255 : (d >= leak_w ? 0 : 255 - d * 255 / leak_w);
        leak_x[x] = (t * t) >> 8;                                     // weicher Auslauf
    }

    const int mist_k = conf.film_mist >= 2 ? 330 : 170;               // /256
    const int hal_k[4] = { 420, 70, 70, 0 };                          // R stark, etwas G, kein B
    const int leak_k[4] = { 700, 300, 300, 30 };                      // Rohwert-Einheiten bei voller Staerke

    //---------------- 2. Durchgang: anwenden ----------------
    unsigned int dith = rnd | 1;
    int (*rm)[256] = glow_rm;
    int *rh = glow_rh;
    for (y = 0; y < rows; y++) {
        int odd = (y ^ FS_ROW0) & 1;
        // vertikal interpolierte Gitterzeilen fuer diese Rohdatenzeile
        if (do_mist) {
            int f = ((y * 256 + 128) / MIST_BS) - 128, c0 = f >> 8, fr = f & 255, ch;
            if (f < 0) { c0 = 0; fr = 0; }
            if (c0 >= mh - 1) { c0 = mh - 1; fr = 0; }
            int c1 = (c0 + 1 < mh) ? c0 + 1 : c0;
            for (ch = 0; ch < 3; ch++)
                for (x = 0; x < mw; x++) {
                    unsigned short *g0 = gm + (ch * mh) * mw;
                    rm[ch][x] = (g0[c0 * mw + x] * (256 - fr) + g0[c1 * mw + x] * fr) >> 8;
                }
        }
        if (do_hal) {
            int f = ((y * 256 + 128) / HAL_BS) - 128, c0 = f >> 8, fr = f & 255;
            if (f < 0) { c0 = 0; fr = 0; }
            if (c0 >= hh - 1) { c0 = hh - 1; fr = 0; }
            int c1 = (c0 + 1 < hh) ? c0 + 1 : c0;
            for (x = 0; x < hw; x++)
                rh[x] = (gh[c0 * hw + x] * (256 - fr) + gh[c1 * hw + x] * fr) >> 8;
        }
        int dy = y - cym;
        int vy = (dy * dy) / norm;
        int ly = 0;
        if (do_leak) {
            int d = y - leak_yc; if (d < 0) d = -d;
            ly = (d >= leak_hh) ? 0 : 255 - d * 255 / leak_hh;
            ly = (ly * ly) >> 8;
        }

        unsigned char *a = (unsigned char *)raw + y * rowlen;
        for (k = 0; k < groups; k++, a += FS_GRP) {
            int v[8], j;
            dec8(a, v);
            for (j = 0; j < 8; j++) {
                if (!v[j]) continue;                     // Defekt-Markierung erhalten
                int xx = k * 8 + j;
                int c = (odd << 1) | (xx & 1);
                int add = 0;
                if (do_mist) {
                    int ch = (c == 3) ? 2 : (c ? 1 : 0);
                    int i0 = ix_m[xx], fr = fx_m[xx];
                    int i1 = (i0 + 1 < mw) ? i0 + 1 : i0;
                    int g = (rm[ch][i0] * (256 - fr) + rm[ch][i1] * fr) >> 8;
                    add += (g * mist_k) >> 8;
                }
                if (do_hal) {
                    int i0 = ix_h[xx], fr = fx_h[xx];
                    int i1 = (i0 + 1 < hw) ? i0 + 1 : i0;
                    int g = (rh[i0] * (256 - fr) + rh[i1] * fr) >> 8;
                    add += (g * hal_k[c]) >> 8;
                }
                if (do_leak && ly)
                    add += (leak_k[c] * ((leak_x[xx] * ly) >> 8)) >> 8;
                int val = v[j] + add;
                if (do_vig) {
                    int r2 = vig_x[xx] + vy; if (r2 > 8191) r2 = 8191;
                    int f = 4096 - ((vig_k * r2) >> 12);
                    if (f < 0) f = 0;
                    dith = dith * 1103515245u + 12345u;          // Zufalls-Dither gegen Stufen
                    val = BLK + (((val - BLK) * f + (int)((dith >> 16) & 4095)) >> 12);
                }
                val = clip10(val);
                v[j] = val ? val : 1;
            }
            enc8(a, v);
        }
    }

out:
    if (gm) free(gm);
    if (gh) free(gh);
    if (acc) free(acc);
    if (ix_m) free(ix_m);
    if (fx_m) free(fx_m);
    if (ix_h) free(ix_h);
    if (fx_h) free(fx_h);
    if (leak_x) free(leak_x);
    if (vig_x) free(vig_x);
}

//---------------- Datumsstempel ----------------
// Orange 7-Segment-Ziffern, leicht kursiv, unten rechts: '26 10 25
static void stamp_rect(int x0, int y0, int w, int h, int slant_base, int slant)
{
    // Orange in Rohwerten (vor Weissabgleich): R hoch, G mittel, B niedrig
    static const unsigned short col[4] = { 620, 300, 300, 45 };
    int x, y;
    for (y = y0; y < y0 + h; y++) {
        int off = ((slant_base - y) * slant) >> 8;    // kursiv: oben weiter rechts
        for (x = x0 + off; x < x0 + off + w; x++) {
            if (x < 0 || y < 0 || x >= (int)camera_sensor.raw_rowpix || y >= (int)camera_sensor.raw_rows) continue;
            int c = (((y ^ FS_ROW0) & 1) << 1) | (x & 1);
            if (get_raw_pixel(x, y) == 0) continue;     // Defekt-Markierung erhalten
            set_raw_pixel(x, y, col[c] << (CAM_SENSOR_BITS_PER_PIXEL - 10));
        }
    }
}

static void stamp_char(int x, int y, int H, int W, int T, char ch, int slant)
{
    static const unsigned char seg[10] = { 0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F };
    int base = y + H;
    if (ch == '\'') { stamp_rect(x + W / 3, y, T, H / 3, base, slant); return; }
    if (ch < '0' || ch > '9') return;
    int m = seg[ch - '0'], h2 = (H - T) / 2;
    if (m & 0x01) stamp_rect(x + T, y,               W - 2 * T, T, base, slant);   // a oben
    if (m & 0x02) stamp_rect(x + W - T, y + T,       T, h2 - T / 2, base, slant);  // b rechts oben
    if (m & 0x04) stamp_rect(x + W - T, y + h2 + T,  T, h2 - T / 2, base, slant);  // c rechts unten
    if (m & 0x08) stamp_rect(x + T, y + H - T,       W - 2 * T, T, base, slant);   // d unten
    if (m & 0x10) stamp_rect(x, y + h2 + T,          T, h2 - T / 2, base, slant);  // e links unten
    if (m & 0x20) stamp_rect(x, y + T,               T, h2 - T / 2, base, slant);  // f links oben
    if (m & 0x40) stamp_rect(x + T, y + h2,          W - 2 * T, T, base, slant);   // g Mitte
}

static void effects_datestamp(void)
{
    if (!conf.film_datestamp) return;
    struct tm *t = get_localtime();
    if (!t) return;
    char s[32];
    sprintf(s, "'%02d %02d %02d", t->tm_year % 100, t->tm_mon + 1, t->tm_mday);
    int jx = camera_sensor.jpeg.x, jy = camera_sensor.jpeg.y;
    int jw = camera_sensor.jpeg.width, jh = camera_sensor.jpeg.height;
    if (jw <= 0 || jh <= 0) { jx = 0; jy = 0; jw = camera_sensor.raw_rowpix; jh = camera_sensor.raw_rows; }
    int H = jh / 24, W = H * 11 / 20, T = H / 8, gap = W * 2 / 5;
    if (T < 2) T = 2;
    int n = strlen(s), i, total = 0;
    for (i = 0; i < n; i++) total += (s[i] == '\'' ? W / 2 : (s[i] == ' ' ? W / 2 : W)) + gap;
    int x = jx + jw - jw / 14 - total;
    int y = jy + jh - jh / 12 - H;
    for (i = 0; i < n; i++) {
        stamp_char(x, y, H, W, T, s[i], 40);
        x += (s[i] == '\'' ? W / 2 : (s[i] == ' ' ? W / 2 : W)) + gap;
    }
}
#endif

//-------------------------------------------------------------------
// Farbrauschen-Filter (Chroma-Rauschunterdrueckung)
//   Trennt je 2x2-Bayer-Block Helligkeit (Gruen) und Farbe (Rot-Gruen,
//   Blau-Gruen). Nur die Farbanteile werden ueber die Nachbarbloecke
//   gemittelt, Gruen und damit die Bilddetails bleiben unangetastet.
//   Bunte Flecken verschwinden, das feine Helligkeitsrauschen (wie Korn)
//   bleibt erhalten.
//-------------------------------------------------------------------
#ifdef FS_RAW
#define CNR_RMAX 4
static int cnr_recip[512];                            // 65536 / e
#define CNR_CLIP 1000

// Farbanteile einer Blockzeile berechnen (aus den Originaldaten)
__attribute__((optimize("O2")))
static void cnr_row(const char *raw, int j, int nb, short *cr, short *cb, short *cl)
{
    int rowlen = camera_sensor.raw_rowlen, groups = nb / 4, g, k, i = 0;
    const unsigned char *a0 = (const unsigned char *)raw + (2 * j + FS_ROW0) * rowlen;
    const unsigned char *a1 = a0 + rowlen;
    int lr = 0, lb = 0, ll = 0;
    for (g = 0; g < groups; g++, a0 += FS_GRP, a1 += FS_GRP) {
        int p[8], q[8];
        dec8(a0, p); dec8(a1, q);
        for (k = 0; k < 8; k += 2, i++) {
            int R = p[k], G1 = p[k+1], G2 = q[k], B = q[k+1];
            if (!R || !G1 || !G2 || !B || R >= CNR_CLIP || G1 >= CNR_CLIP || G2 >= CNR_CLIP || B >= CNR_CLIP) {
                cr[i] = lr; cb[i] = lb; cl[i] = ll;   // Defekt/Clipping: Nachbarwert
                continue;
            }
            int gg = ((G1 + G2) >> 1) - BLK;
            ll = gg;
            lr = (((R - BLK) * WB_R) >> 8) - gg;
            lb = (((B - BLK) * WB_B) >> 8) - gg;
            cr[i] = lr; cb[i] = lb; cl[i] = ll;
        }
    }
}

__attribute__((optimize("O2")))
static void filmsim_cnr_apply(char *raw)
{
    int R = conf.film_cnr == 1 ? 2 : (conf.film_cnr == 2 ? CNR_RMAX : 0);
    if (!R || !raw || !filmsim_photo_mode()) return;
    int RB = 2 * R + 1;
    int nb = (camera_sensor.raw_rowpix / 8) * 4;     // Bloecke pro Zeile
    int nbr = (camera_sensor.raw_rows - FS_ROW0) / 2; // Blockzeilen
    int rowlen = camera_sensor.raw_rowlen;
    short *ring_r = malloc(RB * nb * sizeof(short));
    short *ring_b = malloc(RB * nb * sizeof(short));
    short *ring_l = malloc(RB * nb * sizeof(short));
    int *vr = malloc(nb * sizeof(int));
    int *vb = malloc(nb * sizeof(int));
    int *vl = malloc(nb * sizeof(int));
    if (!ring_r || !ring_b || !ring_l || !vr || !vb || !vl) goto out;
    const int dmax = (R == 2) ? 170 : 250;           // ab dieser Farbabweichung: Kante, nicht Rauschen
    const int invd = 65536 / dmax;
    if (!cnr_recip[12]) { int t; for (t = 1; t < 512; t++) cnr_recip[t] = 65536 / t; }
    wbdiv_init();
    int inv = 65536 / (RB * RB);
    int computed = 0, j, i, d;

    for (j = 0; j < nbr; j++) {
        // benoetigte Zeilen bis j+R berechnen (Originaldaten, noch unveraendert)
        int need = j + R; if (need > nbr - 1) need = nbr - 1;
        while (computed <= need) {
            cnr_row(raw, computed, nb, ring_r + (computed % RB) * nb, ring_b + (computed % RB) * nb,
                    ring_l + (computed % RB) * nb);
            computed++;
        }
        // vertikale Summen
        for (i = 0; i < nb; i++) { vr[i] = 0; vb[i] = 0; vl[i] = 0; }
        for (d = -R; d <= R; d++) {
            int jj = j + d; if (jj < 0) jj = 0; if (jj > nbr - 1) jj = nbr - 1;
            short *sr = ring_r + (jj % RB) * nb, *sb = ring_b + (jj % RB) * nb, *sl = ring_l + (jj % RB) * nb;
            for (i = 0; i < nb; i++) { vr[i] += sr[i]; vb[i] += sb[i]; vl[i] += sl[i]; }
        }
        short *own_r = ring_r + (j % RB) * nb, *own_b = ring_b + (j % RB) * nb;
        // horizontal gleitend summieren und Block schreiben
        int sumr = 0, sumb = 0, suml = 0;
        for (d = -R; d <= R; d++) { int ii = d < 0 ? 0 : d; sumr += vr[ii]; sumb += vb[ii]; suml += vl[ii]; }
        unsigned char *a0 = (unsigned char *)raw + (2 * j + FS_ROW0) * rowlen;
        unsigned char *a1 = a0 + rowlen;
        int g, k; i = 0;
        for (g = 0; g < nb / 4; g++, a0 += FS_GRP, a1 += FS_GRP) {
            int p[8], q[8], changed = 0;
            dec8(a0, p); dec8(a1, q);
            for (k = 0; k < 8; k += 2, i++) {
                int Rv = p[k], G1 = p[k+1], G2 = q[k], Bv = q[k+1];
                if (Rv && G1 && G2 && Bv && Rv < CNR_CLIP && G1 < CNR_CLIP && G2 < CNR_CLIP && Bv < CNR_CLIP) {
                    int gg = ((G1 + G2) >> 1) - BLK;
                    int crs = (sumr * inv + 32768) >> 16;
                    int cbs = (sumb * inv + 32768) >> 16;
                    int mgg = (suml * inv + 32768) >> 16;
                    // Kantenschutz: weicht die Helligkeit oder Farbe der Umgebung
                    // stark ab, liegt eine Kante in der Naehe -> weniger glaetten
                    int e = mgg / 3; if (e < 40) e = 40; if (e > 511) e = 511;
                    int dl = gg - mgg; if (dl < 0) dl = -dl;
                    int w = 256 - ((dl * cnr_recip[e]) >> 8);  // 0..256 (ohne Division)
                    int dr_ = crs - own_r[i], db_ = cbs - own_b[i];
                    int dc = (dr_ < 0 ? -dr_ : dr_) + (db_ < 0 ? -db_ : db_);
                    int w2 = 256 - ((dc * invd) >> 8);
                    if (w2 < w) w = w2;
                    if (w < 0) w = 0;
                    crs = own_r[i] + ((dr_ * w) >> 8);
                    cbs = own_b[i] + ((db_ * w) >> 8);
                    int nr = clip10(div_wb_r(gg + crs) + BLK);
                    int nbv = clip10(div_wb_b(gg + cbs) + BLK);
                    p[k] = nr ? nr : 1;
                    q[k+1] = nbv ? nbv : 1;
                    changed = 1;
                }
                // Fenster eins weiter schieben
                int add = i + R + 1; if (add > nb - 1) add = nb - 1;
                int sub = i - R;     if (sub < 0) sub = 0;
                sumr += vr[add] - vr[sub];
                sumb += vb[add] - vb[sub];
                suml += vl[add] - vl[sub];
            }
            if (changed) { enc8(a0, p); enc8(a1, q); }
        }
    }
out:
    if (ring_r) free(ring_r);
    if (ring_b) free(ring_b);
    if (ring_l) free(ring_l);
    if (vr) free(vr);
    if (vb) free(vb);
    if (vl) free(vl);
}
#else
static void filmsim_cnr_apply(__attribute__ ((unused))char *raw) {}
#endif

void filmsim_cnr_hook(char *raw)
{
    filmsim_cnr_apply(raw);
}

const char* filmsim_cnr_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *m[] = { "Aus", "Normal", "Stark" };
    if (change) conf.film_cnr = (conf.film_cnr + change + 3) % 3;
    return m[conf.film_cnr % 3];
}

void filmsim_effects_apply(char *raw)
{
#ifdef FS_RAW
    if (!raw || !filmsim_photo_mode()) return;
    effects_glow_and_tone(raw);
    effects_datestamp();
#else
    (void)raw;
#endif
}

// Menue-Eintraege
const char* filmsim_mist_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *m[] = { "Aus", "1/8", "1/4" };
    if (change) conf.film_mist = (conf.film_mist + change + 3) % 3;
    return m[conf.film_mist % 3];
}
const char* filmsim_halation_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *m[] = { "Aus", "An" };
    if (change) conf.film_halation = !conf.film_halation;
    return m[conf.film_halation ? 1 : 0];
}
const char* filmsim_vignette_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *m[] = { "Aus", "Leicht", "Stark" };
    if (change) conf.film_vignette = (conf.film_vignette + change + 3) % 3;
    return m[conf.film_vignette % 3];
}
const char* filmsim_leak_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *m[] = { "Aus", "An" };
    if (change) conf.film_leak = !conf.film_leak;
    return m[conf.film_leak ? 1 : 0];
}
const char* filmsim_datestamp_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *m[] = { "Aus", "An" };
    if (change) conf.film_datestamp = !conf.film_datestamp;
    return m[conf.film_datestamp ? 1 : 0];
}
int filmsim_effects_active(void)
{
    return conf.film_mist || conf.film_halation || conf.film_vignette || conf.film_leak || conf.film_datestamp;
}

//-------------------------------------------------------------------
// Dynamik+: knapper belichten (Lichter schuetzen), Schatten per Kurve
// wieder aufhellen. Die Belichtungskorrektur passiert in shooting.c.
//-------------------------------------------------------------------
const char* filmsim_dr_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *m[] = { "Aus", "+1", "+2" };
    if (change) {
        conf.film_dr = (conf.film_dr + change + 3) % 3;
        set_curve(current());
    }
    return m[conf.film_dr % 3];
}

// Belichtungsverschiebung in 1/96 EV (0 = keine), von shooting.c genutzt
int filmsim_dr_tv96_shift(void)
{
    if (conf.film_dr < 1 || conf.film_dr > 2) return 0;
    if (!filmsim_photo_mode()) return 0;
    if (shooting_get_prop(PROPCASE_IS_FLASH_READY)) return 0;   // mit Blitz nicht
    return conf.film_dr * 96;
}
