#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "inflate.h"

/* ---- helpers ------------------------------------------------------------ */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void set_err(char *err, size_t errlen, const char *msg)
{
    if (err && errlen) {
        strncpy(err, msg, errlen - 1);
        err[errlen - 1] = 0;
    }
}

static uint32_t crc32_buf(const uint8_t *p, size_t n)
{
    static uint32_t table[256];
    static int built = 0;
    uint32_t c = 0xffffffffu;
    if (!built) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t k = i;
            for (int j = 0; j < 8; j++)
                k = k & 1 ? 0xedb88320u ^ (k >> 1) : k >> 1;
            table[i] = k;
        }
        built = 1;
    }
    while (n--)
        c = table[(c ^ *p++) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

/* ---- ZIP ---------------------------------------------------------------- */

int zip_open(zip_file *z, uint8_t *data, size_t size, char *err, size_t errlen)
{
    size_t eocd = 0, p;
    int found = 0, i, count;
    uint32_t cd_offset, cd_size;

    memset(z, 0, sizeof(*z));
    z->data = data;
    z->size = size;

    if (size < 22) {
        set_err(err, errlen, "file too small for a ZIP");
        return -1;
    }
    /* End of central directory: last 22 bytes plus up to 64 KB comment. */
    for (p = size - 22;; p--) {
        if (rd32(data + p) == 0x06054b50 && p + 22 + rd16(data + p + 20) <= size) {
            eocd = p;
            found = 1;
            break;
        }
        if (p == 0 || size - p > 22 + 65535)
            break;
    }
    if (!found) {
        set_err(err, errlen, "not a ZIP file");
        return -1;
    }
    count = rd16(data + eocd + 10);
    cd_size = rd32(data + eocd + 12);
    cd_offset = rd32(data + eocd + 16);
    if (count == 0xffff || cd_offset == 0xffffffffu) {
        set_err(err, errlen, "ZIP64 is not supported");
        return -1;
    }
    if ((uint64_t)cd_offset + cd_size > eocd) {
        set_err(err, errlen, "broken ZIP directory");
        return -1;
    }
    {
        size_t clen = rd16(data + eocd + 20);
        if (clen >= sizeof(z->comment))
            clen = sizeof(z->comment) - 1;
        memcpy(z->comment, data + eocd + 22, clen);
        z->comment[clen] = 0;
    }

    z->entries = calloc(count ? count : 1, sizeof(zip_entry));
    if (!z->entries) {
        set_err(err, errlen, "out of memory");
        return -1;
    }
    p = cd_offset;
    for (i = 0; i < count; i++) {
        zip_entry *e = &z->entries[i];
        size_t nlen;
        if (p + 46 > eocd || rd32(data + p) != 0x02014b50) {
            set_err(err, errlen, "broken ZIP directory");
            zip_close(z);
            return -1;
        }
        e->method = rd16(data + p + 10);
        e->crc = rd32(data + p + 16);
        e->csize = rd32(data + p + 20);
        e->usize = rd32(data + p + 24);
        nlen = rd16(data + p + 28);
        e->local_offset = rd32(data + p + 42);
        if (p + 46 + nlen > eocd) {
            set_err(err, errlen, "broken ZIP directory");
            zip_close(z);
            return -1;
        }
        e->name = malloc(nlen + 1);
        if (!e->name) {
            set_err(err, errlen, "out of memory");
            zip_close(z);
            return -1;
        }
        memcpy(e->name, data + p + 46, nlen);
        e->name[nlen] = 0;
        z->count = i + 1;
        p += 46 + nlen + rd16(data + p + 30) + rd16(data + p + 32);
    }
    return 0;
}

void zip_close(zip_file *z)
{
    for (int i = 0; i < z->count; i++)
        free(z->entries[i].name);
    free(z->entries);
    z->entries = NULL;
    z->count = 0;
}

uint8_t *zip_read(const zip_file *z, int i)
{
    const zip_entry *e = &z->entries[i];
    size_t lh = e->local_offset, start;
    uint8_t *out;

    if (lh + 30 > z->size || rd32(z->data + lh) != 0x04034b50)
        return NULL;
    start = lh + 30 + rd16(z->data + lh + 26) + rd16(z->data + lh + 28);
    if (start + e->csize > z->size)
        return NULL;
    out = malloc(e->usize ? e->usize : 1);
    if (!out)
        return NULL;
    if (e->method == 0) {
        if (e->csize != e->usize)
            goto fail;
        memcpy(out, z->data + start, e->usize);
    } else if (e->method == 8) {
        if (inflate_raw(z->data + start, e->csize, out, e->usize) != 0)
            goto fail;
    } else {
        goto fail;
    }
    if (crc32_buf(out, e->usize) != e->crc)
        goto fail;
    return out;
fail:
    free(out);
    return NULL;
}

int zip_name_safe(const char *name)
{
    const char *p = name;
    if (!*name || *name == '/' || strchr(name, '\\') || strchr(name, ':'))
        return 0;
    /* every path component must be non-empty and not "." or ".." */
    while (*p) {
        const char *q = strchr(p, '/');
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len == 0 && q && q[1] != 0)
            return 0;                       /* "a//b" */
        if ((len == 1 && p[0] == '.') || (len == 2 && p[0] == '.' && p[1] == '.'))
            return 0;
        for (size_t k = 0; k < len; k++)
            if ((unsigned char)p[k] < 0x20 || strchr("<>\"|?*", p[k]))
                return 0;
        if (!q)
            break;
        p = q + 1;
    }
    return 1;
}

