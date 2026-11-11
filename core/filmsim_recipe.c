/*
 * Rezepte: A/CHDK/FILMS/RECIPES.TXT lesen und schreiben
 *
 * Ein Block je Platz, "[C1] Name" beginnt ihn, darunter "Schluessel = Wert".
 * Fehlende Schluessel bedeuten aus (bzw. Weissabgleich nicht anfassen).
 * Gross-/Kleinschreibung egal, # beginnt einen Kommentar.
 *
 *   [C1] Summer Day
 *   film     = Summer 200
 *   wb       = daylight
 *   shift    = R+2 B-1
 *   grain    = fine
 *
 * Die Kamera schreibt die Datei beim Speichern eines Rezepts neu (eigene
 * Kommentare gehen dabei verloren). Keine Bibliotheksfunktionen -> am PC testbar.
 */

#include "filmsim_recipe.h"

typedef struct {
    const char *key;
    const char *words;          // mit | getrennt
    const signed char *vals;    // Wert je Wort, 0 = Index des Worts
} recipe_field_t;

static const signed char vals_wb[] = { RECIPE_WB_KEEP, 0, 1, 2, 3, 4, 5, 6 };
static const signed char vals_nr[] = { 0, 3, 1, 2 };

// Reihenfolge wie RF_*
static const recipe_field_t fields[RF_N] = {
    { "wb",       "keep|auto|daylight|cloudy|tungsten|fluorescent|fluorescent-h|custom", vals_wb },
    { "dynamic",  "off|+1|+2", 0 },
    { "nr",       "off|auto|light|strong", vals_nr },
    { "bwfilter", "off|yellow|orange|red", 0 },
    { "grain",    "off|auto|fine|medium|coarse", 0 },
    { "vignette", "off|light|strong", 0 },
    { "mist",     "off|1/8|1/4", 0 },
    { "halation", "off|on", 0 },
    { "leak",     "off|on", 0 },
    { "date",     "off|on", 0 },
    { "frame",    "off|thin", 0 },
};

static int lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

static char *trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t') s++;
    for (e = s; *e; e++) ;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
    return s;
}

static int same(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (lower(*a) != lower(*b)) return 0;
    return !*a && !*b;
}

static void copy(char *d, const char *s, int max)
{
    int n = 0;
    while (*s && n < max) {
        unsigned char c = *s++;
        d[n++] = (c < ' ' || c > '~') ? '_' : c;    // nur ASCII (Menuefont)
    }
    d[n] = 0;
}

// Wort aus der Liste -> Wert; -100 = unbekannt
static int field_value(const recipe_field_t *f, const char *s)
{
    const char *w = f->words;
    int i = 0;
    while (*w) {
        const char *p = s;
        while (*w && *w != '|' && *p && lower(*p) == lower(*w)) { p++; w++; }
        if (!*p && (!*w || *w == '|')) return f->vals ? f->vals[i] : i;
        while (*w && *w != '|') w++;
        if (*w == '|') w++;
        i++;
    }
    return -100;
}

// Wert -> Wort (fuer recipes_format); unbekannte Werte -> erstes Wort
static const char *field_word(const recipe_field_t *f, int v, int *len)
{
    const char *w = f->words, *best = f->words;
    int i = 0;
    for (;;) {
        int val = f->vals ? f->vals[i] : i;
        if (val == v) { best = w; break; }
        while (*w && *w != '|') w++;
        if (!*w) break;
        w++; i++;
    }
    for (*len = 0; best[*len] && best[*len] != '|'; (*len)++) ;
    return best;
}

// "+2" / "-1" / "0", Bereich -max..max; 0 = ungueltig
static int parse_shift_num(const char **ps, int *out)
{
    const char *s = *ps;
    int neg = 0, v = 0, digits = 0;
    if (*s == '+' || *s == '-') neg = (*s++ == '-');
    for (; *s >= '0' && *s <= '9' && digits < 2; s++, digits++) v = v * 10 + (*s - '0');
    if (!digits || v > RECIPE_SHIFT_MAX) return 0;
    *out = neg ? -v : v;
    *ps = s;
    return 1;
}

