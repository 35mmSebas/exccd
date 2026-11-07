#!/usr/bin/env python3
"""
Stimmt die Mischmatrizen (Turquoise, Infra ...) an echten Fotos einer Kamera ab, statt an der
PC-Simulation. Die Kamera mischt im Rohbild, vor Canons Farbmatrix und Tonkurve. Eine Mischung
im fertigen Bild (simulate_looks.py) wirkt dort deshalb anders.

Bausteine:
  - DNG lesen (rawpy), 2x2-Bayer-Bloecke, Weissabgleich aus AsShotNeutral
  - EXCCD-Kette wie auf der Kamera: Kurve (.CV) und mix_block (filmsim_raw.h), Gleitkomma
  - Canon-Modell: 3x3-Farbmatrix und Tonkurve je Kanal, angepasst an ein Paar aus DNG und dem
    JPEG der Kamera (das JPEG entstand aus denselben Rohdaten)

  fit_mix.py check  <dng-ordner>               Canon-Modelle anpassen und gegenpruefen
  fit_mix.py fit    <dng-ordner> <look> [...]  Matrix optimieren, Vorher/Nachher als JPG

Die Paare sind fuer die SX110-Fotos vom 07.11.2026 eingetragen (PAIRS). Fuer eine andere Kamera
WB_R/WB_B/BLK und die Paare anpassen.
"""
import os, re, sys
import numpy as np
import rawpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)

WB_R, WB_B, BLK10 = 470, 441, 31        # SX110: FILMSIM_WB_R/_B, Schwarz auf der 10-Bit-Skala
DS = 8                                   # Bloecke je Rasterpunkt (16 Sensorpixel)

# Rohbild, Kamera-JPEG, Kurve, Mischung (Index wie filmsim_raw.h), Canon-Einstellung
# Violet-Fotos vom 05./06.11.: Damals galten vielleicht noch die WB-Werte der IXUS 60 (486/371),
# die SX110-Werte kamen am 07.11. dazu. check() vergleicht beide.
PAIRS = {
    "off_1030":   ("CRW_1030.DNG", "IMG_1030.JPG", None, 0, "off"),
    "day_1031":   ("CRW_1031.DNG", "IMG_1031.JPG", None, 0, "off"),          # Daylight 100 = MC_OFF
    "off_1071":   ("CRW_1071.DNG", "IMG_1071.JPG", None, 0, "off"),
    "off_1073":   ("CRW_1073.DNG", "IMG_1073.JPG", None, 0, "off"),
    "vio_1043":   ("CRW_1043.DNG", "IMG_1043.JPG", "VIOLET.CV", 1, "sat1"),
    "vio_1047":   ("CRW_1047.DNG", "IMG_1047.JPG", "VIOLET.CV", 1, "sat1"),
    "turq_1072":  ("CRW_1071.DNG", "IMG_1072.JPG", "TURQUOI.CV", 3, "sat1"),
    "turq_1076":  ("CRW_1076.DNG", "IMG_1076.JPG", "TURQUOI.CV", 3, "sat1"),
    "infra_1125": ("CRW_1125.DNG", "IMG_1125.JPG", "INFRA1.CV", 4, "sat1"),   # mit Dynamik+ 1
}
OLD_WB = {}                              # Paar -> (WB_R, WB_B), falls nicht die aktuellen


def matrices():
    src = open(os.path.join(ROOT, "core", "filmsim_raw.h"), encoding="ascii").read()
    out = {}
    for k, n in {1: "violet", 2: "concrete", 3: "turquoise", 4: "infra", 5: "cyano"}.items():
        m = re.search(r"mix_%s\[3\]\[3\] = \{(.*?)\};" % n, src, re.S)
        body = re.sub(r"//.*", "", m.group(1))
        out[k] = np.array([int(x) for x in re.findall(r"-?\d+", body)], float).reshape(3, 3)
    return out


