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
//     und curve_row12 gleich Tabellenzugriff je Pixel, curve_row12_w byte-gleich dazu
//   - mix_block: Grau bleibt ohne Rundungsstich, Defekt-Bloecke bleiben unveraendert

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filmsim_raw.h"
#include "filmsim_dev.h"

#define BPP     CAM_SENSOR_BITS_PER_PIXEL
#define ROWPIX  64
#define ROWLEN  (ROWPIX * BPP / 8)
#define ROWS    4

static unsigned char buf[ROWS * ROWLEN] __attribute__((aligned(4))), orig[ROWS * ROWLEN] __attribute__((aligned(4)));
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
    static unsigned char ref[sizeof buf] __attribute__((aligned(4)));
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
static unsigned short cv[4 * CV_N], lut[3 * 4096];

static void test_curves(void)
{
    static const int src[3] = { 0, 1, 3 };
    static unsigned char bufw[sizeof buf] __attribute__((aligned(4)));
    int c, i, x, y;

    // Kurve 0: Identitaet, 1 und 3: verschiedene Steigungen, ab und zu ueber 1023.
    // Kurve 2 ist die zweite G-Kurve und darf nicht gelesen werden.
    for (c = 0; c < 4; c++)
        for (i = 0; i < CV_N; i++)
            cv[c * CV_N + i] = c == 0 ? i : (c == 2 ? 7 : (unsigned short)(i * (c + 2) / 4));
    curve_lut_build(cv, lut);

    for (i = 1; i < 4096; i++)
        CHECK(lut[i] == i, "Identitaet: lut[%d] = %d", i, lut[i]);
    for (c = 0; c < 3; c++) {
        CHECK(lut[c * 4096] == 0, "Kurve %d: 0 bleibt nicht 0", c);
        for (i = 1; i < CV_N; i++) {
            int k = cv[src[c] * CV_N + i] > 1023 ? 1023 : cv[src[c] * CV_N + i];
            int o = 4 * k > 4095 ? 4095 : (4 * k < 1 ? 1 : 4 * k);
            CHECK(lut[c * 4096 + 4 * i] == o, "Kurve %d Stuetzstelle %d: %d statt %d", c, i, lut[c * 4096 + 4 * i], o);
        }
        for (i = 1; i < 4096; i++)
            CHECK(lut[c * 4096 + i] >= 1 && lut[c * 4096 + i] <= 4095, "Kurve %d: lut[%d] = %d", c, i, lut[c * 4096 + i]);
    }

    // curve_row12 = Tabellenzugriff je Pixel (gerade Spalte ce, ungerade co),
    // Tabellen R + G in der R-Zeile, G + B in der B-Zeile
    memcpy(buf, orig, sizeof buf);
    for (y = 0; y < ROWS; y++) {
        const unsigned short *ce = lut + (y & 1) * 4096;
        curve_row12(buf + y * ROWLEN, ROWPIX / 4, ce, ce + 4096);
    }
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < ROWPIX; x++) {
            int ch = (y & 1) + (x & 1);
            unsigned e = lut[ch * 4096 + get_px(orig, x, y)];
            CHECK(get_px(buf, x, y) == e, "curve_row12 x%d y%d: %u statt %u", x, y, get_px(buf, x, y), e);
        }

    // curve_row12_w: byte-gleich zu curve_row12
    memcpy(bufw, orig, sizeof bufw);
    for (y = 0; y < ROWS; y++) {
        const unsigned short *ce = lut + (y & 1) * 4096;
        curve_row12_w((unsigned int *)(void *)(bufw + y * ROWLEN), ROWPIX / 8, ce, ce + 4096);
    }
    CHECK(memcmp(bufw, buf, sizeof buf) == 0, "curve_row12_w schreibt andere Bytes als curve_row12");
}
#endif

