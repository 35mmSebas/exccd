# Looks – overview

As of v1.9.25 (sources: `sims_builtin[]` in `core/filmsim.c`, `CHDK/FILMS/EXAMPLE.TXT`,
`RECIPES` in `tools/filmsim/make_curves.py`). Please update this file when they change.

## 1. Camera settings (FILMS.TXT)

My Colors: 0 = Off, 3 = B/W, 11 = Custom Color. The sliders Con … Skin (−2..+2) only work
**with mode 11**. Mix: 0 = none, 1 = purple effect (colour swap), 2 = desaturated.

| # | Look          | My Colors | Con | Sharp.  | Sat.  | R  | G  | B  | Skin | Curve      | Mix | Label   |
|---|---------------|-----------|----:|--------:|------:|---:|---:|---:|-----:|------------|----:|---------|
| 0 | Off           | Off       |  0  |   0     |   0   | 0  | 0  | 0  |  0   | –          | 0   | AUS     |
| 1 | Daylight 100  | Off       |  0  |   0     |   0   | 0  | 0  | 0  |  0   | –          | 0   | DAYLITE |
| 2 | Postcard 100  | Custom    |  0  |  +1     |  +2   | +1 | +1 | +1 |  0   | POSTCRD.CV | 0   | POSTCRD |
| 3 | Blush 100     | Custom    | −1  |   0     |  +1   | 0  | 0  | 0  | +1   | BLUSH.CV   | 0   | BLUSH   |
| 4 | Journal 400   | Custom    |  0  |   0     |  −1   | 0  | −1 | 0  |  0   | JOURNAL.CV | 0   | JOURNAL |
| 5 | Studio 160    | Custom    | −1  |  −1     |  −1   | 0  | 0  | 0  | +1   | STUDIO.CV  | 0   | STUDIO  |
| 6 | Drugstore 200 | Custom    |  0  |   0     |  −1   | 0  | +1 | −1 |  0   | DRUGSTR.CV | 0   | DRUGSTR |
| 7 | Scene 500     | Custom    | −2  |  −1     |  −2   | 0  | 0  | 0  |  0   | SCENE.CV   | 0   | SCENE   |
| 8 | Silver 400    | B/W       |  –  |   –     |   –   | –  | –  | –  |  –   | SILVER.CV  | 0   | SILVER  |
| 9 | Portrait 400  | Custom    | −1  |  −1     |   0   | 0  | 0  | −1 | +1   | PORTRT.CV  | 0   | PORTRT  |
|10 | Tungsten 800  | Off       |  –  |   –     |   –   | –  | –  | –  |  –   | TUNGSTN.CV | 0   | TUNGSTN |
|11 | Expired 200   | Custom    | −1  |  −1     |  −1   | 0  | 0  | 0  |  0   | EXPIRED.CV | 0   | EXPIRED |
|12 | Cross 100     | Custom    |  0  |  +1     |  +2   | 0  | 0  | 0  |  0   | CROSS.CV   | 0   | CROSS   |
|13 | Violet 400    | Custom    |  0  |   0     |  +1   | 0  | 0  | 0  |  0   | VIOLET.CV  | 1   | VIOLET  |
|14 | Concrete 100  | Custom    |  0  |  +1     |  −1   | 0  | 0  | 0  |  0   | CONCRET.CV | 2   | CONCRET |
|15 | Ember 200     | Custom    |  0  |   0     |   0   | 0  | 0  | 0  |  0   | EMBER.CV   | 0   | EMBER   |
|16 | Grit 400      | B/W       |  –  |   –     |   –   | –  | –  | –  |  –   | GRIT.CV    | 0   | GRIT    |

## 2. Tone curves (make_curves.py)

Parametric recipes: `contrast` = S-curve (0.3 = strong, <0 flatter), `lift` =
raise the black point (matte), `shoulder` = soft highlight roll-off. Colour shift
as (R, G, B), ±0.03 = clearly visible. Point-curve looks are drawn freely per channel.

| Look         | Type       | Contrast | Lift | Shoulder | Shadows (R,G,B)         | Highlights (R,G,B)      | Character |
|--------------|------------|---------:|-----:|---------:|-------------------------|-------------------------|-----------|
| Postcard     | recipe     |  0.20    | 0.03 | 0.05     | 0 / 0 / +0.010          | +0.010 / 0 / −0.005     | crisp, saturated, open shadows |
| Blush        | recipe     |  0.10    | 0.01 | 0.15     | 0 / 0 / +0.005          | +0.008 / +0.003 / 0     | soft, rosy highlights |
| Journal      | points     | slight S | –    | –        | neutral                 | neutral                 | documentary, muted (measured) |
| Studio       | points     | flat     | –    | –        | open                    | slightly lowered        | flat, open shadows |
| Drugstore    | points     | hard     | –    | –        | cyan                    | warm                    | consumer film look |
| Scene        | recipe     | −0.12    | 0.04 | 0.25     | −0.012 / +0.004 / +0.010| +0.006 / +0.002 / −0.004| very flat, cool shadows |
| Silver (B/W) | recipe     |  0.26    | 0.00 | 0.08     | Gain 1.10 / 1.0 / 0.92  |                         | B/W, light yellow filter |
| Portrait     | recipe     |  0.05    | 0.02 | 0.20     | +0.004 / +0.002 / −0.002| +0.012 / +0.004 / −0.006| soft, warm skin tones |
| Tungsten     | points     | medium   | –    | –        | teal                    | warm specular highlights| cool, tungsten film |
| Expired      | recipe     | −0.05    | 0.09 | 0.20     | +0.028 / −0.015 / +0.018| +0.005 / +0.022 / −0.028| very matte, magenta shadows, yellow-green highlights |
| Cross        | points     | hard     | –    | –        | blue                    | yellow-green            | cross processing |
| Violet       | recipe     |  0.15    | 0.02 | 0.10     | +0.006 / −0.004 / +0.010| +0.006 / 0 / +0.004     | + Mix 1: purple colour swap |
| Concrete     | recipe     |  0.28    | 0.02 | 0.10     | 0 / 0 / +0.004          | +0.004 / +0.002 / 0     | + Mix 2: desaturated, hard |
| Ember        | points     | medium   | –    | –        | red                     | orange to peach         | red to orange gradient, muted blue |
| Grit (B/W)   | recipe     |  0.38    | 0.00 | 0.04     | Gain 1.20 / 1.0 / 0.80  |                         | B/W, hard, orange filter |

Daylight 100 and Off have no film curve (only the Dynamic+ curves, if switched on).

## 3. Notes / open points

| Look | Version | Note |
|------|-------|-------|
| Postcard, Journal, Drugstore | v1.9.21 | shadows opened up, Canon contrast +1 removed |
| Journal | v1.9.24 | curve measured against reference shots |
| Ember | v1.9.24 | toned down a lot (pictures were completely red) |
| Ember | 2026-11-07 | shadows and midtones less red, green rises earlier for a red to orange gradient |