// "R+2 B-1", Reihenfolge egal, fehlender Kanal = 0
static int parse_shift(const char *s, recipe_t *r)
{
    int rv = 0, bv = 0, seen = 0;
    while (*s) {
        int c = lower(*s), *dst;
        if (*s == ' ' || *s == '\t' || *s == ',') { s++; continue; }
        if (c == 'r' && !(seen & 1)) { dst = &rv; seen |= 1; }
        else if (c == 'b' && !(seen & 2)) { dst = &bv; seen |= 2; }
        else return 0;
        s++;
        while (*s == ' ') s++;
        if (!parse_shift_num(&s, dst)) return 0;
    }
    r->shift_r = rv;
    r->shift_b = bv;
    return 1;
}

static void log_add(char **lp, char *lend, int line, const char *msg)
{
    char num[8];
    int i = 0, n = line;
    if (!*lp) return;
    do { num[i++] = '0' + n % 10; n /= 10; } while (n && i < 7);
    const char *pre = "Line ";
    while (*pre && *lp < lend) *(*lp)++ = *pre++;
    while (i && *lp < lend) *(*lp)++ = num[--i];
    if (*lp < lend) *(*lp)++ = ':';
    if (*lp < lend) *(*lp)++ = ' ';
    while (*msg && *lp < lend) *(*lp)++ = *msg++;
    if (*lp < lend) *(*lp)++ = '\r';
    if (*lp < lend) *(*lp)++ = '\n';
    **lp = 0;
}

void recipe_clear(recipe_t *r)
{
    int i;
    r->used = 0;
    r->name[0] = 0;
    r->film[0] = 0;
    r->shift_r = r->shift_b = 0;
    for (i = 0; i < RF_N; i++) r->v[i] = 0;
    r->v[RF_WB] = RECIPE_WB_KEEP;
}

int recipes_parse(char *buf, int len, recipe_t *r, char *log, int log_size)
{
    char *p = buf, *end = buf + len, *lp = log, *lend = log ? log + log_size - 1 : 0;
    int line = 0, i, n = 0;
    recipe_t *cur = 0;

    if (log) log[0] = 0;
    buf[len] = 0;
    for (i = 0; i < RECIPE_SLOTS; i++) recipe_clear(&r[i]);

    while (p < end) {
        char *s = p, *e, *c;
        while (p < end && *p != '\n' && *p != '\r') p++;
        if (p < end && *p == '\r' && p + 1 < end && p[1] == '\n') *p++ = 0;
        if (p < end) *p++ = 0;
        line++;
        for (c = s; *c && *c != '#'; c++) ;     // Kommentar abschneiden
        *c = 0;
        s = trim(s);
        if (!*s) continue;

        if (*s == '[') {
            // [C1] Name
            int slot = (lower(s[1]) == 'c' && s[2] >= '1' && s[2] <= '0' + RECIPE_SLOTS && s[3] == ']') ? s[2] - '1' : -1;
            cur = 0;
            if (slot < 0) { log_add(&lp, lend, line, "expected [C1] to [C4]"); continue; }
            if (r[slot].used) { log_add(&lp, lend, line, "slot used twice, ignored"); continue; }
            cur = &r[slot];
            cur->used = 1;
            copy(cur->name, trim(s + 4), RECIPE_NAME_MAX);
            n++;
            continue;
        }
        if (!cur) { log_add(&lp, lend, line, "setting outside of a [C1] block"); continue; }

        for (e = s; *e && *e != '='; e++) ;
        if (!*e) { log_add(&lp, lend, line, "expected key = value"); continue; }
        *e = 0;
        char *key = trim(s), *val = trim(e + 1);

        if (same(key, "film")) {
            if (same(val, "off") || same(val, "-")) cur->film[0] = 0;
            else copy(cur->film, val, FILMS_TXT_NAME_MAX);
            continue;
        }
        if (same(key, "shift")) {
            if (!parse_shift(val, cur)) log_add(&lp, lend, line, "shift: expected e.g. R+2 B-1 (-9 to +9)");
            continue;
        }
        for (i = 0; i < RF_N && !same(key, fields[i].key); i++) ;
        if (i == RF_N) { log_add(&lp, lend, line, "unknown key, ignored"); continue; }
        int v = field_value(&fields[i], val);
        if (v == -100) { log_add(&lp, lend, line, "unknown value, ignored"); continue; }
        cur->v[i] = v;
    }
    return n;
}