// --- Kanalmischung -----------------------------------------------------------
// Grau darf keinen Stich durch die Rundung bekommen: abgeglichene Abweichung je
// Kanal, gemittelt ueber verrauschte Bloecke, gegen die exakte Mischung derselben
// Rohwerte. Dazu Defekt-Markierungen.
static void test_mix(void)
{
    enum { WR = 486, WB = 371, BK = 31, N = 200000 };
    static const short (*const ms[2])[3] = { mix_violet, mix_concrete };
    static const int lv[] = { 8, 32, 128, 400 };
    int mi, li, i;
    for (mi = 0; mi < 2; mi++)
        for (li = 0; li < 4; li++) {
            long sr = 0, sg = 0, sb = 0;
            for (i = 0; i < N; i++) {
                int p[2], q[2];
                // Dreiecksrauschen +-3 um den Grauwert, gerundet
                #define NZ ((rand() % 7) + (rand() % 7) - 6)
                p[0] = BK + (lv[li] * 256 + WR / 2) / WR + NZ;
                p[1] = BK + lv[li] + NZ;
                q[0] = BK + lv[li] + NZ;
                q[1] = BK + (lv[li] * 256 + WB / 2) / WB + NZ;
                #undef NZ
                mix_block(ms[mi], p, q, 0, BK, WR, WB, FS_WBINV(WR), FS_WBINV(WB));
                sr += p[0] - BK; sg += p[1] - BK; sb += q[1] - BK;
            }
            double dr = (double)sr / N * WR / 256 - lv[li], dg = (double)sg / N - lv[li], db = (double)sb / N * WB / 256 - lv[li];
            // Die Rohwerte sind selbst auf ganze Zahlen gerundet, darum nicht gegen lv pruefen
            double r0 = (double)((lv[li] * 256 + WR / 2) / WR) * WR / 256, b0 = (double)((lv[li] * 256 + WB / 2) / WB) * WB / 256;
            double er = (ms[mi][0][0] * r0 + ms[mi][0][1] * lv[li] + ms[mi][0][2] * b0) / 256 - lv[li];
            double eb = (ms[mi][2][0] * r0 + ms[mi][2][1] * lv[li] + ms[mi][2][2] * b0) / 256 - lv[li];
            double eg = (ms[mi][1][0] * r0 + ms[mi][1][1] * lv[li] + ms[mi][1][2] * b0) / 256 - lv[li];
            CHECK(dr - er > -0.6 && dr - er < 0.6 && dg - eg > -0.2 && dg - eg < 0.2 && db - eb > -0.6 && db - eb < 0.6,
                  "mix %d Grau %d: Bias R %+.2f G %+.2f B %+.2f", mi, lv[li], dr - er, dg - eg, db - eb);
        }
    // Defekt-Markierung: Block bleibt unveraendert
    {
        int p[2] = { 0, 200 }, q[2] = { 200, 200 };
        mix_block(mix_violet, p, q, 0, BK, WR, WB, FS_WBINV(WR), FS_WBINV(WB));
        CHECK(p[0] == 0 && p[1] == 200 && q[0] == 200 && q[1] == 200, "mix: Defekt-Block veraendert");
    }
}

// --- heisse Pixel ------------------------------------------------------------
#define HROWS 16
static unsigned char hbuf[HROWS * ROWLEN] __attribute__((aligned(4)));
static int hin[HROWS][ROWPIX];
static short hring[HP_BUF_SHORTS(ROWPIX)];

// Grundbild: links dunkel, rechts hell, Farben wie unter Tageslicht, leichtes Rauschen
static int hflat(int x, int y)
{
    static const int f[4] = { 60, 100, 100, 75 };      // R G / G B in %
    int base = x < 32 ? 45 : 400;
    return HP_BLK + (base - HP_BLK) * f[((y & 1) << 1) | (x & 1)] / 100 + (x * 7 + y * 13) % 5 - 2;
}

