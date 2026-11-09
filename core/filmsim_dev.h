#ifndef FILMSIM_DEV_H
#define FILMSIM_DEV_H

// DNG-Kopf lesen fuer "RAW entwickeln", ohne CHDK-Abhaengigkeiten.
// Eingebunden von core/filmsim.c und vom PC-Test tools/filmsim/test_raw.c.
//
// Aufbau einer CHDK-DNG (modules/dng.c): TIFF, Little Endian, IFD0 ab Byte 8 mit Weissabgleich
// (AsShotNeutral), Verweisen auf das Hauptbild (SubIFD) und auf die Exif-Daten. Die Rohdaten
// liegen ganz am Dateiende. Darum darf hinten nichts angehaengt werden: Die Dynamik+-Stufe
// steht im Kopf, im sonst leeren Eintrag ImageDescription ("DR1", "DR2").

#define DNG_HEAD_MAX 8192               // so viel liest die Kamera, um den Kopf auszuwerten

typedef struct {
    int wb_r, wb_g, wb_b;               // Canons Weissabgleich (Faktoren), 0 = unbekannt
    int iso;                            // 0 = unbekannt
    int dr;                             // Dynamik+-Stufe der Aufnahme
    int year, mon, day;                 // Aufnahmedatum, year 0 = unbekannt
} dng_meta_t;

#define DNG_TAG_DESCRIPTION 0x10E

// Nicht jede Einheit (Kamera, PC-Tests) braucht jede Funktion
#define DNG_MAYBE_UNUSED __attribute__((unused))

static unsigned dng_u16(const unsigned char *b) { return b[0] | ((unsigned)b[1] << 8); }
static unsigned dng_u32(const unsigned char *b) { return dng_u16(b) | (dng_u16(b + 2) << 16); }

// Position des 12-Byte-Eintrags in dem IFD ab Byte ifd, -1 = nicht gefunden
static int dng_entry(const unsigned char *b, int len, unsigned ifd, unsigned tag)
{
    if (ifd < 8 || ifd + 2 > (unsigned)len) return -1;
    unsigned n = dng_u16(b + ifd), i;
    for (i = 0; i < n; i++) {
        unsigned e = ifd + 2 + i * 12;
        if (e + 12 > (unsigned)len) return -1;
        if (dng_u16(b + e) == tag) return (int)e;
    }
    return -1;
}

// Wert eines Eintrags mit einer Zahl im Eintrag selbst. def, wenn es ihn nicht gibt.
static unsigned dng_value(const unsigned char *b, int len, unsigned ifd, unsigned tag, unsigned def)
{
    int e = dng_entry(b, len, ifd, tag);
    if (e < 0) return def;
    return dng_u16(b + e + 2) == 3 ? dng_u16(b + e + 8) : dng_u32(b + e + 8);    // Typ 3 = SHORT
}

