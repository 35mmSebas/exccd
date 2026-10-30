#!/usr/bin/env python3
"""
Erzeugt die Tonkurven-Dateien (*.CV) fuer die Filmsimulationen der IXUS 60 (SD600).

Format (CHDK curves.flt, "custom RGB"):
  4 Kurven x 1024 Werte, unsigned short little-endian, zusammen 8192 Bytes.
  Reihenfolge laut curves.c + CFA-Muster der SD600 (R G / G B):
     curve0 = R, curve1 = G (Zeile 1), curve2 = G (Zeile 2), curve3 = B
  curve0[0] muss <= 1023 sein, sonst haelt CHDK die Datei fuer eine L-Kurve.

Die Kurven wirken auf die linearen 10-Bit-Rohdaten, also VOR Weissabgleich und
Canon-Gamma. Damit der Look trotzdem planbar ist, wird jede Kurve im
"Anzeige-Raum" definiert: Rohwert -> * typischer WB-Faktor -> Gamma 2.2 ->
Tonkurve/Farbverschiebung -> zurueck. Oberhalb des Clipping-Punkts bleibt alles
unveraendert (Identitaet), damit es keine Spruenge gibt.
"""
import struct, os, sys, math

BLACK = 31
WHITE = 1023
GAMMA = 2.2
# grobe Tageslicht-WB-Faktoren der SD600 (R, G, G, B) relativ zu Gruen
WB = (1.90, 1.0, 1.0, 1.45)

# ---------------------------------------------------------------------------
# Rezepte. Alle Werte im Anzeige-Raum (0..1, Gamma 2.2).
#   contrast : S-Kurve um 0.5 (0 = neutral, 0.3 = deutlich, <0 = flacher)
#   lift     : Schwarzwert anheben (0.03 = leicht matt)
#   shoulder : weiches Abrollen der Lichter (0..0.3)
#   shadow   : (R,G,B) Farbverschiebung in den Schatten  (+/- 0.03 = deutlich)
#   highlight: (R,G,B) Farbverschiebung in den Lichtern
#   gain     : (R,G,B) Kanal-Gewichtung, v.a. fuer Schwarzweiss-Filter-Effekte
# ---------------------------------------------------------------------------
RECIPES = {
    "POSTCRD": dict(contrast=0.28, lift=0.00, shoulder=0.05,
                    shadow=(0.00, 0.00, 0.010), highlight=(0.010, 0.00, -0.005)),
    "BLUSH":   dict(contrast=0.10, lift=0.01, shoulder=0.15,
                    shadow=(0.00, 0.00, 0.005), highlight=(0.008, 0.003, 0.00)),
    "SCENE": dict(contrast=-0.12, lift=0.04, shoulder=0.25,
                    shadow=(-0.012, 0.004, 0.010), highlight=(0.006, 0.002, -0.004)),
    "SILVER":  dict(contrast=0.26, lift=0.00, shoulder=0.08,
                    gain=(1.10, 1.0, 0.92)),          # leichter Gelbfilter-Effekt
    # Stuetzpunkt-Looks: je Kanal (x, y) im Anzeige-Raum, PCHIP-interpoliert,
    # (0,0) und (1,1) werden ergaenzt
    "JOURNAL": dict(points=dict(             # Journal: harte Schatten, kuehle Tiefen, gedaempft
        R=((.08, .04),  (.25, .19),  (.5, .48),  (.75, .77),  (.92, .92)),
        G=((.08, .06),  (.25, .212), (.5, .49),  (.75, .765), (.92, .915)),
        B=((.08, .055), (.25, .215), (.5, .49),  (.75, .75),  (.92, .905)))),
    "STUDIO":  dict(points=dict(             # Studio: flach, offene Schatten
        R=((.08, .097), (.25, .272), (.5, .507), (.75, .738), (.92, .897)),
        G=((.08, .095), (.25, .27),  (.5, .505), (.75, .735), (.92, .895)),
        B=((.08, .095), (.25, .268), (.5, .503), (.75, .733), (.92, .894)))),
    "DRUGSTR": dict(points=dict(             # Drugstore: hart, cyan Schatten, warme Lichter
        R=((.08, .035), (.25, .175), (.5, .49),  (.75, .80),  (.92, .94)),
        G=((.08, .062), (.25, .212), (.5, .505), (.75, .785), (.92, .93)),
        B=((.08, .065), (.25, .215), (.5, .48),  (.75, .74),  (.92, .895)))),
}

