#ifndef FILMSIM_RAW_H
#define FILMSIM_RAW_H

// Rohdaten-Codec und 12-Bit-Tonkurven von EXCCD, ohne CHDK-Abhaengigkeiten.
// Eingebunden von core/filmsim.c und vom PC-Test tools/filmsim/test_raw.c
// (dort einmal mit CAM_SENSOR_BITS_PER_PIXEL=10 und einmal mit 12 gebaut).
//
// dec8/enc8: 8 Pixel einer Zeile <-> int[8] auf der 10-Bit-Skala.

// Beide lesen und schreiben halbwortweise (16 Bit, Little Endian) statt byteweise:
// halb so viele Speicherzugriffe, das Ergebnis ist bitgleich (test_raw.c prueft es
// gegen get_raw_pixel/set_raw_pixel). Voraussetzung: gerade Adressen, also ein
// gerades raw_rowlen. Das gilt fuer alle Kameras von EXCCD (filmsim.c prueft es
// beim Bauen, sonst gaebe es auf dem ARM falsche Werte statt eines Fehlers).
#if CAM_SENSOR_BITS_PER_PIXEL == 12
// 4 Pixel in 6 Byte (Anordnung wie get_raw_pixel/set_raw_pixel in raw.c)
static inline __attribute__((always_inline)) void dec4_12(const unsigned char *a, int *v)
{
    const unsigned short *h = (const unsigned short *)a;
    unsigned h0 = h[0], h1 = h[1], h2 = h[2];
    v[0] = h0 >> 6;
    v[1] = (((h0 & 0x0f) << 8) | (h1 >> 8)) >> 2;
    v[2] = (((h1 & 0xff) << 4) | (h2 >> 12)) >> 2;
    v[3] = (h2 & 0xfff) >> 2;
}
static inline __attribute__((always_inline)) void enc4_12(unsigned char *a, const int *v)
{
    // untere zwei Bit aus dem Original uebernehmen (0 bleibt 0: Defekt-Markierung)
    unsigned short *h = (unsigned short *)a;
    unsigned h0 = h[0], h1 = h[1], h2 = h[2];
    unsigned w0 = v[0] ? (v[0] << 2) | ((h0 >> 4) & 3) : 0;
    unsigned w1 = v[1] ? (v[1] << 2) | ((h1 >> 8) & 3) : 0;
    unsigned w2 = v[2] ? (v[2] << 2) | ((h2 >> 12) & 3) : 0;
    unsigned w3 = v[3] ? (v[3] << 2) | (h2 & 3) : 0;
    h[0] = (unsigned short)((w0 << 4) | (w1 >> 8));
    h[1] = (unsigned short)((w1 << 8) | (w2 >> 4));
    h[2] = (unsigned short)((w2 << 12) | w3);
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
    const unsigned short *h = (const unsigned short *)a;
    unsigned h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4];
    v[0] = h0 >> 6;
    v[1] = ((h0 & 0x3f) << 4) | (h1 >> 12);
    v[2] = (h1 >> 2) & 0x3ff;
    v[3] = ((h1 & 3) << 8) | (h2 >> 8);
    v[4] = ((h2 & 0xff) << 2) | (h3 >> 14);
    v[5] = (h3 >> 4) & 0x3ff;
    v[6] = ((h3 & 0x0f) << 6) | (h4 >> 10);
    v[7] = h4 & 0x3ff;
}
static inline __attribute__((always_inline)) void enc8(unsigned char *a, const int *v)
{
    unsigned short *h = (unsigned short *)a;
    h[0] = (unsigned short)((v[0] << 6) | (v[1] >> 4));
    h[1] = (unsigned short)((v[1] << 12) | (v[2] << 2) | (v[3] >> 8));
    h[2] = (unsigned short)((v[3] << 8) | (v[4] >> 2));
    h[3] = (unsigned short)((v[4] << 14) | (v[5] << 4) | (v[6] >> 6));
    h[4] = (unsigned short)((v[6] << 10) | v[7]);
}
#endif
static inline __attribute__((always_inline)) int clip10(int x) { return x < 0 ? 0 : (x > 1023 ? 1023 : x); }

