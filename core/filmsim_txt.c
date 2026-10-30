/*
 * Parser fuer A/CHDK/FILMS/FILMS.TXT
 *
 * Eine Zeile je Look, Spalten mit | getrennt, Leerzeichen werden getrimmt,
 * Zeilen mit # am Anfang und Leerzeilen werden uebersprungen:
 *
 *   Name | Modus | Kon | Sch | Sat | R | G | B | Haut | Kurve | Lasche | Mix
 *
 *   Name    1..20 Zeichen (laengere werden gekuerzt)
 *   Modus   Canon My Colors 0..11 (0 = aus, 3 = S/W, 11 = Custom Color)
 *   Regler  -2..+2, wirken nur bei Modus 11
 *   Kurve   Datei in A/CHDK/CURVES, Name max. 7 Zeichen, .CV wird ergaenzt;
 *           - = keine
 *   Lasche  Datei A/CHDK/FILMS/<Lasche>.FLM, max. 8 Zeichen; - = Textkarte
 *   Mix     RAW-Kanalmischung 0..2 (optional, Standard 0)
 *
 * Fehlerhafte Zeilen werden uebersprungen und in log vermerkt.
 * Keine Bibliotheksfunktionen -> auch am PC testbar.
 */

#include "filmsim_txt.h"

#define NUM_COLS 12

static char *trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t') s++;
    for (e = s; *e; e++) ;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
    return s;
}

