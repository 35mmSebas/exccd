# EXCCD SD card installer (Windows)

Puts an EXCCD download on an SD card in one step:

1. keeps `CHDK/FILMLOG.CSV`, removes the old `CHDK` folder
2. unpacks the ZIP to the root of the card (every file CRC-checked)
3. writes the `BOOTDISK` mark into the card's boot sector, the same mark CHDK's
   *Make Card Bootable* writes (offset 0x40 on FAT12/16, 0x1E0 on FAT32), and reads it back
4. ejects the card

Only removable drives are offered. Which file system a camera can boot from comes from
the table in `core.c` (FAT32 only for DryOS R47+, i.e. `CAM_DRYOS_2_3_R47`); `test_core`
fails if a camera from `tools/filmsim/cameras.txt` is missing there. exFAT cards are
refused for now.

No external libraries: ZIP reading and DEFLATE (`inflate.c`) are part of the source.
The program is not signed; Windows SmartScreen asks once (*More info → Run anyway*).
It requests administrator rights because raw volume access needs them.

    make                                   # Windows, MinGW / w64devkit
    make CROSS=x86_64-w64-mingw32-         # cross build on Linux (CI)
    make test_core && ./test_core ../filmsim/cameras.txt /tmp/out exccd-*.zip