static void test_hotpix(void)
{
    static const int hot[][2] = { {10, 5}, {11, 8}, {50, 6}, {14, 11},
                                  {4, 3}, {5, 3}, {26, 4}, {27, 4}, {27, 5} };
    const int nhot = sizeof hot / sizeof hot[0];
    int x, y, i;
    for (y = 0; y < HROWS; y++)
        for (x = 0; x < ROWPIX; x++) hin[y][x] = hflat(x, y);
    hin[5][10] = 400; hin[8][11] = 250;                // dunkel: R, G
    hin[6][50] = 1023;                                 // hell, voll gesaettigt
    hin[9][14] = 0; hin[11][14] = 300;                 // Defekt-Markierung + heisser Nachbar
    hin[3][4] = 1023; hin[3][5] = 800;                 // defekte Gruppe: zwei nebeneinander
    hin[4][26] = 1023; hin[4][27] = 150; hin[5][27] = 700; // Gruppe als L, mit schwachem Pixel
    hin[13][6] = 120; hin[13][7] = 110;                // schwache Gruppe: bleibt (koennte Licht sein)
    for (y = 10; y <= 12; y++)                         // echter Lichtpunkt 3 x 3
        for (x = 20; x <= 22; x++) hin[y][x] = 600;
    for (i = 0; i <= 8; i++) hin[2 + i][40 + i] = 900; // Diagonale, 1 Pixel breit
    for (x = 24; x <= 30; x++) hin[13][x] = 700;       // waagrechte Linie, 1 Pixel breit
    for (y = 0; y < HROWS; y++)
        for (x = 0; x < ROWPIX; x++) set_px(hbuf, x, y, (unsigned short)(hin[y][x] << (BPP - 10)));

    int n = hotpix_fix(hbuf, HROWS, ROWPIX, ROWLEN, hring);
    CHECK(n == nhot, "hotpix: %d statt %d Pixel ersetzt", n, nhot);
    for (y = 0; y < HROWS; y++)
        for (x = 0; x < ROWPIX; x++) {
            int v = (int)(get_px(hbuf, x, y) >> (BPP - 10)), is_hot = 0;
            for (i = 0; i < nhot; i++) is_hot |= hot[i][0] == x && hot[i][1] == y;
            if (is_hot) {
                int e = hflat(x, y), dv = v - e;
                CHECK(dv >= -4 && dv <= 4, "hotpix x%d y%d: %d statt ca. %d", x, y, v, e);
            } else
                CHECK(v == hin[y][x], "hotpix x%d y%d: veraendert %d -> %d", x, y, hin[y][x], v);
        }
}

// Unabhaengige Referenz: die Regel direkt auf dem Originalbild. Der Filter muss
// bei jedem Bild dieselben Pixel gleich ersetzen.
static int ref_raised(int m, int a, int b)
{
    int r = (a > b ? a : b) - HP_BLK;
    if (r < 0) r = 0;
    return m - HP_BLK > r + (r >> 1) + HP_MIN;
}

