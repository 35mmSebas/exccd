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
static struct tm tm_now = { .tm_year = 126, .tm_mon = 10, .tm_mday = 7 };
struct tm *get_localtime(void) { return &tm_now; }

// RAW entwickeln: Datei, die raw.c laden wuerde; Weissabgleich des Hilfsschusses (G, R, B)
static char prep_fn[64];
void raw_prepare_develop(const char *f, int prompt) { (void)prompt; if (f) strcpy(prep_fn, f); else prep_fn[0] = 0; }
static int host_wb[3] = { 1000, 1000, 1000 };
long get_property_case(long id, void *buf, long n) { (void)id; memcpy(buf, host_wb, n); return 0; }
static int mbox_count = 0;
void gui_mbox_init(int title, int msg, const unsigned int flags, void (*on_select)(unsigned int btn))
{
    (void)title; (void)msg; (void)flags; (void)on_select;
    mbox_count++;
}

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

    // Filmrahmen: an den Kanten des Ausschnitts, Dunkelbereich unberuehrt.
    // 2 (Negative aus dem Lab-Build) steht vielleicht noch in der CFG und gilt als Thin.
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
        int bh = jh / 48 - 8, bv = jh / 48 - 8, bad = 0, ob = 0, pic = 0;
        int ih = jh / 48 + 8, iv = jh / 48 + 8;
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
                if (rim && v == o) bad++;       // Rand: schwarz, nie Bild
            }
        CHECK(ob == 0, "Rahmen %d: %d Pixel ausserhalb des aktiven Bereichs veraendert", st, ob);
        CHECK(pic == 0, "Rahmen %d: %d Pixel im Bild veraendert", st, pic);
        CHECK(bad == 0, "Rahmen %d: %d Pixel im Rand sind noch Bild", st, bad);
    }

    // Alle neuen Looks mit allem an: laeuft durch, nichts wird 0, Bild aendert sich
    conf.film_grain = 1;
    conf.film_frame = 1;
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

// --- RAW entwickeln -----------------------------------------------------------
#define DNG_HEAD 1024
static void put16(unsigned char *b, unsigned v) { b[0] = v & 255; b[1] = (v >> 8) & 255; }
static void put32(unsigned char *b, unsigned v) { put16(b, v & 0xffff); put16(b + 2, v >> 16); }
static void put_entry(unsigned char *ifd, int i, unsigned tag, unsigned type, unsigned count, unsigned value)
{
    unsigned char *e = ifd + 2 + i * 12;
    put16(e, tag); put16(e + 2, type); put32(e + 4, count); put32(e + 8, value);
}

// Kopf wie modules/dng.c (nur die Eintraege, die EXCCD liest), Rohdaten am Dateiende
static void make_dng(const char *name, int w, int h, int bits, int wr, int wg, int wb, int iso, const char *date)
{
    int rawsize = camera_sensor.raw_size;
    unsigned char *f = malloc(DNG_HEAD + rawsize);
    memset(f, 0, DNG_HEAD);
    f[0] = 'I'; f[1] = 'I'; put16(f + 2, 42); put32(f + 4, 8);
    put16(f + 8, 4);
    put_entry(f + 8, 0, 0x10E, 2, 1, 0);        // ImageDescription, leer
    put_entry(f + 8, 1, 0x14A, 4, 1, 200);
    put_entry(f + 8, 2, 0x8769, 4, 1, 300);
    put_entry(f + 8, 3, 0xC628, 5, 3, 400);
    put16(f + 200, 3);
    put_entry(f + 200, 0, 0x100, 4, 1, w);
    put_entry(f + 200, 1, 0x101, 4, 1, h);
    put_entry(f + 200, 2, 0x102, 3, 1, bits);
    put16(f + 300, 2);
    put_entry(f + 300, 0, 0x8827, 3, 1, iso);
    put_entry(f + 300, 1, 0x9003, 2, 20, 460);
    put32(f + 400, 1000); put32(f + 404, wr);
    put32(f + 408, 1000); put32(f + 412, wg);
    put32(f + 416, 1000); put32(f + 420, wb);
    strcpy((char *)f + 460, date);
    memcpy(f + DNG_HEAD, rawbuf, rawsize);
    fs_put(name, (char *)f, DNG_HEAD + rawsize);
    free(f);
}

