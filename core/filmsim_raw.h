#ifndef FILMSIM_RAW_H
#define FILMSIM_RAW_H

// Rohdaten-Codec und 12-Bit-Tonkurven von EXCCD, ohne CHDK-Abhaengigkeiten.
// Eingebunden von core/filmsim.c und vom PC-Test tools/filmsim/test_raw.c
// (dort einmal mit CAM_SENSOR_BITS_PER_PIXEL=10 und einmal mit 12 gebaut).
//
// dec8/enc8: 8 Pixel einer Zeile <-> int[8] auf der 10-Bit-Skala.

#if CAM_SENSOR_BITS_PER_PIXEL == 12
// 4 Pixel in 6 Byte (Anordnung wie get_raw_pixel/set_raw_pixel in raw.c)
static inline __attribute__((always_inline)) void dec4_12(const unsigned char *a, int *v)
{
    v[0] = ((a[1] << 4) | (a[0] >> 4)) >> 2;
    v[1] = (((a[0] & 0x0f) << 8) | a[3]) >> 2;
    v[2] = ((a[2] << 4) | (a[5] >> 4)) >> 2;
    v[3] = (((a[5] & 0x0f) << 8) | a[4]) >> 2;
}
static inline __attribute__((always_inline)) void enc4_12(unsigned char *a, const int *v)
{
    // untere zwei Bit aus dem Original uebernehmen (0 bleibt 0: Defekt-Markierung)
    int w0 = v[0] ? (v[0] << 2) | ((a[0] >> 4) & 3) : 0;
    int w1 = v[1] ? (v[1] << 2) | (a[3] & 3) : 0;
    int w2 = v[2] ? (v[2] << 2) | ((a[5] >> 4) & 3) : 0;
    int w3 = v[3] ? (v[3] << 2) | (a[4] & 3) : 0;
    a[1] = w0 >> 4;
    a[0] = (w0 << 4) | ((w1 >> 8) & 0x0f);
    a[3] = w1;
    a[2] = w2 >> 4;
    a[5] = (w2 << 4) | ((w3 >> 8) & 0x0f);
    a[4] = w3;
}
static inline __attribute__((always_inline)) void dec8(const unsigned char *a, int *v)
{
    dec4_12(a, v); dec4_12(a + 6, v + 4);
}
static inline __attribute__((always_inline)) void enc8(unsigned char *a, const int *v)
{
    enc4_12(a, v); enc4_12(a + 6, v + 4);
}
#else
static inline __attribute__((always_inline)) void dec8(const unsigned char *a, int *v)
{
    v[0] = (a[1] << 2) | (a[0] >> 6);
    v[1] = ((a[0] & 0x3f) << 4) | (a[3] >> 4);
    v[2] = ((a[3] & 0x0f) << 6) | (a[2] >> 2);
    v[3] = ((a[2] & 0x03) << 8) | a[5];
    v[4] = (a[4] << 2) | (a[7] >> 6);
    v[5] = ((a[7] & 0x3f) << 4) | (a[6] >> 4);
    v[6] = ((a[6] & 0x0f) << 6) | (a[9] >> 2);
    v[7] = ((a[9] & 0x03) << 8) | a[8];
}
static inline __attribute__((always_inline)) void enc8(unsigned char *a, const int *v)
{
    a[1] = v[0] >> 2;
    a[0] = (v[0] << 6) | (v[1] >> 4);
    a[3] = (v[1] << 4) | (v[2] >> 6);
    a[2] = (v[2] << 2) | (v[3] >> 8);
    a[5] = v[3];
    a[4] = v[4] >> 2;
    a[7] = (v[4] << 6) | (v[5] >> 4);
    a[6] = (v[5] << 4) | (v[6] >> 6);
    a[9] = (v[6] << 2) | (v[7] >> 8);
    a[8] = v[7];
}
#endif
static inline __attribute__((always_inline)) int clip10(int x) { return x < 0 ? 0 : (x > 1023 ? 1023 : x); }

// Heisse Pixel
//   Canon ersetzt defekte Pixel erst bei der JPEG-Erzeugung, und nur die aus
//   seiner Werksliste (in den Rohdaten mit 0 markiert). Andere heisse Pixel
//   bleiben im Rohbild. Dynamik+ und Kurven heben sie mit den Schatten an,
//   Kanalmischung und Farbrauschfilter verteilen sie auf die Nachbarn.
//   Deshalb vor allen anderen Schritten: Ein Pixel, der ueber Schwarz mehr
//   als doppelt so hell ist wie alle 8 gleichfarbigen Nachbarn (Abstand 2)
//   plus HP_MIN, wird durch den Mittelwert der 4 direkten gleichfarbigen
//   Nachbarn ersetzt. Echte Lichtpunkte sind durch das Objektiv breiter als
//   ein Pixel: Sie heben auch die direkt angrenzenden (andersfarbigen) Pixel
//   ueber deren eigene Umgebung - dann bleibt der Pixel, wie er ist.
#define HP_BLK 31
#define HP_MIN 16                       // 10-Bit-Skala

