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
#include "filmsim_raw.h"
#include "filmsim_dev.h"
#include "raw.h"
#include "gui_mbox.h"
#include "lang.h"
#include "gui_lang.h"

//-------------------------------------------------------------------
// Kameraeinstellungen fuer EXCCD
//   Jede Kamera setzt sie in platform/<kamera>/platform_camera.h, Block
//   "EXCCD". Der Kern fragt nur diese Schalter ab, nie den Kameranamen.
//   Was CHDK schon weiss (Bit-Tiefe, Bayer-Anordnung, Sensorgroesse),
//   kommt direkt aus camera.h.
//
//   FILMSIM_LABELS       1 = Laschen (.FLM) und Splash passen zur Palette
//                        der Kamera (gebaut aus tools/filmsim/cams/<kamera>/).
//                        Ohne: Textkarten, kein Splash.
//   FILMSIM_COLOR_BG     Palettenindizes fuer Textkarte und Leiste
//   FILMSIM_COLOR_GREY   (Standard: CHDK-Farben)
//   FILMSIM_COLOR_RULE
//   FILMSIM_WB_R/_B      Tageslicht-Gains des Sensors * 256
//                        (Standard: Werte der IXUS 60, bis kalibriert)
//-------------------------------------------------------------------
#ifndef FILMSIM_COLOR_BG
#define FILMSIM_COLOR_BG    COLOR_BLACK
#define FILMSIM_COLOR_GREY  COLOR_GREY
#define FILMSIM_COLOR_RULE  COLOR_GREY_DK
#endif
#ifndef FILMSIM_WB_R
#define FILMSIM_WB_R        486
#define FILMSIM_WB_B        371
#endif

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

// dec8/enc8 lesen halbwortweise, das braucht gerade Zeilenlaengen
#if defined(FS_RAW) && ((CAM_RAW_ROWPIX * CAM_SENSOR_BITS_PER_PIXEL / 8) & 1)
#error "Filmsim: ungerade Zeilenlaenge im Rohbild (filmsim_raw.h)"
#endif

#ifndef PROPCASE_MY_COLORS
    #error "Filmsimulation braucht PROPCASE_MY_COLORS (propset)"
#endif

// Eingebaute Tabelle, falls A/CHDK/FILMS/FILMS.TXT fehlt oder unbrauchbar ist
static const filmsim_t sims_builtin[] = {
    // name              mode        con shp sat  R   G   B  skin  curve           label      mix
    { "Off",             MC_OFF,      0,  0,  0,  0,  0,  0,  0,  0,              "AUS",      0 },
    { "Daylight 100",    MC_OFF,      0,  0,  0,  0,  0,  0,  0,  0,              "DAYLITE",  0 },
    { "Postcard 100",    MC_CUSTOM,   0,  1,  2,  1,  1,  1,  0,  "POSTCRD.CV",   "POSTCRD",  0 },
    { "Blush 100",       MC_CUSTOM,  -1,  0,  1,  0,  0,  0,  1,  "BLUSH.CV",     "BLUSH",    0 },
    { "Journal 400",     MC_CUSTOM,   0,  0, -1,  0, -1,  0,  0,  "JOURNAL.CV",   "JOURNAL",  0 },
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
    { "Ember 200",       MC_CUSTOM,   0,  0,  0,  0,  0,  0,  0,  "EMBER.CV",     "EMBER",    0 },
    { "Grit 400",        MC_BW,       0,  0,  0,  0,  0,  0,  0,  "GRIT.CV",      "GRIT",     0 },
    { "Summer 200",      MC_CUSTOM,   0,  0,  1,  0,  0,  0,  1,  "SUMMER.CV",    "SUMMER",   0 },
    { "Chrome 64",       MC_CUSTOM,   0,  1,  1,  1,  0,  0,  0,  "CHROME.CV",    "CHROME",   0 },
    { "Bypass 400",      MC_CUSTOM,   0,  1, -2,  0,  0,  0,  0,  "BYPASS.CV",    "BYPASS",   0 },
    { "Noir 3200",       MC_BW,       0,  0,  0,  0,  0,  0,  0,  "NOIR.CV",      "NOIR",     0 },
    { "Turquoise 400",   MC_CUSTOM,   0,  0,  1,  0,  0,  0,  0,  "TURQUOI.CV",   "TURQUOI",  3 },
    { "Infra 200",       MC_CUSTOM,   0,  0,  1,  0,  0,  0,  0,  "INFRA.CV",     "INFRA",    4 },
    { "Tintype 50",      MC_SEPIA,    0,  0,  0,  0,  0,  0,  0,  "TINTYPE.CV",   "TINTYPE",  0 },
    { "Cyano 25",        MC_CUSTOM,   0,  0,  0,  0,  0,  0,  0,  "CYANO.CV",     "CYANO",    5 },
    { "Instant 600",     MC_CUSTOM,  -1, -1, -1,  0,  0,  0,  0,  "INSTANT.CV",   "INSTANT",  0 },
    { "Pastel 400",      MC_CUSTOM,  -1, -1, -1,  0,  0,  0,  1,  "PASTEL.CV",    "PASTEL",   0 },
};
#define NUM_BUILTIN ((int)(sizeof(sims_builtin)/sizeof(sims_builtin[0])))

// Aktive Tabelle: eingebaut oder aus FILMS.TXT (Eintrag 0 ist immer "Aus")
static const filmsim_t *sims = sims_builtin;
static int num_sims = NUM_BUILTIN;
#define NUM_SIMS num_sims

// Anzeigename; "Aus" kommt aus der Sprachdatei (english.lng / german.lng)
static const char* sim_name(int i)
{
    return i == 0 ? lang_str(LANG_EXCCD_OFF) : sims[i].name;
}

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

