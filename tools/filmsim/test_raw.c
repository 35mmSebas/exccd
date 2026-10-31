// PC-Test fuer core/filmsim_raw.h (Rohdaten-Codec, 12-Bit-Tonkurven).
// Referenz sind get_raw_pixel/set_raw_pixel aus core/raw.c.
//
//   gcc -Wall -Wextra -I core -DCAM_SENSOR_BITS_PER_PIXEL=10 -o test_raw10 tools/filmsim/test_raw.c
//   gcc -Wall -Wextra -I core -DCAM_SENSOR_BITS_PER_PIXEL=12 -o test_raw12 tools/filmsim/test_raw.c
//
// Geprueft wird:
//   - dec8 liefert jeden Pixel wie get_raw_pixel (12 Bit: >> 2 auf 10 Bit)
//   - enc8 mit unveraenderten Werten aendert kein Byte
//   - enc8 mit neuen Werten schreibt jeden Pixel wie set_raw_pixel
//     (12 Bit: untere zwei Bit des Originals bleiben, 0 bleibt 0)
//   - 12 Bit: Kurventabelle (Stuetzstellen exakt, Identitaet bis 4095, 0 bleibt 0)
//     und curve_row12 gleich Tabellenzugriff je Pixel

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filmsim_raw.h"

#define BPP     CAM_SENSOR_BITS_PER_PIXEL
#define ROWPIX  64
#define ROWLEN  (ROWPIX * BPP / 8)
#define ROWS    4

static unsigned char buf[ROWS * ROWLEN], orig[ROWS * ROWLEN];
static int err = 0;

#define CHECK(c, ...) do { if (!(c)) { if (++err <= 10) { printf(__VA_ARGS__); printf("\n"); } } } while (0)

// --- Referenz aus core/raw.c ------------------------------------------------
static unsigned get_px(const unsigned char *raw, unsigned x, unsigned y)
{
#if BPP == 10
    const unsigned char *addr = raw + y * ROWLEN + (x / 8) * 10;
    switch (x % 8) {
        case 0: return ((0x3fc & (((unsigned short)addr[1]) << 2)) | (addr[0] >> 6));
        case 1: return ((0x3f0 & (((unsigned short)addr[0]) << 4)) | (addr[3] >> 4));
        case 2: return ((0x3c0 & (((unsigned short)addr[3]) << 6)) | (addr[2] >> 2));
        case 3: return ((0x300 & (((unsigned short)addr[2]) << 8)) | (addr[5]));
        case 4: return ((0x3fc & (((unsigned short)addr[4]) << 2)) | (addr[7] >> 6));
        case 5: return ((0x3f0 & (((unsigned short)addr[7]) << 4)) | (addr[6] >> 4));
        case 6: return ((0x3c0 & (((unsigned short)addr[6]) << 6)) | (addr[9] >> 2));
        default: return ((0x300 & (((unsigned short)addr[9]) << 8)) | (addr[8]));
    }
#else
    const unsigned char *addr = raw + y * ROWLEN + (x / 4) * 6;
    switch (x % 4) {
        case 0: return ((unsigned short)(addr[1])        << 4) | (addr[0] >> 4);
        case 1: return ((unsigned short)(addr[0] & 0x0F) << 8) | (addr[3]);
        case 2: return ((unsigned short)(addr[2])        << 4) | (addr[5] >> 4);
        default: return ((unsigned short)(addr[5] & 0x0F) << 8) | (addr[4]);
    }
#endif
}

static void set_px(unsigned char *raw, unsigned x, unsigned y, unsigned short value)
{
#if BPP == 10
    unsigned char *addr = raw + y * ROWLEN + (x / 8) * 10;
    switch (x % 8) {
        case 0: addr[0]=(addr[0]&0x3F)|(value<<6); addr[1]=value>>2;                  break;
        case 1: addr[0]=(addr[0]&0xC0)|(value>>4); addr[3]=(addr[3]&0x0F)|(value<<4); break;
        case 2: addr[2]=(addr[2]&0x03)|(value<<2); addr[3]=(addr[3]&0xF0)|(value>>6); break;
        case 3: addr[2]=(addr[2]&0xFC)|(value>>8); addr[5]=value;                     break;
        case 4: addr[4]=value>>2;                  addr[7]=(addr[7]&0x3F)|(value<<6); break;
        case 5: addr[6]=(addr[6]&0x0F)|(value<<4); addr[7]=(addr[7]&0xC0)|(value>>4); break;
        case 6: addr[6]=(addr[6]&0xF0)|(value>>6); addr[9]=(addr[9]&0x03)|(value<<2); break;
        case 7: addr[8]=value;                     addr[9]=(addr[9]&0xFC)|(value>>8); break;
    }
#else
    unsigned char *addr = raw + y * ROWLEN + (x / 4) * 6;
    switch (x % 4) {
        case 0: addr[0] = (addr[0]&0x0F) | (unsigned char)(value << 4);  addr[1] = (unsigned char)(value >> 4);  break;
        case 1: addr[0] = (addr[0]&0xF0) | (unsigned char)(value >> 8);  addr[3] = (unsigned char)value;         break;
        case 2: addr[2] = (unsigned char)(value >> 4);  addr[5] = (addr[5]&0x0F) | (unsigned char)(value << 4);  break;
        case 3: addr[4] = (unsigned char)value; addr[5] = (addr[5]&0xF0) | (unsigned char)(value >> 8);  break;
    }
#endif
}
// ----------------------------------------------------------------------------