// m deutlich heller (1,5-fach ueber Schwarz) als die gleichfarbigen Pixel a und b?
static inline __attribute__((always_inline)) int hp_raised(int m, int a, int b)
{
    int r = (a > b ? a : b) - HP_BLK;
    if (r < 0) r = 0;
    return m - HP_BLK > r + (r >> 1) + HP_MIN;
}

static void hp_row(const unsigned char *a, int groups, short *out)
{
    int g, k, v[8];
    for (g = 0; g < groups; g++, a += CAM_SENSOR_BITS_PER_PIXEL, out += 8) {
        dec8(a, v);
        for (k = 0; k < 8; k++) out[k] = v[k];
    }
}

// ring: 5 * (rowpix / 8) * 8 shorts. Rueckgabe: Anzahl ersetzter Pixel.
__attribute__((optimize("O2")))
static int hotpix_fix(unsigned char *raw, int rows, int rowpix, int rowlen, short *ring)
{
    static const short recip[5] = { 0, 256, 128, 85, 64 };
    int groups = rowpix / 8, w = groups * 8, y, x, n = 0;
#define HP_ROW(r) (ring + ((r) % 5) * w)
    for (y = 0; y < 4 && y < rows; y++) hp_row(raw + y * rowlen, groups, HP_ROW(y));
    for (y = 2; y < rows - 2; y++) {
        hp_row(raw + (y + 2) * rowlen, groups, HP_ROW(y + 2));
        const short *u = HP_ROW(y - 2), *c = HP_ROW(y), *d = HP_ROW(y + 2);
        for (x = 2; x < w - 2; x++) {
            int v = c[x];
            if (v <= HP_BLK + HP_MIN) continue;
            int lim = ((v - HP_BLK - HP_MIN) >> 1) + HP_BLK;
            if (c[x-2] >= lim || c[x+2] >= lim || u[x] >= lim || d[x] >= lim) continue;
            if (u[x-2] >= lim || u[x+2] >= lim || d[x-2] >= lim || d[x+2] >= lim) continue;
            // echter Lichtpunkt? Direkte Nachbarn mit ihren eigenen Nachbarn quer dazu vergleichen
            const short *u1 = HP_ROW(y - 1), *d1 = HP_ROW(y + 1);
            if (hp_raised(c[x-1], u[x-1], d[x-1]) || hp_raised(c[x+1], u[x+1], d[x+1]) ||
                hp_raised(u1[x], u1[x-2], u1[x+2]) || hp_raised(d1[x], d1[x-2], d1[x+2])) continue;
            // Mittelwert der direkten Nachbarn, Defekt-Markierungen (0) auslassen
            int s = 0, k = 0;
            if (c[x-2]) { s += c[x-2]; k++; }
            if (c[x+2]) { s += c[x+2]; k++; }
            if (u[x])   { s += u[x];   k++; }
            if (d[x])   { s += d[x];   k++; }
            if (!k) continue;
            s = (s * recip[k]) >> 8;
            unsigned char *a = raw + y * rowlen + (x >> 3) * CAM_SENSOR_BITS_PER_PIXEL;
            int p[8];
            dec8(a, p);
            p[x & 7] = s ? s : 1;
            enc8(a, p);
            n++;
        }
    }
#undef HP_ROW
    return n;
}

#if CAM_SENSOR_BITS_PER_PIXEL == 12
#define CV_N 1024

// .CV (4 x 1024 Werte, 10 Bit) -> Tabelle mit 4 x 4096 Eintraegen
static void curve_lut_build(const unsigned short *cv, unsigned short *lut)
{
    int c, v;
    for (c = 0; c < 4; c++) {
        const unsigned short *k = cv + c * CV_N;
        unsigned short *t = lut + c * 4096;
        for (v = 0; v < 4096; v++) {
            int i = v >> 2, f = v & 3;
            int k0 = k[i] > 1023 ? 1023 : k[i];
            // letzte Stuetzstelle: Steigung fortsetzen (4093..4095)
            int k1 = (i < CV_N - 1) ? (k[i + 1] > 1023 ? 1023 : k[i + 1]) : 2 * k0 - (k[i - 1] > 1023 ? 1023 : k[i - 1]);
            int o = k0 * (4 - f) + k1 * f;          // = 4 * Interpolation
            t[v] = v ? (o < 1 ? 1 : (o > 4095 ? 4095 : o)) : 0;   // 0 = Defekt-Markierung
        }
    }
}

// Kurve auf eine Zeile anwenden: ce fuer gerade Spalten, co fuer ungerade
static inline __attribute__((always_inline)) void curve_row12(unsigned char *a, int quads,
                                                              const unsigned short *ce, const unsigned short *co)
{
    int g;
    for (g = 0; g < quads; g++, a += 6) {
        int w0 = ce[(a[1] << 4) | (a[0] >> 4)];
        int w1 = co[((a[0] & 0x0f) << 8) | a[3]];
        int w2 = ce[(a[2] << 4) | (a[5] >> 4)];
        int w3 = co[((a[5] & 0x0f) << 8) | a[4]];
        a[1] = w0 >> 4;
        a[0] = (w0 << 4) | (w1 >> 8);
        a[3] = w1;
        a[2] = w2 >> 4;
        a[5] = (w2 << 4) | (w3 >> 8);
        a[4] = w3;
    }
}
#endif

#endif