// Kanalmischung eines 2x2-Blocks (R G / G B), siehe filmsim_raw_apply.
//   Faktoren *256, Zeilensummen = 256, damit Grau grau bleibt.
//   Violet: Gruen -> Violett, Blau -> Gruen/Tuerkis, Gelb -> Pink
//   Concrete: entsaettigt, Blau/Gruen stark gedaempft, Haut bleibt
static const short mix_violet[3][3] = {
    { 128, 128,   0 },   // R' = 0.5 R + 0.5 G
    {  51,  77, 128 },   // G' = 0.2 R + 0.3 G + 0.5 B
    {  51, 205,   0 },   // B' = 0.2 R + 0.8 G
};
static const short mix_concrete[3][3] = {
    { 192,  51,  13 },
    {  64, 154,  38 },
    {  51, 102, 103 },
};
// Turquoise und Infra sind an echten Fotos der SX110 abgestimmt (tools/filmsim/fit_mix.py, 07.11.2026):
// Kurve und Mischung wie hier, dazu Canons Verarbeitung, nachgebildet aus Paaren von DNG und Kamera-JPEG.
// Ziel je Farbton, Rauschverstaerkung je Zeile hoechstens 1,5. Die alten, am PC im fertigen Bild
// entworfenen Werte wirkten auf der Kamera ganz anders (Turquoise kobaltblau, Infra hellgruen).
// Turquoise: Gruen und Warmes -> Tuerkis, blauer Himmel -> warm
static const short mix_turquoise[3][3] = {
    {  10, -25, 271 },
    { 140, -54, 170 },
    { 190,-201, 267 },
};
// Infra: wie Falschfarben-Infrarotfilm: Gruen -> Rot, Rot -> Orange, Himmel bleibt blau
static const short mix_infra[3][3] = {
    { 285,-167, 138 },
    { 123,-182, 315 },
    { 100,-147, 303 },
};
// Cyano: einfarbig Blau wie eine Cyanotypie. Die Zeilensummen sind hier bewusst
// nicht 256: Grau wird blau, das ist der Look.
static const short mix_cyano[3][3] = {
    {  46,  90,  16 },
    {  69, 134,  23 },
    {  84, 166,  28 },
};

// Kehrwert des Weissabgleichs fuer mix_block: Rohwert = (n * inv + 32768) >> 16
#define FS_WBINV(wb) ((1 << 24) / (wb))

// Gerundet statt abgeschnitten: Abschneiden senkt R und B staerker als G, das
// gibt einen Gruenstich in den Schatten. G wird als Summe beider Gruenpixel
// gerechnet (gg = 2 * Mittel), damit das Halbieren keinen Fehler einbringt.
static inline __attribute__((always_inline)) void mix_block(const short (*m)[3], int *p, int *q, int k, int blk,
                                                            int wb_r, int wb_b, int inv_r, int inv_b)
{
    // Canon markiert defekte Sensorpixel in den Rohdaten mit 0 und ersetzt sie
    // erst bei der JPEG-Erzeugung. Wuerden wir den Block mischen, waere die
    // Markierung weg -> schwarzer Punkt. Solche Bloecke bleiben unveraendert.
    if (!p[k] || !p[k+1] || !q[k] || !q[k+1]) return;
    int r  = ((p[k]   - blk) * wb_r + 128) >> 8;
    int g1 =   p[k+1] - blk;
    int g2 =   q[k]   - blk;
    int b  = ((q[k+1] - blk) * wb_b + 128) >> 8;
    int gg = g1 + g2;
    int nr = (2 * m[0][0] * r + m[0][1] * gg + 2 * m[0][2] * b + 256) >> 9;
    int dg = (2 * m[1][0] * r + (m[1][1] - 256) * gg + 2 * m[1][2] * b + 256) >> 9;   // Gruen-Struktur erhalten
    int nb = (2 * m[2][0] * r + m[2][1] * gg + 2 * m[2][2] * b + 256) >> 9;
    p[k]   = clip10(((nr * inv_r + 32768) >> 16) + blk);
    p[k+1] = clip10(g1 + dg + blk);
    q[k]   = clip10(g2 + dg + blk);
    q[k+1] = clip10(((nb * inv_b + 32768) >> 16) + blk);
    // keine neuen "Defekt"-Markierungen erzeugen
    if (!p[k]) p[k] = 1;
    if (!p[k+1]) p[k+1] = 1;
    if (!q[k]) q[k] = 1;
    if (!q[k+1]) q[k+1] = 1;
}