// raw.c: Datei laden, dann die Kette
static void develop_shot(void)
{
    struct stat st;
    int fd = open(prep_fn, O_RDONLY, 0);
    CHECK(fd >= 0, "Entwickeln ohne Datei");
    stat(prep_fn, &st);
    lseek(fd, st.st_size - camera_sensor.raw_size, 0);
    read(fd, rawbuf, camera_sensor.raw_size);
    close(fd);
    prep_fn[0] = 0;
    camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_PROCESSING;
    filmsim_develop_begin((char *)rawbuf);
    shoot((char *)rawbuf);
}

static void fill_flat(int v)
{
    int x, y;
    for (y = 0; y < (int)camera_sensor.raw_rows; y++)
        for (x = 0; x < (int)camera_sensor.raw_rowpix; x++)
            set_raw_pixel(x, y, (unsigned short)(v << (CAM_SENSOR_BITS_PER_PIXEL - 10)));
}

static void scenario_develop(void)
{
    const char *good = "A/DCIM/100CANON/CRW_0012.DNG";
    int rows = camera_sensor.raw_rows, rowpix = camera_sensor.raw_rowpix, rowlen = camera_sensor.raw_rowlen;
    int bits = camera_sensor.bits_per_pixel, r0 = FS_TEST_ROW0, saves, m;
    int cx = (rowpix / 2) & ~7, cy = (rows / 2) & ~1;    // Mitte, gerade
    rawbuf = malloc(rows * rowlen + 8);
    unsigned char *a = malloc(rows * rowlen);
    CHECK(rawbuf && a, "Speicher");
    camera_on();
    fill_flat(400);
    make_dng(good, rowpix, rows, bits, 2000, 1000, 1500, 100, "2025:03:04 10:11:12");

    // Fremde Dateien werden abgelehnt und nichts wird scharf gestellt
    make_dng("A/DCIM/W.DNG", rowpix + 8, rows, bits, 2000, 1000, 1500, 100, "2025:03:04 10:11:12");
    make_dng("A/DCIM/B.DNG", rowpix, rows, bits == 10 ? 12 : 10, 2000, 1000, 1500, 100, "2025:03:04 10:11:12");
    fs_put("A/DCIM/SMALL.JPG", "xx", 2);
    static const char *bad[] = { "A/DCIM/W.DNG", "A/DCIM/B.DNG", "A/DCIM/SMALL.JPG", "A/DCIM/FEHLT.DNG", 0 };
    for (m = 0; bad[m]; m++) {
        int c = mbox_count;
        filmsim_develop_select(bad[m]);
        CHECK(mbox_count == c + 1, "%s: keine Meldung", bad[m]);
        CHECK(!filmsim_develop_armed() && !prep_fn[0], "%s: scharf gestellt", bad[m]);
    }
    filmsim_develop_select(0);          // Dateiauswahl abgebrochen
    CHECK(!filmsim_develop_armed(), "Abbruch stellt scharf");

    // Look 5 eingelegt: der Film kommt danach zurueck
    filmsim_set(5);
    saves = conf_saves;
    cur_mode = 0;
    filmsim_develop_select(good);
    CHECK(filmsim_develop_armed() && strcmp(prep_fn, good) == 0, "nicht scharf: [%s]", prep_fn);
    CHECK(cur_mode != 0, "Filmauswahl nicht offen");
    filmsim_open_menu();
    for (m = 1; m < 5; m++) press(KEY_LEFT);    // Look 1, Daylight: keine Kurve, keine Mischung
    press(KEY_SET);
    CHECK(conf.film_sim == 1, "Look: %d", conf.film_sim);
    CHECK(conf_saves == saves, "Probierlook gespeichert");
    CHECK(strstr(osd_text(), "Develop CRW_0012.DNG: Daylight 100") != 0, "Statuszeile: [%s]", drawn);

    // Hilfsschuss in anderem Licht: Rohdaten so umrechnen, dass der WB der Aufnahme herauskommt
    host_wb[0] = 1000; host_wb[1] = 1000; host_wb[2] = 3000;
    fill_flat(10);                      // Sensordaten des Hilfsschusses, muessen verschwinden
    int mark = card[fs_find("A/CHDK/FILMLOG.CSV")].size;
    develop_shot();
    CHECK(!filmsim_develop_armed(), "bleibt scharf");
    CHECK(RAW10(cx, cy + r0) == 31 + 369 * 2, "R: %d", RAW10(cx, cy + r0));
    CHECK(RAW10(cx + 1, cy + r0) == 400, "G: %d", RAW10(cx + 1, cy + r0));
    CHECK(RAW10(cx + 1, cy + r0 + 1) == 31 + 185, "B: %d", RAW10(cx + 1, cy + r0 + 1));
    {
        int li = fs_find("A/CHDK/FILMLOG.CSV");
        card[li].data[card[li].size] = 0;
        CHECK(strstr(card[li].data + mark, ";Daylight 100;CRW_0012.DNG") != 0, "Logbuch: [%s]", card[li].data + mark);
    }

    // Film erst zurueck, wenn Canon fertig ist
    ticks(60);
    CHECK(conf.film_sim == 1, "Ruecknahme waehrend der Verarbeitung");
    camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_NONE;
    ticks(60);
    CHECK(conf.film_sim == 5, "eingelegter Film nicht zurueck: %d", conf.film_sim);
    cur_mode = 0;

    // Normales Foto danach: keine dritte Spalte, Ueberschreiben klappt
    host_wb[0] = host_wb[1] = host_wb[2] = 1000;
    filmsim_set(5);
    mark = card[fs_find("A/CHDK/FILMLOG.CSV")].size;
    filmsim_log_shot();
    ticks(30);
    {
        int li = fs_find("A/CHDK/FILMLOG.CSV");
        card[li].data[card[li].size] = 0;
        CHECK(strstr(card[li].data + mark, ".DNG") == 0, "normales Foto mit Quelle: [%s]", card[li].data + mark);
    }

    // Abbrechen: Look und Zustand zurueck
    filmsim_develop_select(good);
    filmsim_open_menu(); press(KEY_RIGHT); press(KEY_SET);
    CHECK(conf.film_sim == 6, "Abbruch-Look: %d", conf.film_sim);
    filmsim_develop_cancel();
    CHECK(!filmsim_develop_armed() && !prep_fn[0], "Abbruch: noch scharf");
    ticks(60);
    CHECK(conf.film_sim == 5, "Abbruch: Film %d", conf.film_sim);
    cur_mode = 0;

    // Sofort neu waehlen, solange die Ruecknahme offen ist: Der eingelegte Film bleibt gemerkt
    filmsim_develop_select(good);
    filmsim_open_menu(); press(KEY_RIGHT); press(KEY_SET);
    filmsim_develop_cancel();
    camera_info.state.gui_mode_none = 0;
    ticks(60);
    filmsim_develop_select(good);
    camera_info.state.gui_mode_none = 1;
    filmsim_develop_cancel();
    ticks(60);
    CHECK(conf.film_sim == 5, "neu waehlen: Film %d", conf.film_sim);
    cur_mode = 0;

    // Datumsstempel: Tag der Aufnahme
    filmsim_set(1);
    conf.film_datestamp = 1;
    fill_flat(400);
    make_dng(good, rowpix, rows, bits, 1000, 1000, 1000, 100, "2025:03:04 10:11:12");
    filmsim_develop_select(good);
    filmsim_open_menu(); press(KEY_SET);
    develop_shot();
    camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_NONE;
    ticks(60);
    memcpy(a, rawbuf, rows * rowlen);
    fill_flat(400);
    shoot((char *)rawbuf);
    ticks(60);
    CHECK(memcmp(a, rawbuf, rows * rowlen) != 0, "Datum: heute und Aufnahmetag gleich");
    fill_flat(400);
    tm_now.tm_year = 125; tm_now.tm_mon = 2; tm_now.tm_mday = 4;
    shoot((char *)rawbuf);
    ticks(60);
    tm_now.tm_year = 126; tm_now.tm_mon = 10; tm_now.tm_mday = 7;
    CHECK(memcmp(a, rawbuf, rows * rowlen) == 0, "Datum: nicht der Aufnahmetag");
    conf.film_datestamp = 0;

    // Farbrauschen Auto richtet sich nach der ISO der Aufnahme, nicht nach dem Hilfsschuss
    {
        int x, y;
        filmsim_cnr_enum(1, 0);
        for (y = 0; y < rows; y++)
            for (x = 0; x < rowpix; x++)
                set_raw_pixel(x, y, (unsigned short)((300 + (x * 31 + y * 17) % 64) << (CAM_SENSOR_BITS_PER_PIXEL - 10)));
        memcpy(a, rawbuf, rows * rowlen);
        shoot((char *)rawbuf);          // ISO 100 laut Kamera: kein Filter
        CHECK(memcmp(a, rawbuf, rows * rowlen) == 0, "Auto bei ISO 100 filtert");
        ticks(60);
        memcpy(rawbuf, a, rows * rowlen);
        make_dng(good, rowpix, rows, bits, 1000, 1000, 1000, 800, "2025:03:04 10:11:12");
        filmsim_develop_select(good);
        filmsim_open_menu(); press(KEY_SET);
        develop_shot();
        camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_NONE;
        CHECK(memcmp(a, rawbuf, rows * rowlen) != 0, "Auto nimmt nicht die ISO der Aufnahme");
        ticks(60);
        filmsim_cnr_enum(-1, 0);
        cur_mode = 0;
    }

    // Dynamik+: Stufe im Kopf vermerken, Groesse und Rohdaten bleiben, Entwicklung nimmt ihre Kurve
    {
        int ci, size;
        fill_flat(400);
        make_dng(good, rowpix, rows, bits, 1000, 1000, 1000, 100, "2025:03:04 10:11:12");
        ci = fs_find(good);
        size = card[ci].size;
        memcpy(a, card[ci].data + DNG_HEAD, rows * rowlen);
        CHECK(filmsim_dng_mark(good) == -1, "Vermerk ohne Dynamik+");
        conf.film_dr = 2;
        CHECK(filmsim_dr_tv96_shift(1) == 192, "Dynamik+");
        CHECK(filmsim_dng_mark(good) >= 0, "Vermerk fehlt");
        conf.film_dr = 0;
        filmsim_dr_curve_hook(1);       // Normale Aufnahme zu Ende, Stufe wieder unbekannt
        CHECK(card[ci].size == size, "Groesse %d statt %d", card[ci].size, size);
        CHECK(memcmp(a, card[ci].data + DNG_HEAD, rows * rowlen) == 0, "Rohdaten veraendert");
        filmsim_set(0);
        filmsim_develop_select(good);
        filmsim_open_menu(); press(KEY_SET);
        filmsim_develop_begin((char *)rawbuf);
        filmsim_dr_curve_hook(0);
#if CAM_SENSOR_BITS_PER_PIXEL == 12     // bei 10 Bit laedt curves.flt, das gibt es am PC nicht
        CHECK(strstr(conf.curve_file, "DR2.CV") != 0, "Kurve: [%s]", conf.curve_file);
#endif
        filmsim_log_shot();
        filmsim_dr_curve_hook(1);
        CHECK(conf.film_dr == 0, "Einstellung geaendert");
        ticks(60);
    }
    free(a);
    free(rawbuf);
}

int main(int argc, char **argv)
{
    const char *what = argc > 1 ? argv[1] : "";
    const char *root = argc > 2 ? argv[2] : ".";
    if (strcmp(what, "names") == 0) scenario_names(argc > 3 ? argv[3] : "0");
    else if (strcmp(what, "picker") == 0) scenario_picker();
    else if (strcmp(what, "raw") == 0) scenario_raw(root);
    else if (strcmp(what, "develop") == 0) scenario_develop();
    else { printf("Szenario?\n"); return 2; }
    printf("%s (%d Fehler)\n", fails ? "FEHLER" : "ok", fails);
    return fails != 0;
}
