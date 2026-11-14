# EXCCD

Film looks for old Canon point-and-shoot cameras, built on [CHDK](https://chdk.fandom.com/).

EXCCD adds a film selection to the camera: press the **Print** button, flip through the
film labels and pick a look. Each look combines Canon's own *My Colors* settings with a
tone curve that is applied to the raw data before the JPEG is created, so the pictures
come out of the camera finished.

> **Public beta, work in progress.** EXCCD is a hobby project. Every old camera model behaves a
> little differently, so looks get re-tuned as real photos come in and things change between
> versions. Feedback and pull requests are very welcome.

| Camera | Firmware | Status |
|---|---|---|
| Digital IXUS 60 / PowerShot SD600 | 1.00d | tested up to v1.9.x, v1.10 features in testing; offered on the website |
| IXUS 132 / PowerShot ELPH 115 IS | 1.00b | in testing (built, partly tested) |
| PowerShot SX110 IS | 1.00b | in testing (built, partly tested) |
| IXUS 145 / PowerShot ELPH 135 | 1.00c | ported, never run on a real camera |

> The camera menus are in English. For German: *MENU → System → Language... → `german.lng`*.

## Looks

| Look | Type | Character |
|---|---|---|
| Daylight 100 | Colour slide | Natural balance |
| Postcard 100 | Colour slide | Rich saturation |
| Blush 100 | Colour slide | Soft portrait |
| Journal 400 | Colour slide | Documentary chroma |
| Studio 160 | Colour negative | Neutral shadow |
| Drugstore 200 | Colour negative | Retro negative |
| Scene 500 | Cinema | Cinematic flat |
| Silver 400 | Black and white | Fine mono |
| Portrait 400 | Colour negative | Warm skin |
| Tungsten 800 | Cinema | Neon nights |
| Expired 200 | Colour negative | Faded memory |
| Cross 100 | Colour slide | Cross processed |
| Violet 400 | Colour negative | Purple foliage |
| Concrete 100 | Colour negative | Muted urban |
| Ember 200 | Colour negative | Red scale |
| Grit 400 | Black and white | Pushed grain |
| Summer 200 | Colour negative | Golden warmth |
| Chrome 64 | Colour slide | Deep reds |
| Bypass 400 | Cinema | Silver retained |
| Noir 3200 | Black and white | Hard pushed |
| Turquoise 400 | Colour negative | Teal leaves |
| Infra 200 | Colour negative | Red foliage |
| Tintype 50 | Alternative process | Warm metal |
| Cyano 25 | Alternative process | Sun print |
| Instant 600 | Instant | Creamy fade |
| Pastel 400 | Colour negative | Airy soft |

Every look also has two **Dynamic+** variants (DR+1, DR+2) that expose shorter to protect
highlights and lift the shadows with the curve. They only shift the exposure when the camera
picks the shutter speed: in Tv, M and long shutter mode, and with flash, the normal curve is used.

## Installation

The easy way is the download page (GitHub Pages of this repository): pick your camera and
looks, download the ZIP and follow the step-by-step guide there. In short:

1. Check the firmware version. The Windows installer can do it (*Check firmware…*), or copy
   `ver.req` and `vers.req` to the card and press *FUNC/SET + DISP* in playback mode.
2. Format the SD card **in the camera**. IXUS 60 and SX110 only start EXCCD automatically from
   cards up to 2 GB, IXUS 132/145 from cards up to 32 GB.
3. Run `exccd-installer.exe` (from the page or the [Releases](../../releases)): it unpacks the
   ZIP and makes the card bootable. It is not signed, so Windows asks *More info → Run anyway*.
4. Lock the card's write-protect switch and put it in the camera.

Without the installer (macOS, Linux): delete an old `CHDK` folder, unpack the ZIP to the top
level of the card and, on the IXUS 60, start EXCCD in playback mode with *MENU → Firm Update*.
*MENU → System → Make Card Bootable...* then makes it start on its own from a locked card.

EXCCD runs from the SD card only. Nothing is written to the camera's firmware; without the
card the camera is unchanged.

## Usage

| Button | In shooting mode |
|---|---|
| **Print** | Open the film selection with the active look, press again to close without changes |
| Left/right | Flip through the looks |
| Up/down | Jump five looks |
| **FUNC/SET** | Apply the look and close |
| **MENU** | Options (recipes, look settings, camera settings, raw, system) |

The film selection shows the keys at the bottom (*SET OK, MENU Options*). The Options menu is
grouped like this:

| Entry | What is in it | Saved in a recipe |
|---|---|---|
| Recipe | Load C1 to C4 with left/right | |
| Look: *film* ▸ | White balance, WB shift, Dynamic+, colour noise filter, B/W filter, Effects ▸ (grain, halation, mist, vignette, light leak, date, frame), Save as recipe ▸ | yes |
| Shutter, Focus, Zoom, Flash, ND filter, Double exposure | Settings for the situation | no |
| RAW ▸ | Save RAW, RAW develop, delete RAW files | no |
| Reset to defaults, System ▸ | Palette, language, build info, bootable card | no |

The name of the active look is shown at the bottom of the screen. For every photo,
`A/CHDK/FILMLOG.CSV` records which look was active. If a step had to be skipped for lack of
memory, the line says which (`low memory, skipped: ...`) and the status line shows `low mem!`
until a photo goes through completely again.

**RAW develop** (Options → RAW): render a saved DNG again with a different look. Set Options → RAW →
Save RAW to DNG when shooting. Later, choose RAW develop, pick the DNG and the look, then take one
photo: what is in front of the lens does not matter, the camera only needs a shot to run its
JPEG engine. The status line shows `Develop <file>: <look>` until then; choose RAW develop
again to cancel. You get a new JPEG. The DNG stays untouched, your loaded look comes back.

White balance, Dynamic+ level, date stamp and Auto colour noise filter come from the original
photo. The JPEG's Exif data (date, exposure, orientation) comes from the helper shot, so hold
the camera the same way and do not use digital zoom. `FILMLOG.CSV` names the source file in a
third column. Canon's JPEG engine only runs after a shot, so this cannot run in playback mode.

**Deleting RAW files** (Options → RAW): Canon's own delete only removes the JPEG, so DNGs pile up
on the card. *Delete RAW...* opens the same browser as RAW develop; SET deletes the DNG under the
cursor, also in full view (Right), after a yes/no question. *Delete RAW without JPEG* removes every
DNG whose JPEG you already deleted in Canon playback. *Delete all RAW* removes every DNG on the card.
Both say how many files and MB first; JPEGs are never touched.

**White balance** (Options → Look): sets the camera's own white balance (Auto, Daylight, Cloudy,
Tungsten, Fluorescent, Fluorescent H, Custom). **WB shift** fine-tunes the colour on top of
that, on a grid like the white balance shift some cameras offer: left/right moves from cyan to
red, up/down from yellow to blue, each −9 to +9. SET applies, MENU cancels, the zoom lever goes
back to 0. The shift is worked into the look's tone curve,
so it costs no time per photo. It only shows in the photo, not on the screen, and it is off
while the film is Off (the menu then shows "needs a look"). With black-and-white looks it works
like a weak colour filter. One step is about 3 %; this is not calibrated yet.