def load_dng(path):
    """Bloecke (H, W, 4) R G1 G2 B auf der 12-Bit-Skala, Schwarz, WB-Faktoren (R, G, B)."""
    with rawpy.imread(path) as r:
        a = r.raw_image.astype(np.float32)
        black = float(np.mean(r.black_level_per_channel))
        wb = np.array(r.camera_whitebalance[:3], float)
    q = np.stack([a[0::2, 0::2], a[0::2, 1::2], a[1::2, 0::2], a[1::2, 1::2]], 2)
    return q, black, wb / wb[1]


def curve(q, name):
    if not name:
        return q
    cv = np.fromfile(os.path.join(ROOT, "CHDK", "CURVES", name), "<u2").reshape(4, 1024).astype(float)
    cv = np.minimum(cv, 1023)
    x = np.arange(1024)
    return np.stack([4 * np.interp(q[..., c] / 4, x, cv[c]) for c in range(4)], 2)


def mix(q, m, wbr=None, wbb=None):
    """mix_block in Gleitkomma, 12-Bit rein und raus. m: 3x3 in 1/256."""
    if m is None:
        return q
    WB_R, WB_B = wbr or globals()["WB_R"], wbb or globals()["WB_B"]
    v = q / 4
    r = (v[..., 0] - BLK10) * WB_R / 256
    g1, g2 = v[..., 1] - BLK10, v[..., 2] - BLK10
    b = (v[..., 3] - BLK10) * WB_B / 256
    gg = g1 + g2
    nr = (2 * m[0, 0] * r + m[0, 1] * gg + 2 * m[0, 2] * b) / 512
    dg = (2 * m[1, 0] * r + (m[1, 1] - 256) * gg + 2 * m[1, 2] * b) / 512
    nb = (2 * m[2, 0] * r + m[2, 1] * gg + 2 * m[2, 2] * b) / 512
    out = np.stack([nr * 256 / WB_R, g1 + dg, g2 + dg, nb * 256 / WB_B], 2) + BLK10
    return np.clip(out, 0, 1023) * 4


def to_lin(q, black, wb):
    """Bloecke -> lineares RGB mit Canons Weissabgleich der Aufnahme (0..1)."""
    rgb = np.stack([q[..., 0], (q[..., 1] + q[..., 2]) / 2, q[..., 3]], 2) - black
    # Wie Canon: Was ueber der Saettigung liegt, wird weiss statt rosa (R/B mit WB > 1 reichen sonst weiter)
    return np.clip(np.clip(rgb, 0, None) / (4095 - black) * wb, 0, 1)


