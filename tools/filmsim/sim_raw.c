// Laesst die RAW-Effekte aus core/filmsim_raw.h am PC ueber ein Rohbild laufen,
// mit demselben Code wie auf der Kamera. Aufruf ueber simulate_looks.py raw ...
//
//   gcc -O2 -I core -DCAM_SENSOR_BITS_PER_PIXEL=10 -o sim_raw tools/filmsim/sim_raw.c
//   sim_raw <ein.bin> <aus.bin> <zeilen> <spalten> <korn 0,1%> <rahmen 0/1/2>
//
// ein/aus: je Pixel unsigned short (10 Bit), Zeile fuer Zeile, Bayer R G / G B.

#include <stdio.h>
#include <stdlib.h>
#include "filmsim_raw.h"

#define WB_R 486
#define WB_B 371
#define BLK  31

static void die(const char *m) { fprintf(stderr, "%s\n", m); exit(1); }

int main(int argc, char **argv)
{
    if (argc < 7) die("Parameter fehlen");
    int rows = atoi(argv[3]), rowpix = atoi(argv[4]), pct10 = atoi(argv[5]), style = atoi(argv[6]);
    int rowlen = rowpix * CAM_SENSOR_BITS_PER_PIXEL / 8, x, y, g;
    unsigned short *px = malloc(rows * rowpix * 2);
    unsigned char *raw = calloc(rows, rowlen);
    FILE *f = fopen(argv[1], "rb");
    if (!f || !px || !raw) die("Eingabe");
    if (fread(px, 2, rows * rowpix, f) != (size_t)(rows * rowpix)) die("Eingabe zu kurz");
    fclose(f);
    for (y = 0; y < rows; y++)
        for (g = 0; g < rowpix / 8; g++) {
            int v[8];
            for (x = 0; x < 8; x++) v[x] = px[y * rowpix + g * 8 + x];
            enc8(raw + y * rowlen + g * CAM_SENSOR_BITS_PER_PIXEL, v);
        }
    if (pct10) {
        short amp[GRAIN_AMP_N];
        short *up = malloc(rowpix);
        grain_amp_build(amp, pct10);
        grain_apply(raw, rows, rowpix, rowlen, 0, 65536 / WB_R, 65536 / WB_B, amp, up, 12345);
        free(up);
    }
    if (style) {                        // Filmrahmen (nur noch Thin)
        static const short black[4] = { BLK, BLK, BLK, BLK };
        frame_draw(raw, rowlen, 0, 0, 0, rowpix, rows, black);
    }
    for (y = 0; y < rows; y++)
        for (g = 0; g < rowpix / 8; g++) {
            int v[8];
            dec8(raw + y * rowlen + g * CAM_SENSOR_BITS_PER_PIXEL, v);
            for (x = 0; x < 8; x++) px[y * rowpix + g * 8 + x] = (unsigned short)v[x];
        }
    f = fopen(argv[2], "wb");
    if (!f) die("Ausgabe");
    fwrite(px, 2, rows * rowpix, f);
    fclose(f);
    return 0;
}
