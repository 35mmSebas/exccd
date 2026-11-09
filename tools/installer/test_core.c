/* PC test for the installer core.
 *
 *   test_core <cameras.txt> <outdir> <zip>...
 *
 * - every camera in cameras.txt must be in the installer's table
 * - boot-sector detection and marking on synthetic FAT12/16/32/exFAT/NTFS sectors
 * - every ZIP is unpacked completely (CRC checked) into <outdir>/<n>/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif
#include "core.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void wr16(uint8_t *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static void wr32(uint8_t *p, unsigned v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* Boot sector as a camera would write it for a card of totsec sectors. */
static void make_fat(uint8_t *s, int fat32, unsigned totsec, unsigned spc, unsigned fatsz)
{
    memset(s, 0, 512);
    s[0] = 0xeb; s[1] = 0x3c; s[2] = 0x90;
    memcpy(s + 3, "CANON   ", 8);
    wr16(s + 11, 512);
    s[13] = spc;
    wr16(s + 14, fat32 ? 32 : 1);
    s[16] = 2;
    wr16(s + 17, fat32 ? 0 : 512);
    if (totsec < 65536 && !fat32)
        wr16(s + 19, totsec);
    else
        wr32(s + 32, totsec);
    s[21] = 0xf8;
    if (fat32) {
        wr32(s + 36, fatsz);
        wr32(s + 44, 2);
        s[66] = 0x29;
        memcpy(s + 82, "FAT32   ", 8);
    } else {
        wr16(s + 22, fatsz);
        s[38] = 0x29;
        memcpy(s + 54, "FAT16   ", 8);
    }
    s[510] = 0x55; s[511] = 0xaa;
}

static void test_bootsec(void)
{
    uint8_t s[512];

    make_fat(s, 0, 3970048, 64, 243);       /* 2 GB, FAT16, 32 KB clusters */
    CHECK(bootsec_fs(s) == FS_FAT16);
    CHECK(!bootsec_is_bootable(s));
    CHECK(bootsec_mark(s) == 0);
    CHECK(bootsec_is_bootable(s));
    CHECK(memcmp(s + 0x40, "BOOTDISK", 8) == 0);
    CHECK(bootsec_fs(s) == FS_FAT16);       /* mark must not change the BPB */

    make_fat(s, 1, 15523840, 64, 1895);     /* 8 GB, FAT32, 32 KB clusters */
    CHECK(bootsec_fs(s) == FS_FAT32);
    CHECK(!bootsec_is_bootable(s));
    CHECK(bootsec_mark(s) == 0);
    CHECK(memcmp(s + 0x1e0, "BOOTDISK", 8) == 0);
    CHECK(bootsec_is_bootable(s));
    CHECK(bootsec_fs(s) == FS_FAT32);

    make_fat(s, 0, 30000, 8, 12);           /* 15 MB -> FAT12 */
    CHECK(bootsec_fs(s) == FS_FAT12);
    CHECK(bootsec_mark_offset(FS_FAT12) == 0x40);

    memset(s, 0, 512);
    memcpy(s + 3, "EXFAT   ", 8);
    s[510] = 0x55; s[511] = 0xaa;
    CHECK(bootsec_fs(s) == FS_EXFAT);
    CHECK(bootsec_mark(s) == -1);

    make_fat(s, 0, 3970048, 64, 243);
    memcpy(s + 3, "NTFS    ", 8);
    CHECK(bootsec_fs(s) == FS_UNKNOWN);

    memset(s, 0, 512);                      /* unformatted */
    CHECK(bootsec_fs(s) == FS_UNKNOWN);
    CHECK(bootsec_mark(s) == -1);
}

static void test_names(void)
{
    CHECK(zip_name_safe("DISKBOOT.BIN"));
    CHECK(zip_name_safe("CHDK/"));
    CHECK(zip_name_safe("CHDK/CURVES/BLUSH.CV"));
    CHECK(!zip_name_safe("../evil"));
    CHECK(!zip_name_safe("CHDK/../../evil"));
    CHECK(!zip_name_safe("/abs"));
    CHECK(!zip_name_safe("C:/x"));
    CHECK(!zip_name_safe("a\\b"));
    CHECK(!zip_name_safe("a//b"));
    CHECK(!zip_name_safe("./a"));
    CHECK(!zip_name_safe(""));
}

#define MAX_KNOWN 16