static void films_read_file(void)
{
    struct stat st;
    char *buf = 0, *log = 0;
    int n = 0, len = -1;

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
        strcpy(log, "File is empty or larger than 16 KB\r\n");
    }

    if (n > 0) {
        films_file[0] = sims_builtin[0];    // "Aus"
        sims = films_file;
        num_sims = n + 1;
    } else {
        strcat(log, "No valid looks - using the built-in table\r\n");
    }

    if (log[0]) {
        char hdr[48];
        sprintf(hdr, "FILMS.TXT: %d looks loaded\r\n", n);
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

// Der Look wird ueber seinen Namen gespeichert: Mit einer anderen FILMS.TXT oder
// neuer Reihenfolge bleibt so derselbe Film gewaehlt, statt dass der Index auf einen
// anderen zeigt. Gibt es ihn nicht mehr: "Aus". Aeltere Einstellungen ohne Namen
// behalten den Index und bekommen hier den Namen.
static void film_name_store(void)
{
    int i = conf.film_sim;
    conf.film_name[0] = 0;
    if (i > 0 && i < num_sims) {
        strncpy(conf.film_name, sims[i].name, sizeof(conf.film_name) - 1);
        conf.film_name[sizeof(conf.film_name) - 1] = 0;
    }
}

static void films_load(void)
{
    static int done = 0;
    int i;
    if (done) return;
    done = 1;
    films_read_file();
    if (!conf.film_name[0]) {
        if (conf.film_sim < 0 || conf.film_sim >= num_sims) conf.film_sim = 0;
        film_name_store();
        return;
    }
    for (i = 1; i < num_sims; i++)
        if (strcmp(sims[i].name, conf.film_name) == 0) {
            conf.film_sim = i;
            return;
        }
    conf.film_sim = 0;
    conf.film_name[0] = 0;
}

static const filmsim_t* current(void)
{
    int i = conf.film_sim;
    if (i < 0 || i >= NUM_SIMS) i = 0;
    return &sims[i];
}

const char* filmsim_name(void)
{
    int i = conf.film_sim;
    return sim_name(i < 0 || i >= NUM_SIMS ? 0 : i);
}

static int curve_file_ok = 0;     // Diagnose: Kurvendatei gefunden?
static int curve_dr = 0;          // Dynamik+-Stufe der geladenen Kurve
static volatile int shot_dr = -1; // Dynamik+-Stufe, mit der wirklich belichtet wurde (-1 = unbekannt)
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
//   mit 3 x 4096 Eintraegen (R, G, B), linear zwischen den 10-Bit-Stuetzstellen
//   interpoliert (Schwarz 127 / Weiss 4095 liegen exakt auf 31 / 1023).
//   Das Anwenden ist dann nur noch ein Tabellenzugriff je Pixel.
//-------------------------------------------------------------------
#if defined(FS_RAW) && CAM_SENSOR_BITS_PER_PIXEL == 12
#define FS_OWN_CURVES 1
static unsigned short *curve_lut = 0;

static void curve_lut_free(void)
{
    if (curve_lut) free(curve_lut);
    curve_lut = 0;
}

static int curve_lut_load(const char *fn)
{
    unsigned short *cv = malloc(4 * CV_N * sizeof(unsigned short));
    int ok = 0;
    curve_lut_free();
    if (!cv) return 0;
    int fd = open(fn, O_RDONLY, 0777);
    if (fd >= 0) {
        ok = read(fd, cv, 4 * CV_N * sizeof(unsigned short)) == 4 * CV_N * (int)sizeof(unsigned short);
        close(fd);
    }
    if (ok) curve_lut = malloc(3 * 4096 * sizeof(unsigned short));
    if (curve_lut) curve_lut_build(cv, curve_lut);
    free(cv);
    return curve_lut != 0;
}

// Aus raw.c, an der Stelle von curves.flt
__attribute__((optimize("O2"))) void filmsim_curve_apply(char *raw)
{
    if (!curve_lut || !raw) return;
    int rows = camera_sensor.raw_rows, rowpix = camera_sensor.raw_rowpix;
    int rowlen = camera_sensor.raw_rowlen, y;
    // Wortweise nur bei ausgerichteten Zeilen aus ganzen 8-Pixel-Gruppen
    int words = !(((unsigned int)raw | (unsigned int)rowlen) & 3) && !(rowpix & 7);
    for (y = 0; y < rows; y++) {
        // R-Zeile: Tabelle R (gerade Spalten) / G, B-Zeile: G / B
        const unsigned short *ce = curve_lut + ((y ^ FS_ROW0) & 1) * 4096;
        const unsigned short *co = ce + 4096;
        unsigned char *a = (unsigned char *)raw + y * rowlen;
        if (words) curve_row12_w((unsigned int *)a, rowpix / 8, ce, co);
        else       curve_row12(a, rowpix / 4, ce, co);
    }
}
#endif

// Kurve laden bzw. abschalten. dr: Dynamik+-Stufe (0..2), deren Kurve gilt
static void set_curve_dr(const filmsim_t *s, int dr)
{
#if defined(OPT_CURVES) || defined(FS_OWN_CURVES)
    curve_dr = dr;
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
    (void)s; (void)dr;
#endif
}

static void set_curve(const filmsim_t *s)
{
    set_curve_dr(s, (conf.film_dr >= 1 && conf.film_dr <= 2) ? conf.film_dr : 0);
}

// Simulation vollstaendig anwenden (bei Menueauswahl / Start)
void filmsim_apply(void)
{
    const filmsim_t *s = current();
    if (filmsim_photo_mode())
        set_canon_props(s);
    set_curve(s);
}

//-------------------------------------------------------------------
// Filmrolle und Ueberraschungsfilm
//   Rolle: 12/24/36 Aufnahmen, die Statuszeile zaehlt mit. Solange die Rolle nicht
//   voll ist, laesst die Filmauswahl keinen anderen Film zu (wie im echten Leben;
//   raus kommt man ueber Optionen -> Filmrolle). Die Kamera nimmt danach weiter auf,
//   das naechste Foto beginnt eine neue Rolle.
//   Ueberraschungsfilm: Mit jeder vollen Rolle (ohne Rolle: nach jedem Foto) wird
//   zufaellig ein neuer Film eingelegt.
//-------------------------------------------------------------------
static const short roll_sizes[4] = { 0, 12, 24, 36 };
static volatile int roll_shot = 0;      // vom RAW-Hook: Foto gemacht, Rest erledigt filmsim_tick

// RAW entwickeln (Ablauf siehe filmsim_develop_select)
static volatile int dev_armed = 0;      // das naechste Foto ist der Hilfsschuss
static volatile int dev_active = 0;     // der Hilfsschuss laeuft gerade durch den RAW-Hook
static volatile int dev_restore = 0;    // fertig: eingelegten Film wieder herstellen
static int dev_prev = -1;               // eingelegter Film
static int dev_has_meta = 0;            // 0: reines RAW ohne Aufnahmedaten
static dng_meta_t dev_meta;
static char dev_name[16];               // Dateiname fuer Statuszeile und Logbuch

static int roll_total(void)
{
    return (conf.film_roll > 0 && conf.film_roll < 4) ? roll_sizes[conf.film_roll] : 0;
}

static int roll_locked(void)
{
    int t = roll_total();
    return t && conf.film_roll_used > 0 && conf.film_roll_used < t;
}

static void surprise_pick(void)
{
    if (NUM_SIMS < 3) return;
    unsigned int r = (unsigned int)get_tick_count() * 2654435761u;
    int v = 1 + (int)((r >> 16) % (unsigned)(NUM_SIMS - 1));
    if (v == conf.film_sim) v = (v < NUM_SIMS - 1) ? v + 1 : 1;
    filmsim_set(v);
}

// Aus filmsim_tick: Kamera im Fotomodus, keine Menues, kein Bild in Arbeit
static void roll_tick(void)
{
    if (!roll_shot || camera_info.state.is_shutter_half_press) return;
    roll_shot = 0;
    int t = roll_total();
    if (conf.film_surprise && (!t || conf.film_roll_used >= t))
        surprise_pick();
    if (t || conf.film_surprise) conf_save();   // Zaehler und Film ueberstehen das Ausschalten
}

// Aus filmsim_tick wie roll_tick: Canon ist mit dem JPEG fertig, erst jetzt den Film zuruecknehmen
static void dev_tick(void)
{
    if (!dev_restore || camera_info.state.is_shutter_half_press) return;
    dev_restore = 0;
    if (dev_prev >= 0 && dev_prev != conf.film_sim) filmsim_set(dev_prev);
    dev_prev = -1;
}

//-------------------------------------------------------------------
// RAW entwickeln: eine gespeicherte DNG mit einem anderen Look neu ausgeben
//   Canon erzeugt JPEGs nur nach einer Aufnahme. Deshalb: DNG und Look waehlen, einmal
//   ausloesen. raw.c ersetzt die Sensordaten dieses Hilfsschusses durch die DNG, danach laeuft
//   die normale Kette, und das Original bleibt erhalten. Der eingelegte Film und die Filmrolle
//   aendern sich dabei nicht.
//
//   Vom Hilfsschuss stammen sonst nur Canons Einstellungen. Deshalb wird korrigiert:
//     Weissabgleich:  Canon rechnet mit den Faktoren des Hilfsschusses. Die Rohdaten werden so
//                     umgerechnet, dass im JPEG die Faktoren der Aufnahme herauskommen.
//     Dynamik+:       die Aufhell-Kurve gehoert zur Aufnahme, nicht zur Menueeinstellung
//     Farbrauschen:   Auto richtet sich nach der ISO der Aufnahme
//     Datum:          der Datumsstempel zeigt den Aufnahmetag
//   Exif-Daten und Ausrichtung im JPEG kann EXCCD nicht aendern, die kommen vom Hilfsschuss.
//-------------------------------------------------------------------
int filmsim_develop_armed(void)
{
    return dev_armed;
}

// Passt die Datei zu dieser Kamera? Liest auch die Aufnahmedaten.
static int dev_read_file(const char *fn)
{
    struct stat st;
    int ok = 0;
    if (strlen(fn) >= 64 || stat(fn, &st) != 0 || st.st_size < (int)camera_sensor.raw_size) return 0;
    dev_has_meta = 0;
    if (st.st_size == (int)camera_sensor.raw_size) return 1;    // CHDK-RAW ohne Kopf
    unsigned char *head = malloc(DNG_HEAD_MAX);
    if (!head) return 0;
    int fd = open(fn, O_RDONLY, 0777);
    if (fd >= 0) {
        int n = read(fd, head, DNG_HEAD_MAX);
        close(fd);
        ok = dng_parse(head, n, camera_sensor.raw_rowpix, camera_sensor.raw_rows, camera_sensor.bits_per_pixel, &dev_meta);
        dev_has_meta = ok;
    }
    free(head);
    return ok;
}

// Rueckruf der Dateiauswahl: Datei merken, dann die Filmauswahl zum Waehlen des Looks
void filmsim_develop_select(const char *fn)
{
    if (!fn) return;                    // abgebrochen
    if (!dev_read_file(fn)) {
        gui_mbox_init(LANG_MENU_RAW_DEVELOP, LANG_EXCCD_DEV_BAD, MBOX_FUNC_RESTORE|MBOX_TEXT_CENTER|MBOX_BTN_OK, NULL);
        return;
    }
    const char *b = strrchr(fn, '/');
    strncpy(dev_name, b ? b + 1 : fn, sizeof(dev_name) - 1);
    dev_name[sizeof(dev_name) - 1] = 0;
    raw_prepare_develop(fn, 0);
    // Ruecknahme der letzten Entwicklung noch offen: Dann ist dev_prev weiter der eingelegte Film
    if (!dev_restore || dev_prev < 0) dev_prev = conf.film_sim;
    dev_restore = 0;
    dev_armed = 1;
    filmsim_open_menu();
}

void filmsim_develop_cancel(void)
{
    raw_prepare_develop(0, 0);
    dev_armed = 0;
    dev_restore = 1;                    // uebernimmt dev_tick
}

// Aus raw.c: Die Datei ist geladen, gleich laeuft die Kette
void filmsim_develop_begin(char *raw)
{
    dev_armed = 0;
    dev_active = 1;
    shot_dr = dev_has_meta ? dev_meta.dr : 0;
#ifdef FS_RAW
    if (dev_has_meta && raw) {
        int wb[3] = { 0, 0, 0 };        // G, R, B wie in modules/dng.c
        get_property_case(camera_info.props.wb_adj, &wb, sizeof(wb));
        int fr = wb_factor(dev_meta.wb_r, dev_meta.wb_g, wb[1], wb[0]);
        int fb = wb_factor(dev_meta.wb_b, dev_meta.wb_g, wb[2], wb[0]);
        if (fr != WB_ONE || fb != WB_ONE)
            wb_rescale((unsigned char *)raw, camera_sensor.raw_rows, camera_sensor.raw_rowpix, camera_sensor.raw_rowlen,
                       FS_ROW0, 31, fr, fb);
    }
#else
    (void)raw;
#endif
}

// Aus raw.c nach dem Speichern der DNG: Dynamik+-Stufe in den Kopf schreiben, damit die spaetere
// Entwicklung die passende Kurve nimmt. Rueckgabe: offene Datei fuer raw_closefile, sonst -1.
// Die Datei behaelt ihre Groesse (die Rohdaten muessen am Dateiende bleiben).
int filmsim_dng_mark(const char *fn)
{
    if (shot_dr < 1 || shot_dr > 2) return -1;
    unsigned char *head = malloc(DNG_HEAD_MAX);
    if (!head) return -1;
    int fd = open(fn, O_RDWR, 0777);
    if (fd >= 0) {
        int n = read(fd, head, DNG_HEAD_MAX);
        int e = n > 8 ? dng_entry(head, n, dng_u32(head + 4), DNG_TAG_DESCRIPTION) : -1;
        if (e >= 0) {
            unsigned char v[8] = { 4, 0, 0, 0, 'D', 'R', (unsigned char)('0' + shot_dr), 0 };
            lseek(fd, e + 4, 0);        // SEEK_SET: Anzahl und Text direkt im Eintrag
            write(fd, v, sizeof(v));
        }
    }
    free(head);
    return fd;
}

const char* filmsim_roll_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, (int)"12", (int)"24", (int)"36" };
    if (change) {
        conf.film_roll = (conf.film_roll + change + 4) % 4;
        conf.film_roll_used = 0;        // andere Rolle: frisch anfangen
    }
    return lang_str(m[conf.film_roll % 4]);
}