int zip_is_exccd(const zip_file *z)
{
    int boot = 0, chdk = 0;
    for (int i = 0; i < z->count; i++) {
        const char *n = z->entries[i].name;
        if (strcmp(n, "DISKBOOT.BIN") == 0)
            boot = 1;
        if (strncmp(n, "CHDK/", 5) == 0)
            chdk = 1;
    }
    return boot && chdk;
}

uint64_t zip_total_size(const zip_file *z)
{
    uint64_t sum = 0;
    for (int i = 0; i < z->count; i++)
        sum += z->entries[i].usize;
    return sum;
}

/* ---- Cameras ------------------------------------------------------------ */

/* Keep in step with tools/filmsim/cameras.txt (test_core checks it).
 * fat32_boot: platform_camera.h defines CAM_DRYOS_2_3_R47.
 * firm_update: the full ZIP contains PS.FIR.
 * fw: the sub from cameras.txt as the camera shows it.
 * known_fw: every version of this camera in camera_list.csv, for the
 * blink table of "Check firmware" (test_core checks it). */
const camera cameras[] = {
    { "ixus60_sd600",    "IXUS 60 / PowerShot SD600",        0, 1, "1.00d", "1.00a 1.00d" },
    { "ixus145_elph135", "IXUS 145 / PowerShot ELPH 135",    1, 0, "1.00c", "1.00c" },
    { "ixus132_elph115", "IXUS 132 / PowerShot ELPH 115 IS", 1, 0, "1.00b", "1.00b" },
    { "sx110is",         "PowerShot SX110 IS",               0, 0, "1.00b", "1.00b" },
};
const int camera_count = sizeof(cameras) / sizeof(cameras[0]);

const camera *camera_find(const char *platform)
{
    for (int i = 0; i < camera_count; i++)
        if (strcmp(cameras[i].platform, platform) == 0)
            return &cameras[i];
    return NULL;
}

/* Does s contain word on its own, i.e. not as part of a longer platform name?
 * Platform names contain '_', so the characters around the match must not be
 * lower-case letters, digits or '_' ("sx110is" must not match "sx110is2"). */
static int contains_word(const char *s, const char *word)
{
    size_t wl = strlen(word);
    for (const char *p = s; (p = strstr(p, word)) != NULL; p++) {
        char before = p == s ? 0 : p[-1];
        char after = p[wl];
        int okb = !(before == '_' || (before >= 'a' && before <= 'z') || (before >= '0' && before <= '9'));
        int oka = !(after == '_' || (after >= 'a' && after <= 'z') || (after >= '0' && after <= '9'));
        if (okb && oka)
            return 1;
    }
    return 0;
}

