// Nachgebildete Kamera fuer tools/filmsim/test_filmsim.py: alles, was core/filmsim.c von
// CHDK braucht, als Stummel (Tasten, Bildschirm, Karte im Speicher, Propcases), dazu die
// Testszenarien. Bewusst dieselben CHDK-Header wie die Firmware.

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
#include "lang.h"
#include "gui_lang.h"
#include "lang_host.h"

extern int printf(const char *, ...);
extern int _open(const char *, int, ...);        // Windows-Dateizugriff fuer die echten Kurven
extern int _read(int, void *, unsigned int);
extern int _close(int);
extern void exit(int);

// --- Pruefen ----------------------------------------------------------------
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { if (++fails <= 25) { printf("FEHLER Zeile %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

// --- Kamerazustand ----------------------------------------------------------
Conf conf;
#include "camera_info.c"                         // camera_sensor, camera_screen, camera_info wie in der Firmware

long link_text_start, link_data_start, link_bss_start, link_bss_end;
static unsigned char host_colors[64];
unsigned char *chdk_colors = host_colors;

int canon_menu_active = 0;
char canon_shoot_menu_active = 0;
CMenu root_menu;
gui_handler menuGuiHandler;

static int now_ms = 10000;
long get_tick_count() { return now_ms; }

// --- Texte ------------------------------------------------------------------
char *lang_str(int id)
{
    int i;
    if (id >= 100000) return (char *)host_lits[id - 100000];
    for (i = 0; host_lang[i].s; i++)
        if (host_lang[i].id == id) return (char *)host_lang[i].s;
    return "?";
}

// --- Karte im Speicher ------------------------------------------------------
#define MAX_FILES 32
static struct { char name[64]; char *data; int size; int pos; } card[MAX_FILES];

static int fs_find(const char *n)
{
    int i;
    for (i = 0; i < MAX_FILES; i++)
        if (card[i].data && strcmp(card[i].name, n) == 0) return i;
    return -1;
}
static int fs_put(const char *n, const char *d, int len)
{
    int i = fs_find(n);
    if (i < 0) { for (i = 0; i < MAX_FILES && card[i].data; i++); }
    if (i >= MAX_FILES) { printf("Karte voll\n"); exit(2); }
    strcpy(card[i].name, n);
    card[i].data = malloc(len + 200000);
    if (len) memcpy(card[i].data, d, len);
    card[i].size = len;
    card[i].pos = 0;
    return i;
}
int open(const char *n, int flags, int mode)
{
    int i = fs_find(n);
    (void)mode;
    if (i < 0) {
        if (!(flags & O_CREAT)) return -1;
        i = fs_put(n, "", 0);
    }
    if (flags & O_TRUNC) card[i].size = 0;
    card[i].pos = 0;
    return i + 100;
}
int close(int fd) { (void)fd; return 0; }
int read(int fd, void *b, long n)
{
    int i = fd - 100, left = card[i].size - card[i].pos;
    if (n > left) n = left;
    memcpy(b, card[i].data + card[i].pos, n);
    card[i].pos += n;
    return n;
}
int write(int fd, const void *b, long n)
{
    int i = fd - 100;
    memcpy(card[i].data + card[i].pos, b, n);
    card[i].pos += n;
    if (card[i].pos > card[i].size) card[i].size = card[i].pos;
    return n;
}
int lseek(int fd, long off, int whence)
{
    int i = fd - 100;
    card[i].pos = whence == 2 ? card[i].size + off : off;
    return card[i].pos;
}
int stat(const char *n, struct stat *st)
{
    int i = fs_find(n);
    if (i < 0) return -1;
    if (st) st->st_size = card[i].size;
    return 0;
}
int remove(const char *n)
{
    int i = fs_find(n);
    if (i >= 0) card[i].data = 0;
    return 0;
}

// --- Propcases, Bildschirm, Tasten -----------------------------------------
static short props[512];
int shooting_get_prop(int id) { return props[id & 511]; }
void shooting_set_prop(int id, int v) { props[id & 511] = (short)v; }
short shooting_get_iso_market() { return 100; }
short shooting_get_canon_iso_mode(void) { return 0; }
void shooting_set_iso_mode(int v) { (void)v; }

static char drawn[4096];
static int restore_count = 0;
void gui_set_need_restore(void) { restore_count++; }
static void drawn_add(const char *s)
{
    if (strlen(drawn) + strlen(s) + 2 < sizeof(drawn)) { strcat(drawn, s); strcat(drawn, "\n"); }
}
int draw_string(coord x, coord y, const char *s, twoColors c) { (void)x; (void)y; (void)c; drawn_add(s); return 0; }
void draw_string_scaled(coord x, coord y, const char *s, twoColors c, int a, int b) { (void)x; (void)y; (void)c; (void)a; (void)b; drawn_add(s); }
void draw_rectangle(coord a, coord b, coord c, coord d, twoColors e, int f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; }
void draw_hline(coord a, coord b, int c, color d) { (void)a; (void)b; (void)c; (void)d; }
twoColors user_color(confColor c) { (void)c; return 0; }

static gui_handler *cur_mode = 0;
gui_handler *gui_set_mode(gui_handler *h) { cur_mode = h; return h; }
void gui_menu_init(CMenu *m) { (void)m; }
static int alt_exits = 0;
void exit_alt(void) { alt_exits++; }
void exit_alt_on_release(void) { alt_exits++; }
static int conf_saves = 0;
void conf_save(void) { conf_saves++; }

static long next_key = 0;
long kbd_get_autoclicked_key(void) { long k = next_key; next_key = 0; return k; }
long kbd_is_key_pressed(long k) { (void)k; return 0; }
long get_jogdial_direction(void) { return 0; }
// Modulstummel
#ifdef OPT_CURVES
static void curve_init_stub(void) {}
static libcurves_sym curves_stub;
libcurves_sym *libcurves = &curves_stub;
#endif
void filmsim_zoom_tick(void) {}
const char *filmsim_zoom_str(void) { return 0; }
const char *filmsim_tv_str(void) { return 0; }
const char *filmsim_focus_str(void) { return 0; }
long get_target_file_num(void) { return 123; }
void get_target_dir_name(char *d) { strcpy(d, "A/DCIM/100CANON"); }
static struct tm tm_now;
struct tm *get_localtime(void) { tm_now.tm_year = 126; tm_now.tm_mon = 10; tm_now.tm_mday = 7; return &tm_now; }

// --- Rohbild ----------------------------------------------------------------
static unsigned char *rawbuf;
static int bpp(void) { return CAM_SENSOR_BITS_PER_PIXEL; }

unsigned short get_raw_pixel(unsigned int x, unsigned int y)
{
#if CAM_SENSOR_BITS_PER_PIXEL == 10
    unsigned char *a = rawbuf + y * camera_sensor.raw_rowlen + (x / 8) * 10;
    switch (x % 8) {
        case 0: return ((0x3fc & (((unsigned short)a[1]) << 2)) | (a[0] >> 6));
        case 1: return ((0x3f0 & (((unsigned short)a[0]) << 4)) | (a[3] >> 4));
        case 2: return ((0x3c0 & (((unsigned short)a[3]) << 6)) | (a[2] >> 2));
        case 3: return ((0x300 & (((unsigned short)a[2]) << 8)) | (a[5]));
        case 4: return ((0x3fc & (((unsigned short)a[4]) << 2)) | (a[7] >> 6));
        case 5: return ((0x3f0 & (((unsigned short)a[7]) << 4)) | (a[6] >> 4));
        case 6: return ((0x3c0 & (((unsigned short)a[6]) << 6)) | (a[9] >> 2));
        default: return ((0x300 & (((unsigned short)a[9]) << 8)) | (a[8]));
    }
#else
    unsigned char *a = rawbuf + y * camera_sensor.raw_rowlen + (x / 4) * 6;
    switch (x % 4) {
        case 0: return ((unsigned short)(a[1]) << 4) | (a[0] >> 4);
        case 1: return ((unsigned short)(a[0] & 0x0F) << 8) | (a[3]);
        case 2: return ((unsigned short)(a[2]) << 4) | (a[5] >> 4);
        default: return ((unsigned short)(a[5] & 0x0F) << 8) | (a[4]);
    }
#endif
}
void set_raw_pixel(unsigned int x, unsigned int y, unsigned short v)
{
#if CAM_SENSOR_BITS_PER_PIXEL == 10
    unsigned char *a = rawbuf + y * camera_sensor.raw_rowlen + (x / 8) * 10;
    switch (x % 8) {
        case 0: a[0]=(a[0]&0x3F)|(v<<6); a[1]=v>>2;                 break;
        case 1: a[0]=(a[0]&0xC0)|(v>>4); a[3]=(a[3]&0x0F)|(v<<4);   break;
        case 2: a[2]=(a[2]&0x03)|(v<<2); a[3]=(a[3]&0xF0)|(v>>6);   break;
        case 3: a[2]=(a[2]&0xFC)|(v>>8); a[5]=v;                    break;
        case 4: a[4]=v>>2;               a[7]=(a[7]&0x3F)|(v<<6);   break;
        case 5: a[6]=(a[6]&0x0F)|(v<<4); a[7]=(a[7]&0xC0)|(v>>4);   break;
        case 6: a[6]=(a[6]&0xF0)|(v>>6); a[9]=(a[9]&0x03)|(v<<2);   break;
        case 7: a[8]=v;                  a[9]=(a[9]&0xFC)|(v>>8);   break;
    }
#else
    unsigned char *a = rawbuf + y * camera_sensor.raw_rowlen + (x / 4) * 6;
    switch (x % 4) {
        case 0: a[0] = (a[0]&0x0F) | (unsigned char)(v << 4); a[1] = (unsigned char)(v >> 4); break;
        case 1: a[0] = (a[0]&0xF0) | (unsigned char)(v >> 8); a[3] = (unsigned char)v;        break;
        case 2: a[2] = (unsigned char)(v >> 4); a[5] = (a[5]&0x0F) | (unsigned char)(v << 4); break;
        case 3: a[4] = (unsigned char)v; a[5] = (a[5]&0xF0) | (unsigned char)(v >> 8);        break;
    }
#endif
}
#if cam_CFAPattern == 0x02010100
#define FS_TEST_ROW0 0
#else
#define FS_TEST_ROW0 1
#endif
#define RAW10(x, y) ((int)(get_raw_pixel((x), (y)) >> (CAM_SENSOR_BITS_PER_PIXEL - 10)))

// --- filmsim.c --------------------------------------------------------------
extern void filmsim_log_shot(void);
extern void filmsim_hotpix_hook(char *);
extern void filmsim_double_hook(char *);
extern void filmsim_cnr_hook(char *);
extern void filmsim_dr_curve_hook(int);
extern void filmsim_raw_apply(char *);
extern void filmsim_effects_apply(char *);
extern void filmsim_time_mark(int);
extern void filmsim_time_log(void);
#if CAM_SENSOR_BITS_PER_PIXEL == 12
extern void filmsim_curve_apply(char *);
#endif

// Reihenfolge wie raw_process() in core/raw.c
static void shoot(char *raw)
{
    filmsim_time_mark(0);
    filmsim_hotpix_hook(raw);
    filmsim_time_mark(1);
    filmsim_double_hook(raw);
    filmsim_time_mark(2);
    filmsim_cnr_hook(raw);
    filmsim_time_mark(3);
    filmsim_dr_curve_hook(0);
#if CAM_SENSOR_BITS_PER_PIXEL == 12
    filmsim_curve_apply(raw);
#endif
    filmsim_time_mark(4);
    filmsim_raw_apply(raw);
    filmsim_time_mark(5);
    filmsim_effects_apply(raw);
    filmsim_time_mark(6);
    filmsim_log_shot();
    filmsim_time_log();
    filmsim_dr_curve_hook(1);
}

// Zeit laufen lassen, Hauptschleife nachbilden
static void ticks(int n)
{
    while (n-- > 0) {
        now_ms += 20;
        filmsim_tick();
    }
}

static void camera_on(void)
{
    camera_info.state.mode_rec = 1;
    camera_info.state.mode_video = 0;
    camera_info.state.mode_play = 0;
    camera_info.state.gui_mode_none = 1;
    camera_info.state.gui_mode = GUI_MODE_NONE;
    camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_NONE;
    camera_screen.width = 360;
    camera_screen.height = 240;
    canon_menu_active = (int)(long long)&canon_menu_active - 4;
    conf.film_iso_prev = -1;
    ticks(60);
}

static const char *osd_text(void)
{
    // Nach einem Filmwechsel zeigt die Zeile erst den Hinweis, bis Canon die Farben uebernimmt
    camera_info.state.is_shutter_half_press = 1;
    ticks(2);
    camera_info.state.is_shutter_half_press = 0;
    ticks(2);
    drawn[0] = 0;
    now_ms += 8000;                     // Nachlauf der Canon-Symbole abwarten
    filmsim_draw_osd();                 // setzt den Nachlauf beim ersten Aufruf nach dem Start
    now_ms += 8000;
    drawn[0] = 0;
    filmsim_draw_osd();
    return drawn;
}

static void press(long key)
{
    next_key = key;
    drawn[0] = 0;
    cur_mode->kbd_process();
    cur_mode->redraw(0);
}

// --- Szenarien ---------------------------------------------------------------
#define FILMS_TXT "A/CHDK/FILMS/FILMS.TXT"
static const char two_looks[] =
    "Pastel 400 | 11 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 0\n"
    "Noir 3200  |  3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 0\n";

// films_load laeuft einmal je Start, darum ein Prozess je Variante (argv[3], test_filmsim.py)
static void scenario_names(const char *variant)
{
    int v = variant ? atoi(variant) : 0;
    switch (v) {
    case 0:     // neue Tabelle, Name gewinnt gegen den Index
        strcpy(conf.film_name, "Noir 3200");
        conf.film_sim = 2;
        camera_on();
        CHECK(strcmp(filmsim_name(), "Noir 3200") == 0, "Name gewinnt: %s", filmsim_name());
        CHECK(conf.film_sim == 20, "Index %d statt 20", conf.film_sim);
        break;
    case 1:     // andere FILMS.TXT, gleicher Look an anderer Stelle
        fs_put(FILMS_TXT, two_looks, sizeof(two_looks) - 1);
        strcpy(conf.film_name, "Noir 3200");
        conf.film_sim = 20;
        camera_on();
        CHECK(conf.film_sim == 2, "FILMS.TXT: Index %d statt 2", conf.film_sim);
        CHECK(strcmp(filmsim_name(), "Noir 3200") == 0, "FILMS.TXT: %s", filmsim_name());
        break;
    case 2:     // Look gibt es nicht mehr: Aus
        fs_put(FILMS_TXT, two_looks, sizeof(two_looks) - 1);
        strcpy(conf.film_name, "Postcard 100");
        conf.film_sim = 2;
        camera_on();
        CHECK(conf.film_sim == 0, "fehlender Look: Index %d", conf.film_sim);
        CHECK(conf.film_name[0] == 0, "fehlender Look: Name %s", conf.film_name);
        break;
    case 3:     // alte Einstellung ohne Namen: Index bleibt, Name wird nachgetragen
        conf.film_name[0] = 0;
        conf.film_sim = 4;
        camera_on();
        CHECK(conf.film_sim == 4, "alt: Index %d", conf.film_sim);
        CHECK(strcmp(conf.film_name, "Journal 400") == 0, "alt: Name %s", conf.film_name);
        break;
    case 4:     // alter Index ausserhalb der Tabelle
        conf.film_name[0] = 0;
        conf.film_sim = 99;
        camera_on();
        CHECK(conf.film_sim == 0, "alt zu gross: Index %d", conf.film_sim);
        break;
    case 5:     // Auswahl merkt sich den Namen, Aus loescht ihn
        camera_on();
        filmsim_set(18);
        CHECK(strcmp(conf.film_name, "Chrome 64") == 0, "set: %s", conf.film_name);
        filmsim_set(0);
        CHECK(conf.film_name[0] == 0, "set Aus: %s", conf.film_name);
        break;
    }
}

static void scenario_picker(void)
{
    int n;
    camera_on();
    filmsim_set(3);
    filmsim_open_menu();
    CHECK(cur_mode != 0, "Auswahl nicht offen");
    cur_mode->redraw(1);
    CHECK(strstr(drawn, "3/26") != 0, "Position fehlt: [%s]", drawn);
    press(KEY_RIGHT);
    CHECK(strstr(drawn, "4/26") != 0, "rechts: [%s]", drawn);
    press(KEY_LEFT); press(KEY_LEFT);
    CHECK(strstr(drawn, "2/26") != 0, "links: [%s]", drawn);
    press(KEY_DOWN);
    CHECK(strstr(drawn, "7/26") != 0, "runter +5: [%s]", drawn);
    press(KEY_UP); press(KEY_UP);
    CHECK(strstr(drawn, "23/26") == 0 && strstr(drawn, "24/26") != 0, "hoch -5 ueber den Anfang (2-5 = 24 von 0..26): [%s]", drawn);
    for (n = 0; n < 27; n++) press(KEY_RIGHT);
    CHECK(strstr(drawn, "24/26") != 0, "Runde: [%s]", drawn);
    for (n = 0; n < 2; n++) press(KEY_RIGHT);
    CHECK(strstr(drawn, "26/26") != 0, "Ende: [%s]", drawn);
    press(KEY_RIGHT);
    CHECK(strstr(drawn, "/26") == 0 && strstr(drawn, "Off") != 0, "Off ohne Position: [%s]", drawn);
    press(KEY_SET);
    CHECK(conf.film_sim == 0, "SET: Film %d", conf.film_sim);
    CHECK(alt_exits == 1, "SET schliesst nicht");
}

static void scenario_roll(void)
{
    int i;
    char *raw = 0;
    camera_on();
    filmsim_set(5);
    filmsim_roll_enum(1, 0);
    CHECK(conf.film_roll == 1 && conf.film_roll_used == 0, "Rolle an: %d %d", conf.film_roll, conf.film_roll_used);
    CHECK(strstr(osd_text(), "| 1/12") != 0, "Statuszeile: [%s]", drawn);
    // Vor dem ersten Foto ist der Wechsel frei
    filmsim_open_menu(); press(KEY_RIGHT); press(KEY_SET);
    CHECK(conf.film_sim == 6, "Wechsel vor Foto 1: %d", conf.film_sim);
    cur_mode = 0;
    for (i = 0; i < 5; i++) { filmsim_log_shot(); ticks(30); }
    CHECK(conf.film_roll_used == 5, "Zaehler %d", conf.film_roll_used);
    CHECK(conf_saves > 0, "Zaehler nicht gespeichert");
    CHECK(strstr(osd_text(), "| 6/12") != 0, "Statuszeile: [%s]", drawn);
    // Gesperrt: anderer Film geht nicht, Hinweis in der Leiste
    int ex = alt_exits;
    filmsim_open_menu(); press(KEY_RIGHT); press(KEY_SET);
    CHECK(conf.film_sim == 6, "Sperre wirkt nicht: %d", conf.film_sim);
    CHECK(strstr(drawn, "Roll loaded: 7 left") != 0, "Hinweis: [%s]", drawn);
    CHECK(alt_exits == ex, "Auswahl geschlossen");
    press(KEY_LEFT);
    CHECK(strstr(drawn, "6/26") != 0, "Hinweis bleibt: [%s]", drawn);
    press(KEY_SET);                     // derselbe Film: schliesst normal
    CHECK(alt_exits == ex + 1, "derselbe Film schliesst nicht");
    // Rolle voll
    for (i = 0; i < 7; i++) { filmsim_log_shot(); ticks(30); }
    CHECK(conf.film_roll_used == 12, "voll: %d", conf.film_roll_used);
    CHECK(strstr(osd_text(), "roll done") != 0, "Rolle voll: [%s]", drawn);
    filmsim_open_menu(); press(KEY_RIGHT); press(KEY_SET);
    CHECK(conf.film_sim == 7, "volle Rolle sperrt: %d", conf.film_sim);
    // Naechstes Foto: neue Rolle, wieder gesperrt
    filmsim_log_shot(); ticks(30);
    CHECK(conf.film_roll_used == 1, "neue Rolle: %d", conf.film_roll_used);
    CHECK(strstr(osd_text(), "| 2/12") != 0, "neue Rolle: [%s]", drawn);
    filmsim_open_menu(); press(KEY_RIGHT); press(KEY_SET);
    CHECK(conf.film_sim == 7, "neue Rolle sperrt nicht: %d", conf.film_sim);
    // Raus ueber das Menue
    filmsim_roll_enum(-1, 0);
    CHECK(conf.film_roll == 0 && conf.film_roll_used == 0, "Rolle aus: %d %d", conf.film_roll, conf.film_roll_used);
    filmsim_open_menu(); press(KEY_RIGHT); press(KEY_SET);
    CHECK(conf.film_sim == 8, "Rolle aus, Sperre bleibt: %d", conf.film_sim);
    CHECK(strstr(osd_text(), "/12") == 0, "Statuszeile: [%s]", drawn);
    (void)raw;
}

static void scenario_surprise(void)
{
    int i, last, changed = 0, seen[64] = {0}, kinds = 0;
    camera_on();
    filmsim_set(5);
    filmsim_surprise_enum(1, 0);
    CHECK(conf.film_surprise == 1, "An");
    last = conf.film_sim;
    for (i = 0; i < 200; i++) {
        filmsim_log_shot();
        now_ms += 1237;
        ticks(30);
        CHECK(conf.film_sim >= 1 && conf.film_sim <= 26, "Film %d", conf.film_sim);
        if (conf.film_sim != last) changed++;
        last = conf.film_sim;
        if (!seen[conf.film_sim]++) kinds++;
    }
    CHECK(changed == 200, "nur %d von 200 Fotos mit neuem Film", changed);
    CHECK(kinds >= 20, "nur %d verschiedene Filme", kinds);
    // Nicht waehrend Halbdruck oder Verarbeitung
    filmsim_log_shot();
    last = conf.film_sim;
    camera_info.state.is_shutter_half_press = 1;
    ticks(30);
    CHECK(conf.film_sim == last, "Wechsel bei Halbdruck");
    camera_info.state.is_shutter_half_press = 0;
    camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_PROCESSING;
    ticks(30);
    CHECK(conf.film_sim == last, "Wechsel waehrend der Verarbeitung");
    camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_NONE;
    ticks(30);
    CHECK(conf.film_sim != last, "kein Wechsel danach");
    // Mit Rolle: erst wenn sie voll ist
    filmsim_roll_enum(1, 0);
    last = conf.film_sim;
    for (i = 0; i < 11; i++) { filmsim_log_shot(); now_ms += 500; ticks(30); }
    CHECK(conf.film_sim == last, "Wechsel mitten in der Rolle");
    filmsim_log_shot(); now_ms += 500; ticks(30);
    CHECK(conf.film_sim != last, "kein Wechsel bei voller Rolle");
    CHECK(conf.film_roll_used == 12, "Zaehler %d", conf.film_roll_used);
}

// Rohbild: Verlauf, Spalte fuer Spalte verschieden, je Bayer-Farbe plausibel
static void raw_fill_test(void)
{
    int x, y;
    for (y = 0; y < (int)camera_sensor.raw_rows; y++)
        for (x = 0; x < (int)camera_sensor.raw_rowpix; x++) {
            int v = 200 + (x * 500) / camera_sensor.raw_rowpix + ((y * 100) / camera_sensor.raw_rows);
            set_raw_pixel(x, y, (unsigned short)(v << (CAM_SENSOR_BITS_PER_PIXEL - 10)));
        }
}

static void load_card_curve(const char *root, const char *name)
{
    static char buf[8192];
    char path[256], fn[64];
    sprintf(path, "%s/CHDK/CURVES/%s", root, name);
    sprintf(fn, "A/CHDK/CURVES/%s", name);
    int f = _open(path, 0x8000);                // _O_RDONLY | _O_BINARY
    if (f < 0) { printf("Kurve fehlt: %s\n", path); exit(2); }
    int n = _read(f, buf, sizeof(buf));
    _close(f);
    fs_put(fn, buf, n);
}

static void scenario_raw(const char *root)
{
    int rows = camera_sensor.raw_rows, rowpix = camera_sensor.raw_rowpix, rowlen = camera_sensor.raw_rowlen;
    int ax = camera_sensor.active_area.x1, ay = camera_sensor.active_area.y1;
    int jx = ax + camera_sensor.jpeg.x, jy = ay + camera_sensor.jpeg.y;
    int jw = camera_sensor.jpeg.width, jh = camera_sensor.jpeg.height;
    int x, y, st, i;
    static const char *curves[] = { "SUMMER.CV", "CHROME.CV", "BYPASS.CV", "NOIR.CV", "TURQUOI.CV", "INFRA.CV",
                                    "TINTYPE.CV", "CYANO.CV", "INSTANT.CV", "PASTEL.CV", 0 };
    rawbuf = malloc(rows * rowlen + 8);
    unsigned char *orig = malloc(rows * rowlen);
    CHECK(rawbuf && orig, "Speicher");
    CHECK(((rowlen & 1) == 0), "ungerade Zeilenlaenge %d", rowlen);
    CHECK(jw > 0 && jh > 0 && jx + jw <= rowpix && jy + jh <= rows, "Ausschnitt %d %d %d %d im Rohbild %d x %d", jx, jy, jw, jh, rowpix, rows);
    for (i = 0; curves[i]; i++) load_card_curve(root, curves[i]);
    camera_on();

    // Daylight ohne Extras: Rohbild bleibt bitgleich
    raw_fill_test();
    memcpy(orig, rawbuf, rows * rowlen);
    filmsim_set(1);
    shoot((char *)rawbuf);
    CHECK(memcmp(orig, rawbuf, rows * rowlen) == 0, "Daylight veraendert das Rohbild");

    // Filmrahmen: an den Kanten des Ausschnitts, Dunkelbereich unberuehrt
    for (st = 1; st <= 2; st++) {
        // Heller als jede Farbe des Rahmens: Ein gleicher Wert heisst sicher "nicht gefuellt"
        for (y = 0; y < rows; y++)
            for (x = 0; x < rowpix; x++)
                set_raw_pixel(x, y, (unsigned short)((850 + (x * 100) / rowpix) << (CAM_SENSOR_BITS_PER_PIXEL - 10)));
        memcpy(orig, rawbuf, rows * rowlen);
        filmsim_set(0);
        conf.film_frame = st;
        conf.film_grain = 0;
        shoot((char *)rawbuf);
        conf.film_frame = 0;
        // Der Rahmen beginnt bis zu 8 Pixel ausserhalb des Ausschnitts (aktiver Bereich) und
        // ist von dort aus gemessen: Auf dem Bild mindestens so breit wie diese Werte.
        int bh = jh / 48 - 8, bv = (st == 2 ? jh / 11 : jh / 48) - 8, bad = 0, ob = 0, pic = 0;
        int ih = jh / 48 + 8, iv = (st == 2 ? jh / 11 : jh / 48) + 8;
        for (y = 0; y < rows; y++)
            for (x = 0; x < rowpix; x++) {
                int v = get_raw_pixel(x, y), o;
                unsigned char *sv = rawbuf; rawbuf = orig; o = get_raw_pixel(x, y); rawbuf = sv;
                int out = x < ax || y < ay || x >= camera_sensor.active_area.x2 || y >= camera_sensor.active_area.y2;
                if (out && v != o) ob++;
                int ii = x >= jx + ih && x < jx + jw - ih && y >= jy + iv && y < jy + jh - iv;
                int rim = x >= jx && x < jx + jw && y >= jy && y < jy + jh &&
                          !(x >= jx + bh && x < jx + jw - bh && y >= jy + bv && y < jy + jh - bv);
                if (ii && v != o) pic++;        // Bild selbst bleibt
                if (rim && v == o) bad++;       // Rand: schwarz, Perforation oder Schrift, nie Bild
            }
        CHECK(ob == 0, "Rahmen %d: %d Pixel ausserhalb des aktiven Bereichs veraendert", st, ob);
        CHECK(pic == 0, "Rahmen %d: %d Pixel im Bild veraendert", st, pic);
        CHECK(bad == 0, "Rahmen %d: %d Pixel im Rand sind noch Bild", st, bad);
    }

    // Alle neuen Looks mit allem an: laeuft durch, nichts wird 0, Bild aendert sich
    conf.film_grain = 1;
    conf.film_frame = 2;
    conf.film_datestamp = 1;
    conf.film_vignette = 1;
    conf.film_mist = 1;
    conf.film_halation = 1;
    conf.film_leak = 1;
    conf.film_cnr = 2;
    for (i = 17; i <= 26; i++) {
        raw_fill_test();
        memcpy(orig, rawbuf, rows * rowlen);
        filmsim_set(i);
        shoot((char *)rawbuf);
        int zeros = 0;
        for (y = 0; y < rows; y += 7)
            for (x = 0; x < rowpix; x += 3)
                if (get_raw_pixel(x, y) == 0) zeros++;
        CHECK(zeros == 0, "%s: %d neue Defekt-Markierungen", filmsim_name(), zeros);
        CHECK(memcmp(orig, rawbuf, rows * rowlen) != 0, "%s: Bild unveraendert", filmsim_name());
    }
    conf.film_mist = conf.film_halation = conf.film_leak = conf.film_cnr = conf.film_vignette = conf.film_datestamp = 0;
    conf.film_frame = 0;

    // Korn Auto folgt dem Filmnamen: Noir 3200 staerker als Chrome 64
    double sd[2] = { 0, 0 };
    int idx[2] = { 18, 20 };
    for (st = 0; st < 2; st++) {
        for (y = 0; y < rows; y++)
            for (x = 0; x < rowpix; x++) set_raw_pixel(x, y, (unsigned short)(300 << (CAM_SENSOR_BITS_PER_PIXEL - 10)));
        filmsim_set(idx[st]);
        extern void filmsim_effects_apply(char *);
        filmsim_effects_apply((char *)rawbuf);
        double s = 0; int n = 0;
        for (y = FS_TEST_ROW0; y + 1 < rows; y += 2)
            for (x = 1; x < rowpix; x += 2) {
                int d = RAW10(x, y) - 300;
                s += d * d; n++;
            }
        sd[st] = s / n;
    }
    CHECK(sd[1] > sd[0] * 4, "Korn Auto: Varianz Noir %.1f, Chrome %.1f", sd[1], sd[0]);
    conf.film_grain = 0;
    free(rawbuf);
    free(orig);
}

int main(int argc, char **argv)
{
    const char *what = argc > 1 ? argv[1] : "";
    const char *root = argc > 2 ? argv[2] : ".";
    if (strcmp(what, "names") == 0) scenario_names(argc > 3 ? argv[3] : "0");
    else if (strcmp(what, "picker") == 0) scenario_picker();
    else if (strcmp(what, "roll") == 0) scenario_roll();
    else if (strcmp(what, "surprise") == 0) scenario_surprise();
    else if (strcmp(what, "raw") == 0) scenario_raw(root);
    else { printf("Szenario?\n"); return 2; }
    printf("%s (%d Fehler)\n", fails ? "FEHLER" : "ok", fails);
    return fails != 0;
}
