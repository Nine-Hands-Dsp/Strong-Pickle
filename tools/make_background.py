"""
Strong Pickle (Studio Wiga) - fond du plugin a partir du visuel fourni (design/source/strong_pickle_visuel.png).

Le visuel est la reference : son eclairage (lumiere en haut a gauche) et son grain sont conserves.
Le visuel (912 x 1152) a exactement les proportions du cadre 760 x 958 : 1 px du visuel = 0,8333 unite.
Ce script :
  1. efface les reperes blancs des potards (ils sont redessines et tournent dans le plugin),
  2. efface les leviers des deux inverseurs (rendus 3D dans le plugin : bas / intermediaire / haut),
  3. remplace la LED bleue allumee par une LED jaune eteinte (bague chromee) et retire la lueur bleue
     de la facade ; calcule le calque d'eclairage jaune de la facade (alpha : seule la lumiere ajoutee),
  4. vide l'ecran de statut et la barre de presets (textes effaces, cadres et fleches gardes) : le plugin y ecrit,
  5. met le tout a l'echelle du cadre 1520 x 1916 (= 2 x 760 x 958) et ecrit Layout.h.

Usage : python3 tools/make_background.py
"""
import os, json, math
import numpy as np, cv2
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, 'design', 'source', 'strong_pickle_visuel.png')

A = cv2.cvtColor(cv2.imread(SRC), cv2.COLOR_BGR2RGB).astype(np.float32)
H, W = A.shape[:2]
K = 760.0 / W                              # unites par pixel du visuel
YY, XX = np.mgrid[0:H, 0:W].astype(np.float32)
lum = lambda a: a.mean(-1)
rng = np.random.default_rng(7)

# ------------------------------------------------------------------ reperes mesures sur le visuel (px)
KNOBS = [(307.0, 194.0, 71.0), (602.0, 192.0, 71.0), (456.0, 424.0, 58.0)]   # loudness, sustain, filter : centre, rayon du moletage
TOGGLES = [(285.5, 428.5), (626.0, 428.0)]                                   # crunch, scoop : centre de la bague
TOG_RING = 24.5
LED = (455.5, 823.0)
FOOT = (452.0, 956.0); FOOT_R = 54.0
STATUS_BOX = (234.0, 545.0, 446.0, 57.0)
PRESET_BOX = (222.0, 612.0, 469.0, 39.0)

def bilinear(img, x, y):
    x = np.clip(x, 0, W - 2); y = np.clip(y, 0, H - 2)
    x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
    return (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x0 + 1] * fx * (1 - fy)
            + img[y0 + 1, x0] * (1 - fx) * fy + img[y0 + 1, x0 + 1] * fx * fy)

def ring_fill(cx, cy, mask, rmax):
    """efface en interpolant, sur chaque cercle centre sur la commande, entre les angles voisins non masques
    (les reflets d'un chapeau tourne ou d'une bague varient doucement avec l'angle)."""
    NA = 1440
    th = np.linspace(-math.pi, math.pi, NA, endpoint=False)
    ys, xs = np.where(mask)
    rr = np.hypot(xs - cx, ys - cy); aa = np.arctan2(ys - cy, xs - cx)
    for r0 in range(0, int(rmax) + 2):
        sel = (rr >= r0 - 0.5) & (rr < r0 + 0.5)
        if not sel.any(): continue
        r_ = max(r0, 0.5)
        px = cx + r_ * np.cos(th); py = cy + r_ * np.sin(th)
        prof = bilinear(A, px, py)
        mpx = np.clip(np.round(px).astype(int), 0, W - 1); mpy = np.clip(np.round(py).astype(int), 0, H - 1)
        bad = mask[mpy, mpx]
        if bad.all(): continue
        good = np.where(~bad)[0]
        ext = np.concatenate([good - NA, good, good + NA])
        for c in range(3):
            prof[:, c] = np.interp(np.arange(NA), ext, np.tile(prof[good, c], 3))
        idx = ((aa[sel] + math.pi) / (2 * math.pi) * NA).astype(int) % NA
        A[ys[sel], xs[sel]] = prof[idx]

# ------------------------------------------------------------------ 1. reperes des potards
for cx, cy, R in KNOBS:
    r = np.hypot(XX - cx, YY - cy)
    mk = (r < R * 0.86) & (lum(A) > 95)
    mk = cv2.dilate(mk.astype(np.uint8), np.ones((5, 5), np.uint8)).astype(bool) & (r < R * 0.88)
    ring_fill(cx, cy, mk, R)

