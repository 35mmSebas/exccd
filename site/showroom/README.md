# Showroom

The `#compare` page (folder name stays showroom): one photo per scene, developed in many looks, with a slider to compare each look
against Daylight 100.

- Scenes are listed in `scenes.txt` (id, camera, source DNG, title), in the order the page shows them.
- Each scene folder holds `<LABEL>.jpg` per look (label column of `CHDK/FILMS/EXAMPLE.TXT`).
  `DAYLITE.jpg` is the neutral reference and must be there.
- `<LABEL>-sim.jpg` is a PC preview (`tools/filmsim/simulate_looks.py`); the page marks it as *simulated*.
  A real `<LABEL>.jpg` always wins over the simulation.
- 1600 px on the long side, no Exif. Only your own photos, CC BY-SA 4.0 like the looks.

## Filling it from the camera

1. Shoot the scene with DNG on (Daylight 100 is the best film for the original).
2. On the camera: *RAW* → *RAW develop*, pick the scene's DNG, pick a film, take the helper shot.
   Repeat for every look you want to show. `FILMLOG.CSV` notes film and source DNG for each JPEG.
3. Card in the PC, then:

   ```
   python tools/site/showroom.py import M:\ --dry-run
   python tools/site/showroom.py import M:\
   ```

   It sorts the developed JPEGs into the scene folders by source DNG and film, scales them and removes the
   simulation of the same look. The original Daylight 100 JPEG of the scene becomes `DAYLITE.jpg`.

New scene: add a line to `scenes.txt`. PC previews until the real ones exist:
`python tools/site/showroom.py sim <scene> <daylight.jpg> POSTCRD CHROME ...`

`tools/site/build_site.py` lists everything in `data/site.json`.
