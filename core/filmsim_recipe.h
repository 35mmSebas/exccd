#ifndef FILMSIM_RECIPE_H
#define FILMSIM_RECIPE_H

// Rezepte: Film + Weissabgleich + WB-Verschiebung + Look-Einstellungen + Effekte
// in A/CHDK/FILMS/RECIPES.TXT (Plaetze C1..C4). Parser und Schreiber kommen ohne
// Bibliotheksfunktionen aus und laufen auch am PC (tools/filmsim/test_recipes.c).

#include "filmsim_txt.h"

#define RECIPE_SLOTS        4
#define RECIPE_NAME_MAX     16
#define RECIPE_FILE_MAX     8192    // groessere Dateien werden ignoriert
#define RECIPE_SHIFT_MAX    9       // WB-Verschiebung R/B: -9..+9

// Weissabgleich im Rezept: -1 = Canon-Einstellung nicht anfassen, sonst Index
// in diese Liste (die Kamera rechnet ihn in PROPCASE_WB_MODE um)
#define RECIPE_WB_KEEP          (-1)
#define RECIPE_WB_AUTO          0
#define RECIPE_WB_DAYLIGHT      1
#define RECIPE_WB_CLOUDY        2
#define RECIPE_WB_TUNGSTEN      3
#define RECIPE_WB_FLUORESCENT   4
#define RECIPE_WB_FLUORESCENT_H 5
#define RECIPE_WB_CUSTOM        6
#define RECIPE_WB_N             7

// Einstellungen mit festen Stufen. Die Werte sind die von conf.film_* (siehe include/conf.h).
enum {
    RF_WB,          // RECIPE_WB_*
    RF_DR,          // conf.film_dr        0..2
    RF_NR,          // conf.film_cnr       0, 1, 2, 3 (auto)
    RF_BWFILTER,    // conf.film_bwfilter  0..3
    RF_GRAIN,       // conf.film_grain     0..4
    RF_VIGNETTE,    // conf.film_vignette  0..2
    RF_MIST,        // conf.film_mist      0..2
    RF_HALATION,    // conf.film_halation  0/1
    RF_LEAK,        // conf.film_leak      0/1
    RF_DATE,        // conf.film_datestamp 0/1
    RF_FRAME,       // conf.film_frame     0/1
    RF_N
};

typedef struct {
    char used;
    char name[RECIPE_NAME_MAX + 1];
    char film[FILMS_TXT_NAME_MAX + 1];  // Name aus der Filmliste, "" = Aus
    signed char shift_r, shift_b;       // -9..+9
    signed char v[RF_N];
} recipe_t;

// Zerlegt den Dateiinhalt (buf wird veraendert, braucht len+1 Bytes) in r[0..RECIPE_SLOTS-1].
// log (optional): Fehlermeldungen je Zeile. Rueckgabe: Anzahl belegter Plaetze.
extern int recipes_parse(char *buf, int len, recipe_t *r, char *log, int log_size);

// Schreibt alle belegten Plaetze als Text. Rueckgabe: Laenge, -1 = zu klein.
extern int recipes_format(const recipe_t *r, char *out, int size);

// Leerer Platz: alles aus, Weissabgleich nicht anfassen
extern void recipe_clear(recipe_t *r);

#endif