static void test_hotpix_ref(void)
{
    static const short recip[5] = { 0, 256, 128, 85, 64 };
    static int im[HROWS][ROWPIX], ex[HROWS][ROWPIX];
    int run, x, y, total = 0;
    for (run = 0; run < 400; run++) {
        // abwechselnd dunkel/hell/gemischt, wenige bis sehr viele heisse Pixel, Defekte, Weiss
        int base = run % 4 == 0 ? 36 : (run % 4 == 1 ? 60 : (run % 4 == 2 ? 300 : 0));
        int hot = (run % 5 == 4) ? 3 : 40 >> (run % 5);
        for (y = 0; y < HROWS; y++)
            for (x = 0; x < ROWPIX; x++) {
                int b = base ? base : (rand() % 2 ? 40 : 500);
                int v = b + rand() % 7 - 3;
                if (rand() % 100 < hot) v = 100 + rand() % 924;
                if (rand() % 500 == 0) v = 0;
                if (rand() % 500 == 0) v = 1023;
                im[y][x] = v;
                ex[y][x] = v;
                set_px(hbuf, x, y, (unsigned short)(v << (BPP - 10)));
            }
        int nref = 0;
        for (y = 2; y < HROWS - 2; y++)
            for (x = 2; x < ROWPIX - 2; x++) {
                int v = im[y][x], lim, s = 0, k = 0;
                if (v <= HP_BLK + HP_MIN) continue;
                lim = ((v - HP_BLK - HP_MIN) >> 1) + HP_BLK;
                if (im[y][x-2] >= lim || im[y][x+2] >= lim || im[y-2][x] >= lim || im[y+2][x] >= lim) continue;
                if (im[y-2][x-2] >= lim || im[y-2][x+2] >= lim || im[y+2][x-2] >= lim || im[y+2][x+2] >= lim) continue;
                int up = ref_raised(im[y][x-1], im[y-2][x-1], im[y+2][x-1]) + ref_raised(im[y][x+1], im[y-2][x+1], im[y+2][x+1]) +
                         ref_raised(im[y-1][x], im[y-1][x-2], im[y-1][x+2]) + ref_raised(im[y+1][x], im[y+1][x-2], im[y+1][x+2]);
                int pk = v;
                if (im[y][x-1] > pk) pk = im[y][x-1];
                if (im[y][x+1] > pk) pk = im[y][x+1];
                if (im[y-1][x] > pk) pk = im[y-1][x];
                if (im[y+1][x] > pk) pk = im[y+1][x];
                if (up && (up >= 3 || pk - HP_BLK < HP_STRONG)) continue;
                if (im[y][x-2]) { s += im[y][x-2]; k++; }
                if (im[y][x+2]) { s += im[y][x+2]; k++; }
                if (im[y-2][x]) { s += im[y-2][x]; k++; }
                if (im[y+2][x]) { s += im[y+2][x]; k++; }
                if (!k) continue;
                s = (s * recip[k]) >> 8;
                ex[y][x] = s ? s : 1;
                nref++;
            }
        int n = hotpix_fix(hbuf, HROWS, ROWPIX, ROWLEN, hring);
        total += n;
        CHECK(n == nref, "hotpix Referenz, Lauf %d: %d statt %d ersetzt", run, n, nref);
        for (y = 0; y < HROWS; y++)
            for (x = 0; x < ROWPIX; x++) {
                int v = (int)(get_px(hbuf, x, y) >> (BPP - 10));
                CHECK(v == ex[y][x], "hotpix Referenz, Lauf %d x%d y%d: %d statt %d", run, x, y, v, ex[y][x]);
            }
    }
    CHECK(total > 400, "hotpix Referenz: nur %d Ersetzungen, Test zu schwach", total);
}

// --- Filmkorn --------------------------------------------------------------
#define get_px_rows get_px
#define set_px_rows set_px
#define GROWS 256
static unsigned char gbuf[GROWS * ROWLEN] __attribute__((aligned(4)));