const char* filmsim_surprise_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, LANG_EXCCD_ON };
    if (change) conf.film_surprise = !conf.film_surprise;
    return lang_str(m[conf.film_surprise ? 1 : 0]);
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

    filmsim_zoom_tick();

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
    roll_tick();
    dev_tick();

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
    film_name_store();
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

// EXCCD-Farben wie in den Laschen (Kamerapalette, Aufnahmemodus, alle deckend)
#define EX_BG       FILMSIM_COLOR_BG
#define EX_GREY     FILMSIM_COLOR_GREY
#define EX_RULE     FILMSIM_COLOR_RULE

static int sel = 0;                 // gerade angezeigter Film
static int need_draw = 0;           // 1 = alles, 2 = nur Leiste
static int roll_hint = 0;           // Leiste zeigt: Rolle noch nicht voll
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
#ifndef FILMSIM_LABELS
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
    const char *n = sim_name(i);
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
    const char *n = sim_name(sel);
    int len = strlen(n);
    int x0 = (camera_screen.width - LABEL_W) / 2, x1 = x0 + LABEL_W - FONT_WIDTH;
    int x = (camera_screen.width - len * FONT_WIDTH) / 2;
    draw_rectangle(0, LABEL_TOP + LABEL_H, camera_screen.width - 1, camera_screen.height - 1,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
    draw_string(x0, BAR_Y, "<", MAKE_COLOR(COLOR_BLACK, EX_GREY));
    draw_string(x1, BAR_Y, ">", MAKE_COLOR(COLOR_BLACK, EX_GREY));
    if (roll_hint) {
        char hint[40];
        sprintf(hint, lang_str(LANG_EXCCD_ROLL_LOCKED), roll_total() - conf.film_roll_used);
        draw_string((camera_screen.width - (int)strlen(hint) * FONT_WIDTH) / 2, BAR_Y, hint,
                    MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        return;
    }
    // Position in der Liste, bei vielen Looks sonst schwer zu schaetzen
    if (sel > 0) {
        char pos[12];
        sprintf(pos, "%d/%d", sel, NUM_SIMS - 1);
        draw_string(x0 + 2 * FONT_WIDTH, BAR_Y, pos, MAKE_COLOR(COLOR_BLACK, EX_GREY));
    }
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
    case KEY_LEFT: case JOGDIAL_LEFT:
        sel = (sel > 0) ? sel - 1 : NUM_SIMS - 1;
        roll_hint = 0;
        need_draw = 1;
        break;
    case KEY_RIGHT: case JOGDIAL_RIGHT:
        sel = (sel < NUM_SIMS - 1) ? sel + 1 : 0;
        roll_hint = 0;
        need_draw = 1;
        break;
    case KEY_UP:                    // grosser Sprung
        sel = (sel + NUM_SIMS - 5 % NUM_SIMS) % NUM_SIMS;
        roll_hint = 0;
        need_draw = 1;
        break;
    case KEY_DOWN:
        sel = (sel + 5) % NUM_SIMS;
        roll_hint = 0;
        need_draw = 1;
        break;
    case KEY_SET:
        // Beim Entwickeln gilt die Sperre nicht: Der eingelegte Film kommt danach zurueck
        if (!dev_armed && roll_locked() && sel != conf.film_sim) {
            roll_hint = 1;
            need_draw = 2;
            break;
        }
        filmsim_set(sel);
        if (!dev_armed) conf_save();
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
    roll_hint = 0;
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

// Anti-Farbrauschen: conf.film_cnr 0 aus, 1 leicht, 2 stark, 3 auto
// (3 statt 0, damit alte Einstellungen gueltig bleiben).
// Auto richtet sich nach der ISO des Bildes. Rueckgabe 0 / 1 / 2.
#define CNR_AUTO 3
static int cnr_level(void)
{
    if (conf.film_cnr != CNR_AUTO) return conf.film_cnr;
    int iso = (dev_active && dev_has_meta && dev_meta.iso > 0) ? dev_meta.iso : shooting_get_iso_market();
    return iso >= 800 ? 2 : (iso >= 400 ? 1 : 0);
}

// Kleine Anzeige des aktiven Films im Sucherbild
void filmsim_draw_osd(void)
{
    static char buf[160];
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
    if (!dev_armed && conf.film_sim == 0 && !preview_pending && !conf.tv_override_enabled
        && !conf.film_double && !conf.subj_dist_override_koef
        && !conf.film_mist && !conf.film_halation && !conf.film_vignette
        && !conf.film_leak && !conf.film_datestamp && !conf.film_dr && !conf.film_cnr
        && !conf.film_zoom && !conf.film_grain && !conf.film_frame && !conf.film_roll
        && !conf.film_surprise) return;
    strcpy(buf, filmsim_name());
    if (dev_armed) {
        // Wichtiger als alles andere: Das naechste Foto ist kein Foto
        sprintf(buf, lang_str(LANG_EXCCD_DEV_OSD), dev_name, filmsim_name());
    } else if (preview_pending) {
        strcat(buf, " - ");
        strcat(buf, lang_str(LANG_EXCCD_HALF_PRESS));
    } else {
        extern const char* filmsim_tv_str(void);
        const char *tv = filmsim_tv_str();
        if (tv) { strcat(buf, " | "); strcat(buf, tv); }
        extern const char* filmsim_focus_str(void);
        const char *fo = filmsim_focus_str();
        if (fo) { strcat(buf, " | "); strcat(buf, fo); }
        const char *zo = filmsim_zoom_str();
        if (zo) { strcat(buf, " | "); strcat(buf, zo); }
        if (conf.film_double) strcat(buf, conf.film_double == 1 ? " | 2x 1/2" : " | 2x 2/2");
        if (roll_total()) {
            if (conf.film_roll_used >= roll_total()) {
                strcat(buf, " | ");
                strcat(buf, lang_str(LANG_EXCCD_ROLL_DONE));
            } else {
                sprintf(buf + strlen(buf), " | %d/%d", conf.film_roll_used + 1, roll_total());
            }
        }
        if (conf.film_dr == 1) strcat(buf, " | DR+1");
        if (conf.film_dr == 2) strcat(buf, " | DR+2");
        if (conf.film_cnr) strcat(buf, conf.film_cnr == CNR_AUTO ? " | NR auto" : " | NR");
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
// Violet und Concrete: mix_violet / mix_concrete in filmsim_raw.h
// Farbfilter fuer Schwarzweiss: alle drei Kanaele = gewichtete Helligkeit
static const short mix_bw[4][3][3] = {
    {{0}},                                                          // kein Filter
    {{102,128,26},{102,128,26},{102,128,26}},                       // Gelb   .40 .50 .10
    {{141,102,13},{141,102,13},{141,102,13}},                       // Orange .55 .40 .05
    {{192, 64, 0},{192, 64, 0},{192, 64, 0}},                       // Rot    .75 .25 .00
};
// WB-Normierung (Rohwert * f / 256), Tageslicht-Gains des Sensors
#define WB_R FILMSIM_WB_R
#define WB_B FILMSIM_WB_B
#define BLK  31


// Rueckrechnung des Weissabgleichs: (n << 8) / WB als Tabelle statt Division.
// Die alten ARM-Prozessoren haben keinen Divisionsbefehl - jede Division
// kostet dort Dutzende Takte. Die Tabelle enthaelt exakt dieselben Werte
// wie die Division (gleiche Formel), das Ergebnis ist bitgleich.
#define WBDIV_MIN  (-1024)
#define WBDIV_MAX  4095
// 20 KB: erst anlegen, wenn der Farbrauschfilter sie braucht, und dann behalten.
// Fest im Kern wuerden sie gegen das Groessenlimit der Kamera zaehlen.
static short *wbdiv_r = 0, *wbdiv_b = 0;
static int wbdiv_init(void)
{
    int n, size = (WBDIV_MAX - WBDIV_MIN + 1) * sizeof(short);
    if (wbdiv_r) return 1;
    short *t = malloc(2 * size);
    if (!t) return 0;
    wbdiv_b = t + (WBDIV_MAX - WBDIV_MIN + 1);
    for (n = WBDIV_MIN; n <= WBDIV_MAX; n++) {
        t[n - WBDIV_MIN] = (short)((n << 8) / WB_R);
        wbdiv_b[n - WBDIV_MIN] = (short)((n << 8) / WB_B);
    }
    wbdiv_r = t;
    return 1;
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
    if (s->mix == 1)      m = mix_violet;
    else if (s->mix == 2) m = mix_concrete;
    else if (s->mix == 3) m = mix_turquoise;
    else if (s->mix == 4) m = mix_infra;
    else if (s->mix == 5) m = mix_cyano;
    else if (s->mycolors == MC_BW && conf.film_bwfilter > 0 && conf.film_bwfilter < 4)
                          m = mix_bw[conf.film_bwfilter];
    else return;
    int rows = (camera_sensor.raw_rows - FS_ROW0) & ~1;
    int groups = camera_sensor.raw_rowpix / 8;
    int rowlen = camera_sensor.raw_rowlen;
    int y, g, k;
    for (y = 0; y < rows; y += 2) {
        unsigned char *a0 = (unsigned char *)raw + (y + FS_ROW0) * rowlen;   // R G R G
        unsigned char *a1 = a0 + rowlen;                         // G B G B
        for (g = 0; g < groups; g++, a0 += FS_GRP, a1 += FS_GRP) {
            int p[8], q[8];
            dec8(a0, p); dec8(a1, q);
            for (k = 0; k < 8; k += 2)
                mix_block(m, p, q, k, BLK, WB_R, WB_B, FS_WBINV(WB_R), FS_WBINV(WB_B));
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
#ifndef FILMSIM_LABELS
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

// "100CANON/IMG_0123.JPG;Postcard 100" fuer das aktuelle Foto
static void shot_line(char *line)
{
    char dir[32];
    get_target_dir_name(dir);                     // "A/DCIM/100CANON"
    const char *d = strrchr(dir, '/');
    d = d ? d + 1 : dir;
    sprintf(line, "%s/IMG_%04d.JPG;%s", d, (int)get_target_file_num(), filmsim_name());
}

void filmsim_log_shot(void)
{
    char line[96];
    if (dev_active) {                   // Entwickeln: zaehlt nicht als Aufnahme der Rolle
        dev_active = 0;
        dev_restore = 1;
    } else {
        if (roll_total()) {
            if (conf.film_roll_used >= roll_total()) conf.film_roll_used = 0;   // neue Rolle
            conf.film_roll_used++;
        }
        roll_shot = 1;
        dev_name[0] = 0;
    }
    const char *fn = "A/CHDK/FILMLOG.CSV";
    struct stat st;
    int is_new = (stat(fn, &st) != 0);
    shot_line(line);
    if (dev_name[0]) {                  // dritte Spalte: entwickelt aus dieser Datei
        strcat(line, ";");
        strcat(line, dev_name);
        dev_name[0] = 0;
    }
    strcat(line, "\r\n");
    int fd = open(fn, O_RDWR | O_CREAT, 0777);
    if (fd < 0) return;
    if (is_new) {
        const char *hdr = "File;Film\r\n";
        write(fd, hdr, strlen(hdr));
    } else {
        lseek(fd, 0, 2);                          // SEEK_END
    }
    write(fd, line, strlen(line));
    close(fd);
}

//-------------------------------------------------------------------
// Zeitmessung: Rechenzeit der EXCCD-Schritte je Foto in ms
//   Nur wenn A/CHDK/FILMTIME.CSV auf der Karte liegt (leere Datei anlegen).
//   Ohne Datei kostet es einen stat() je Foto und schreibt nichts.
//   raw.c ruft filmsim_time_mark(0..FS_T_STEPS) zwischen den Schritten auf.
//-------------------------------------------------------------------
#define FILMTIME_CSV "A/CHDK/FILMTIME.CSV"
#define FS_T_STEPS 6                    // Hotpixel, Doppelb., Farbrauschen, Kurve, Mischung, Effekte
static int time_on = 0;
static int time_ms[FS_T_STEPS + 1];

void filmsim_time_mark(int i)
{
    if (i == 0) {
        struct stat st;
        time_on = (stat(FILMTIME_CSV, &st) == 0);
    }
    if (time_on && i >= 0 && i <= FS_T_STEPS) time_ms[i] = get_tick_count();
}

void filmsim_time_log(void)
{
    char line[128];
    struct stat st;
    int i;
    if (!time_on || stat(FILMTIME_CSV, &st) != 0) return;
    int fd = open(FILMTIME_CSV, O_RDWR, 0777);
    if (fd < 0) return;
    if (st.st_size == 0) {
        const char *hdr = "File;Film;Hotpixel;Double;ColorNR;Curve;Mix;Effects;Total\r\n";
        write(fd, hdr, strlen(hdr));
    } else {
        lseek(fd, 0, 2);                          // SEEK_END
    }
    shot_line(line);
    for (i = 1; i <= FS_T_STEPS; i++)
        sprintf(line + strlen(line), ";%d", time_ms[i] - time_ms[i - 1]);
    sprintf(line + strlen(line), ";%d\r\n", time_ms[FS_T_STEPS] - time_ms[0]);
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
    static const int modes[] = { LANG_EXCCD_OFF, LANG_EXCCD_ON };
    if (change) {
        if (conf.film_double) { conf.film_double = 0; remove(DBL_FILE); }
        else conf.film_double = 1;
    }
    return lang_str(modes[conf.film_double ? 1 : 0]);
}

// Menue "Farbfilter (S/W)": wirkt nur bei Schwarzweissfilmen
const char* filmsim_bwfilter_enum(int change, __attribute__ ((unused))int arg)
{
    static const int modes[] = { LANG_EXCCD_OFF, LANG_EXCCD_YELLOW, LANG_EXCCD_ORANGE, LANG_EXCCD_RED };
    if (current()->mycolors != MC_BW) return lang_str(LANG_EXCCD_BW_ONLY);
    if (change) conf.film_bwfilter = (conf.film_bwfilter + change + 4) % 4;
    return lang_str(modes[conf.film_bwfilter % 4]);
}

const char* filmsim_bwfilter_str(void)
{
    static const int osd[] = { 0, LANG_EXCCD_OSD_YELLOW, LANG_EXCCD_OSD_ORANGE, LANG_EXCCD_OSD_RED };
    if (current()->mycolors != MC_BW || !(conf.film_bwfilter % 4)) return 0;
    return lang_str(osd[conf.film_bwfilter % 4]);
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

// Sichtbarer JPEG-Ausschnitt in Koordinaten des Rohbilds. camera_sensor.jpeg zaehlt
// ab dem aktiven Sensorbereich (so steht es in der DNG), die Rohdaten ab Zeile und
// Spalte 0 inklusive Dunkelbereich: Beides muss addiert werden.
static void jpeg_rect(int *jx, int *jy, int *jw, int *jh)
{
    *jx = camera_sensor.active_area.x1 + camera_sensor.jpeg.x;
    *jy = camera_sensor.active_area.y1 + camera_sensor.jpeg.y;
    *jw = camera_sensor.jpeg.width;
    *jh = camera_sensor.jpeg.height;
    if (*jw <= 0 || *jh <= 0) { *jx = 0; *jy = 0; *jw = camera_sensor.raw_rowpix; *jh = camera_sensor.raw_rows; }
    if (*jx < 0) *jx = 0;
    if (*jy < 0) *jy = 0;
    if (*jx + *jw > (int)camera_sensor.raw_rowpix) *jw = camera_sensor.raw_rowpix - *jx;
    if (*jy + *jh > (int)camera_sensor.raw_rows) *jh = camera_sensor.raw_rows - *jy;
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
    int jx, jy, jw, jh;
    jpeg_rect(&jx, &jy, &jw, &jh);
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

//---------------- Filmkorn ----------------
// Staerke im fertigen Bild in 0,1 %. Auto richtet sich nach der Zahl im Filmnamen.
static int grain_pct10(void)
{
    static const short fixed[] = { 0, 0, 7, 14, 24 };
    if (conf.film_grain != 1) return (conf.film_grain > 1 && conf.film_grain < 5) ? fixed[conf.film_grain] : 0;
    const char *n = current()->name, *e = n + strlen(n);
    int speed = 0, mul = 1;
    while (e > n && e[-1] >= '0' && e[-1] <= '9') { speed += (*--e - '0') * mul; mul *= 10; }
    if (speed <= 0) return 9;
    return speed <= 100 ? 6 : speed <= 200 ? 9 : speed <= 400 ? 13 : speed <= 800 ? 18 : 28;
}

__attribute__((optimize("O2"))) static void effects_grain(char *raw)
{
    int pct = grain_pct10();
    if (!pct) return;
    int nb = (camera_sensor.raw_rowpix / 8) * 4;
    short *amp = malloc(GRAIN_AMP_N * sizeof(short));
    short *up = malloc(nb * sizeof(short));
    if (amp && up) {
        grain_amp_build(amp, pct);
        grain_apply((unsigned char *)raw, camera_sensor.raw_rows, camera_sensor.raw_rowpix, camera_sensor.raw_rowlen,
                    FS_ROW0, 65536 / WB_R, 65536 / WB_B, amp, up, (unsigned int)get_tick_count() * 2654435761u);
    }
    if (amp) free(amp);
    if (up) free(up);
}

//---------------- Filmrahmen ----------------
static void effects_frame(char *raw)
{
    if (conf.film_frame != FRAME_THIN && conf.film_frame != FRAME_FILM) return;
    int jx, jy, jw, jh;
    jpeg_rect(&jx, &jy, &jw, &jh);
    // Etwas ueber den Ausschnitt hinaus (nur im aktiven Bereich): Falls Canons JPEG
    // ein paar Pixel groesser ist, bleibt kein Bildstreifen ausserhalb des Rahmens.
    {
        int ax = camera_sensor.active_area.x1, ay = camera_sensor.active_area.y1;
        int ex = camera_sensor.active_area.x2, ey = camera_sensor.active_area.y2;
        int x1 = jx + jw + 8, y1 = jy + jh + 8;
        if (ex > ax && ey > ay) {
            jx = jx - 8 < ax ? ax : jx - 8;
            jy = jy - 8 < ay ? ay : jy - 8;
            jw = (x1 > ex ? ex : x1) - jx;
            jh = (y1 > ey ? ey : y1) - jy;
        }
    }
#define FR_RGB(r, g, b) { BLK + (r) * 256 / WB_R, BLK + (g), BLK + (g), BLK + (b) * 256 / WB_B }
    static const short black[4] = { BLK, BLK, BLK, BLK };
    static const short hole[4] = FR_RGB(440, 440, 440);         // Perforation: helles Grau
    static const short ink[4] = FR_RGB(620, 200, 18);           // Randschrift: Bernstein
#undef FR_RGB
    char num[12];
    int t = roll_total();
    sprintf(num, "%dA", t ? (conf.film_roll_used % t) + 1 : (int)(get_target_file_num() % 36) + 1);
    frame_draw((unsigned char *)raw, camera_sensor.raw_rowlen, FS_ROW0, jx, jy, jw, jh, conf.film_frame,
               black, hole, ink, filmsim_name(), num);
}

static void effects_datestamp(void)
{
    if (!conf.film_datestamp) return;
    struct tm *t = get_localtime();
    if (!t) return;
    char s[32];
    if (dev_active && dev_has_meta && dev_meta.year)    // Entwickeln: Tag der Aufnahme
        sprintf(s, "'%02d %02d %02d", dev_meta.year % 100, dev_meta.mon, dev_meta.day);
    else
        sprintf(s, "'%02d %02d %02d", t->tm_year % 100, t->tm_mon + 1, t->tm_mday);
    int jx, jy, jw, jh;
    jpeg_rect(&jx, &jy, &jw, &jh);
    int H = jh / 24, W = H * 11 / 20, T = H / 8, gap = W * 2 / 5;
    if (T < 2) T = 2;
    int n = strlen(s), i, total = 0;
    for (i = 0; i < n; i++) total += (s[i] == '\'' ? W / 2 : (s[i] == ' ' ? W / 2 : W)) + gap;
    int x = jx + jw - jw / 14 - total;
    int y = jy + jh - jh / 12 - H;
    if (conf.film_frame == FRAME_FILM) y -= frame_border_v(FRAME_FILM, jh);   // nicht auf den Filmrand
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
    int level = cnr_level();
    int R = level == 1 ? 2 : (level == 2 ? CNR_RMAX : 0);
    if (!R || !raw || !filmsim_photo_mode()) return;
    int RB = 2 * R + 1;
    int nb = (camera_sensor.raw_rowpix / 8) * 4;     // Bloecke pro Zeile
    int nbr = (camera_sensor.raw_rows - FS_ROW0) / 2; // Blockzeilen
    int rowlen = camera_sensor.raw_rowlen;
    if (!wbdiv_init()) return;
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

//-------------------------------------------------------------------
// Heisse Pixel entfernen (siehe hotpix_fix in filmsim_raw.h), sobald
// EXCCD das Rohbild veraendert. Bei "Aus" ohne Extras bleibt alles bei Canon.
//-------------------------------------------------------------------
void filmsim_hotpix_hook(char *raw)
{
#ifdef FS_RAW
    extern int filmsim_effects_active(void);
    if (!raw || !filmsim_photo_mode()) return;
    // Daylight 100 hat keine Kurve und keine Mischung: Das Rohbild bleibt wie bei "Aus"
    const filmsim_t *s = current();
    int bw_filter = s->mycolors == MC_BW && conf.film_bwfilter > 0 && conf.film_bwfilter < 4;
    if (!s->curve && !s->mix && !bw_filter && !conf.film_dr && !cnr_level() && !conf.film_double
        && !filmsim_effects_active())
        return;
    short *buf = malloc(HP_BUF_SHORTS(camera_sensor.raw_rowpix) * sizeof(short));
    if (!buf) return;
    hotpix_fix((unsigned char *)raw, camera_sensor.raw_rows, camera_sensor.raw_rowpix,
               camera_sensor.raw_rowlen, buf);
    free(buf);
#else
    (void)raw;
#endif
}

const char* filmsim_cnr_enum(int change, __attribute__ ((unused))int arg)
{
    // Anzeige-Reihenfolge Aus, Auto, Leicht, Stark -> gespeicherter Wert
    static const int m[] = { LANG_EXCCD_OFF, LANG_EXCCD_AUTO, LANG_EXCCD_LIGHT, LANG_EXCCD_STRONG };
    static const int   val[] = { 0, CNR_AUTO, 1, 2 };
    int i;
    for (i = 0; i < 4 && val[i] != conf.film_cnr; i++);
    if (i == 4) i = 0;
    if (change) { i = (i + change + 4) % 4; conf.film_cnr = val[i]; }
    return lang_str(m[i]);
}

void filmsim_effects_apply(char *raw)
{
#ifdef FS_RAW
    if (!raw || !filmsim_photo_mode()) return;
    effects_glow_and_tone(raw);
    effects_grain(raw);
    effects_frame(raw);
    effects_datestamp();
#else
    (void)raw;
#endif
}

// Menue-Eintraege
const char* filmsim_mist_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, (int)"1/8", (int)"1/4" };
    if (change) conf.film_mist = (conf.film_mist + change + 3) % 3;
    return lang_str(m[conf.film_mist % 3]);
}
const char* filmsim_halation_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, LANG_EXCCD_ON };
    if (change) conf.film_halation = !conf.film_halation;
    return lang_str(m[conf.film_halation ? 1 : 0]);
}
const char* filmsim_vignette_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, LANG_EXCCD_LIGHT, LANG_EXCCD_STRONG };
    if (change) conf.film_vignette = (conf.film_vignette + change + 3) % 3;
    return lang_str(m[conf.film_vignette % 3]);
}
const char* filmsim_leak_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, LANG_EXCCD_ON };
    if (change) conf.film_leak = !conf.film_leak;
    return lang_str(m[conf.film_leak ? 1 : 0]);
}
const char* filmsim_datestamp_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, LANG_EXCCD_ON };
    if (change) conf.film_datestamp = !conf.film_datestamp;
    return lang_str(m[conf.film_datestamp ? 1 : 0]);
}
int filmsim_effects_active(void)
{
    return conf.film_mist || conf.film_halation || conf.film_vignette || conf.film_leak || conf.film_datestamp
        || conf.film_grain || conf.film_frame;
}
const char* filmsim_grain_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, LANG_EXCCD_AUTO, LANG_EXCCD_FINE, LANG_EXCCD_MEDIUM, LANG_EXCCD_COARSE };
    if (change) conf.film_grain = (conf.film_grain + change + 5) % 5;
    return lang_str(m[conf.film_grain % 5]);
}
const char* filmsim_frame_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, LANG_EXCCD_THIN, LANG_EXCCD_NEGATIVE };
    if (change) conf.film_frame = (conf.film_frame + change + 3) % 3;
    return lang_str(m[conf.film_frame % 3]);
}