static void test_codec(void)
{
    unsigned char ref[sizeof buf];
    int x, y, k;

    // dec8 = get_raw_pixel, enc8 unveraendert = keine Aenderung
    memcpy(buf, orig, sizeof buf);
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < ROWPIX; x += 8) {
            unsigned char *a = buf + y * ROWLEN + (x / 8) * BPP;
            int v[8];
            dec8(a, v);
            for (k = 0; k < 8; k++)
                CHECK(v[k] == (int)(get_px(orig, x + k, y) >> (BPP - 10)),
                      "dec8 x%d y%d: %d statt %u", x + k, y, v[k], get_px(orig, x + k, y) >> (BPP - 10));
            enc8(a, v);
        }
    CHECK(memcmp(buf, orig, sizeof buf) == 0, "enc8(dec8) aendert Bytes");

    // enc8 neue Werte = set_raw_pixel
    memcpy(buf, orig, sizeof buf);
    memcpy(ref, orig, sizeof ref);
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < ROWPIX; x += 8) {
            unsigned char *a = buf + y * ROWLEN + (x / 8) * BPP;
            int v[8];
            dec8(a, v);
            for (k = 0; k < 8; k++) {
                v[k] = (k == 3 && y == 1) ? 0 : (v[k] * 7 + k + y) & 1023;
                unsigned o = get_px(orig, x + k, y);
                unsigned w = (BPP == 12 && v[k]) ? ((unsigned)v[k] << 2) | (o & 3) : (unsigned)v[k];
                set_px(ref, x + k, y, (unsigned short)w);
            }
            enc8(a, v);
        }
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < ROWPIX; x++)
            CHECK(get_px(buf, x, y) == get_px(ref, x, y),
                  "enc8 x%d y%d: %u statt %u", x, y, get_px(buf, x, y), get_px(ref, x, y));
    CHECK(memcmp(buf, ref, sizeof buf) == 0, "enc8 schreibt andere Bytes als set_raw_pixel");
}

#if BPP == 12
static unsigned short cv[4 * CV_N], lut[4 * 4096];

static void test_curves(void)
{
    int c, i, x, y;

    // Kurve 0: Identitaet, 1..3: verschiedene Steigungen, ab und zu ueber 1023
    for (c = 0; c < 4; c++)
        for (i = 0; i < CV_N; i++)
            cv[c * CV_N + i] = c == 0 ? i : (unsigned short)(i * (c + 2) / 4);
    curve_lut_build(cv, lut);

    for (i = 1; i < 4096; i++)
        CHECK(lut[i] == i, "Identitaet: lut[%d] = %d", i, lut[i]);
    for (c = 0; c < 4; c++) {
        CHECK(lut[c * 4096] == 0, "Kurve %d: 0 bleibt nicht 0", c);
        for (i = 1; i < CV_N; i++) {
            int k = cv[c * CV_N + i] > 1023 ? 1023 : cv[c * CV_N + i];
            int o = 4 * k > 4095 ? 4095 : (4 * k < 1 ? 1 : 4 * k);
            CHECK(lut[c * 4096 + 4 * i] == o, "Kurve %d Stuetzstelle %d: %d statt %d", c, i, lut[c * 4096 + 4 * i], o);
        }
        for (i = 1; i < 4096; i++)
            CHECK(lut[c * 4096 + i] >= 1 && lut[c * 4096 + i] <= 4095, "Kurve %d: lut[%d] = %d", c, i, lut[c * 4096 + i]);
    }

    // curve_row12 = Tabellenzugriff je Pixel (gerade Spalte ce, ungerade co)
    memcpy(buf, orig, sizeof buf);
    for (y = 0; y < ROWS; y++) {
        const unsigned short *ce = lut + ((y & 1) << 1) * 4096;
        curve_row12(buf + y * ROWLEN, ROWPIX / 4, ce, ce + 4096);
    }
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < ROWPIX; x++) {
            int ch = ((y & 1) << 1) | (x & 1);
            unsigned e = lut[ch * 4096 + get_px(orig, x, y)];
            CHECK(get_px(buf, x, y) == e, "curve_row12 x%d y%d: %u statt %u", x, y, get_px(buf, x, y), e);
        }
}
#endif

int main(void)
{
    unsigned i;
    srand(1);
    for (i = 0; i < sizeof orig; i++) orig[i] = (unsigned char)rand();
    // ein paar Defekt-Markierungen (0) und Weiss
    set_px(orig, 5, 0, 0);
    set_px(orig, 6, 2, (1 << BPP) - 1);

    test_codec();
#if BPP == 12
    test_curves();
#endif
    printf("test_raw %d Bit: %s (%d Fehler)\n", BPP, err ? "FEHLER" : "OK", err);
    return err != 0;
}
