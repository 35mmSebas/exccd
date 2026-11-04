/* Platform-independent part of the EXCCD installer: reading the ZIP,
 * understanding the SD card's boot sector, and knowing which camera
 * can boot from which file system. Tested on the PC by test_core.c. */
#ifndef EXCCD_CORE_H
#define EXCCD_CORE_H
#include <stddef.h>
#include <stdint.h>

/* ---- ZIP ---------------------------------------------------------------- */

typedef struct {
    char *name;             /* as stored in the ZIP, '/' separated */
    uint16_t method;        /* 0 = stored, 8 = deflate */
    uint32_t crc, csize, usize, local_offset;
} zip_entry;

typedef struct {
    uint8_t *data;          /* whole ZIP file, owned by the caller */
    size_t size;
    zip_entry *entries;
    int count;
    char comment[256];
} zip_file;

/* Parse the central directory. Returns 0 or -1 with a message in err. */
int zip_open(zip_file *z, uint8_t *data, size_t size, char *err, size_t errlen);
void zip_close(zip_file *z);

/* Uncompress entry i into a new malloc'd buffer (usize bytes, CRC checked).
 * Returns NULL on error. A zero-length entry returns a 1-byte buffer. */
uint8_t *zip_read(const zip_file *z, int i);

/* 1 if the name is a safe relative path (no "..", no drive, no backslash). */
int zip_name_safe(const char *name);

/* 1 if the ZIP looks like an EXCCD download: DISKBOOT.BIN at the top level
 * and a CHDK/ folder. */
int zip_is_exccd(const zip_file *z);

/* Total uncompressed size of all entries. */
uint64_t zip_total_size(const zip_file *z);

/* ---- Cameras ------------------------------------------------------------ */

typedef struct {
    const char *platform;   /* CHDK platform name, as in cameras.txt */
    const char *name;       /* shown to the user */
    int fat32_boot;         /* 1: boots from FAT32 (DryOS R47+); 0: FAT12/16 only */
    int firm_update;        /* 1: can start EXCCD via MENU -> Firm Update (PS.FIR) */
} camera;

extern const camera cameras[];
extern const int camera_count;

const camera *camera_find(const char *platform);

/* Guess the camera from the ZIP comment ("CHDK for <platform> fw:...") or,
 * failing that, from the file name (exccd-<ver>-<platform>-<sub>-...zip).
 * Returns NULL if neither names a known camera. */
const camera *camera_detect(const zip_file *z, const char *filename);

/* ---- Boot sector -------------------------------------------------------- */

enum { FS_UNKNOWN = 0, FS_FAT12, FS_FAT16, FS_FAT32, FS_EXFAT };

const char *fs_name(int fs);

/* File system type from the volume's first sector (512 bytes are enough). */
int bootsec_fs(const uint8_t *sector);

/* Offset of the "BOOTDISK" mark for this file system, -1 if there is none. */
int bootsec_mark_offset(int fs);

int bootsec_is_bootable(const uint8_t *sector);

/* Writes the mark into the sector buffer. Returns 0, or -1 if the file system
 * has no known mark position. */
int bootsec_mark(uint8_t *sector);

/* ---- Decision ----------------------------------------------------------- */

enum {
    CHECK_OK = 0,           /* install and make bootable */
    CHECK_FILES_ONLY,       /* can't be made bootable, but Firm Update works */
    CHECK_NEED_FAT16,       /* camera needs a FAT16 card (2 GB or smaller) */
    CHECK_EXFAT,            /* exFAT card (SDXC, over 32 GB) */
    CHECK_BAD_FS            /* not FAT at all (NTFS, unformatted, ...) */
};

/* What to do with a card of file system fs in camera cam. */
int check_card(const camera *cam, int fs);

#endif