**Recipes** (Options → Recipe): a recipe stores the film, white balance, WB shift, Dynamic+,
colour noise filter, B/W filter and all effects in one of four slots, C1 to C4. Choosing a
recipe sets all of it at once, including the camera's white balance. Shutter, focus, zoom,
flash, ND, RAW and double exposure are left alone, they depend on the situation. To save one,
set everything up, then Options → Look → Save as recipe and pick a slot: the list shows the four
slots with their names, one press saves (a used slot asks first). A new slot is named after the film. If you change something after loading a recipe, the menu and the status
line show a `*` after its slot. The recipes live in `A/CHDK/FILMS/RECIPES.TXT`, a text file you
can edit on the PC to rename recipes, write new ones or share them (the file explains the
format). `wb = keep` leaves the camera's white balance as it is. Mistakes in the file end up in
`A/CHDK/FILMS/RECIPES.LOG`, and so does the reason when saving fails (file over 8 KB or
unreadable, card full). Saving on the camera rewrites the file, so your own comments in it
are lost.

## Building

Toolchain: [xPack GNU Arm Embedded GCC 10.3.1-2.3](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v10.3.1-2.3),
a host GCC, GNU make and Info-ZIP `zip`.

```sh
export PATH=/path/to/xpack-arm-none-eabi-gcc-10.3.1-2.3/bin:$PATH
tools/filmsim/build.sh                  # every camera in tools/filmsim/cameras.txt
tools/filmsim/build.sh ixus60_sd600     # one camera
```