//-------------------------------------------------------------------
// Dynamik+: knapper belichten (Lichter schuetzen), Schatten per Kurve
// wieder aufhellen. Die Belichtungskorrektur passiert in shooting.c.
//-------------------------------------------------------------------
const char* filmsim_dr_enum(int change, __attribute__ ((unused))int arg)
{
    static const int m[] = { LANG_EXCCD_OFF, (int)"+1", (int)"+2" };
    if (change) {
        conf.film_dr = (conf.film_dr + change + 3) % 3;
        set_curve(current());
    }
    return lang_str(m[conf.film_dr % 3]);
}

// Belichtungsverschiebung in 1/96 EV (0 = keine), von shooting.c bei jeder
// Aufnahme genutzt (bei Tv-Override oder Skript mit auto = 0, ohne Verschiebung)
int filmsim_dr_tv96_shift(int automatic)
{
    shot_dr = 0;
    if (!automatic) return 0;
    if (conf.film_dr < 1 || conf.film_dr > 2) return 0;
    if (!filmsim_photo_mode()) return 0;
    if (shooting_get_prop(PROPCASE_IS_FLASH_READY)) return 0;   // mit Blitz nicht
    shot_dr = conf.film_dr;
    return conf.film_dr * 96;
}

// Vor den Kurven (raw.c): Wurde nicht knapper belichtet (Blitz, Tv-Override),
// darf auch die Aufhell-Kurve nicht wirken -> fuer dieses Bild die normale
// Kurve laden. Danach (end = 1) wieder die Kurve der Menueeinstellung.
void filmsim_dr_curve_hook(int end)
{
    int want = end ? ((conf.film_dr >= 1 && conf.film_dr <= 2) ? conf.film_dr : 0) : shot_dr;
    if (end) shot_dr = -1;
    if (want < 0 || !filmsim_photo_mode() || want == curve_dr) return;
    set_curve_dr(current(), want);
}

