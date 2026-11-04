// Zoom-Optionen fuer EXCCD
//   conf.film_zoom: 0 = frei (Canon), 1 = Stufen, 2+k = fest auf Stufe k
//
//   Stufen: Weitwinkel-Ende, gaengige Kleinbild-Brennweiten innerhalb des
//   Zoombereichs, Tele-Ende. Jede Stufe ist die naechstgelegene echte
//   Zoomstufe des Objektivs. In beiden Modi sieht die Canon-Firmware den
//   Zoomhebel nicht (kein optischer und kein Digitalzoom). Das Objektiv
//   faehrt CHDK selbst - im Hauptprozess, nicht in der Tastatur-Task,
//   weil shooting_set_zoom() wartet, bis der Zoom steht.

#include "platform.h"
#include "stdlib.h"
#include "camera_info.h"
#include "conf.h"
#include "shooting.h"
#include "keyboard.h"
#include "lens.h"
#include "gui.h"
#include "filmsim.h"
#include "lang.h"
#include "gui_lang.h"

#define ZOOM_FREE   0
#define ZOOM_STEPS  1
#define ZOOM_FIXED  2

// Kleinbild-Brennweiten in mm, die als Zwischenstufen in Frage kommen
static const short nominal_mm[] = { 24, 28, 35, 50, 85, 105, 135, 200, 300 };
#define NUM_NOMINAL ((int)(sizeof(nominal_mm)/sizeof(nominal_mm[0])))
#define MAX_STOPS   (NUM_NOMINAL + 2)

static short stop_zp[MAX_STOPS];    // Zoomstufe des Objektivs
static short stop_mm[MAX_STOPS];    // angezeigte Brennweite
static int   num_stops = 0;

static int efl_mm(int zp)
{
    return (get_effective_focal_length(zp) + 500) / 1000;
}

static int nearest_zp(int mm)
{
    int zp, best = 0, best_d = 0x7fffffff;
    for (zp = 0; zp < zoom_points; zp++) {
        int d = get_effective_focal_length(zp) - mm * 1000;
        if (d < 0) d = -d;
        if (d < best_d) { best_d = d; best = zp; }
    }
    return best;
}

static void add_stop(int zp, int mm)
{
    if (num_stops > 0 && stop_zp[num_stops - 1] >= zp) return;
    stop_zp[num_stops] = zp;
    stop_mm[num_stops] = mm;
    num_stops++;
}

// Stufen einmalig aus der Brennweitentabelle der Kamera bauen
static void build_stops(void)
{
    if (num_stops) return;
    int tele = zoom_points - 1;
    int wide_mm = efl_mm(0), tele_mm = efl_mm(tele);
    int i;
    add_stop(0, wide_mm);
    for (i = 0; i < NUM_NOMINAL; i++) {
        int mm = nominal_mm[i];
        // nicht naeher als 8 % an den Enden (IXUS 60: 35 mm = Weitwinkel)
        if (mm * 100 < wide_mm * 108 || mm * 108 > tele_mm * 100) continue;
        int zp = nearest_zp(mm);
        if (zp > 0 && zp < tele) add_stop(zp, mm);
    }
    add_stop(tele, tele_mm);
}

static int fixed_stop(void)
{
    build_stops();
    int k = conf.film_zoom - ZOOM_FIXED;
    if (k < 0 || k >= num_stops) return -1;
    return k;
}

const char* filmsim_zoom_enum(int change, __attribute__ ((unused))int arg)
{
    static char buf[32];
    build_stops();
    int n = ZOOM_FIXED + num_stops;
    if (conf.film_zoom < 0 || conf.film_zoom >= n) conf.film_zoom = 0;
    if (change) conf.film_zoom = (conf.film_zoom + change + n) % n;
    if (conf.film_zoom == ZOOM_FREE)  return lang_str(LANG_EXCCD_ZOOM_FREE);
    if (conf.film_zoom == ZOOM_STEPS) return lang_str(LANG_EXCCD_ZOOM_STEPS);
    const char *f = lang_str(LANG_EXCCD_ZOOM_FIXED);
    if (strlen(f) > 20) f = "%d mm";               // Schutz vor zu langen Texten aus der .lng
    sprintf(buf, f, stop_mm[conf.film_zoom - ZOOM_FIXED]);
    return buf;
}

// Kurzform fuer die Statuszeile, 0 bei freiem Zoom
const char* filmsim_zoom_str(void)
{
    static char buf[8];
    if (conf.film_zoom == ZOOM_FREE) return 0;
    build_stops();
    int zp = lens_get_zoom_point(), mm = efl_mm(zp), i;
    for (i = 0; i < num_stops; i++)
        if (stop_zp[i] == zp) mm = stop_mm[i];
    sprintf(buf, "%dmm", mm);
    return buf;
}

static volatile int step_request = 0;   // +1 / -1 aus der Tastatur-Task

// Aus kbd_process() (Tastatur-Task) ausserhalb des ALT-Modus.
// 1 = Tasten vor der Canon-Firmware verbergen (Zoomhebel gedrueckt)
int filmsim_zoom_kbd(void)
{
#if CAM_HAS_ZOOM_LEVER
    static int prev = 0;
    if (conf.film_zoom == ZOOM_FREE || !filmsim_photo_mode()) {
        prev = 0;
        return 0;
    }
    int key = kbd_is_key_pressed(KEY_ZOOM_IN) ? 1 : kbd_is_key_pressed(KEY_ZOOM_OUT) ? -1 : 0;
    if (key && key != prev && conf.film_zoom == ZOOM_STEPS) step_request = key;
    prev = key;
    return key != 0;
#else
    return 0;
#endif
}

// Darf CHDK jetzt das Objektiv fahren?
static int zoom_ready(void)
{
    static int photo_since = 0;
    int now = get_tick_count();
    if (!filmsim_photo_mode()) { photo_since = 0; return 0; }
    if (!photo_since) photo_since = now | 1;
    // nach dem Einschalten / Moduswechsel faehrt das Objektiv noch aus
    if (now < 3000 || now - photo_since < 1000) return 0;
    if (camera_info.state.gui_mode != GUI_MODE_NONE) return 0;
    if (camera_info.state.is_shutter_half_press || shooting_in_progress()) return 0;
    return 1;
}

// Wird ca. alle 20 ms aus filmsim_tick() aufgerufen
void filmsim_zoom_tick(void)
{
    static int tries = 0, last_target = -1;
    if (!zoom_ready()) {
        step_request = 0;
        tries = 0;
        return;
    }
    int cur = lens_get_zoom_point();

    if (conf.film_zoom == ZOOM_STEPS) {
        int dir = step_request;
        step_request = 0;
        if (!dir) return;
        build_stops();
        int i, target = -1;
        if (dir > 0) {
            for (i = 0; i < num_stops; i++)
                if (stop_zp[i] > cur) { target = stop_zp[i]; break; }
        } else {
            for (i = num_stops - 1; i >= 0; i--)
                if (stop_zp[i] < cur) { target = stop_zp[i]; break; }
        }
        if (target >= 0) {
            shooting_set_zoom(target);
            gui_set_need_restore();
        }
        return;
    }

    int k = fixed_stop();
    if (k < 0) return;
    int target = stop_zp[k];
    if (target != last_target) { last_target = target; tries = 0; }
    // hoechstens 3 Versuche je Aufnahme-Sitzung, falls das Objektiv nicht folgt
    if (cur != target && tries < 3) {
        tries++;
        shooting_set_zoom(target);
        gui_set_need_restore();
    }
}