static void test_cameras(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int n = 0;
    CHECK(f != NULL);
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char plat[64], sub[16], fw[8];
        const camera *cam;
        if (line[0] == '#' || sscanf(line, "%63s %15s", plat, sub) != 2)
            continue;
        n++;
        if (!(cam = camera_find(plat))) {
            printf("FAIL: camera %s from cameras.txt is missing in core.c\n", plat);
            failures++;
        } else {
            /* sub "100d" is firmware "1.00d" */
            snprintf(fw, sizeof(fw), "%c.%s", sub[0], sub + 1);
            if (strcmp(cam->fw, fw) != 0) {
                printf("FAIL: %s: core.c says firmware %s, cameras.txt %s\n", plat, cam->fw, fw);
                failures++;
            }
        }
    }
    fclose(f);
    CHECK(n == camera_count);

    /* known_fw against camera_list.csv in the repo root (cameras.txt is in tools/filmsim/) */
    {
        char csv[512], known[MAX_KNOWN][128];
        const char *slash = strrchr(path, '/');
        snprintf(csv, sizeof(csv), "%.*s../../camera_list.csv", slash ? (int)(slash - path + 1) : 0, path);
        memset(known, 0, sizeof(known));
        f = fopen(csv, "r");
        CHECK(f != NULL);
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                char plat[64], sub[16];
                if (sscanf(line, "%63[^,],%15[^,]", plat, sub) != 2)
                    continue;
                for (int k = 0; k < camera_count && k < MAX_KNOWN; k++)
                    if (strcmp(cameras[k].platform, plat) == 0) {
                        char v[8];
                        snprintf(v, sizeof(v), "%c.%s", sub[0], sub + 1);
                        if (known[k][0])
                            strcat(known[k], " ");
                        strcat(known[k], v);
                    }
            }
            fclose(f);
            for (int k = 0; k < camera_count && k < MAX_KNOWN; k++)
                if (strcmp(cameras[k].known_fw, known[k]) != 0) {
                    printf("FAIL: %s: core.c knows \"%s\", camera_list.csv \"%s\"\n",
                           cameras[k].platform, cameras[k].known_fw, known[k]);
                    failures++;
                }
        }
    }

    CHECK(camera_detect(NULL, "exccd-v1.9.24-sx110is-100b-7looks.zip") == camera_find("sx110is"));
    CHECK(camera_detect(NULL, "C:\\Users\\x\\Downloads\\exccd-v1.9.24-ixus132_elph115-100b-3looks (1).zip")
          == camera_find("ixus132_elph115"));
    CHECK(camera_detect(NULL, "exccd-v1.9.24-sx110is2-100b.zip") == NULL);
    CHECK(camera_detect(NULL, "random.zip") == NULL);

    {
        const camera *old = camera_find("sx110is"), *ixus60 = camera_find("ixus60_sd600"),
                     *r47 = camera_find("ixus132_elph115");
        CHECK(check_card(old, FS_FAT16) == CHECK_OK);
        CHECK(check_card(old, FS_FAT32) == CHECK_NEED_FAT16);
        CHECK(check_card(ixus60, FS_FAT32) == CHECK_FILES_ONLY);
        CHECK(check_card(r47, FS_FAT32) == CHECK_OK);
        CHECK(check_card(r47, FS_EXFAT) == CHECK_EXFAT);
        CHECK(check_card(r47, FS_UNKNOWN) == CHECK_BAD_FS);
    }
}

static uint8_t *load(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long n;
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(n ? n : 1);
    if (buf && fread(buf, 1, n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *size = n;
    return buf;
}

static void mkdirs(const char *path)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            MKDIR(tmp);
            *p = '/';
        }
    MKDIR(tmp);
}

static void test_zip(const char *path, const char *outdir)
{
    size_t size;
    uint8_t *data = load(path, &size);
    zip_file z;
    char err[128];
    int files = 0;

    CHECK(data != NULL);
    if (!data)
        return;
    if (zip_open(&z, data, size, err, sizeof(err)) != 0) {
        printf("FAIL %s: %s\n", path, err);
        failures++;
        free(data);
        return;
    }
    CHECK(zip_is_exccd(&z));
    const camera *cam = camera_detect(&z, path);
    for (int i = 0; i < z.count; i++) {
        const char *name = z.entries[i].name;
        char out[1024];
        size_t len = strlen(name);
        CHECK(zip_name_safe(name));
        snprintf(out, sizeof(out), "%s/%s", outdir, name);
        if (len && name[len - 1] == '/') {
            mkdirs(out);
            continue;
        }
        uint8_t *buf = zip_read(&z, i);
        if (!buf) {
            printf("FAIL %s: cannot unpack %s\n", path, name);
            failures++;
            continue;
        }
        char *slash = strrchr(out, '/');
        *slash = 0;
        mkdirs(out);
        *slash = '/';
        FILE *f = fopen(out, "wb");
        CHECK(f != NULL);
        if (f) {
            fwrite(buf, 1, z.entries[i].usize, f);
            fclose(f);
        }
        free(buf);
        files++;
    }
    printf("%s: %d entries, %d files, %llu bytes, camera %s\n", path, z.count, files,
           (unsigned long long)zip_total_size(&z), cam ? cam->name : "(unknown)");
    zip_close(&z);
    free(data);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s cameras.txt outdir [zip...]\n", argv[0]);
        return 2;
    }
    test_bootsec();
    test_names();
    test_cameras(argv[1]);
    for (int i = 3; i < argc; i++) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/%d", argv[2], i - 2);
        mkdirs(dir);
        test_zip(argv[i], dir);
    }
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all installer core tests passed\n");
    return 0;
}