//-------------------------------------------------------------------
// Defekte Sensorspalten (A/CHDK/BADCOLS.TXT)
//   Alte CCDs bekommen helle Spalten ueber die ganze Bildhoehe. Jede Zeile
//   der Datei: eine Spaltennummer im Rohbild (wie in der DNG), # = Kommentar.
//   Die Spalte wird aus den Nachbarn gleicher Farbe (x-2, x+2) neu
//   berechnet, vor DNG und JPEG. Jeder Sensor hat andere Spalten, darum
//   eine Datei auf der Karte und kein Wert im Code.
//-------------------------------------------------------------------
#define BADCOLS_TXT "A/CHDK/BADCOLS.TXT"
#define BADCOLS_MAX 16
static short badcols[BADCOLS_MAX];
static int badcols_n = -1;          // -1 = Datei noch nicht gelesen

static void badcols_load(void)
{
    char buf[256], *p;
    int fd, len;
    badcols_n = 0;
    fd = open(BADCOLS_TXT, O_RDONLY, 0777);
    if (fd < 0) return;
    len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0) return;
    buf[len] = 0;
    for (p = buf; *p && badcols_n < BADCOLS_MAX; ) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p >= '0' && *p <= '9') {
            int x = strtol(p, &p, 10);
            if (x >= 2 && x < camera_sensor.raw_rowpix - 2) badcols[badcols_n++] = x;
        }
        while (*p && *p != '\n') p++;
    }
}

void filmsim_badcols_apply(void)
{
    int i, y;
    if (badcols_n < 0) badcols_load();
    for (i = 0; i < badcols_n; i++) {
        int x = badcols[i];
        for (y = 0; y < camera_sensor.raw_rows; y++)
            set_raw_pixel(x, y, (get_raw_pixel(x - 2, y) + get_raw_pixel(x + 2, y)) / 2);
    }
}