# ------------------------------------------------------------------ 2. leviers des inverseurs
for cx, cy in TOGGLES:
    r = np.hypot(XX - cx, YY - cy)
    # levier (vers le bas sur le visuel) : tige et boule dans la bague interieure
    mk = (r < 15.5) & ((YY > cy - 7) & (np.abs(XX - cx) < 10.5))
    mk |= (r < 9.5)
    ring_fill(cx, cy, mk, 16)

# ------------------------------------------------------------------ 3. LED : lueur bleue retiree, facade refaite
lx, ly = LED
r = np.hypot(XX - lx, YY - ly)
ref_ring = (r > 62) & (r < 80)
# composante basse frequence de la facade autour de la LED : moyenne radiale prise hors de la lueur
low = cv2.GaussianBlur(A, (0, 0), 7)
target = np.median(low[ref_ring], axis=0)
w = np.clip((62 - r) / 30.0, 0, 1)[..., None]
A = A - (low - target) * w                       # retire la teinte et l'eclat bleus, garde le grain et le texte
# coeur (LED + bague) : grain de peinture recopie d'une zone propre
core = r < 15
src = A[int(ly) + 40:int(ly) + 72, int(lx) - 60:int(lx) - 28]
hp = src - cv2.GaussianBlur(src, (0, 0), 3)
ys, xs = np.where(core)
A[ys, xs] = target + hp[(ys - ys.min()) % hp.shape[0], (xs - xs.min()) % hp.shape[1]]
A = np.clip(A, 0, 255)

# ------------------------------------------------------------------ 4. textes de l'ecran et de la barre de presets
# la zone du texte est refaite en recopiant (par translation horizontale) des cases propres de la meme rangee :
# le cadrillage discret de l'ecran et le degrade vertical de la barre sont conserves ; raccords fondus sur 8 px.
def retile(x_from, x_to, y_from, y_to, src_x0, period, feather=8.0):
    xs = np.arange(int(x_from), int(x_to))
    ys = np.arange(int(y_from), int(y_to))
    sxs = src_x0 + np.mod(xs - src_x0, period)
    XXs, YYs = np.meshgrid(sxs, ys)
    patch = bilinear(A, XXs.astype(np.float32), YYs.astype(np.float32))
    wx = np.clip(np.minimum(xs - x_from, x_to - 1 - xs) / feather, 0, 1)[None, :, None]
    A[int(y_from):int(y_to), int(x_from):int(x_to)] = A[int(y_from):int(y_to), int(x_from):int(x_to)] * (1 - wx) + patch * wx
retile(346, 572, 549, 598, 242.0, 112.5)        # ecran : texte entre x 361 et 551
retile(360, 556, 615, 649, 262.0, 90.0)         # barre de presets : texte entre x 375 et 537 (fleches gardees)
A = np.clip(A, 0, 255)

# ------------------------------------------------------------------ 5. mise a l'echelle du cadre 1520 x 1916
W2, H2 = 1520, 1916
S2 = W2 / W                                      # px du visuel -> px du fond
big = cv2.resize(A, (W2, H2), interpolation=cv2.INTER_LANCZOS4)
sharp = big + 0.35 * (big - cv2.GaussianBlur(big, (0, 0), 1.2))   # rend un peu du piqué perdu a l'agrandissement
img = np.clip(sharp, 0, 255)

PX = 2.0                                          # px du fond par unite
U = lambda v: v * K                               # px visuel -> unites
L = np.array([-0.46, -0.62, 0.64]); L /= np.linalg.norm(L)
HV = (L + np.array([0, 0, 1.0])); HV /= np.linalg.norm(HV)

def normals(h, s):
    gy, gx = np.gradient(h, 1.0 / s)
    n = np.dstack([-gx, -gy, np.ones_like(h)])
    return n / np.linalg.norm(n, axis=2, keepdims=True)