Result: `dist/<version>/exccd-<version>-<platform>-<sub>-full.zip`, next to the build logs. The script runs `make clean`
before and after each camera, because CHDK shares the object files in `core/`, `lib/` and
`modules/` between all platforms.

On Windows, keep LF line endings. The repository's `.gitattributes` takes care of that;
an older checkout with `core.autocrlf=true` will not build.

Tests on the PC, for the `FILMS.TXT` parser and for the raw codec and 12-bit curves
(`core/filmsim_raw.h`, checked against CHDK's `get_raw_pixel`/`set_raw_pixel`):

```sh
gcc -Wall -Wextra -I core -o test_films_txt tools/filmsim/test_films_txt.c core/filmsim_txt.c
./test_films_txt CHDK/FILMS/EXAMPLE.TXT
gcc -Wall -Wextra -I core -DCAM_SENSOR_BITS_PER_PIXEL=10 -o test_raw10 tools/filmsim/test_raw.c && ./test_raw10
gcc -Wall -Wextra -I core -DCAM_SENSOR_BITS_PER_PIXEL=12 -o test_raw12 tools/filmsim/test_raw.c && ./test_raw12
```

To measure the processing time per shot on a camera, put an empty file `A/CHDK/FILMTIME.CSV` on the
card. EXCCD then adds one line per shot with the time of each step in ms and the free memory before
and after (total, largest block, number of blocks, in KB); without the file nothing is written.
A largest block that keeps shrinking over many shots means the memory is fragmenting.

When a core change must not touch another camera, `tools/filmsim/compare_build.sh <git-ref> <platform>`
builds both states with the same version and timestamp and compares `main.bin` and every object file.

GitHub Actions builds every push, one job per camera in `tools/filmsim/cameras.txt`.
A tag `vX.Y.Z` that matches `FILMSIM_VERSION` in `core/filmsim.h` creates a release with the
ZIPs of the cameras marked `stabil` and publishes the download page on GitHub Pages.
Cameras marked `test` are only build artifacts.

The download page is static. `tools/site/build_site.py` splits the firmware ZIP into a base
ZIP and one small ZIP per look; the browser copies the chosen entries into a new ZIP and adds
a matching `FILMS.TXT`, without a build per download. It takes one firmware ZIP per camera
and shows them in a camera menu; cameras marked `test` appear as beta. CI only passes the
`stabil` ones. To try it locally:

```sh
python tools/site/build_site.py dist/v1.10.0/*.zip _site
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

## Contributing

Bug reports, test results and pull requests are welcome. For a bug, please open an issue with
the camera, firmware, EXCCD version and, if possible, a photo (a photo of the display helps too).
New camera models are especially welcome: [`tools/filmsim/cams/README.md`](tools/filmsim/cams/README.md)
describes what a port needs. Code comments and internal notes are partly in German; English
issues and pull requests are perfectly fine.

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