def pchip(pts):
    """Monotone kubische Interpolation (Fritsch-Carlson, wie scipy PchipInterpolator)."""
    xs = [0.0] + [p[0] for p in pts] + [1.0]
    ys = [0.0] + [p[1] for p in pts] + [1.0]
    n = len(xs)
    h = [xs[i + 1] - xs[i] for i in range(n - 1)]
    d = [(ys[i + 1] - ys[i]) / h[i] for i in range(n - 1)]
    m = [0.0] * n
    for i in range(1, n - 1):
        if d[i - 1] * d[i] > 0:
            w1, w2 = 2 * h[i] + h[i - 1], h[i] + 2 * h[i - 1]
            m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
    def edge(h0, h1, d0, d1):
        e = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1)
        if e * d0 <= 0: return 0.0
        if d0 * d1 <= 0 and abs(e) > abs(3 * d0): return 3 * d0
        return e
    m[0] = edge(h[0], h[1], d[0], d[1])
    m[-1] = edge(h[-1], h[-2], d[-1], d[-2])
    def f(t):
        t = min(max(t, 0.0), 1.0)
        i = 0
        while i < n - 2 and t > xs[i + 1]: i += 1
        s = (t - xs[i]) / h[i]
        h00, h10 = (1 + 2 * s) * (1 - s) ** 2, s * (1 - s) ** 2
        h01, h11 = s * s * (3 - 2 * s), s * s * (s - 1)
        return h00 * ys[i] + h10 * h[i] * m[i] + h01 * ys[i + 1] + h11 * h[i] * m[i + 1]
    return f

def tone(t, r):
    k = r.get("contrast", 0.0)
    y = t + k * (t - 0.5) * t * (1 - t) * 4          # S-Kurve, Endpunkte fix
    s = r.get("shoulder", 0.0)
    if s:                                              # weiches Abrollen oben
        y = y - s * (y ** 3) * (1 - y) * 2
    lift = r.get("lift", 0.0)
    return lift + (1 - lift) * y

def dr_gain(lin, G):
    """Dynamik+: knapp belichtetes Bild wieder aufhellen, Lichter weich
    zusammenfassen. Steigung G in den Schatten, f(1) = 1 (kein Sprung)."""
    if G <= 1: return lin
    return lin * G / (1 + (G - 1) * lin)

def channel_curve(ch, r, G=1):
    rgb = (0, 1, 1, 2)[ch]
    g = WB[ch]
    sh = r.get("shadow", (0, 0, 0))[rgb]
    hi = r.get("highlight", (0, 0, 0))[rgb]
    gain = r.get("gain", (1, 1, 1))[rgb]
    pts = r.get("points")
    look = pchip(pts["RGB"[rgb]]) if pts else None
    out = []
    for v in range(1024):
        if v <= BLACK:
            out.append(v); continue
        x = (v - BLACK) / (WHITE - BLACK)              # linear 0..1
        lin = x * g * gain
        if lin >= 1.0:
            out.append(v); continue
        lin_dr = dr_gain(lin, G)
        t = lin_dr ** (1 / GAMMA)
        if look:
            y = look(t)
        else:
            y = tone(t, r)
        y += sh * 6.75 * t * (1 - t) ** 2              # Glocke um 1/3
        y += hi * 6.75 * t * t * (1 - t)               # Glocke um 2/3
        y = min(max(y, 0.0), 1.0)
        ylin = (y ** GAMMA) / (g * gain)
        o = BLACK + ylin * (WHITE - BLACK)
        out.append(int(round(min(max(o, 0), WHITE))))
    # monoton erzwingen
    for i in range(1, 1024):
        if out[i] < out[i - 1]:
            out[i] = out[i - 1]
    return out

def test_curves():
    # Diagnose-Kurve: Rot stark angehoben, Blau abgesenkt -> unuebersehbarer Rotstich
    def mk(f):
        return [v if v <= BLACK else int(min(WHITE, max(BLACK, BLACK + f((v - BLACK) / (WHITE - BLACK)) * (WHITE - BLACK)))) for v in range(1024)]
    r = mk(lambda x: min(1.0, x * 2.5))
    g = mk(lambda x: x)
    b = mk(lambda x: x * 0.5)
    return [r, g, g, b]

def write(name, r, outdir, G=1):
    curves = test_curves() if name == "TEST" else [channel_curve(c, r, G) for c in range(4)]
    assert curves[0][0] <= 1023
    data = b"".join(struct.pack("<1024H", *c) for c in curves)
    assert len(data) == 8192
    with open(os.path.join(outdir, name + ".CV"), "wb") as f:
        f.write(data)

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--test"]
    outdir = args[0] if args else "."
    os.makedirs(outdir, exist_ok=True)
    extra = [("TEST", {})] if "--test" in sys.argv else []
    for n, r in list(RECIPES.items()) + extra:
        write(n, r, outdir)
        print("->", n + ".CV")
        if n != "TEST":
            # Dynamik+ Varianten: 1 = -1 EV belichtet (Faktor 2), 2 = -2 EV (Faktor 4)
            for lvl, G in ((1, 2.0), (2, 4.0)):
                write(n + str(lvl), r, outdir, G)
    # nur Dynamik+, ohne Filmkurve (fuer "Aus" und "Daylight 100")
    for lvl, G in ((1, 2.0), (2, 4.0)):
        write("DR" + str(lvl), {}, outdir, G)
