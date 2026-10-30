#ifndef FILMSIM_TXT_H
#define FILMSIM_TXT_H

// Filmtabelle: Datentyp und Parser fuer A/CHDK/FILMS/FILMS.TXT
// Der Parser kommt ohne Bibliotheksfunktionen aus und laesst sich daher
// auch am PC testen (tools/filmsim/test_films_txt.c).

// Canon My-Colors-Modi (propset 1, siehe include/propset1.h)
#define MC_OFF      0
#define MC_VIVID    1
#define MC_NEUTRAL  2
#define MC_BW       3
#define MC_SEPIA    4
#define MC_POSFILM  5
#define MC_CUSTOM   11

typedef struct {
    const char *name;       // Anzeige im Menue / OSD
    short mycolors;         // Canon My-Colors-Modus
    // Custom-Color-Regler, nur bei MC_CUSTOM: -2..+2
    signed char contrast, sharpness, saturation, red, green, blue, skin;
    const char *curve;      // Kurvendatei ohne Pfad, 0 = keine
    const char *label;      // Lasche A/CHDK/FILMS/<label>.FLM, 0 = Textkarte
    int mix;                // RAW-Kanalmischung: 0 = keine, 1 = Lomochrome Purple, 2 = Metropolis
} filmsim_t;

#define FILMS_TXT_MAX       24      // Looks aus der Datei (ohne "Aus")
#define FILMS_TXT_NAME_MAX  20      // passt in Statuszeile und Filmauswahl
#define FILMS_TXT_FILE_MAX  16384   // groessere Dateien werden ignoriert

// Texte eines Eintrags (filmsim_t zeigt hier hinein)
typedef struct {
    char name[FILMS_TXT_NAME_MAX + 1];
    char curve[13];         // 8.3, Name max. 7 Zeichen (Platz fuer DR-Endung 1/2)
    char label[9];          // ohne .FLM
} films_txt_str_t;

// Zerlegt den Dateiinhalt (buf wird dabei veraendert, braucht len+1 Bytes).
// Gueltige Zeilen landen in sims[0..max-1] / str[0..max-1].
// log (optional): Fehlermeldungen je Zeile, nullterminiert.
// Rueckgabe: Anzahl gueltiger Eintraege.
extern int films_txt_parse(char *buf, int len, filmsim_t *sims, films_txt_str_t *str, int max,
                           char *log, int log_size);

#endif