def shrink(x, f):
    h, w = x.shape[0] // f * f, x.shape[1] // f * f
    return x[:h, :w].reshape(h // f, f, w // f, f, -1).mean((1, 3))


def load_jpeg(path):
    return np.asarray(Image.open(path).convert("RGB"), np.float32) / 255


# JPEG-Ausschnitt im Rohbild (Sensorpixel). Einmal je Kamera per Korrelation bestimmt.
_offset = {}


def register(lin, jpg):
    """Versatz des JPEG im Blockraster: (by, bx) so, dass jpg[2y, 2x] ~ lin[by + y, bx + x]."""
    key = jpg.shape
    if key in _offset:
        return _offset[key]
    L = np.log(lin[..., 1] + 1e-3)
    J = np.log(jpg[..., 1] ** 2.2 + 1e-3)
    J2 = shrink(J[..., None], 2)[..., 0]
    h, w = J2.shape
    best = None
    for by in range(0, L.shape[0] - h + 1, 2):
        for bx in range(0, L.shape[1] - w + 1, 2):
            a = L[by:by + h:8, bx:bx + w:8].ravel()
            b = J2[::8, ::8].ravel()
            c = np.corrcoef(a, b)[0, 1]
            if best is None or c > best[0]:
                best = (c, by, bx)
    c, by0, bx0 = best
    for by in range(max(0, by0 - 2), by0 + 3):
        for bx in range(max(0, bx0 - 2), bx0 + 3):
            if by + h > L.shape[0] or bx + w > L.shape[1]:
                continue
            c = np.corrcoef(L[by:by + h:4, bx:bx + w:4].ravel(), J2[::4, ::4].ravel())[0, 1]
            if c > best[0]:
                best = (c, by, bx)
    _offset[key] = best[1:]
    print("  JPEG-Ausschnitt: Bloecke y %d x %d (Korrelation %.3f)" % (best[1], best[2], best[0]))
    return best[1:]


# Matrizen, mit denen die Kamera die Paare aufgenommen hat (Stand v1.10.0-lab vor dem Abstimmen).
# Nicht aus filmsim_raw.h lesen: Dort stehen nach dem Abstimmen die neuen Werte.
SHOT_MATS = {
    1: np.array([[128, 128, 0], [51, 77, 128], [51, 205, 0]], float),        # Violet
    3: np.array([[38, 38, 180], [51, 154, 51], [141, 90, 25]], float),       # Turquoise
    4: np.array([[108, 136, 12], [154, 64, 38], [13, 38, 205]], float),      # Infra
}


def sample(folder, name, mats, m_override=None):
    """Rasterpunkte (N, 3): lineares Eingangs-RGB fuer Canon und JPEG-Wert."""
    dng, jp, cv, mi, _ = PAIRS[name]
    q, black, wb = load_dng(os.path.join(folder, dng))
    m = m_override if m_override is not None else (SHOT_MATS[mi] if mi else None)
    lin = to_lin(mix(curve(q, cv), m, *OLD_WB.get(name, (None, None))), black, wb)
    jpg = load_jpeg(os.path.join(folder, jp))
    by, bx = register(to_lin(q, black, wb), jpg)
    h, w = jpg.shape[0] // 2, jpg.shape[1] // 2
    X = shrink(lin[by:by + h, bx:bx + w], DS)
    Y = shrink(jpg[:h * 2, :w * 2], 2 * DS)
    return X.reshape(-1, 3), Y.reshape(-1, 3), X.shape[:2]


class Canon:
    """Y = T(X @ M.T): Farbmatrix und Tonkurve je Kanal (monoton, stueckweise linear in log)."""
    KN = np.linspace(-9, 1.2, 60)

    def fit(self, X, Y, it=12):
        ok = (Y.max(1) < 0.97) & (Y.min(1) > 0.02) & (X.min(1) > 1e-5)
        X, Y = X[ok], Y[ok]
        Z = Y ** 2.2
        for _ in range(it):
            self.M = np.linalg.lstsq(X, Z, rcond=None)[0].T
            P = np.clip(X @ self.M.T, 1e-6, None)
            self.T = []
            for c in range(3):
                lp = np.log2(P[:, c])
                idx = np.digitize(lp, self.KN)
                ys = np.array([np.median(Y[idx == i, c]) if np.sum(idx == i) > 8 else np.nan for i in range(len(self.KN) + 1)])
                xs = np.concatenate([[self.KN[0] - 0.1], (self.KN[:-1] + self.KN[1:]) / 2, [self.KN[-1] + 0.1]])
                good = ~np.isnan(ys)
                t = np.interp(xs, xs[good], ys[good])
                self.T.append((xs, np.maximum.accumulate(t)))
            # Rueckweg: Z = T^-1(Y)
            Z = np.stack([2 ** np.interp(Y[:, c], self.T[c][1] + np.arange(len(self.T[c][1])) * 1e-7, self.T[c][0]) for c in range(3)], 1)
        return self

    def __call__(self, X):
        P = np.log2(np.clip(X @ self.M.T, 1e-6, None))
        return np.clip(np.stack([np.interp(P[:, c], *self.T[c]) for c in range(3)], 1), 0, 1)


def err(pred, Y):
    ok = (Y.max(1) < 0.97) & (Y.min(1) > 0.02)
    d = (pred[ok] - Y[ok]) * 255
    return np.sqrt((d ** 2).mean()), d.mean(0)


def fit_group(data, keys):
    return Canon().fit(np.concatenate([data[k][0] for k in keys]), np.concatenate([data[k][1] for k in keys]))


def check(folder):
    mats = matrices()
    for name in ("vio_1043", "vio_1047"):
        res = []
        for wbs in ((None, None), (486, 371)):
            OLD_WB[name] = wbs
            X, Y, _ = sample(folder, name, mats)
            res.append((Canon().fit(X, Y)(X) - Y).std())
        OLD_WB[name] = (None, None) if res[0] <= res[1] else (486, 371)
        print("  %s: WB-Werte %s passen besser (Rest %.4f / %.4f)" % (name, "SX110" if res[0] <= res[1] else "IXUS 60", *res))
    data = {k: sample(folder, k, mats) for k in PAIRS}
    print("Jedes Paar vorhergesagt aus einem Modell, das es NICHT gesehen hat (RMS 8 Bit, Abweichung R G B):")
    for k in PAIRS:
        group = [j for j in PAIRS if PAIRS[j][4] == PAIRS[k][4] and j != k and PAIRS[j][0] != PAIRS[k][0]]
        e, bias = err(fit_group(data, group)(data[k][0]), data[k][1])
        e_in, _ = err(fit_group(data, group + [k])(data[k][0]), data[k][1])
        print("  %-11s ohne %5.1f  mit %5.1f   R %+5.1f G %+5.1f B %+5.1f" % (k, e, e_in, *bias))
    return data




# ---------------------------------------------------------------------------------------------
# Ziele und Optimierung

def neutral(folder, dng, off):
    """Canons neutrales Bild (Off) aus der DNG, im Raster von sample()."""
    q, black, wb = load_dng(os.path.join(folder, dng))
    lin = to_lin(q, black, wb)
    by, bx = _offset[(2592, 3456, 3)]
    X = shrink(lin[by:by + 1296, bx:bx + 1728], DS)
    return X, off(X.reshape(-1, 3)).reshape(X.shape)


def intended(img, label):
    """Bisherige Absicht des Looks: simulate_looks.py auf Canons neutralem Bild."""
    import simulate_looks as sl
    look = sl.table()[label]
    return sl.simulate(img, look, sl.mix_matrices())


def save_row(path, imgs, scale=3):
    ims = [Image.fromarray((np.clip(i, 0, 1) * 255).astype(np.uint8)) for i in imgs]
    ims = [i.resize((i.width * scale, i.height * scale), Image.LANCZOS) for i in ims]
    out = Image.new("RGB", (sum(i.width for i in ims) + 8 * (len(ims) - 1), max(i.height for i in ims)), "white")
    x = 0
    for i in ims:
        out.paste(i, (x, 0))
        x += i.width + 8
    out.save(path, quality=90)


# Neues Ziel fuer Infra (Falschfarben-Infrarot), im linearen Anzeige-Raum auf Canons neutralem Bild:
# Gruen -> Rot, Rot -> Gelb/Orange, Himmel tiefblau, Grau bleibt (Zeilensumme 1), dann kraeftiger.
AERO_SAT = 1.4
AERO = np.array([[0.30, 1.20, -0.50], [0.85, 0.15, 0.00], [-0.30, 0.40, 0.90]])


def target(img, look):
    if look == "infra":
        lin = np.clip(img, 0, 1) ** 2.2 @ AERO.T
        y = lin @ np.array([0.2126, 0.7152, 0.0722])
        lin = y[..., None] + AERO_SAT * (lin - y[..., None])
        return np.clip(np.clip(lin, 0, 1) ** (1 / 2.2), 0, 1)
    return intended(img, {"turquoise": "TURQUOI"}[look])


# Trainingsfotos (Tageslicht, Laub, Haut, Himmel) und die Kurve, mit der die Kamera sie bekaeme
TRAIN = ["CRW_1071", "CRW_1077", "CRW_1082", "CRW_1088", "CRW_1089", "CRW_1092", "CRW_1096", "CRW_1097",
         "CRW_1102", "CRW_1125", "CRW_1126", "CRW_1134"]
LOOKS = {"turquoise": (3, "TURQUOI"), "infra": (4, "INFRA")}


def grid(folder, name):
    """Bloecke im JPEG-Ausschnitt, schon verkleinert (Kurve und Mischung dann im Raster)."""
    q, black, wb = load_dng(os.path.join(folder, name + ".DNG"))
    by, bx = _offset[(2592, 3456, 3)]
    g = shrink(q[by:by + 1296, bx:bx + 1728], DS)
    with rawpy.imread(os.path.join(folder, name + ".DNG")) as r:
        pass
    head = open(os.path.join(folder, name + ".DNG"), "rb").read(8192)
    import struct
    ifd = struct.unpack_from("<I", head, 4)[0]
    dr = 0
    for i in range(struct.unpack_from("<H", head, ifd)[0]):
        e = ifd + 2 + 12 * i
        if struct.unpack_from("<H", head, e)[0] == 0x10E and head[e + 8:e + 10] == b"DR":
            dr = head[e + 10] - 48
    return g, black, wb, dr


def fit(folder, looks):
    from scipy.optimize import minimize
    data = check(folder)
    off = fit_group(data, [k for k in PAIRS if PAIRS[k][4] == "off"])
    sat = fit_group(data, [k for k in PAIRS if PAIRS[k][4] == "sat1"])
    mats = matrices()
    imgs = []
    for n in TRAIN:
        g, black, wb, dr = grid(folder, n)
        neu = off(to_lin(g, black, wb).reshape(-1, 3)).reshape(*g.shape[:2], 3)
        imgs.append((n, g, black, wb, dr, neu))
    out_dir = sys.argv[-1] if sys.argv[-1].endswith("/") else None
    result = {}
    for look in looks:
        mi, cv = LOOKS[look]
        prep = []
        for n, g, black, wb, dr, neu in imgs:
            cname = cv + (str(dr) if dr else "") + ".CV"
            prep.append((n, curve(g, cname), black, wb, neu, target(neu, look)))

        def render(m, c, black, wb, shape):
            return sat(to_lin(mix(c, m), black, wb).reshape(-1, 3)).reshape(*shape, 3)

        def full(p):
            p = np.asarray(p).reshape(3, 2)
            return np.concatenate([p, 256 - p.sum(1, keepdims=True)], 1)

        def loss(p):
            m = full(p)
            e = 0
            for n, c, black, wb, neu, tgt in prep:
                d = render(m, c, black, wb, neu.shape[:2]) - tgt
                # Nur Farbe zaehlt: gleiche Abweichung in allen drei Kanaelen ist Helligkeit (Tonkurve,
                # Canons Belichtung), die die Matrix nicht aendern soll (Zeilensumme 256)
                d = d - d.mean(-1, keepdims=True)
                e += (d ** 2).mean()
            return e / len(prep) + 1e-8 * (m ** 2).sum()

        m0 = mats[mi]
        r0 = loss(m0[:, :2].ravel())
        # Startwerte: die bisherige Matrix und die Wunschmatrix des Anzeige-Raums, in Canons Rohfarbraum
        # uebersetzt (M^-1 A M; Canons Farbmatrix ist linear, nur Kurve und Kappen stoeren)
        A = {"infra": AERO, "turquoise": None}[look]
        if A is not None:
            A = (np.eye(3) * AERO_SAT + (1 - AERO_SAT) * np.outer(np.ones(3), [0.2126, 0.7152, 0.0722])) @ A
        else:
            import simulate_looks as sl
            A = sl.mix_matrices()[mi]
        Mc = sat.M / sat.M.sum(1, keepdims=True)
        # mix_block rechnet mit festen Tageslicht-Faktoren, Canon mit denen des Fotos: fuer Tageslicht gleich
        a0 = np.linalg.inv(Mc) @ A @ Mc * 256
        best = None
        for start in (m0, a0):
            r = minimize(loss, start[:, :2].ravel(), method="Powell", options={"maxiter": 6000, "xtol": 0.5, "ftol": 1e-8})
            print("  Start %s: %.1f" % ("alt" if start is m0 else "M^-1 A M", np.sqrt(r.fun) * 255))
            if best is None or r.fun < best.fun:
                best = r
        m = np.round(full(best.x)).astype(int)
        m[:, 2] = 256 - m[:, :2].sum(1)
        r1 = loss(m[:, :2].ravel().astype(float))
        print("%s: Fehler vorher %.1f, nachher %.1f (RMS 8 Bit)" % (look, np.sqrt(r0) * 255, np.sqrt(r1) * 255))
        print(m)
        result[look] = m
        if out_dir:
            for n, c, black, wb, neu, tgt in prep:
                save_row(out_dir + "%s_%s.jpg" % (look, n), [neu, tgt, render(m0, c, black, wb, neu.shape[:2]),
                                                             render(m, c, black, wb, neu.shape[:2])], scale=2)
    return result


if __name__ == "__main__":
    if sys.argv[1] == "check":
        check(sys.argv[2])
    elif sys.argv[1] == "fit":
        fit(sys.argv[2], [a for a in sys.argv[3:] if not a.endswith("/")])


def compare(folder, look, cands, names, out):
    """Je Foto eine Zeile: echtes Kamera-JPEG (Referenz) und das Kameramodell mit jeder Matrix."""
    data = check(folder)
    sat = fit_group(data, [k for k in PAIRS if PAIRS[k][4] == "sat1"])
    mi, cv = LOOKS[look]
    rows = []
    for n in names:
        g, black, wb, dr = grid(folder, n)
        c = curve(g, cv + (str(dr) if dr else "") + ".CV")
        ref = shrink(load_jpeg(os.path.join(folder, n.replace("CRW", "IMG") + ".JPG"))[:2592, :3456], 2 * DS)
        row = [ref] + [sat(to_lin(mix(c, m), black, wb).reshape(-1, 3)).reshape(*g.shape[:2], 3) for m in cands]
        rows.append(row)
    for i, row in enumerate(rows):
        save_row(out + "_%d.jpg" % i, row, scale=2)


def analytic(folder, look):
    data = check(folder)
    sat = fit_group(data, [k for k in PAIRS if PAIRS[k][4] == "sat1"])
    mi = LOOKS[look][0]
    if look == "infra":
        A = (np.eye(3) * AERO_SAT + (1 - AERO_SAT) * np.outer(np.ones(3), [0.2126, 0.7152, 0.0722])) @ AERO
    else:
        import simulate_looks as sl
        A = sl.mix_matrices()[mi]
    Mc = sat.M / sat.M.sum(1, keepdims=True)
    a = np.linalg.inv(Mc) @ A @ Mc * 256
    a = np.round(a).astype(int)
    a[:, 2] = 256 - a[:, :2].sum(1)
    return a


# ---------------------------------------------------------------------------------------------
# Ziele als Farbtoene: Jeder Bildpunkt bekommt seinen Ziel-Farbton aus dem Farbton, den er im echten
# Kamera-JPEG hat (Lab-Winkel). Helligkeit bleibt aussen vor, Grau soll grau bleiben.
# (Winkel in Lab: Rot ~30, Orange ~60, Gelb ~90, Gruen ~135, Cyan ~200, Blau ~280)

HUE_MAP = {
    # Infra: Gruen -> Karmin, Rot -> Gelborange, Gelb/Orange -> Orange, Blau bleibt
    "infra": [(0, 70), (40, 70), (70, 55), (100, 20), (135, 5), (175, 0), (220, 270), (290, 285), (340, 60), (360, 70)],
    # Turquoise: Blau -> Gold, Gruen -> Tuerkis, Warmes -> Blau
    "turquoise": [(0, 255), (40, 250), (70, 230), (100, 205), (135, 195), (175, 190), (220, 90), (290, 75), (340, 240), (360, 255)],
}


def srgb_lab(x):
    x = np.clip(x, 0, 1)
    lin = np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)
    xyz = lin @ np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]]).T
    xyz /= np.array([0.9505, 1.0, 1.089])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)


