/*
 * PC-Test fuer den FILMS.TXT-Parser (core/filmsim_txt.c)
 *
 *   gcc -Wall -Wextra -I core -o test_films_txt tools/filmsim/test_films_txt.c core/filmsim_txt.c
 *   ./test_films_txt CHDK/FILMS/BEISPIEL.TXT
 *
 * Prueft, dass BEISPIEL.TXT genau die eingebaute Tabelle ergibt, und
 * dass fehlerhafte Zeilen sauber uebersprungen werden.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filmsim_txt.h"

// Kopie von sims_builtin[] aus core/filmsim.c (ohne "Aus")
static const filmsim_t expected[] = {
    { "Daylight 100",    MC_OFF,      0,  0,  0,  0,  0,  0,  0,  0,              "DAYLITE",  0 },
    { "Postcard 100",    MC_CUSTOM,   0,  1,  2,  1,  1,  1,  0,  "POSTCRD.CV",   "POSTCRD",  0 },
    { "Blush 100",       MC_CUSTOM,  -1,  0,  1,  0,  0,  0,  1,  "BLUSH.CV",     "BLUSH",    0 },
    { "Journal 400",     MC_CUSTOM,   0,  0, -1, -1, -1,  0,  0,  "JOURNAL.CV",   "JOURNAL",  0 },
    { "Studio 160",      MC_CUSTOM,  -1, -1, -1,  0,  0,  0,  1,  "STUDIO.CV",    "STUDIO",   0 },
    { "Drugstore 200",   MC_CUSTOM,   0,  0, -1,  0,  1, -1,  0,  "DRUGSTR.CV",   "DRUGSTR",  0 },
    { "Scene 500",       MC_CUSTOM,  -2, -1, -2,  0,  0,  0,  0,  "SCENE.CV",     "SCENE",    0 },
    { "Silver 400",      MC_BW,       0,  0,  0,  0,  0,  0,  0,  "SILVER.CV",    "SILVER",   0 },
};
#define NUM_EXPECTED ((int)(sizeof(expected)/sizeof(expected[0])))

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FEHLER %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int str_eq(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

static int sim_eq(const filmsim_t *a, const filmsim_t *b)
{
    return str_eq(a->name, b->name) && a->mycolors == b->mycolors
        && a->contrast == b->contrast && a->sharpness == b->sharpness && a->saturation == b->saturation
        && a->red == b->red && a->green == b->green && a->blue == b->blue && a->skin == b->skin
        && str_eq(a->curve, b->curve) && str_eq(a->label, b->label) && a->mix == b->mix;
}

static void print_sim(const filmsim_t *s)
{
    printf("  %-20s %2d  %2d %2d %2d %2d %2d %2d %2d  %-12s %-8s %d\n", s->name, s->mycolors,
           s->contrast, s->sharpness, s->saturation, s->red, s->green, s->blue, s->skin,
           s->curve ? s->curve : "-", s->label ? s->label : "-", s->mix);
}

static int parse_str(const char *text, filmsim_t *sims, films_txt_str_t *str, int max, char *log)
{
    int len = strlen(text);
    char *buf = malloc(len + 1);
    memcpy(buf, text, len);
    int n = films_txt_parse(buf, len, sims, str, max, log, 1024);
    free(buf);
    return n;
}

static void test_example(const char *path)
{
    filmsim_t sims[FILMS_TXT_MAX];
    films_txt_str_t str[FILMS_TXT_MAX];
    char log[1024];
    FILE *f = fopen(path, "rb");
    if (!f) { printf("FEHLER: %s nicht gefunden\n", path); fails++; return; }
    char *buf = malloc(FILMS_TXT_FILE_MAX + 1);
    int len = fread(buf, 1, FILMS_TXT_FILE_MAX, f);
    fclose(f);
    int n = films_txt_parse(buf, len, sims, str, FILMS_TXT_MAX, log, sizeof(log));
    printf("%s: %d Looks\n", path, n);
    for (int i = 0; i < n; i++) print_sim(&sims[i]);
    CHECK(log[0] == 0, "unerwartete Meldungen:\n%s", log);
    CHECK(n == NUM_EXPECTED, "%d statt %d Looks", n, NUM_EXPECTED);
    for (int i = 0; i < n && i < NUM_EXPECTED; i++)
        CHECK(sim_eq(&sims[i], &expected[i]), "Eintrag %d (%s) weicht von der eingebauten Tabelle ab", i, sims[i].name);
    free(buf);
}

static void test_errors(void)
{
    filmsim_t sims[FILMS_TXT_MAX];
    films_txt_str_t str[FILMS_TXT_MAX];
    char log[1024];
    const char *text =
        "\xEF\xBB\xBF# BOM, CRLF, Kommentare\r\n"
        "\r\n"
        "   # eingerueckter Kommentar\r\n"
        "  Gut 1 | 11 | +2 | -2 | 0 | 0 | 0 | 0 | 0 | look | lasche.flm | 1\r\n"   // 1
        "Ohne Mix | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | sw.cv | SW\r\n"               // 2
        "Ohne Lasche | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | -\r\n"                     // 3
        "Zu wenig | 0 | 0 | 0\r\n"
        "Zu viel | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 0 | 9\r\n"
        "Modus | 12 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 0\r\n"
        "Regler | 11 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | - | - | 0\r\n"
        "Text | 11 | x | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 0\r\n"
        "Kurve lang | 11 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | ABCDEFGH.CV | - | 0\r\n"
        "Kurve Endung | 11 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | ABC.TXT | - | 0\r\n"
        "Lasche lang | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | ABCDEFGHI | 0\r\n"
        "Mix | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 3\r\n"
        " | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 0\r\n"
        "Ein sehr langer Lookname ueber 20 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 0\r\n"  // 4
        "Letzte Zeile ohne Umbruch | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - |";           // 5, Mix leer
    int n = parse_str(text, sims, str, FILMS_TXT_MAX, log);
    printf("\nFehlerfaelle: %d Looks\n", n);
    for (int i = 0; i < n; i++) print_sim(&sims[i]);
    printf("Protokoll:\n%s", log);
    CHECK(n == 5, "%d statt 5 Looks", n);
    if (n == 5) {
        filmsim_t g1 = { "Gut 1", 11, 2, -2, 0, 0, 0, 0, 0, "LOOK.CV", "LASCHE", 1 };
        CHECK(sim_eq(&sims[0], &g1), "Gut 1 falsch");
        CHECK(str_eq(sims[1].curve, "SW.CV") && str_eq(sims[1].label, "SW") && sims[1].mix == 0, "Ohne Mix falsch");
        CHECK(!sims[2].curve && !sims[2].label, "Ohne Lasche falsch");
        CHECK(strlen(sims[3].name) == FILMS_TXT_NAME_MAX, "Name nicht gekuerzt");
        CHECK(str_eq(sims[4].name, "Letzte Zeile ohne Um"), "letzte Zeile falsch: %s", sims[4].name);
    }
    int lines = 0;
    for (const char *p = log; *p; p++) if (*p == '\n') lines++;
    CHECK(lines == 12, "%d statt 12 Meldungen", lines);
}

static void test_limits(void)
{
    filmsim_t sims[FILMS_TXT_MAX];
    films_txt_str_t str[FILMS_TXT_MAX];
    char log[1024], text[4096] = "";
    for (int i = 0; i < FILMS_TXT_MAX + 3; i++)
        sprintf(text + strlen(text), "Look %d | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | - | - | 0\n", i);
    int n = parse_str(text, sims, str, FILMS_TXT_MAX, log);
    printf("\nGrenze: %d Looks, Protokoll: %s", n, log);
    CHECK(n == FILMS_TXT_MAX, "%d statt %d Looks", n, FILMS_TXT_MAX);
    CHECK(strstr(log, "zu viele") != NULL, "keine Meldung bei zu vielen Looks");

    // kleines Protokoll darf nicht ueberlaufen
    char tiny[8];
    char t2[] = "A|99\nB|99\n";
    films_txt_parse(t2, strlen(t2), sims, str, FILMS_TXT_MAX, tiny, sizeof(tiny));
    CHECK(strlen(tiny) == sizeof(tiny) - 1, "Protokoll nicht sauber abgeschnitten: %d", (int)strlen(tiny));

    // leere Datei
    char t3[] = "";
    CHECK(films_txt_parse(t3, 0, sims, str, FILMS_TXT_MAX, 0, 0) == 0, "leere Datei");
}

int main(int argc, char **argv)
{
    test_example(argc > 1 ? argv[1] : "CHDK/FILMS/BEISPIEL.TXT");
    test_errors();
    test_limits();
    printf("\n%s (%d Fehler)\n", fails ? "FEHLGESCHLAGEN" : "OK", fails);
    return fails != 0;
}
