# Cameras in EXCCD – porting guide

New camera models are welcome. This page shows what a port needs. If you get stuck, open an
issue: a half-finished port with notes is already a big help.

All cameras share one core (`core/filmsim.c`, `core/filmsim_raw.h`). The core never asks for
the camera's name, only for:

- what CHDK already knows: `CAM_SENSOR_BITS_PER_PIXEL` (10 or 12), `cam_CFAPattern`,
  sensor size
- the EXCCD switches in the block `EXCCD` at the end of `platform/<camera>/platform_camera.h`
  (meaning and defaults are described at the top of `core/filmsim.c`)

Each camera has a folder here with everything that only concerns that camera:

| File | Content |
|---|---|
| `notes.md` | Firmware, start-up, sensor, buttons, open points |
| `kameratest.md` | Checklist for the test on the camera |
| `PALREC.BIN` | Palette dump of the camera (EXCCD options → System → save palette) |
| `CHDK/FILMS/*.FLM` | Labels in this camera's palette (`EXCCD_CAM=<platform> python tools/filmsim/make_labels.py`); without them the IXUS 60 labels are used |
| others | e.g. `palette_foto.json` (measured colours for the labels) |

The existing notes and checklists are in German. English is perfectly fine for new ones.

## Adding a camera

1. **Branch** `<platform>` from `main`. It only lives until the camera test.
2. **Entry** in `tools/filmsim/cameras.txt` with status `test`.
3. **Check CHDK:** the camera must exist in CHDK 1.6.1 (`platform/<platform>/sub/<fw>/`).
   The core handles 10 and 12 bit and the Bayer patterns R G / G B and G B / R G. For
   anything else the build stops with `#error`; then extend `core/filmsim_raw.h` and test
   it in `tools/filmsim/test_raw.c`.
4. **Block `EXCCD`** in `platform_camera.h`. A comment is enough to start with: without
   switches you get text cards in CHDK colours and the white balance gains of the IXUS 60.
   Also set the ALT button (`CAM_ALT_BUTTON_*`) and the menu cursor (white/black).
5. **Folder** `tools/filmsim/cams/<platform>/` with `notes.md` and `kameratest.md`
   (template: `ixus145_elph135/`).
6. **Boot rules** for the SD installer: add the camera to the table in
   `tools/installer/core.c` (FAT32 only for DryOS R47+). `test_core` checks it against
   `cameras.txt`. For the download page add an entry to `CAMERA_META` in
   `tools/site/build_site.py`.
7. **Build:** `tools/filmsim/build.sh <platform>`
8. **Check the other cameras:** `tools/filmsim/compare_build.sh main` must report
   "identical" for every other camera, unless a core change is meant to affect all of them.
9. **Camera test.** Then tune palette, labels and white balance (`FILMSIM_LABELS`,
   `FILMSIM_COLOR_*`, `FILMSIM_WB_R/_B`), set the status to `stabil` and merge into `main`.

One version number applies to all cameras. The release notes say per camera whether it was
tested on that version.

## Status

- `stabil`: tested on a real camera, part of the release and the download page.
- `test`: built by CI as an artifact, not published.

Without a camera on the desk nothing can be tested, so photos from people who own one (of
the pictures and of the display) are the most useful contribution.