//-------------------------------------------------------------------

typedef struct { char *p, *end; int ok; } out_t;

static void put(out_t *o, const char *s, int n)
{
    while (n-- > 0 && *s) {
        if (o->p >= o->end) { o->ok = 0; return; }
        *o->p++ = *s++;
    }
}

static void puts_(out_t *o, const char *s)
{
    put(o, s, 1 << 20);
}

static void put_shift(out_t *o, int v)
{
    char b[3];
    b[0] = v < 0 ? '-' : '+';
    if (v < 0) v = -v;
    b[1] = '0' + v % 10;
    b[2] = 0;
    puts_(o, b);
}

static const char head[] =
    "# EXCCD recipes\r\n"
    "# The camera writes this file when you save a recipe (Options -> Save recipe).\r\n"
    "# You can edit it on the PC. Up to 4 recipes, [C1] to [C4], each with a name\r\n"
    "# of up to 16 characters. A missing line means off.\r\n"
    "#\r\n"
    "# film      a film from the film list, or off\r\n"
    "# wb        keep, auto, daylight, cloudy, tungsten, fluorescent, fluorescent-h, custom\r\n"
    "#           (keep = leave the camera's white balance as it is)\r\n"
    "# shift     white balance shift, R-9..R+9 B-9..B+9\r\n"
    "# dynamic   off, +1, +2\r\n"
    "# nr        off, auto, light, strong\r\n"
    "# bwfilter  off, yellow, orange, red\r\n"
    "# grain     off, auto, fine, medium, coarse\r\n"
    "# vignette  off, light, strong\r\n"
    "# mist      off, 1/8, 1/4\r\n"
    "# halation  off, on\r\n"
    "# leak      off, on\r\n"
    "# date      off, on\r\n"
    "# frame     off, thin\r\n";

int recipes_format(const recipe_t *r, char *out, int size)
{
    out_t o = { out, out + size - 1, 1 };
    int i, k, len;
    puts_(&o, head);
    for (i = 0; i < RECIPE_SLOTS; i++) {
        const recipe_t *c = &r[i];
        char slot[6] = { '[', 'C', (char)('1' + i), ']', ' ', 0 };
        if (!c->used) continue;
        puts_(&o, "\r\n");
        puts_(&o, slot);
        puts_(&o, c->name);
        puts_(&o, "\r\nfilm      = ");
        puts_(&o, c->film[0] ? c->film : "off");
        puts_(&o, "\r\nshift     = R");
        put_shift(&o, c->shift_r);
        puts_(&o, " B");
        put_shift(&o, c->shift_b);
        for (k = 0; k < RF_N; k++) {
            const char *w = field_word(&fields[k], c->v[k], &len);
            puts_(&o, "\r\n");
            const char *key = fields[k].key;
            int pad = 10;
            puts_(&o, key);
            while (*key++) pad--;
            put(&o, "          ", pad);     // Werte untereinander
            puts_(&o, "= ");
            put(&o, w, len);
        }
        puts_(&o, "\r\n");
    }
    *o.p = 0;
    return o.ok ? (int)(o.p - out) : -1;
}