// 1 = DNG, die zu diesem Sensor passt
static DNG_MAYBE_UNUSED int dng_parse(const unsigned char *b, int len, int width, int height, int bits, dng_meta_t *m)
{
    int i, e;
    unsigned ifd0, sub, exif;
    for (i = 0; i < (int)sizeof(*m); i++) ((unsigned char *)m)[i] = 0;
    if (len < 16 || b[0] != 'I' || b[1] != 'I' || b[2] != 42 || b[3] != 0) return 0;
    ifd0 = dng_u32(b + 4);
    sub = dng_value(b, len, ifd0, 0x14A, 0);
    if ((int)dng_value(b, len, sub, 0x100, 0) != width || (int)dng_value(b, len, sub, 0x101, 0) != height
        || (int)dng_value(b, len, sub, 0x102, 0) != bits) return 0;

    // AsShotNeutral: drei Brueche 1000 / Faktor (R, G, B)
    e = dng_entry(b, len, ifd0, 0xC628);
    if (e >= 0) {
        unsigned o = dng_u32(b + e + 8);
        if (o + 24 <= (unsigned)len && dng_u32(b + o) && dng_u32(b + o + 8) && dng_u32(b + o + 16)) {
            m->wb_r = (int)(dng_u32(b + o + 4)  * 1000u / dng_u32(b + o));
            m->wb_g = (int)(dng_u32(b + o + 12) * 1000u / dng_u32(b + o + 8));
            m->wb_b = (int)(dng_u32(b + o + 20) * 1000u / dng_u32(b + o + 16));
        }
    }

    e = dng_entry(b, len, ifd0, DNG_TAG_DESCRIPTION);
    if (e >= 0 && dng_u32(b + e + 4) == 4 && b[e + 8] == 'D' && b[e + 9] == 'R' && (b[e + 10] == '1' || b[e + 10] == '2'))
        m->dr = b[e + 10] - '0';

    exif = dng_value(b, len, ifd0, 0x8769, 0);
    m->iso = (int)dng_value(b, len, exif, 0x8827, 0);
    e = dng_entry(b, len, exif, 0x9003);
    if (e >= 0) {
        unsigned o = dng_u32(b + e + 8);
        if (o + 10 <= (unsigned)len && b[o + 4] == ':' && b[o + 7] == ':') {
            m->year = (b[o] - '0') * 1000 + (b[o + 1] - '0') * 100 + (b[o + 2] - '0') * 10 + (b[o + 3] - '0');
            m->mon  = (b[o + 5] - '0') * 10 + (b[o + 6] - '0');
            m->day  = (b[o + 8] - '0') * 10 + (b[o + 9] - '0');
            if (m->year < 1990 || m->mon < 1 || m->mon > 12 || m->day < 1 || m->day > 31) m->year = 0;
        }
    }
    return 1;
}

// Vorschaubild der CHDK-DNG (IFD0: unkomprimiert, 8 Bit RGB, ein Streifen, modules/dng.c).
// 1 = vorhanden, dann Position im File und Groesse. Fehlt es (z. B. Remote-Aufnahme), 0.
static DNG_MAYBE_UNUSED int dng_thumb_info(const unsigned char *b, int len, unsigned *off, int *w, int *h)
{
    if (len < 16 || b[0] != 'I' || b[1] != 'I' || b[2] != 42 || b[3] != 0) return 0;
    unsigned ifd0 = dng_u32(b + 4);
    *w = (int)dng_value(b, len, ifd0, 0x100, 0);
    *h = (int)dng_value(b, len, ifd0, 0x101, 0);
    *off = dng_value(b, len, ifd0, 0x111, 0);
    if (dng_value(b, len, ifd0, 0x103, 0) != 1 || dng_value(b, len, ifd0, 0x115, 0) != 3) return 0;
    if (*w < 16 || *h < 16 || *w > 256 || *h > 256 || *off < 8) return 0;
    return dng_value(b, len, ifd0, 0x117, 0) >= (unsigned)(*w * *h * 3);
}

// Faktor *4096 fuer die Rohdaten eines Kanals, damit das JPEG den Weissabgleich der Aufnahme
// bekommt, obwohl Canon die Faktoren des Hilfsschusses anwendet. Die Division haelt alles in 32 Bit.
#define WB_ONE 4096
static DNG_MAYBE_UNUSED int wb_factor(int shot_x, int shot_g, int now_x, int now_g)
{
    if (shot_x <= 0 || shot_g <= 0 || now_x <= 0 || now_g <= 0) return WB_ONE;
    unsigned a = (unsigned)shot_x * (unsigned)now_g, b = (unsigned)shot_g * (unsigned)now_x;
    while (a >= (1u << 26) || b >= (1u << 26)) { a >>= 1; b >>= 1; }
    if (b < (1u << 7)) return WB_ONE;
    int f = (int)((a << 5) / (b >> 7));
    if (f < WB_ONE / 4) f = WB_ONE / 4;         // mehr waere kein Weissabgleich, sondern ein Lesefehler
    if (f > WB_ONE * 4) f = WB_ONE * 4;
    if (f > WB_ONE - 20 && f < WB_ONE + 20) f = WB_ONE;     // unter 0,5 %: Rechenzeit sparen
    return f;
}

#endif