def hue_targets(look, ref):
    lab = srgb_lab(ref)
    c = np.hypot(lab[..., 1], lab[..., 2])
    h = np.degrees(np.arctan2(lab[..., 2], lab[..., 1])) % 360
    xs, ys = zip(*HUE_MAP[look])
    ys = np.unwrap(np.radians(ys))
    th = np.interp(h, xs, ys)
    tc = c * 1.1
    ab = np.stack([tc * np.cos(th), tc * np.sin(th)], -1)
    ok = (lab[..., 0] > 15) & (ref.max(-1) < 0.97)
    return ab, ok


NOISE_MAX, NOISE_W = 1.5, 3000.0
EXTRA_STARTS = {}


def fit_hue(folder, look, names, out=None):
    from scipy.optimize import minimize
    data = check(folder)
    sat = fit_group(data, [k for k in PAIRS if PAIRS[k][4] == "sat1"])
    mi, cv = LOOKS[look]
    prep = []
    for n in names:
        g, black, wb, dr = grid(folder, n)
        c = curve(g, cv + (str(dr) if dr else "") + ".CV")
        ref = shrink(load_jpeg(os.path.join(folder, n.replace("CRW", "IMG") + ".JPG"))[:2592, :3456], 2 * DS)
        ab, ok = hue_targets(look, ref)
        prep.append((n, c, black, wb, ref, ab, ok))

    def full(p):
        p = np.asarray(p).reshape(3, 2)
        return np.concatenate([p, 256 - p.sum(1, keepdims=True)], 1)

    def render(m, c, black, wb):
        return sat(to_lin(mix(c, m), black, wb).reshape(-1, 3)).reshape(*c.shape[:2], 3)

    def loss(p):
        m = full(p)
        e = 0
        for n, c, black, wb, ref, ab, ok in prep:
            lab = srgb_lab(render(m, c, black, wb))
            e += (((lab[..., 1:] - ab) ** 2).sum(-1)[ok]).mean()
        # Rauschen: Jede Zeile verstaerkt unabhaengiges Rauschen um |Zeile| / 256. Ueber NOISE_MAX kostet es.
        gain = np.sqrt((m ** 2).sum(1)) / 256
        return e / len(prep) + NOISE_W * (np.clip(gain - NOISE_MAX, 0, None) ** 2).sum()

    mats = dict(SHOT_MATS)
    starts = [mats[mi], analytic(folder, look)] + [np.array(x) for x in EXTRA_STARTS.get(look, [])]
    best = None
    for s in starts:
        r = minimize(loss, s[:, :2].ravel().astype(float), method="Powell", options={"maxiter": 6000, "xtol": 0.5, "ftol": 1e-8})
        print("  Start: %.1f -> %.1f (Delta ab)" % (np.sqrt(loss(s[:, :2].ravel().astype(float))), np.sqrt(r.fun)))
        if best is None or r.fun < best.fun:
            best = r
    m = np.round(full(best.x)).astype(int)
    m[:, 2] = 256 - m[:, :2].sum(1)
    print(look, "alt %.1f, neu %.1f" % (np.sqrt(loss(mats[mi][:, :2].ravel().astype(float))), np.sqrt(loss(m[:, :2].ravel().astype(float)))))
    print(m)
    if out:
        for i, (n, c, black, wb, ref, ab, ok) in enumerate(prep):
            lab = srgb_lab(ref)
            save_row(out + "_%s.jpg" % n, [ref, render(mats[mi], c, black, wb), render(m, c, black, wb)], scale=2)
    return m


def render_sample(folder, dng, look, dst, width=1200):
    """Beispielbild: das Kameramodell mit der aktuellen Matrix auf einer echten DNG, volle Aufloesung."""
    data = check(folder)
    sat = fit_group(data, [k for k in PAIRS if PAIRS[k][4] == "sat1"])
    mi, cv = LOOKS[look]
    q, black, wb = load_dng(os.path.join(folder, dng + ".DNG"))
    by, bx = _offset[(2592, 3456, 3)]
    q = q[by:by + 1296, bx:bx + 1728]
    _, _, _, dr = grid(folder, dng)
    lin = to_lin(mix(curve(q, cv + (str(dr) if dr else "") + ".CV"), matrices()[mi]), black, wb)
    img = sat(lin.reshape(-1, 3)).reshape(*lin.shape[:2], 3)
    im = Image.fromarray((img * 255 + 0.5).astype(np.uint8))
    im = im.resize((width, width * 3 // 4), Image.LANCZOS)
    im.save(dst, quality=88)