# LED eteinte : bague chromee fine (comme le visuel) + lentille jaune sombre
LED_R = 6.6                                        # rayon de la lentille (unites)
def render_led(LR=LED_R, base=(58, 44, 12), hi=(80, 66, 30)):
    R = LR * 1.42; SS = 3; k = PX * SS
    n = int(math.ceil((R * 2 + 8) * k))
    yk, xk = np.mgrid[0:n, 0:n].astype(np.float32)
    lx_, ly_ = (xk + 0.5) / k - n / k / 2, (yk + 0.5) / k - n / k / 2
    rr = np.hypot(lx_, ly_)
    hk = np.where(rr < LR, 1.8 + 1.6 * np.sqrt(np.clip(1 - (rr / LR) ** 2, 0, 1)), 0)
    ring = (rr >= LR) & (rr < R)
    hk = np.where(ring, 2.6 * np.sin(np.clip((rr - LR) / (R - LR), 0, 1) * math.pi), hk)
    Nk = normals(hk, k)
    ndl = np.clip((Nk * L).sum(-1), 0, 1); ndh = np.clip((Nk * HV).sum(-1), 0, 1)
    chrome = 60 + 150 * ndl ** 2 + 255 * ndh ** 50
    lens = np.dstack([base[0] + hi[0] * ndh ** 30, base[1] + hi[1] * ndh ** 30, base[2] + hi[2] * ndh ** 30]) + 230 * (ndh ** 500)[..., None]
    rgb = np.where(ring[..., None], np.dstack([chrome, chrome, chrome * 0.98]), lens)
    gorge = (rr >= LR - 0.7) & (rr < LR + 0.3)
    rgb = np.where(gorge[..., None], rgb * 0.3, rgb)
    m = (rr < R).astype(np.float32)
    rgb = cv2.resize(rgb, (n // SS, n // SS), interpolation=cv2.INTER_AREA)
    m = cv2.resize(m, (n // SS, n // SS), interpolation=cv2.INTER_AREA)
    return rgb, m
LEDU = (U(LED[0]), U(LED[1]))
lrgb, lm = render_led(); ln_ = lrgb.shape[0]
xa, ya = int(round(LEDU[0] * PX - ln_ / 2)), int(round(LEDU[1] * PX - ln_ / 2))
shadow = np.zeros((H2, W2), np.float32); shadow[ya + 3:ya + 3 + ln_, xa + 2:xa + 2 + ln_] = lm
img *= (1 - 0.45 * cv2.GaussianBlur(shadow, (0, 0), 3)[..., None])
img[ya:ya + ln_, xa:xa + ln_] = img[ya:ya + ln_, xa:xa + ln_] * (1 - lm[..., None]) + lrgb * lm[..., None]

# calque d'eclairage par la LED jaune (la facade recoit la lumiere ; le grain s'allume, les creux restent sombres)
GLOW = (LEDU[0] - 128.0, LEDU[1] - 128.0, 256.0, 256.0)
gx0, gy0, gw, gh = GLOW
sx, sy, sw, sh = int(gx0 * PX), int(gy0 * PX), int(gw * PX), int(gh * PX)
regbg = img[sy:sy + sh, sx:sx + sw].copy()
gv, gu = np.mgrid[0:sh, 0:sw].astype(np.float32)
gu = (gu + 0.5) / PX + gx0; gv = (gv + 0.5) / PX + gy0
d = np.hypot(gu - LEDU[0], gv - LEDU[1])
hsrc = 8.0
E = hsrc ** 3 / (d * d + hsrc * hsrc) ** 1.5
E = 1.25 * E + 0.16 * np.exp(-(d / 60.0) ** 2)
yel = np.array([1.55, 1.12, 0.22], np.float32)
lit = regbg * (1 + E[..., None] * yel * 1.9) + (E[..., None] ** 2) * yel * 55
add = np.clip(lit, 0, 255) - regbg
a = np.clip(add.max(-1) / 60.0, 0, 1)
a = np.maximum(a, np.clip(E * 0.9, 0, 1))
srcg = np.where(a[..., None] > 1e-3, regbg + add / np.maximum(a[..., None], 1e-3), regbg)
glow = np.dstack([np.clip(srcg, 0, 255), a[..., None] * 255]).astype(np.uint8)
Image.fromarray(glow, 'RGBA').resize((512, 512), Image.LANCZOS).save(os.path.join(ROOT, 'glow_yellow.png'))

# ------------------------------------------------------------------ 6. leviers des inverseurs (rendu 3D, lumiere du visuel)
# 3 images carrees : levier bas (comme le visuel), intermediaire (~55 ms pendant la bascule), haut.
# Seuls le levier et son ombre ont de l'alpha : la bague reste celle du fond.
TOG_HALF = 26.0                                   # demi-cote de l'image (unites)
def render_lever(direction):
    SS = 3; k = PX * SS
    n = int(round(TOG_HALF * 2 * k))
    yk, xk = np.mgrid[0:n, 0:n].astype(np.float32)
    x_ = (xk + 0.5) / k - TOG_HALF; y_ = (yk + 0.5) / k - TOG_HALF
    hgt = np.full((n, n), -1.0, np.float32); nrm = np.zeros((n, n, 3), np.float32); nrm[..., 2] = 1
    rod_r = 2.5; ball_r = 5.2; tip = 7.6 * direction        # longueur projetee (unites) : 0 = levier vers l'oeil
    # tige : de l'axe vers la boule
    if direction != 0:
        y0, y1 = (0.0, tip) if tip > 0 else (tip, 0.0)
        sel = (np.abs(x_) < rod_r) & (y_ >= y0) & (y_ <= y1)
        s = np.clip(x_ / rod_r, -0.999, 0.999)
        nn = np.dstack([s, np.zeros_like(s), np.sqrt(1 - s * s)])
        nn[..., 1] = -0.45 * direction                       # la tige monte vers l'oeil depuis l'axe
        hz = 4 + 3 * np.sqrt(1 - s * s)
        put = sel & (hz > hgt)
        hgt[put] = hz[put]; nrm[put] = nn[put]
    # boule au bout
    rb = np.hypot(x_, y_ - tip)
    sel = rb < ball_r
    z = np.sqrt(np.clip(1 - (rb / ball_r) ** 2, 0, 1))
    nn = np.dstack([x_ / ball_r, (y_ - tip) / ball_r, z])
    hz = 9 + 4 * z
    put = sel & (hz > hgt)
    hgt[put] = hz[put]; nrm[put] = nn[put]
    nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    obj = hgt > 0
    ndl = np.clip((nrm * L).sum(-1), 0, 1); ndh = np.clip((nrm * HV).sum(-1), 0, 1)
    env = 0.5 + 0.5 * np.clip(-nrm[..., 1] * 0.7 + nrm[..., 2] * 0.5, -1, 1)
    c = 14 + 120 * ndl ** 2.2 + 70 * env ** 2 + 255 * ndh ** 45   # nickel poli : plus de contraste que du metal mat
    rgb = np.dstack([c, c * 1.0, c * 0.97])
    al = obj.astype(np.float32)
    # ombre portee vers le bas a droite (douce)
    sh = np.roll(np.roll(al, int(2.6 * k), 0), int(1.8 * k), 1)
    sh = cv2.GaussianBlur(sh, (0, 0), 1.8 * k) * 0.55
    out_a = al + sh * (1 - al)
    out_rgb = np.where(al[..., None] > 0, rgb, 0) * al[..., None] / np.maximum(out_a[..., None], 1e-4)
    rgba = np.dstack([out_rgb, out_a * 255])
    rgba = cv2.resize(rgba, (n // SS, n // SS), interpolation=cv2.INTER_AREA)
    return np.clip(rgba, 0, 255).astype(np.uint8)
frames = [render_lever(1), render_lever(0), render_lever(-1)]          # bas, intermediaire, haut
fs = frames[0].shape[0]
strip = np.concatenate(frames, axis=0)
Image.fromarray(strip, 'RGBA').save(os.path.join(ROOT, 'toggle_strip.png'))

# ------------------------------------------------------------------ 6b. motif du cornichon sur la coque (v1.4, design fourni)
# design/source/strong_pickle_visuel_motif.jpg = la pedale avec le dessin du cornichon (trait creme). Le dessin est
# extrait par difference avec ce fond (recale : matrice affine mesuree par correlation ECC), agrandi, son contour est
# rendu net (le visuel est 2,3 fois moins defini que le fond), puis il est peint a l'encre creme des etiquettes,
# avec le grain de la peinture. Jamais sur les potards, inverseurs, footswitch, LED, ecran, barre de presets, ni sur les
# lettres deja creme (etiquettes, STRONG / PICKLE / FUZZ).
MOTIF = os.path.join(ROOT, 'design', 'source', 'strong_pickle_visuel_motif.jpg')
P2R = np.array([[1.87032343e+00, -3.46272783e-04, 1.19107749e+02],
                [6.66725997e-04, 1.86938975e+00, -9.17126969e+00]], np.float64)   # px du visuel motif -> px du fond
img_pre_motif = img.copy()
if os.path.exists(MOTIF):
    Mph = cv2.cvtColor(cv2.imread(MOTIF), cv2.COLOR_BGR2RGB).astype(np.float32)
    hP, wP = Mph.shape[:2]
    bgP = cv2.warpAffine(img, P2R, (wP, hP), flags=cv2.INTER_AREA | cv2.WARP_INVERSE_MAP)
    Rp, Gp, Bp = Mph[..., 0], Mph[..., 1], Mph[..., 2]
    cream = np.clip((Rp - bgP[..., 0] - 30.0) / 70.0, 0, 1) * np.clip((45.0 - (Gp - Rp)) / 20.0, 0, 1)
    cream *= np.clip((bgP[..., 1] - bgP[..., 0] - 25.0) / 20.0, 0, 1)     # seulement sur la peinture verte du fond
    Hb, Wb = img.shape[:2]
    al = cv2.warpAffine(cream, P2R, (Wb, Hb), flags=cv2.INTER_CUBIC)
    al = cv2.GaussianBlur(np.clip(al, 0, 1), (0, 0), 1.0)
    al = np.clip((al - 0.36) / 0.28, 0, 1); al = al * al * (3 - 2 * al)     # contour net (epaisseur du trait du visuel)
    yyb, xxb = np.mgrid[0:Hb, 0:Wb].astype(np.float32)
    keep = np.ones((Hb, Wb), np.float32)
    def _hole(cx, cy, r, soft=6.0):
        global keep
        keep *= np.clip((np.hypot(xxb - cx, yyb - cy) - r) / soft, 0, 1)
    for (cx, cy, R) in KNOBS: _hole(U(cx) * PX, U(cy) * PX, U(R) * PX * 1.10)
    for (cx, cy) in TOGGLES: _hole(U(cx) * PX, U(cy) * PX, 52)
    _hole(U(LED[0]) * PX, U(LED[1]) * PX, 40)
    _hole(U(FOOT[0]) * PX, U(FOOT[1]) * PX, U(FOOT_R) * PX * 1.28)
    for (bx_, by_, bw_, bh_) in (STATUS_BOX, PRESET_BOX):
        x0_, y0_, x1_, y1_ = U(bx_) * PX - 8, U(by_) * PX - 8, U(bx_ + bw_) * PX + 8, U(by_ + bh_) * PX + 8
        keep *= 1 - ((xxb > x0_) & (xxb < x1_) & (yyb > y0_) & (yyb < y1_)).astype(np.float32)
    al *= keep
    n_, lab_, st_, _ = cv2.connectedComponentsWithStats((al > 0.5).astype(np.uint8), 8)
    small = np.zeros(n_, bool); small[1:] = st_[1:, 4] < 18                  # poussieres isolees
    al *= ~cv2.dilate(small[lab_].astype(np.uint8), np.ones((3, 3), np.uint8)).astype(bool)
    inkm = np.array([200, 198, 152], np.float32)                               # creme du trait (mesure sur le visuel)
    grain_ = img - cv2.GaussianBlur(img, (0, 0), 3.0)
    shade = cv2.GaussianBlur(img.mean(-1), (0, 0), 60.0); shade = (shade / np.median(shade[keep > 0.5]))[..., None]
    ink_ = np.clip(inkm * np.clip(shade, 0.75, 1.15) + 0.4 * grain_, 0, 255)
    img = img * (1 - 0.88 * al[..., None]) + ink_ * (0.88 * al[..., None])
    print('motif : %d px de trait' % int((al > 0.5).sum()))

# ------------------------------------------------------------------ 7. « PICKLE » qui s'allume en vert quand l'effet est actif
# Les lettres creme du visuel deviennent des lettres lumineuses vertes (coeur clair, bord sature),
# avec un halo serre et une lueur douce sur la peinture autour. Calque RGBA (alpha = lumiere seule),
# affiche par le plugin avec une opacite = niveau de la LED (meme fondu).
PK_BOX = (340.0, 706.0, 236.0, 64.0)              # zone du mot dans le visuel (px)
bx, by, bw, bh = PK_BOX
gA = A.mean(-1); sA = A.max(-1) - A.min(-1)
# v1.1 : masque des lettres pris sur le fond final lui-meme (meme agrandissement, meme accentuation) :
# les lettres allumees recouvrent exactement les lettres eteintes, sans decalage ni liseré creme.
px0, py0 = int(round(U(bx - 26) * PX)), int(round(U(by - 26) * PX))      # coin du calque sur un pixel entier
pw, ph = int(round(U(bw + 52) * PX)), int(round(U(bh + 52) * PX))
PKU = (px0 / PX, py0 / PX, pw / PX, ph / PX)                               # rectangle du calque (unites)
reg_ = img_pre_motif[py0:py0 + ph, px0:px0 + pw]      # masque des lettres sans le motif
lum_ = reg_.mean(-1); sat_ = reg_.max(-1) - reg_.min(-1)
yy_, xx_ = np.mgrid[0:ph, 0:pw]
inside_ = (xx_ >= (U(bx) * PX - px0)) & (xx_ < (U(bx + bw) * PX - px0)) & (yy_ >= (U(by) * PX - py0)) & (yy_ < (U(by + bh) * PX - py0))
m = (np.clip((lum_ - 95) / 75.0, 0, 1) * np.clip((95 - sat_) / 30.0, 0, 1) * inside_).astype(np.float32)
_n, _lab, _st, _ = cv2.connectedComponentsWithStats((m > 0.3).astype(np.uint8), 8)
_keep = np.zeros(_n, bool); _keep[1:] = _st[1:, 4] >= 120                  # seulement les lettres (pas les paillettes)
m = m * cv2.dilate(_keep[_lab].astype(np.uint8), np.ones((5, 5), np.uint8)).astype(np.float32)
# couverture : le contour antialiasé des lettres creme est recouvert (1 px de plus) pour ne laisser aucun liseré
m = np.maximum(m, cv2.dilate((m > 0.25).astype(np.float32), np.ones((3, 3), np.uint8)) * 0.85)
base = img[py0:py0 + ph, px0:px0 + pw]
# coeur des lettres : vert lumineux, plus clair au centre des traits (distance au bord)
dist = cv2.distanceTransform((m > 0.5).astype(np.uint8), cv2.DIST_L2, 3)
core = np.clip(dist / 5.0, 0, 1)[..., None]
green_edge = np.array([60, 185, 52], np.float32); green_core = np.array([150, 225, 125], np.float32)   # v1.0 : moins brillant (demande)
letter_rgb = green_edge + (green_core - green_edge) * core
halo1 = cv2.GaussianBlur(m, (0, 0), 3.0)
halo2 = cv2.GaussianBlur(m, (0, 0), 14.0)
glowc = np.array([80, 255, 60], np.float32)
lit = base * (1 + (0.40 * halo1 + 0.55 * halo2)[..., None] * np.array([0.25, 1.0, 0.2], np.float32)) \
      + (0.22 * halo1)[..., None] * glowc
lit = lit * (1 - m[..., None]) + letter_rgb * m[..., None]
lit = np.clip(lit, 0, 255)
add = np.abs(lit - base).max(-1)
a = np.clip(np.maximum(m, add / 60.0), 0, 1)
srcp = np.where(a[..., None] > 1e-3, base + (lit - base) / np.maximum(a[..., None], 1e-3), base)
pk = np.dstack([np.clip(srcp, 0, 255), a[..., None] * 255]).astype(np.uint8)
Image.fromarray(pk, 'RGBA').save(os.path.join(ROOT, 'pickle_glow.png'))

# ------------------------------------------------------------------ 8. jack OUT (gauche) a la taille exacte du jack IN (droite)
# Dans le visuel, le jack gauche est plus grand que le droit. Il est efface (texture brossee horizontale du fond
# prolongee ligne par ligne) puis recolle reduit a la hauteur du jack droit, toujours contre le bord du boitier,
# a la meme hauteur : il garde son propre eclairage (lumiere en haut a gauche).
def _green(a):
    return (a[..., 1] - np.maximum(a[..., 0], a[..., 2])) > 25
def _bbox(m):
    ys, xs = np.where(m); return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
JY0, JY1 = 800, 1140                               # bande verticale des jacks (px du fond 1520 x 1916)
band = img[JY0:JY1].copy()
LX0, LX1 = 150, 262                                 # zone du jack gauche (fond brosse a gauche, boitier vert a droite)
RX0, RX1 = 1255, 1380
def _jack(x0, x1, bgcols):
    reg = band[:, x0:x1]
    bg = np.median(band[:, bgcols[0]:bgcols[1]], axis=1)
    g = _green(reg)
    d = np.abs(reg - bg[:, None]).max(-1) * (~g)
    m = (d > 22).astype(np.uint8)
    n, lab, st, _ = cv2.connectedComponentsWithStats(m, 8)
    big = 1 + int(np.argmax(st[1:, 4]))
    core = cv2.morphologyEx((lab == big).astype(np.uint8), cv2.MORPH_CLOSE, np.ones((7, 7), np.uint8))
    # le jack est plein : chaque ligne est remplie de son premier pixel jusqu'au bord vert du boitier
    hard = np.zeros_like(core)
    rows = [y for y in range(core.shape[0]) if core[y].sum() >= 3]
    y_a, y_b = rows[0], rows[-1]
    outer = np.full(core.shape[0], np.nan)            # bord exterieur du jack (cote fond), ligne par ligne
    for y in rows:
        xs = np.where(core[y])[0]; outer[y] = xs.min() if x0 < 700 else xs.max()
    yy = np.arange(y_a, y_b + 1); ok = ~np.isnan(outer[yy])
    ob = np.interp(yy, yy[ok], outer[yy][ok])           # lignes manquantes (zones sombres du jack) : interpolees
    # creux etroits du contour (parties sombres du jack confondues avec le fond) : ouverture sur 25 lignes
    from scipy.ndimage import grey_opening, grey_closing
    ob = grey_opening(ob, size=25) if x0 < 700 else grey_closing(ob, size=25)
    for y, o in zip(yy, ob):
        o = int(round(o)); gx = np.where(g[y])[0]
        if x0 < 700:                                   # jack gauche : jusqu'au premier pixel vert a droite
            stop = gx[gx > o]; hard[y, o:(stop[0] if len(stop) else o + 1)] = 1
        else:                                          # jack droit : depuis le dernier pixel vert a gauche
            start = gx[gx < o]; hard[y, (start[-1] + 1 if len(start) else o):o + 1] = 1
    soft = cv2.GaussianBlur(hard.astype(np.float32), (0, 0), 0.8)
    soft = np.maximum(soft * (cv2.dilate(hard, np.ones((3, 3), np.uint8)) > 0), cv2.erode(hard, np.ones((3, 3), np.uint8)))
    return reg, hard, soft
regL, hardL, softL = _jack(LX0, LX1, (150, 190))
regR, hardR, softR = _jack(RX0, RX1, (1400, 1440))
bl = _bbox(hardL); br = _bbox(hardR)
hL, hR = bl[3] - bl[1], br[3] - br[1]
k = hR / float(hL)
print('jack gauche', bl, 'h', hL, '| jack droit', br, 'h', hR, '| echelle', round(k, 3))
# effacement du jack gauche : texture brossee prise 60 px plus a gauche, meme ligne
er = cv2.GaussianBlur(cv2.dilate(hardL, np.ones((9, 9), np.uint8)).astype(np.float32), (0, 0), 1.5)
er *= ~_green(regL)                                                   # le bord vert du boitier reste intact
src = band[:, LX0 - 60:LX1 - 60]
out = band.copy()
out[:, LX0:LX1] = regL * (1 - er[..., None]) + src * er[..., None]
# recollage reduit : cote boitier (bord droit du jack) fixe, centre vertical fixe
x0, y0, x1, y1 = bl
cw, ch = x1 - x0, y1 - y0
crop = regL[y0:y1, x0:x1]; cm = softL[y0:y1, x0:x1]
nw, nh = max(1, int(round(cw * k))), hR                             # meme hauteur que le jack droit, proportions gardees
crop_s = cv2.resize(crop, (nw, nh), interpolation=cv2.INTER_AREA)
cm_s = np.clip(cv2.resize(cm, (nw, nh), interpolation=cv2.INTER_AREA), 0, 1)
cy = (y0 + y1) / 2.0
ny0 = int(round(cy - nh / 2.0)); nx0 = LX0 + x1 - nw
t = out[ny0:ny0 + nh, nx0:nx0 + nw]
t[:] = t * (1 - cm_s[..., None]) + crop_s * cm_s[..., None]
img[JY0:JY1] = out

# ------------------------------------------------------------------ 9. inverseur OCTAVE (v1.3, ajout Strong Pickle)
# Une bague d'inverseur identique a celle de SCOOP (copiee du visuel, meme eclairage) est posee a droite du footswitch,
# a l'aplomb de SCOOP, avec l'etiquette OCTAVE dans la police des etiquettes (Barlow Condensed Bold, meme couleur).
import sys as _sys
_sys.path.insert(0, HERE)
from label_font import render as _render_label
OCT_V = (TOGGLES[1][0], LED[1])                    # v1.4.1 : x de SCOOP, a la hauteur de la LED
sx, sy = U(TOGGLES[1][0]) * PX, U(TOGGLES[1][1]) * PX
tx, ty = U(OCT_V[0]) * PX, U(OCT_V[1]) * PX
R0, R1, RP = 44.0, 57.0, 80
def _patch(cx, cy):
    x0, y0 = int(round(cx)) - RP, int(round(cy)) - RP
    return x0, y0, img[y0:y0 + 2 * RP, x0:x0 + 2 * RP]
yy9, xx9 = np.mgrid[-RP:RP, -RP:RP].astype(np.float32)
rr9 = np.hypot(xx9 + 0.5, yy9 + 0.5)
mk9 = np.clip((R1 - rr9) / (R1 - R0), 0, 1)
mk9 = mk9 * mk9 * (3 - 2 * mk9)
ann = (rr9 > 62) & (rr9 < 76)
_, _, ps = _patch(sx, sy)
x0t, y0t, pt = _patch(tx, ty)
corr = np.median(pt[ann], axis=0) - np.median(ps[ann], axis=0)      # teinte locale de la peinture
pt[:] = pt * (1 - mk9[..., None]) + (ps + corr) * mk9[..., None]
# etiquettes : OCTAVE sous l'inverseur (comme CRUNCH / SCOOP), FAT au-dessus et BRIGHT en dessous du levier
# (positions de l'inverseur Fat / Bright de l'Ultimate Octave ; milieu = sans octave)
inkc = np.array([160, 181, 136], np.float32)
def _label(text, cx, cy, cap):
    lab = _render_label(text, 'BarlowCondensed-Bold.ttf', cap, cap * 0.13).astype(np.float32) / 255.0
    lab = cv2.GaussianBlur(lab, (0, 0), 0.55)
    lh, lw = lab.shape
    ly0 = int(round(cy - lh / 2.0)); lx0 = int(round(cx - lw / 2.0))
    reg9 = img[ly0:ly0 + lh, lx0:lx0 + lw]
    grain = reg9 - cv2.GaussianBlur(reg9, (0, 0), 3.0)               # grain de la peinture garde dans les lettres
    reg9[:] = reg9 * (1 - 0.95 * lab[..., None]) + (inkc + 0.35 * grain) * (0.95 * lab[..., None])
    return (lx0, ly0, lw, lh)
print('etiquettes', _label('OCTAVE', tx, ty + (849.0 - sy), 24.5), _label('FAT', tx, ty - 70, 18.0), _label('BRIGHT', tx, ty + 70, 18.0))
# petite LED d'octave (verte, eteinte dans le fond ; le plugin l'allume quand l'octave est en service) sous OCTAVE
OCT_LED_R = 4.4
OCTLEDU = (tx / PX, (ty + 196.0) / PX)
orgb, om = render_led(OCT_LED_R, base=(20, 44, 16), hi=(40, 80, 34)); on_ = orgb.shape[0]
oxa, oya = int(round(OCTLEDU[0] * PX - on_ / 2)), int(round(OCTLEDU[1] * PX - on_ / 2))
osh = np.zeros(img.shape[:2], np.float32); osh[oya + 2:oya + 2 + on_, oxa + 2:oxa + 2 + on_] = om
img *= (1 - 0.45 * cv2.GaussianBlur(osh, (0, 0), 2.5)[..., None])
img[oya:oya + on_, oxa:oxa + on_] = img[oya:oya + on_, oxa:oxa + on_] * (1 - om[..., None]) + orgb * om[..., None]
OCTU = (U(OCT_V[0]), U(OCT_V[1]))
print('octave : inverseur', OCTU)

# ------------------------------------------------------------------ sorties
Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(os.path.join(ROOT, 'background.jpg'), quality=94, subsampling=0)

P = lambda p: '{ %.2ff, %.2ff }' % tuple(p)
Rc = lambda r: '{ %.2ff, %.2ff, %.2ff, %.2ff }' % tuple(r)
kp = [(U(cx), U(cy)) for cx, cy, _ in KNOBS]
kr = [U(R) for _, _, R in KNOBS]
tp = [(U(cx), U(cy)) for cx, cy in TOGGLES]
st = tuple(U(v) for v in STATUS_BOX); pr = tuple(U(v) for v in PRESET_BOX)
lay = dict(knobs=kp, knobR=kr, toggles=tp, octave=OCTU, octLed=OCTLEDU, togHalf=TOG_HALF, led=LEDU, ledR=LED_R, foot=(U(FOOT[0]), U(FOOT[1])),
           footR=U(FOOT_R), glow=GLOW, pickle=PKU, status=st, preset=pr)
json.dump(lay, open(os.path.join(HERE, 'layout.json'), 'w'), indent=1)
open(os.path.join(ROOT, 'Layout.h'), 'w').write(f"""// genere par tools/make_background.py - ne pas modifier a la main (repere 760 x 958)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{{
    const juce::Point<float> knobPos[3] = {{ {P(kp[0])}, {P(kp[1])}, {P(kp[2])} }};
    const float knobR[3] = {{ {kr[0]:.2f}f, {kr[1]:.2f}f, {kr[2]:.2f}f }};      // rayon du moletage
    const juce::Point<float> crunchPos {P(tp[0])}, scoopPos {P(tp[1])}, octavePos {P(OCTU)};
    const juce::Point<float> octLedPos {P(OCTLEDU)}; const float octLedR = {OCT_LED_R:.2f}f;   // LED d'octave
    const float togHalf = {TOG_HALF:.2f}f;
    const juce::Point<float> ledPos {P(LEDU)}; const float ledR = {LED_R:.2f}f;
    const juce::Point<float> footPos {P((U(FOOT[0]), U(FOOT[1])))}; const float footR = {U(FOOT_R):.2f}f;
    const juce::Rectangle<float> glowRect {Rc(GLOW)};
    const juce::Rectangle<float> pickleRect {Rc(PKU)};    // calque « PICKLE » allume (vert)
    const juce::Rectangle<float> statusRect {Rc(st)};
    const juce::Rectangle<float> presetRect {Rc(pr)};
}}
""")
print('ok', img.shape, 'toggle frame', fs)