const camera *camera_detect(const zip_file *z, const char *filename)
{
    char lower[512];
    const char *base = filename;
    size_t i;

    if (z) {
        const char *c = strstr(z->comment, "CHDK for ");
        if (c) {
            for (int k = 0; k < camera_count; k++)
                if (contains_word(c + 9, cameras[k].platform))
                    return &cameras[k];
        }
    }
    if (!filename)
        return NULL;
    for (const char *p = filename; *p; p++)
        if (*p == '/' || *p == '\\')
            base = p + 1;
    for (i = 0; base[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (char)(base[i] >= 'A' && base[i] <= 'Z' ? base[i] + 32 : base[i]);
    lower[i] = 0;
    for (int k = 0; k < camera_count; k++)
        if (contains_word(lower, cameras[k].platform))
            return &cameras[k];
    return NULL;
}

/* ---- Boot sector -------------------------------------------------------- */

const char *fs_name(int fs)
{
    switch (fs) {
    case FS_FAT12: return "FAT12";
    case FS_FAT16: return "FAT16";
    case FS_FAT32: return "FAT32";
    case FS_EXFAT: return "exFAT";
    default: return "?";
    }
}

int bootsec_fs(const uint8_t *s)
{
    uint32_t bps, spc, rsvd, nfats, rootent, fatsz, totsec, rootsec, datasec, clusters;

    if (memcmp(s + 3, "EXFAT   ", 8) == 0)
        return FS_EXFAT;
    if (s[510] != 0x55 || s[511] != 0xaa)
        return FS_UNKNOWN;
    bps = rd16(s + 11);
    spc = s[13];
    rsvd = rd16(s + 14);
    nfats = s[16];
    rootent = rd16(s + 17);
    if ((bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) ||
        spc == 0 || (spc & (spc - 1)) != 0 || rsvd == 0 || nfats == 0)
        return FS_UNKNOWN;                  /* not a FAT BPB (e.g. NTFS) */
    if (memcmp(s + 3, "NTFS    ", 8) == 0)
        return FS_UNKNOWN;
    fatsz = rd16(s + 22);
    if (fatsz == 0)
        fatsz = rd32(s + 36);
    totsec = rd16(s + 19);
    if (totsec == 0)
        totsec = rd32(s + 32);
    if (fatsz == 0 || totsec == 0)
        return FS_UNKNOWN;
    rootsec = (rootent * 32 + bps - 1) / bps;
    if ((uint64_t)rsvd + (uint64_t)nfats * fatsz + rootsec >= totsec)
        return FS_UNKNOWN;
    datasec = totsec - (rsvd + nfats * fatsz + rootsec);
    clusters = datasec / spc;
    /* Cluster count decides the FAT type (Microsoft FAT spec). */
    if (clusters < 4085)
        return FS_FAT12;
    if (clusters < 65525)
        return rd16(s + 22) != 0 ? FS_FAT16 : FS_UNKNOWN;
    return rd16(s + 22) == 0 ? FS_FAT32 : FS_UNKNOWN;
}

int bootsec_mark_offset(int fs)
{
    /* Same positions CHDK's "Make Card Bootable" uses: inside the boot code
     * area, right after the (extended) BPB. */
    if (fs == FS_FAT12 || fs == FS_FAT16)
        return 0x40;
    if (fs == FS_FAT32)
        return 0x1e0;
    return -1;
}

int bootsec_is_bootable(const uint8_t *s)
{
    int off = bootsec_mark_offset(bootsec_fs(s));
    return off >= 0 && memcmp(s + off, "BOOTDISK", 8) == 0;
}

int bootsec_mark(uint8_t *s)
{
    int off = bootsec_mark_offset(bootsec_fs(s));
    if (off < 0)
        return -1;
    memcpy(s + off, "BOOTDISK", 8);
    return 0;
}

/* ---- Decision ----------------------------------------------------------- */

int check_card(const camera *cam, int fs)
{
    switch (fs) {
    case FS_FAT12:
    case FS_FAT16:
        return CHECK_OK;
    case FS_FAT32:
        if (cam->fat32_boot)
            return CHECK_OK;
        return cam->firm_update ? CHECK_FILES_ONLY : CHECK_NEED_FAT16;
    case FS_EXFAT:
        return CHECK_EXFAT;
    default:
        return CHECK_BAD_FS;
    }
}
