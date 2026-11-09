# Sample photos

Example photos for the download page, shown next to the label of each look.

- Name: `<LABEL>-<n>.jpg`, for example `POSTCRD-1.jpg`, `POSTCRD-2.jpg`
  (`<LABEL>` is the label column of `CHDK/FILMS/EXAMPLE.TXT`)
- Straight out of the camera with that look, scaled to 1200 px on the long side
- A preview rendered on the PC (`tools/filmsim/simulate_looks.py`) instead of a real shot is named
  `<LABEL>-sim-<n>.jpg`; the page marks it as *simulated*. Replace it with a real photo when you have one
- Only your own photos, published under CC BY-SA 4.0 like the looks

`tools/site/build_site.py` picks them up automatically.
