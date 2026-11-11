/*
 * PC-Test fuer den RECIPES.TXT-Parser (core/filmsim_recipe.c)
 *
 *   gcc -Wall -Wextra -I core -o test_recipes tools/filmsim/test_recipes.c core/filmsim_recipe.c
 *   ./test_recipes CHDK/FILMS/RECIPES.TXT
 *
 * Prueft ein Beispiel mit Fehlern, dann Schreiben -> Lesen ohne Verlust,
 * und dass die mitgelieferte Datei (Argument) fehlerfrei ist.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filmsim_recipe.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static const char sample[] =
    "# comment\r\n"
    "[C2] Summer Day   # trailing comment\r\n"
    "film = summer 200\r\n"
    "WB = Daylight\r\n"
    "shift = B-1 R+2\r\n"
    "grain = fine\r\n"
    "nr = auto\r\n"
    "dynamic = +2\r\n"
    "mist = 1/4\r\n"
    "date = on\r\n"
    "bogus = 1\r\n"
    "grain = huge\r\n"
    "\n"
    "[C1]\n"
    "film = off\n"
    "shift = R+10\n"
    "[C2] twice\n"
    "[C7] nope\n"
    "wb = keep\n"
    "[C4] Tungsten night with a very long name\n"
    "film = Tungsten 800\n"
    "wb = fluorescent-h\n"
    "frame = thin\n";

static int same_recipe(const recipe_t *a, const recipe_t *b)
{
    return a->used == b->used && !strcmp(a->name, b->name) && !strcmp(a->film, b->film)
        && a->shift_r == b->shift_r && a->shift_b == b->shift_b && !memcmp(a->v, b->v, sizeof(a->v));
}

int main(int argc, char **argv)
{
    recipe_t r[RECIPE_SLOTS], r2[RECIPE_SLOTS];
    char buf[4096], log[1024], out[RECIPE_FILE_MAX];
    int n, i;

    strcpy(buf, sample);
    n = recipes_parse(buf, strlen(sample), r, log, sizeof(log));
    printf("%s", log);
    CHECK(n == 3);
    CHECK(r[0].used && r[0].name[0] == 0 && r[0].film[0] == 0 && r[0].v[RF_WB] == RECIPE_WB_KEEP);
    CHECK(r[0].shift_r == 0);                       // R+10 abgelehnt
    CHECK(r[1].used && !strcmp(r[1].name, "Summer Day") && !strcmp(r[1].film, "summer 200"));
    CHECK(r[1].v[RF_WB] == RECIPE_WB_DAYLIGHT && r[1].shift_r == 2 && r[1].shift_b == -1);
    CHECK(r[1].v[RF_GRAIN] == 2 && r[1].v[RF_NR] == 3 && r[1].v[RF_DR] == 2);
    CHECK(r[1].v[RF_MIST] == 2 && r[1].v[RF_DATE] == 1 && r[1].v[RF_HALATION] == 0);
    CHECK(!r[2].used);
    CHECK(r[3].used && strlen(r[3].name) == RECIPE_NAME_MAX && r[3].v[RF_WB] == RECIPE_WB_FLUORESCENT_H);
    CHECK(r[3].v[RF_FRAME] == 1);
    CHECK(strstr(log, "Line 11:") && strstr(log, "Line 12:") && strstr(log, "Line 16:"));
    CHECK(strstr(log, "Line 17:") && strstr(log, "Line 18:") && strstr(log, "Line 19:"));

    // Schreiben -> Lesen
    int len = recipes_format(r, out, sizeof(out));
    CHECK(len > 0);
    n = recipes_parse(out, len, r2, log, sizeof(log));
    CHECK(n == 3 && log[0] == 0);
    for (i = 0; i < RECIPE_SLOTS; i++) CHECK(same_recipe(&r[i], &r2[i]));
    CHECK(recipes_format(r, out, 100) == -1);       // zu klein

    // Alle Werte jeder Stufe einmal
    recipe_t all[RECIPE_SLOTS];
    for (i = 0; i < RECIPE_SLOTS; i++) recipe_clear(&all[i]);
    all[0].used = 1; strcpy(all[0].name, "A");
    all[0].v[RF_NR] = 1; all[0].v[RF_WB] = RECIPE_WB_CUSTOM; all[0].shift_r = -9; all[0].shift_b = 9;
    all[0].v[RF_BWFILTER] = 3; all[0].v[RF_VIGNETTE] = 2; all[0].v[RF_LEAK] = 1; all[0].v[RF_HALATION] = 1;
    len = recipes_format(all, out, sizeof(out));
    n = recipes_parse(out, len, r2, log, sizeof(log));
    CHECK(n == 1 && log[0] == 0 && same_recipe(&all[0], &r2[0]));

    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        CHECK(f != NULL);
        if (f) {
            len = fread(out, 1, sizeof(out) - 1, f);
            fclose(f);
            n = recipes_parse(out, len, r, log, sizeof(log));
            printf("%s: %d recipes\n%s", argv[1], n, log);
            CHECK(n > 0 && log[0] == 0);
        }
    }

    printf(fails ? "%d FAILED\n" : "all tests passed\n", fails);
    return fails != 0;
}