static void test_grain(void)
{
    static short amp[GRAIN_AMP_N], up[ROWPIX];
    const int ir = 65536 / 486, ib = 65536 / 371;
    int x, y, level;
    grain_amp_build(amp, 20);                // 2 %
    CHECK(amp[0] == 0 && amp[31] == 0 && amp[1023] == 0, "Korn in Schwarz/Weiss: %d %d %d", amp[0], amp[31], amp[1023]);
    // Flaechen verschiedener Helligkeit: Mittelwert bleibt, Korn ist einfarbig und passt zur Staerke
    static const int levels[] = { 60, 150, 300, 500, 800 };
    double sd_mid = 0;
    for (level = 0; level < 5; level++) {
        int gv = levels[level], rv = 31 + (gv - 31) * 256 / 486, bv = 31 + (gv - 31) * 256 / 371;
        for (y = 0; y < GROWS; y++)
            for (x = 0; x < ROWPIX; x++)
                set_px_rows(gbuf, x, y, (unsigned short)(((y & 1) ? ((x & 1) ? bv : gv) : ((x & 1) ? gv : rv)) << (BPP - 10)));
        grain_apply(gbuf, GROWS, ROWPIX, ROWLEN, 0, ir, ib, amp, up, 4711);
        double sum = 0, sq = 0, cross = 0;
        int n = 0;
        for (y = 0; y + 1 < GROWS; y += 2)
            for (x = 0; x + 1 < ROWPIX; x += 2) {
                int g1 = (int)(get_px_rows(gbuf, x + 1, y) >> (BPP - 10)) - gv;
                int g2 = (int)(get_px_rows(gbuf, x, y + 1) >> (BPP - 10)) - gv;
                int r = (int)(get_px_rows(gbuf, x, y) >> (BPP - 10)) - rv;
                CHECK(g1 == g2, "Korn: beide Gruenpixel gleich, %d != %d", g1, g2);
                // Rot folgt Gruen (gleiche Richtung, nach Weissabgleich gleich gross)
                CHECK(g1 * r >= 0 || abs(g1) <= 2, "Korn: Rot gegenlaeufig %d %d", g1, r);
                sum += g1; sq += g1 * g1; n++;
                if (x + 3 < ROWPIX)
                    cross += g1 * ((int)(get_px_rows(gbuf, x + 3, y) >> (BPP - 10)) - gv);
            }
        double mean = sum / n, sd = sq / n;
        { double t = sd; double r = t > 0 ? t : 1; int i; for (i = 0; i < 30; i++) r = 0.5 * (r + t / r); sd = r; }
        double corr = cross / n / (sd * sd + 1e-9);
        printf("  Korn: G=%d Mittel %+.2f, Streuung %.1f, Nachbar-Korrelation %.2f\n", gv, mean, sd, corr);
        CHECK(mean > -1.0 && mean < 1.0, "Korn: Mittelwert verschoben %.2f bei %d", mean, gv);
        CHECK(sd > 0.5, "Korn: zu schwach %.2f bei %d", sd, gv);
        CHECK(corr > 0.25, "Korn: keine Klumpen, Korrelation %.2f", corr);
        if (gv == 300) sd_mid = sd;
    }
    // Soll bei G=300 (Anzeige .55): 2 % sind etwa 0,02 * knots/.01/16 Rohwerte
    int k = grain_knots[300 >> 5];
    double soll = k / 16.0 * 2.0;
    CHECK(sd_mid > soll * 0.6 && sd_mid < soll * 1.4, "Korn: Staerke %.1f statt etwa %.1f", sd_mid, soll);

    // Defekt-Markierungen bleiben, nichts wird neu 0, gleicher Startwert = gleiches Bild
    for (y = 0; y < GROWS; y++)
        for (x = 0; x < ROWPIX; x++) set_px_rows(gbuf, x, y, (unsigned short)(40 << (BPP - 10)));
    set_px_rows(gbuf, 6, 8, 0);
    grain_apply(gbuf, GROWS, ROWPIX, ROWLEN, 0, ir, ib, amp, up, 7);
    for (y = 0; y < GROWS; y++)
        for (x = 0; x < ROWPIX; x++) {
            unsigned v = get_px_rows(gbuf, x, y);
            if (x == 6 && y == 8) CHECK(v == 0, "Korn: Defekt-Markierung weg");
            else CHECK(v != 0, "Korn: neue 0 bei x%d y%d", x, y);
        }
}

// --- Filmrahmen ------------------------------------------------------------
static void test_frame(void)
{
    static const short black[4] = { 31, 31, 31, 31 };
    int x, y;
    // Ausschnitt mit Abstand zum Rand: darf nichts ausserhalb anfassen
    int jx = 8, jy = 4, jw = 48, jh = GROWS - 8;
    int b = frame_border(jh);
    CHECK(b >= 1, "Rahmen zu schmal fuer den Test: %d", b);
    for (y = 0; y < GROWS; y++)
        for (x = 0; x < ROWPIX; x++) set_px_rows(gbuf, x, y, (unsigned short)(600 << (BPP - 10)));
    set_px_rows(gbuf, jx + 2, jy + 1, 0);
    frame_draw(gbuf, ROWLEN, 0, jx, jy, jw, jh, black);
    for (y = 0; y < GROWS; y++)
        for (x = 0; x < ROWPIX; x++) {
            int v = (int)(get_px_rows(gbuf, x, y) >> (BPP - 10));
            int in_img = x >= jx && x < jx + jw && y >= jy && y < jy + jh;
            int in_pic = x >= jx + b && x < jx + jw - b && y >= jy + b && y < jy + jh - b;
            if (!in_img || in_pic) CHECK(v == 600, "Rahmen: x%d y%d veraendert (%d)", x, y, v);
            else if (x == jx + 2 && y == jy + 1) CHECK(v == 0, "Rahmen: Defekt-Markierung weg");
            else CHECK(v == 31, "Rahmen: x%d y%d nicht schwarz (%d)", x, y, v);
        }
}