// Weissabgleich nachtraeglich: R und B ueber Schwarz mit fr / fb (*4096) multiplizieren.
// Gruen und die Defekt-Markierungen (0) bleiben. row0 = 1 bei G B / R G (GBRG).
__attribute__((noinline))
static void wb_rescale(unsigned char *raw, int rows, int rowpix, int rowlen, int row0, int blk, int fr, int fb)
{
    int y, g, k, groups = rowpix / 8;
    for (y = 0; y < rows; y++) {
        int blue = (y ^ row0) & 1, f = blue ? fb : fr;
        unsigned char *a = raw + y * rowlen;
        if (f == 4096) continue;
        for (g = 0; g < groups; g++, a += CAM_SENSOR_BITS_PER_PIXEL) {
            int v[8];
            dec8(a, v);
            for (k = blue; k < 8; k += 2) {
                if (!v[k]) continue;
                int n = clip10(blk + (((v[k] - blk) * f + 2048) >> 12));
                v[k] = n ? n : 1;
            }
            enc8(a, v);
        }
    }
}

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
//   Ausnahme: Gruppen defekter Pixel (2-3 nebeneinander, gemessen an der SX110).
//   Dort sind nur Nachbarn auf einer oder zwei Seiten hell, bei einem echten
//   hellen Lichtpunkt auch die gegenueberliegenden. Ab HP_STRONG ueber Schwarz
//   zaehlt deshalb nur noch ein Anstieg an mindestens 3 von 4 Nachbarn als Licht.
#define HP_BLK 31
#define HP_MIN 16                       // 10-Bit-Skala
#define HP_STRONG 256

// m deutlich heller (1,5-fach ueber Schwarz) als die gleichfarbigen Pixel a und b?
static inline __attribute__((always_inline)) int hp_raised(int m, int a, int b)
{
    int r = (a > b ? a : b) - HP_BLK;
    if (r < 0) r = 0;
    return m - HP_BLK > r + (r >> 1) + HP_MIN;
}

// Ab diesem Wert gilt ein Nachbar als hell genug: Dann ist v kein heisser Pixel.
#define HP_LIM(v) ((((v) - HP_BLK - HP_MIN) >> 1) + HP_BLK)

// Nur fuer die seltenen Verdachtsfaelle: bewusst nicht eingebettet, spart Code
__attribute__((noinline))
static int hp_get(const unsigned char *raw, int rowlen, int x, int y)
{
    int v[8];
    dec8(raw + y * rowlen + (x >> 3) * CAM_SENSOR_BITS_PER_PIXEL, v);
    return v[x & 7];
}

