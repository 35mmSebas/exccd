# EXCCD

Film looks for old CCD point-and-shoot cameras, built on [CHDK](https://chdk.fandom.com/).

EXCCD adds a film selection to the camera: press the **Print** button, flip through the
film labels and pick a look. Each look combines Canon's own *My Colors* settings with a
tone curve that is applied to the raw data before the JPEG is created, so the pictures
come out of the camera finished.

Supported camera: **Canon Digital IXUS 60 / PowerShot SD600, firmware 1.00d**.
More cameras are planned.

> The camera menus are currently in German.

## Looks

| Look | Type | Character |
|---|---|---|
| Daylight 100 | Color slide | Natural balance |
| Postcard 100 | Color slide | Rich saturation |
| Blush 100 | Color slide | Soft portrait |
| Journal 400 | Color slide | Documentary chroma |
| Studio 160 | Color negative | Neutral shadow |
| Drugstore 200 | Color negative | Retro negative |
| Scene 500 | Cinema | Cinematic flat |
| Silver 400 | Black and white | Fine mono |
| Portrait 400 | Color negative | Warm skin |
| Tungsten 800 | Cinema | Neon nights |
| Expired 200 | Color negative | Faded memory |
| Cross 100 | Color slide | Cross processed |
| Violet 400 | Color negative | Purple foliage |
| Concrete 100 | Color negative | Muted urban |
| Ember 200 | Color negative | Red scale |
| Grit 400 | Black and white | Pushed grain |

Every look also has two **Dynamic+** variants (DR+1, DR+2) that expose shorter to protect
highlights and lift the shadows with the curve.

## Installation

1. Check the firmware version of your camera. EXCCD only runs on firmware **1.00d**.
   See the [CHDK wiki](https://chdk.fandom.com/wiki/FAQ#Q._How_can_I_get_the_original_firmware_version_number_of_my_camera.3F)
   for how to find it.
2. Download `exccd-<version>-ixus60_sd600-100d-full.zip` from the
   [Releases](../../releases) page.
3. Delete an old `CHDK` folder from the SD card, if there is one.
   Old files (for example a previous `FILMS.TXT`) can point to labels or curves that no
   longer exist.
4. Unpack the ZIP to the root of the SD card.
5. Start EXCCD, either
   - **once per power-on:** switch on in playback mode, open *MENU → Firm Update* and
     confirm, or
   - **automatically:** make the card bootable (for example with
     [STICK](https://chdk.fandom.com/wiki/STICK) or CHDK's own *Make Card Bootable* option)
     and lock the card's write-protect switch.

EXCCD runs from the SD card only. Nothing is written to the camera's firmware; without the
card the camera is unchanged.

## Usage

| Button | In shooting mode |
|---|---|
| **Print** | Open the film selection with the active look, press again to close without changes |
| Left/right, up/down | Flip through the looks |
| **FUNC/SET** | Apply the look and close |
| **MENU** | Options (raw, Dynamic+, system) |

The name of the active look is shown at the bottom of the screen. For every photo,
`A/CHDK/FILMLOG.CSV` records which look was active.

## Building

Toolchain: [xPack GNU Arm Embedded GCC 10.3.1-2.3](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v10.3.1-2.3),
a host GCC, GNU make and Info-ZIP `zip`.

```sh
export PATH=/path/to/xpack-arm-none-eabi-gcc-10.3.1-2.3/bin:$PATH
tools/filmsim/build.sh                  # every camera in tools/filmsim/cameras.txt
tools/filmsim/build.sh ixus60_sd600     # one camera
```

Result: `dist/exccd-<version>-<platform>-<sub>-full.zip`. The script runs `make clean`
before and after each camera, because CHDK shares the object files in `core/`, `lib/` and
`modules/` between all platforms.

On Windows, keep LF line endings. The repository's `.gitattributes` takes care of that;
an older checkout with `core.autocrlf=true` will not build.

Tests on the PC, for the `FILMS.TXT` parser and for the raw codec and 12-bit curves
(`core/filmsim_raw.h`, checked against CHDK's `get_raw_pixel`/`set_raw_pixel`):

```sh
gcc -Wall -Wextra -I core -o test_films_txt tools/filmsim/test_films_txt.c core/filmsim_txt.c
./test_films_txt CHDK/FILMS/BEISPIEL.TXT
gcc -Wall -Wextra -I core -DCAM_SENSOR_BITS_PER_PIXEL=10 -o test_raw10 tools/filmsim/test_raw.c && ./test_raw10
gcc -Wall -Wextra -I core -DCAM_SENSOR_BITS_PER_PIXEL=12 -o test_raw12 tools/filmsim/test_raw.c && ./test_raw12
```

When a core change must not touch another camera, `tools/filmsim/compare_build.sh <git-ref> <platform>`
builds both states with the same version and timestamp and compares `main.bin` and every object file.

GitHub Actions builds every push, one job per camera in `tools/filmsim/cameras.txt`.
A tag `vX.Y.Z` that matches `FILMSIM_VERSION` in `core/filmsim.h` creates a release with the
ZIPs of the cameras marked `stabil` and publishes the download page on GitHub Pages.
Cameras marked `test` are only build artifacts.

The download page is static. `tools/site/build_site.py` splits the firmware ZIP into a base
ZIP and one small ZIP per look; the browser copies the chosen entries into a new ZIP and adds
a matching `FILMS.TXT`, without a build per download. To try it locally:

```sh
python tools/site/build_site.py bin/ixus60_sd600-100d-1.6.1-0-full.zip _site
python -m http.server -d _site
```

### Where things are

| Path | Content |
|---|---|
| `core/filmsim.c` | Film selection, look logic, OSD |
| `core/filmsim_raw.h` | Raw codec (10/12 bit) and 12-bit tone curves, also used by the PC test |
| `core/filmsim_txt.c` | Parser for `A/CHDK/FILMS/FILMS.TXT` |
| `platform/<camera>/platform_camera.h` | CHDK camera settings; block `EXCCD` with the `FILMSIM_*` switches |
| `tools/filmsim/cameras.txt` | Cameras EXCCD is built for, with status `stabil` or `test` |
| `tools/filmsim/cams/<camera>/` | Per camera: palette, notes, camera test; [porting guide](tools/filmsim/cams/README.md) |
| `CHDK/CURVES/`, `CHDK/FILMS/` | Curves, labels and film table of the looks |
| `tools/filmsim/` | Generators for curves (`make_curves.py`) and labels (`make_label_art.py`, `make_labels.py`) |
| `site/`, `tools/site/` | Download page: pick looks, the browser assembles the ZIP for the SD card |

The first commit of this repository is unchanged CHDK 1.6.1 (GitHub mirror
[petabyt/chdk](https://github.com/petabyt/chdk), commit `ad402e1`, path
`branches/release-1_6`, SVN r6307). A diff against that commit shows every EXCCD change:

```sh
git diff $(git rev-list --max-parents=0 HEAD) HEAD
```

## License

- **Code:** GNU General Public License v2, like CHDK. See [`COPYING`](COPYING).
- **Looks** (curves, labels, film table): [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
  See [`LICENSE-LOOKS.md`](LICENSE-LOOKS.md).
- **Font** Archivo Black: SIL Open Font License 1.1, see [`tools/filmsim/fonts/OFL.txt`](tools/filmsim/fonts/OFL.txt).

The original CHDK readme is in [`README.CHDK`](README.CHDK).

## Disclaimer

EXCCD is an independent project. It is not affiliated with, endorsed by or sponsored by
Canon Inc. Canon, IXUS and PowerShot are trademarks of Canon Inc. and are used only to
name the supported camera.

Use at your own risk. CHDK and EXCCD come without any warranty (see `COPYING`).