static int to_upper(int c)
{
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

static int is_name_char(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// Ganzzahl mit optionalem Vorzeichen; 0 = ungueltig
static int parse_int(const char *s, int lo, int hi, int *out)
{
    int neg = 0, v = 0, digits = 0;
    if (*s == '+' || *s == '-') neg = (*s++ == '-');
    for (; *s >= '0' && *s <= '9'; s++, digits++) {
        v = v * 10 + (*s - '0');
        if (v > 1000) return 0;
    }
    if (!digits || *s) return 0;
    if (neg) v = -v;
    if (v < lo || v > hi) return 0;
    *out = v;
    return 1;
}

// "-" oder leer = keine Datei (Rueckgabe 0), Fehler = -1, sonst 1
// Name in Grossbuchstaben nach out, Endung ext wird ergaenzt bzw. entfernt
static int parse_file(const char *s, const char *ext, int keep_ext, int base_max, char *out)
{
    int n = 0, i;
    const char *e;
    if (!*s || (s[0] == '-' && !s[1])) return 0;
    for (; *s && *s != '.'; s++) {
        int c = to_upper(*s);
        if (!is_name_char(c) || n >= base_max) return -1;
        out[n++] = c;
    }
    if (!n) return -1;
    if (*s == '.') {                    // Endung angegeben: muss passen
        s++;
        for (e = ext; *e; e++, s++)
            if (to_upper(*s) != *e) return -1;
        if (*s) return -1;
    }
    if (keep_ext) {
        out[n++] = '.';
        for (i = 0; ext[i]; i++) out[n++] = ext[i];
    }
    out[n] = 0;
    return 1;
}

static void log_add(char **lp, char *lend, int line, const char *msg)
{
    char num[8];
    int i = 0, n = line;
    const char *p;
    char *l = *lp;
    if (!l) return;
    do { num[i++] = '0' + n % 10; n /= 10; } while (n && i < 7);
    for (p = "Zeile "; *p && l < lend; ) *l++ = *p++;
    while (i > 0 && l < lend) *l++ = num[--i];
    for (p = ": "; *p && l < lend; ) *l++ = *p++;
    for (p = msg; *p && l < lend; ) *l++ = *p++;
    for (p = "\r\n"; *p && l < lend; ) *l++ = *p++;
    *l = 0;
    *lp = l;
}

static const char *col_names[NUM_COLS] = {
    "Name", "Modus", "Kontrast", "Schaerfe", "Saettigung", "Rot", "Gruen", "Blau",
    "Hautton", "Kurve", "Lasche", "Mix"
};

int films_txt_parse(char *buf, int len, filmsim_t *sims, films_txt_str_t *str, int max,
                    char *log, int log_size)
{
    char *p = buf, *end = buf + len;
    char *lp = (log && log_size > 0) ? log : 0;
    char *lend = lp ? log + log_size - 1 : 0;
    int count = 0, line = 0;
    if (lp) *lp = 0;

    // UTF-8-BOM ueberspringen
    if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;

    while (p < end) {
        char *ln = p, *col[NUM_COLS], *q;
        char msg[48];
        int ncol = 0, v[NUM_COLS], i, k;

        while (p < end && *p != '\n') p++;
        if (p < end) *p++ = 0; else *end = 0;   // buf muss len+1 Bytes haben
        line++;
        for (q = ln; *q; q++) if (*q == '\r') *q = 0;
        ln = trim(ln);
        if (!*ln || *ln == '#') continue;

        if (count >= max) {
            log_add(&lp, lend, line, "zu viele Looks, Rest ignoriert");
            break;
        }

        // Spalten trennen
        col[ncol++] = ln;
        for (q = ln; *q; q++) {
            if (*q == '|') {
                *q = 0;
                if (ncol >= NUM_COLS) { ncol++; break; }
                col[ncol++] = q + 1;
            }
        }
        if (ncol < NUM_COLS - 2 || ncol > NUM_COLS) {
            log_add(&lp, lend, line, "falsche Spaltenzahl (10 bis 12 erwartet)");
            continue;
        }
        for (i = 0; i < ncol; i++) col[i] = trim(col[i]);

        filmsim_t *s = &sims[count];
        films_txt_str_t *t = &str[count];

        // Name
        if (!*col[0]) { log_add(&lp, lend, line, "Name fehlt"); continue; }
        for (k = 0; col[0][k] && k < FILMS_TXT_NAME_MAX; k++) t->name[k] = col[0][k];
        t->name[k] = 0;
        if (col[0][k]) log_add(&lp, lend, line, "Name auf 20 Zeichen gekuerzt");

        // Modus, Regler
        if (!parse_int(col[1], 0, 11, &v[1])) { log_add(&lp, lend, line, "Modus muss 0..11 sein"); continue; }
        for (i = 2; i <= 8; i++) {
            if (!parse_int(col[i], -2, 2, &v[i])) break;
        }
        if (i <= 8) {
            for (k = 0, q = (char *)col_names[i]; *q; ) msg[k++] = *q++;
            for (q = " muss -2..+2 sein"; *q; ) msg[k++] = *q++;
            msg[k] = 0;
            log_add(&lp, lend, line, msg);
            continue;
        }

        // Kurve, Lasche
        k = parse_file(col[9], "CV", 1, 7, t->curve);
        if (k < 0) { log_add(&lp, lend, line, "Kurve: Name max. 7 Zeichen A-Z 0-9 _, Endung .CV"); continue; }
        s->curve = k ? t->curve : 0;
        k = (ncol > 10) ? parse_file(col[10], "FLM", 0, 8, t->label) : 0;
        if (k < 0) { log_add(&lp, lend, line, "Lasche: Name max. 8 Zeichen A-Z 0-9 _"); continue; }
        s->label = k ? t->label : 0;

        // Mix
        v[11] = 0;
        if (ncol > 11 && *col[11] && !parse_int(col[11], 0, 2, &v[11])) {
            log_add(&lp, lend, line, "Mix muss 0..2 sein");
            continue;
        }

        s->name       = t->name;
        s->mycolors   = v[1];
        s->contrast   = v[2];
        s->sharpness  = v[3];
        s->saturation = v[4];
        s->red        = v[5];
        s->green      = v[6];
        s->blue       = v[7];
        s->skin       = v[8];
        s->mix        = v[11];
        count++;
    }
    return count;
}