// Vollstaendige Pruefung eines Verdachtsfalls. Rueckgabe: Ersatzwert oder -1.
__attribute__((noinline))
static int hp_check(const unsigned char *raw, int rowlen, int x, int y, int v)
{
    static const short recip[5] = { 0, 256, 128, 85, 64 };
#define HP_PX(dx, dy) hp_get(raw, rowlen, x + (dx), y + (dy))
    int lim = HP_LIM(v);
    int cl = HP_PX(-2, 0), cr = HP_PX(2, 0), un = HP_PX(0, -2), dn = HP_PX(0, 2);
    if (cl >= lim || cr >= lim || un >= lim || dn >= lim) return -1;
    if (HP_PX(-2, -2) >= lim || HP_PX(2, -2) >= lim || HP_PX(-2, 2) >= lim || HP_PX(2, 2) >= lim) return -1;
    // echter Lichtpunkt? Direkte Nachbarn mit ihren eigenen Nachbarn quer dazu vergleichen
    int nl = HP_PX(-1, 0), nr = HP_PX(1, 0), nu = HP_PX(0, -1), nd = HP_PX(0, 1);
    int raised = hp_raised(nl, HP_PX(-1, -2), HP_PX(-1, 2)) + hp_raised(nr, HP_PX(1, -2), HP_PX(1, 2)) +
                 hp_raised(nu, HP_PX(-2, -1), HP_PX(2, -1)) + hp_raised(nd, HP_PX(-2, 1), HP_PX(2, 1));
    if (raised) {
        int peak = v;
        if (nl > peak) peak = nl;
        if (nr > peak) peak = nr;
        if (nu > peak) peak = nu;
        if (nd > peak) peak = nd;
        if (raised >= 3 || peak - HP_BLK < HP_STRONG) return -1;
    }
#undef HP_PX
    // Mittelwert der direkten Nachbarn, Defekt-Markierungen (0) auslassen
    int s = 0, k = 0;
    if (cl) { s += cl; k++; }
    if (cr) { s += cr; k++; }
    if (un) { s += un; k++; }
    if (dn) { s += dn; k++; }
    if (!k) return -1;
    s = (s * recip[k]) >> 8;
    return s ? s : 1;
}

// Gemerkte Ersetzungen einer Zeile schreiben (je Eintrag: x, Wert)
static void hp_flush(unsigned char *raw, int rowlen, int y, const short *q, int cnt)
{
    int i;
    for (i = 0; i < cnt; i++) {
        unsigned char *a = raw + y * rowlen + (q[2 * i] >> 3) * CAM_SENSOR_BITS_PER_PIXEL;
        int p[8];
        dec8(a, p);
        p[q[2 * i] & 7] = q[2 * i + 1];
        enc8(a, p);
    }
}

// Puffer fuer die Ersetzungen von 3 Zeilen. Gleichfarbige ersetzte Pixel liegen nie
// im Abstand 2 (jeder muesste mehr als doppelt so hell sein wie der andere), also
// hoechstens rowpix/2 + 2 Eintraege je Zeile zu je 2 shorts.
#define HP_BUF_SHORTS(rowpix) (3 * ((rowpix) + 4))

// Vortest je 8-Pixel-Gruppe, ohne Zeilenpuffer: Je zwei gleichfarbige Pixel im
// Abstand 2 werden verglichen. Ist der hellere nicht deutlich heller als der
// dunklere, scheiden beide aus; sonst kann nur der hellere heiss sein. Nur diese
// seltenen Faelle lesen die Nachbarzeilen. Ersetzungen werden erst geschrieben,
// wenn keine spaetere Pruefung die Zeile mehr liest: Alle Vergleiche sehen so die
// Originalwerte. Rueckgabe: Anzahl ersetzter Pixel.
__attribute__((optimize("O2")))
static int hotpix_fix(unsigned char *raw, int rows, int rowpix, int rowlen, short *buf)
{
    int groups = rowpix / 8, w = groups * 8, cap = rowpix + 4, y, g, n = 0;
    int cnt[3] = { 0, 0, 0 };
    for (y = 2; y < rows - 2; y++) {
        int slot = y % 3;
        short *q = buf + slot * cap;
        hp_flush(raw, rowlen, y - 3, q, cnt[slot]);      // Zeile y - 3 wird nicht mehr gelesen
        int c = 0;
        const unsigned char *a = raw + y * rowlen;
        for (g = 0; g < groups; g++, a += CAM_SENSOR_BITS_PER_PIXEL) {
            int v[8];
            dec8(a, v);
#define HP_PAIR(i, j) do { \
                int hi = v[i], lo = v[j], x = g * 8 + (i); \
                if (hi < lo) { hi = v[j]; lo = v[i]; x = g * 8 + (j); } \
                if (hi > HP_BLK + HP_MIN && lo < HP_LIM(hi) && x >= 2 && x < w - 2) { \
                    int s = hp_check(raw, rowlen, x, y, hi); \
                    if (s >= 0 && c < cap / 2) { q[2 * c] = (short)x; q[2 * c + 1] = (short)s; c++; n++; } \
                } \
            } while (0)
            HP_PAIR(0, 2);
            HP_PAIR(1, 3);
            HP_PAIR(4, 6);
            HP_PAIR(5, 7);
#undef HP_PAIR
        }
        cnt[slot] = c;
    }
    for (; y < rows + 1; y++)                            // die letzten drei Zeilen
        if (y >= 5) hp_flush(raw, rowlen, y - 3, buf + (y % 3) * cap, cnt[y % 3]);
    return n;
}