// Weissabgleich nachtraeglich: Faktor, Bayer-Lage, Gruen und Defekte bleiben
static void test_wb(void)
{
    int row0, x, y;
    CHECK(wb_factor(2000, 1000, 1000, 1000) == 8192, "wb_factor doppelt: %d", wb_factor(2000, 1000, 1000, 1000));
    CHECK(wb_factor(1500, 1000, 3000, 1000) == 2048, "wb_factor halb: %d", wb_factor(1500, 1000, 3000, 1000));
    CHECK(wb_factor(1500, 1024, 1500, 1024) == WB_ONE, "wb_factor gleich");
    CHECK(wb_factor(1502, 1000, 1500, 1000) == WB_ONE, "wb_factor unter 0,5 %%");
    CHECK(wb_factor(0, 1000, 1500, 1000) == WB_ONE && wb_factor(1500, 1000, 0, 0) == WB_ONE, "wb_factor unbekannt");
    CHECK(wb_factor(60000, 100, 100, 60000) == WB_ONE * 4, "wb_factor Grenze: %d", wb_factor(60000, 100, 100, 60000));
    // grosse Werte laufen nicht ueber
    CHECK(abs(wb_factor(60000, 60000, 30000, 60000) - 8192) < 40, "wb_factor gross: %d", wb_factor(60000, 60000, 30000, 60000));

    for (row0 = 0; row0 < 2; row0++) {
        for (y = 0; y < ROWS; y++)
            for (x = 0; x < ROWPIX; x++)
                set_px(buf, x, y, (unsigned short)(400 << (BPP - 10)));
        set_px(buf, 8, row0, 0);                                // Defekt auf einem R-Pixel
        set_px(buf, 10, row0, (unsigned short)(900 << (BPP - 10)));   // laeuft ueber
        wb_rescale(buf, ROWS, ROWPIX, ROWLEN, row0, 31, 8192, 2048);
        for (y = 0; y < ROWS; y++)
            for (x = 0; x < ROWPIX; x++) {
                int rrow = ((y ^ row0) & 1) == 0;
                int want = 400;
                if (rrow && !(x & 1)) want = 31 + 369 * 2;      // R
                if (!rrow && (x & 1)) want = 31 + 185;          // B, gerundet
                if (y == row0 && x == 8) want = 0;
                if (y == row0 && x == 10) want = 1023;
                CHECK((int)(get_px(buf, x, y) >> (BPP - 10)) == want, "wb_rescale row0 %d x%d y%d: %d statt %d",
                      row0, x, y, (int)(get_px(buf, x, y) >> (BPP - 10)), want);
            }
    }
}

int main(void)
{
    unsigned i;
    srand(1);
    for (i = 0; i < sizeof orig; i++) orig[i] = (unsigned char)rand();
    // ein paar Defekt-Markierungen (0) und Weiss
    set_px(orig, 5, 0, 0);
    set_px(orig, 6, 2, (1 << BPP) - 1);

    test_codec();
    test_hotpix();
    test_hotpix_ref();
    test_mix();
    test_grain();
    test_frame();
    test_wb();
#if BPP == 12
    test_curves();
#endif
    printf("test_raw %d Bit: %s (%d Fehler)\n", BPP, err ? "FEHLER" : "OK", err);
    return err != 0;
}