// Filmkorn
//   Je 2x2-Bayer-Block ein Zufallswert: Alle vier Pixel aendern sich um denselben
//   Betrag (R und B auf den Weissabgleich umgerechnet). Das Korn ist also einfarbig
//   und mindestens 2 Pixel gross, bunte Flecken kann es nicht geben.
//   Der Zufallswert folgt n = 3/8 * (links + oben) + e: Benachbarte Bloecke haengen
//   zusammen, das gibt Kornklumpen statt feinem Rauschen. Es wiederholt sich nichts,
//   anders als bei einer Textur, die auf dem Himmel als Muster auffiele.
//   Die Staerke kommt aus einer Tabelle je Helligkeit (grain_amp_build): im Rohbild
//   (linear) gleich viel Korn wie im fertigen Bild (Gamma), am staerksten in den
//   Mitten, in tiefem Schwarz und reinem Weiss keines.
//
// Stuetzstellen alle 32 Rohwerte, Korn von 1 % im Bild in Rohwerten * 16, siehe
// tools/filmsim/make_curves.py: grain_knots() (Gamma 2.2, Schwarz 31, Weiss 1023).
static const short grain_knots[33] = {
    0, 3, 40, 66, 89, 108, 126, 141, 154, 166, 175, 183, 190, 195, 198, 200, 200, 199, 196, 192,
    186, 179, 170, 160, 148, 135, 120, 103, 86, 66, 45, 23, 0
};
#define GRAIN_AMP_N 1024

// Standardabweichung von n: gemessen im PC-Test, rund 180. pct10 = Kornstaerke im Bild in 0,1 %.
static void grain_amp_build(short *amp, int pct10)
{
    int v;
    for (v = 0; v < GRAIN_AMP_N; v++) {
        int i = v >> 5, f = v & 31;
        int k1 = (i < 32) ? grain_knots[i + 1] : grain_knots[32];
        int k = (grain_knots[i] * (32 - f) + k1 * f) >> 5;
        // delta = (n * amp) >> 12 mit n ~ 180: amp = k / 16 * pct10 / 10 * 4096 / 180
        amp[v] = v <= HP_BLK ? 0 : (short)((k * pct10 * 582) >> 12);
    }
}

// up: nb Eintraege (Bloecke je Zeile), wird hier genullt.
// ir, ib: Kehrwert des Weissabgleichs * 256 (65536 / wb).
__attribute__((optimize("O2")))
static void grain_apply(unsigned char *raw, int rows, int rowpix, int rowlen, int row0,
                        int ir, int ib, const short *amp, short *up, unsigned int seed)
{
    int groups = rowpix / 8, nb = groups * 4, y, g, k, i;
    unsigned int rnd = seed | 1;
    for (i = 0; i < nb; i++) up[i] = 0;
    for (y = row0; y + 1 < rows; y += 2) {
        unsigned char *a0 = raw + y * rowlen, *a1 = a0 + rowlen;
        int left = 0;
        i = 0;
        for (g = 0; g < groups; g++, a0 += CAM_SENSOR_BITS_PER_PIXEL, a1 += CAM_SENSOR_BITS_PER_PIXEL) {
            int p[8], q[8];
            dec8(a0, p); dec8(a1, q);
            for (k = 0; k < 8; k += 2, i++) {
                rnd = rnd * 1664525u + 1013904223u;
                int e = (int)((rnd >> 24) + ((rnd >> 16) & 255) + ((rnd >> 8) & 255)) - 382;
                int n = (((left + up[i]) * 3 + 4) >> 3) + e;
                left = n;
                up[i] = (short)n;
                if (!p[k] || !p[k+1] || !q[k] || !q[k+1]) continue;      // Defekt-Markierungen bleiben
                int d = (n * amp[p[k+1]] + 2048) >> 12;
                int v;
                v = clip10(p[k]   + ((d * ir + 128) >> 8)); p[k]   = v ? v : 1;
                v = clip10(p[k+1] + d);               p[k+1] = v ? v : 1;
                v = clip10(q[k]   + d);               q[k]   = v ? v : 1;
                v = clip10(q[k+1] + ((d * ib + 128) >> 8)); q[k+1] = v ? v : 1;
            }
            enc8(a0, p); enc8(a1, q);
        }
    }
}

// Filmrahmen
//   Rechteck im Rohbild fuellen. col: Rohwerte je Bayer-Farbe (R, G der R-Zeile,
//   G der B-Zeile, B). Defekt-Markierungen (0) bleiben, Canon ersetzt sie durch
//   die Nachbarn - die sind hier gleich hell.
static void raw_fill(unsigned char *raw, int rowlen, int row0, int x0, int y0, int x1, int y1, const short *col)
{
    int y, g, j;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    for (y = y0; y < y1; y++) {
        int odd = (y ^ row0) & 1;
        for (g = x0 >> 3; g <= (x1 - 1) >> 3; g++) {
            unsigned char *a = raw + y * rowlen + g * CAM_SENSOR_BITS_PER_PIXEL;
            int v[8];
            dec8(a, v);
            for (j = 0; j < 8; j++) {
                int x = g * 8 + j;
                if (x >= x0 && x < x1 && v[j]) v[j] = col[(odd << 1) | (x & 1)];
            }
            enc8(a, v);
        }
    }
}

// Schrift 5 x 7 fuer den Filmrand: A-Z, 0-9, Leerzeichen, - und . Je Zeichen 5 Spalten.
static const unsigned char frame_font[][5] = {
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},
    {0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
};

static int frame_glyph(int c)
{
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + c - '0';
    if (c == '-') return 36;
    if (c == '.') return 37;
    return -1;
}

// Text mit Pixelgroesse sz ab (x, y) schreiben, Rueckgabe: Breite in Rohpixeln
static int frame_text(unsigned char *raw, int rowlen, int row0, int x, int y, int sz, const char *s, const short *col)
{
    int x0 = x;
    for (; *s; s++, x += 6 * sz) {
        int g = frame_glyph(*s), cx, cy;
        if (g < 0) continue;
        for (cx = 0; cx < 5; cx++)
            for (cy = 0; cy < 7; cy++)
                if (frame_font[g][cx] & (1 << cy))
                    raw_fill(raw, rowlen, row0, x + cx * sz, y + cy * sz, x + (cx + 1) * sz, y + (cy + 1) * sz, col);
    }
    return x - x0;
}

#define FRAME_THIN 1
#define FRAME_FILM 2

// Raender (in Rohpixeln) fuer ein Bild der Hoehe jh: Film beschneidet 4:3 auf etwa
// 3:2 wie ein Kleinbild-Negativ.
static inline int frame_border_v(int style, int jh) { return style == FRAME_FILM ? jh / 11 : jh / 48; }
static inline int frame_border_h(int jh) { return jh / 48; }

// Rahmen zeichnen. black, hole, ink: Rohwerte je Bayer-Farbe; left/right: Randbeschriftung
// (nur FRAME_FILM). jx..jh: sichtbarer Bildausschnitt.
static void frame_draw(unsigned char *raw, int rowlen, int row0, int jx, int jy, int jw, int jh, int style,
                       const short *black, const short *hole, const short *ink, const char *left, const char *right)
{
    int bv = frame_border_v(style, jh), bh = frame_border_h(jh);
    int x1 = jx + jw, y1 = jy + jh, k;
    raw_fill(raw, rowlen, row0, jx, jy, x1, jy + bv, black);
    raw_fill(raw, rowlen, row0, jx, y1 - bv, x1, y1, black);
    raw_fill(raw, rowlen, row0, jx, jy + bv, jx + bh, y1 - bv, black);
    raw_fill(raw, rowlen, row0, x1 - bh, jy + bv, x1, y1 - bv, black);
    if (style != FRAME_FILM) return;

    // Perforation aussen am Rand, Beschriftung innen, wie auf dem Negativ
    int pitch = jw / 28, hw = pitch * 11 / 20, hh = bv * 9 / 20, my = bv / 14;
    int sz = bv / 28;
    if (sz < 1) sz = 1;
    for (k = 0; k < 28; k++) {
        int hx = jx + k * pitch + (pitch - hw) / 2;
        raw_fill(raw, rowlen, row0, hx, jy + my, hx + hw, jy + my + hh, hole);
        raw_fill(raw, rowlen, row0, hx, y1 - my - hh, hx + hw, y1 - my, hole);
    }
    int ty = y1 - bv + sz;                       // zwischen Bild und Perforation
    if (left) frame_text(raw, rowlen, row0, jx + 3 * pitch, ty, sz, left, ink);
    if (right) {
        int n = 0;
        while (right[n]) n++;
        frame_text(raw, rowlen, row0, x1 - 3 * pitch - n * 6 * sz, ty, sz, right, ink);
    }
}

#if CAM_SENSOR_BITS_PER_PIXEL == 12
#define CV_N 1024

// .CV (4 x 1024 Werte, 10 Bit: R, G, G, B) -> Tabelle mit 3 x 4096 Eintraegen
// in der Reihenfolge R, G, B. Beide G-Kurven sind gleich (make_curves.py prueft
// das), die zweite wird nicht gelesen. Eine R-Zeile nutzt die Tabellen R + G,
// eine B-Zeile G + B: ce = lut + (B-Zeile ? 4096 : 0), co = ce + 4096.
static void curve_lut_build(const unsigned short *cv, unsigned short *lut)
{
    static const unsigned char src[3] = { 0, 1, 3 };
    int c, v;
    for (c = 0; c < 3; c++) {
        const unsigned short *k = cv + src[c] * CV_N;
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

// Dasselbe wortweise: 8 Pixel = 12 Byte = 3 Worte, deutlich weniger Speicherzugriffe.
// a muss auf 4 Byte ausgerichtet sein, Little Endian (alle CHDK-Kameras).
static inline __attribute__((always_inline)) void curve_row12_w(unsigned int *a, int groups,
                                                                const unsigned short *ce, const unsigned short *co)
{
    int g;
    for (g = 0; g < groups; g++, a += 3) {
        unsigned int w0 = a[0], w1 = a[1], w2 = a[2];
        unsigned int v0 = ce[(w0 & 0xffff) >> 4];
        unsigned int v1 = co[((w0 & 0x0f) << 8) | (w0 >> 24)];
        unsigned int v2 = ce[((w0 >> 12) & 0xff0) | ((w1 & 0xffff) >> 12)];
        unsigned int v3 = co[w1 & 0xfff];
        unsigned int v4 = ce[w1 >> 20];
        unsigned int v5 = co[((w1 >> 8) & 0xf00) | ((w2 >> 8) & 0xff)];
        unsigned int v6 = ce[((w2 & 0xff) << 4) | (w2 >> 28)];
        unsigned int v7 = co[(w2 >> 16) & 0xfff];
        a[0] = (v0 << 4) | (v1 >> 8) | ((v2 >> 4) << 16) | ((v1 & 0xff) << 24);
        a[1] = ((v2 & 0x0f) << 12) | v3 | (((v4 << 4) | (v5 >> 8)) << 16);
        a[2] = ((v5 & 0xff) << 8) | (v6 >> 4) | ((((v6 & 0x0f) << 12) | v7) << 16);
    }
}
#endif

#endif
