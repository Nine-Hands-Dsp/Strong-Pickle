"""
STRONG PICKLE (NINE HANDS | DSP) - visuel du plugin v2 (nov. 2026), entierement synthetise : pedale simple 1235 x 1800 px.

Reprend la disposition validee de la v1.4.1 (potards, inverseurs, ecran, barre de presets, LED, footswitch, textes,
cornichon), redessinee avec les pieces du kit NINE HANDS | DSP :
  - photos fournies par l'utilisateur (assets/kit) : potard, LED, footswitch, embases jack, inverseurs haut / milieu / bas ;
  - cornichon vectoriel design/source/cornichon.svg (gravure creme) ;
  - textes, graduations et logo NINE HANDS | DSP : dessines en vectoriel par le plugin (Layout.h), jamais dans le fond.
Une seule lumiere pour toute la pedale : en haut a gauche (ombres portees vers le bas a droite).

Regle des pedales simples : image 1235 x 1800 px, pedale plein cadre (le boitier touche le haut et le bas, les jacks
touchent les bords gauche et droit), fond transparent seulement autour des jacks et dans les coins arrondis.
Dessin dans un repere de travail 760 x 958 unites (2 px par unite), puis recadrage + mise a l'echelle.
Sorties (racine du depot) :
  background.png       fond 1235 x 1800 RGBA (boitier, cornichon, potards, LED eteintes, footswitch, jacks)
  glow_yellow.png      facade eclairee par la LED principale (alpha = seule la lumiere ajoutee)
  glow_yellow_oct.png  idem pour la LED d'octave
  pickle_glow.png      halo vert du mot PICKLE sur la facade (alpha)
  led_on.png           lentille allumee (jaune)
  fs_refl.png          reflet jaune de la LED sur le flanc de l'ecrou du footswitch
  toggle_strip.png     inverseur : haut / milieu / bas empiles (photos recalees : aucun decalage pendant l'animation)
  Layout.h, tools/layout.json
Usage : python3 tools/make_art.py
"""
import os, sys, math, subprocess, io, json
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import photo_controls as PC
import toggle_photo as TP

AS = 2
UW, UH = 760, 958
W, H = UW * AS, UH * AS
L = np.array([-0.46, -0.62, 0.64]); L /= np.linalg.norm(L)
V = np.array([0.0, 0.0, 1.0]); HV = (L + V) / np.linalg.norm(L + V)

# ------------------------------------------------------------------ geometrie (unites de travail)
OUT_W, OUT_H = 1235, 1800
JACK_LEN = 104.0
_jtest = PC.jack_sprite(JACK_LEN, AS, 1)
JACK_PROT = _jtest[1] - 1.5 / AS
PY0, PH, PR = 34.0, 890.0, 30.0
PW = OUT_W / OUT_H * PH - 2 * JACK_PROT
PX0 = (UW - PW) / 2
CX = PX0 + PW / 2
CROP_X0, CROP_W = PX0 - JACK_PROT, PW + 2 * JACK_PROT
SC = (OUT_H / 2.0) / PH                           # unites de travail -> unites du plugin (617,5 x 900)

# disposition de la v1.4.1 (repere 760 x 958 du visuel d'origine, boitier x 131..629 / y 22.5..928.5, axe x = 380)
# -> nouveau boitier : les ecarts horizontaux suivent la largeur du boitier, les tailles suivent la hauteur
OLD_CX, OLD_HW, OLD_Y0, OLD_H = 380.0, 248.75, 22.5, 906.0
KXn = (PW / 2) / OLD_HW
SV = PH / OLD_H
NX = lambda x: CX + (x - OLD_CX) * KXn
NY = lambda y: PY0 + (y - OLD_Y0) * SV

KNOBS = [(NX(255.83), NY(161.67), 'LOUDNESS', 59.17 * SV, NY(252.9)),
         (NX(501.67), NY(160.00), 'SUSTAIN',  59.17 * SV, NY(252.5)),
         (NX(380.00), NY(353.33), 'FILTER',   48.33 * SV, NY(425.0))]
TOG_R = 24.0 * SV                                  # rayon de la rondelle des inverseurs
TOG_S = TOG_R * 3.6                                # cote du sprite (levier + ombre)
TOGGLES = [(NX(238.3), NY(357.3), 'CRUNCH', NY(425.0)), (NX(521.7), NY(357.1), 'SCOOP', NY(424.6))]
OCT = (NX(521.7), NY(685.83))                      # inverseur OCTAVE 3 positions (haut Fat, milieu off, bas Bright)
LED = (CX, NY(685.83))
OCT_LED = (NX(477.67), NY(685.83))
FS = (CX, NY(796.67))
STATUS = (NX(195.0), NY(454.17), 371.67 * KXn, 47.5 * SV)
PRESET = (NX(185.0), NY(510.0), 390.83 * KXn, 32.5 * SV)
JACK_Y = NY(487.0)
LOGO_C, LOGO_W = (CX, NY(905.4)), 190.0
LED_LENS = 7.6 * SV
FS_FLATS = 96.0 * SV
# cornichon : gravure vectorielle (design/source/cornichon.svg), place comme en v1.4.1 (reduit de 12 %, descendu ~3 mm)

# ------------------------------------------------------------------ outils
YY, XX = np.mgrid[0:H, 0:W].astype(np.float32)
UX, UY = (XX + 0.5) / AS, (YY + 0.5) / AS

def rrect_sd(x, y, x0, y0, w, h, r):
    cx, cy = x0 + w / 2, y0 + h / 2
    qx = np.abs(x - cx) - (w / 2 - r); qy = np.abs(y - cy) - (h / 2 - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r

def fbm(shape, scales, amps, seed=0):
    r = np.random.default_rng(seed); out = np.zeros(shape, np.float32)
    for s, a in zip(scales, amps):
        n = r.standard_normal((max(2, shape[0] // s + 2), max(2, shape[1] // s + 2))).astype(np.float32)
        out += a * cv2.resize(n, (shape[1], shape[0]), interpolation=cv2.INTER_CUBIC)
    return out

def over(dst, rgba, x0, y0):
    h, w = rgba.shape[:2]
    X0, Y0 = int(round(x0)), int(round(y0))
    sx0, sy0 = max(0, -X0), max(0, -Y0)
    ex, ey = min(w, dst.shape[1] - X0), min(h, dst.shape[0] - Y0)
    a = rgba[sy0:ey, sx0:ex, 3:4] / 255.0
    reg = dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex]
    dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex] = reg * (1 - a) + rgba[sy0:ey, sx0:ex, :3] * a

# ------------------------------------------------------------------ 1. boitier : peinture verte metallisee (comme le visuel valide)
sd = rrect_sd(UX, UY, PX0, PY0, PW, PH, PR)
body = np.clip(0.5 - sd * AS, 0, 1)
img = np.zeros((H, W, 3), np.float32)
EDGE = 9.0
d_in = np.clip(-sd, 0, None)
hgt = np.where(d_in < EDGE, np.sqrt(np.clip(EDGE ** 2 - (EDGE - d_in) ** 2, 0, None)), EDGE)
gy, gx = np.gradient(cv2.GaussianBlur(hgt, (0, 0), 1.0), 1.0 / AS)
nrm = np.dstack([-gx, -gy, np.ones_like(hgt)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
ndl = np.clip(nrm @ L, 0, 1); ndh = np.clip(nrm @ HV, 0, 1)
paint = np.array([41.0, 92.0, 32.0])                      # vert de la pedale (mesure sur le visuel valide)
encl = paint * (0.52 + 0.74 * ndl)[..., None] + (64 * ndh ** 40 + 16 * ndh ** 6)[..., None] * np.array([0.85, 1.0, 0.8])
rng = np.random.default_rng(11)
flake = (rng.random((H, W)) ** 16).astype(np.float32)
flake = cv2.GaussianBlur(flake, (0, 0), 0.65) * 190
sheen = np.exp(-(((UX - 250) / 320) ** 2 + ((UY - 220) / 360) ** 2))
grain = fbm((H, W), [1, 2], [1.6, 0.9], 3) + 0.8 * fbm((H, W), [90, 260], [0.8, 1.0], 4)
encl += grain[..., None] * np.array([0.55, 1.0, 0.5])
encl += (flake * (0.45 + 0.8 * sheen))[..., None] * np.array([0.62, 1.0, 0.55])
encl += (20.0 * sheen)[..., None] * np.array([0.45, 1.0, 0.4])
shade = 1.0 + 0.06 * np.clip(1 - (UX - PX0) / PW * 0.7 - (UY - PY0) / PH * 0.7, -1, 1)     # un peu plus clair en haut a gauche
encl *= shade[..., None]
img = encl * body[..., None]

# ------------------------------------------------------------------ 2. serigraphie vectorielle (dessinee par le plugin) + reserves du dessin
SS = 2; K = AS * SS; KW, KH = W * SS, H * SS
FONT = {'medium': os.path.join(ROOT, 'assets', 'fonts', 'Poppins-Medium.ttf'),
        'bold': os.path.join(ROOT, 'assets', 'fonts', 'Poppins-Bold.ttf'),
        'light': os.path.join(ROOT, 'assets', 'fonts', 'Poppins-Light.ttf')}
ink_im = Image.new('L', (KW, KH), 0); dr = ImageDraw.Draw(ink_im)
pk_im = Image.new('L', (KW, KH), 0); pk_dr = ImageDraw.Draw(pk_im)
VTEXT, VTICKS = [], []
def font(kind, em): return ImageFont.truetype(FONT[kind], max(4, int(round(em * K))))
def text(x, y, txt, kind, em, track, anchor='m', d=None, record=True):
    f = font(kind, em)
    widths = [f.getlength(ch) for ch in txt]
    total = sum(widths) + track * K * (len(txt) - 1)
    cx = x * K - total / 2 if anchor == 'm' else x * K
    if record:
        VTEXT.append((txt, kind, em, track, (cx + total / 2) / K, y, 0))
    for ch, w in zip(txt, widths):
        (d or dr).text((cx, y * K), ch, font=f, fill=255, anchor='lm'); cx += w + track * K
    return total / K
LAB_EM, LAB_TR = 17.0, 2.2
for (x, y, name, R, ly) in KNOBS:
    text(x, ly, name, 'medium', LAB_EM, LAB_TR)
    for i in range(11):                                       # graduations : 11 traits, 7 h -> 5 h, tous de meme taille
        a = math.radians(-150 + 30 * i)
        r0, r1 = R * 1.11, R * 1.235
        VTICKS.append((x + r0 * math.sin(a), y - r0 * math.cos(a), x + r1 * math.sin(a), y - r1 * math.cos(a), max(1.6, 0.036 * R)))
        dr.line([((x + r0 * math.sin(a)) * K, (y - r0 * math.cos(a)) * K), ((x + r1 * math.sin(a)) * K, (y - r1 * math.cos(a)) * K)],
                fill=255, width=int(max(1.6, 0.036 * R) * K))
for (x, y, name, ly) in TOGGLES:
    text(x, ly, name, 'medium', LAB_EM, LAB_TR)
# nom : STRONG / PICKLE / FUZZ (PICKLE s'allume en vert avec l'effet : dessine a part par le plugin)
text(CX, NY(573.3), 'STRONG', 'medium', 22.0, 9.0)
PICKLE_EM, PICKLE_TR = 60.0, 2.0
pw_ = text(CX, NY(615.4), 'PICKLE', 'bold', PICKLE_EM, PICKLE_TR, record=False)
text(CX, NY(615.4), 'PICKLE', 'bold', PICKLE_EM, PICKLE_TR, d=pk_dr, record=False)
PICKLE_TXT = ('PICKLE', 'bold', PICKLE_EM, PICKLE_TR, CX, NY(615.4), 0)
text(CX, NY(654.5), 'FUZZ', 'medium', 19.0, 8.0)
# inverseur OCTAVE : FAT au-dessus, BRIGHT en dessous, OCTAVE a droite (comme en v1.4.1)
text(OCT[0], OCT[1] - 50.0 * SV, 'FAT', 'medium', 12.5, 1.6)        # au-dessus du bout du levier (position haute)
text(OCT[0], OCT[1] + 47.0 * SV, 'BRIGHT', 'medium', 12.5, 1.6)
text(OCT[0] + TOG_R + 7.0, OCT[1], 'OCTAVE', 'medium', LAB_EM, LAB_TR, anchor='l')
# OUT / IN a la hauteur des jacks (horizontaux, comme sur le visuel)
text(NX(163.3), JACK_Y, 'OUT', 'medium', 15.0, 1.8)
text(NX(596.7), JACK_Y, 'IN', 'medium', 15.0, 1.8)
print('PICKLE : %.1f unites de large' % pw_)
ink = cv2.resize(np.asarray(ink_im, np.float32) / 255.0, (W, H), interpolation=cv2.INTER_AREA)
pk_ink = cv2.resize(np.asarray(pk_im, np.float32) / 255.0, (W, H), interpolation=cv2.INTER_AREA)

# ------------------------------------------------------------------ 3. cornichon (gravure vectorielle creme), sous tous les elements
CORN = os.path.join(ROOT, 'design', 'source', 'cornichon.svg')
# placement (unites de travail) : centre de la boite du dessin et largeur, d'apres la v1.4.1 (boite 157..559.5 x 222..810)
OLD_BOX = (157.0, 559.5, 222.0, 810.0)
ocx, ocy = 0.5 * (OLD_BOX[0] + OLD_BOX[1]), 0.5 * (OLD_BOX[2] + OLD_BOX[3])
CORN_C = (NX(ocx), NY(ocy))
CORN_H = (OLD_BOX[3] - OLD_BOX[2]) * SV                    # hauteur du dessin (meme echelle en largeur : rien n'est etire)
# le SVG : contenu dans x 230..929, y 498..1519 du viewBox (30 458 1100 1100) -> on rend le viewBox et on place la boite
VB = (30.0, 458.0, 1100.0, 1100.0); SB = (230.0, 929.0, 498.0, 1519.0)
k_u = CORN_H / (SB[3] - SB[2])                             # unites de travail par unite SVG
npx = int(round(VB[2] * k_u * AS))
png = subprocess.run(['rsvg-convert', '-w', str(npx), '-h', str(npx), CORN], capture_output=True, check=True).stdout
cv_ = np.asarray(Image.open(io.BytesIO(png)).convert('RGBA')).astype(np.float32)
c_a = cv_[..., 3] / 255.0
sbx, sby = (0.5 * (SB[0] + SB[1]) - VB[0]) * k_u, (0.5 * (SB[2] + SB[3]) - VB[1]) * k_u     # centre de la boite dans le rendu (unites)
x0p, y0p = int(round((CORN_C[0] - sbx) * AS)), int(round((CORN_C[1] - sby) * AS))
lay = np.zeros((H, W), np.float32)
sx0, sy0 = max(0, -x0p), max(0, -y0p); ex, ey = min(npx, W - x0p), min(npx, H - y0p)
lay[y0p + sy0:y0p + ey, x0p + sx0:x0p + ex] = c_a[sy0:ey, sx0:ex]
# reserves : textes (boite de chaque mot + marge), potards, inverseurs, LED, footswitch, logo
keep = np.zeros((H, W), np.float32)
words = cv2.morphologyEx(((ink + pk_ink) > 0.05).astype(np.uint8), cv2.MORPH_CLOSE, np.ones((9, 41), np.uint8))
n_, lab_, st_, _ = cv2.connectedComponentsWithStats(words, 8)
for i in range(1, n_):
    x_, y_, w_, h_, a_ = st_[i]
    if h_ < 160 and w_ > 10:
        m = 6 * AS
        keep[max(0, y_ - m):y_ + h_ + m, max(0, x_ - m):x_ + w_ + m] = 1.0
_lw = int(LOGO_W * AS)
_lg = np.asarray(Image.open(io.BytesIO(subprocess.run(['rsvg-convert', '-w', str(_lw), os.path.join(ROOT, 'NineHandsLogo.svg')],
                 capture_output=True, check=True).stdout)).convert('RGBA'))[..., 3]
_lh = _lg.shape[0]; _x0, _y0 = int(LOGO_C[0] * AS - _lw / 2), int(LOGO_C[1] * AS - _lh / 2)
keep[_y0 - 12:_y0 + _lh + 12, _x0 - 12:_x0 + _lw + 12] = 1.0
for (x, y, _, R, _ly) in KNOBS: keep = np.maximum(keep, (np.hypot(UX - x, UY - y) < R + 3).astype(np.float32))
for (x, y) in [t[:2] for t in TOGGLES] + [OCT]: keep = np.maximum(keep, (np.hypot(UX - x, UY - y) < TOG_R - 0.5).astype(np.float32))
for c in (LED, OCT_LED): keep = np.maximum(keep, (np.hypot(UX - c[0], UY - c[1]) < LED_LENS * 1.6).astype(np.float32))
keep = np.maximum(keep, (np.hypot(UX - FS[0], UY - FS[1]) < FS_FLATS * 0.62).astype(np.float32))
keep = cv2.GaussianBlur(keep, (0, 0), 1.0 * AS)
on_top = np.clip((-sd - 6.0) / 2.0, 0, 1)
cut = np.clip(keep * 1.4, 0, 1)
# petits morceaux du dessin coupes par une reserve : retires en entier plutot que tronques (pas de fragment isole)
n0, lab0, st0, _ = cv2.connectedComponentsWithStats((lay > 0.4).astype(np.uint8), 8)
kept = np.bincount(lab0.ravel(), weights=(cut < 0.5).ravel().astype(np.float64), minlength=n0)
frac = kept / np.maximum(st0[:, 4], 1)
drop = np.zeros(n0, bool); drop[1:] = (frac[1:] < 0.98) & (st0[1:, 4] < 1200 * AS * AS / 4)
dropm = cv2.dilate(drop[lab0].astype(np.uint8), np.ones((5, 5), np.uint8)).astype(np.float32)
# bouts de trait coupes par une reserve : fragments restants plus petits que ~12 unites retires aussi
vis = ((lay > 0.4) & (cut < 0.5) & (dropm < 0.5)).astype(np.uint8)
n1, lab1, st1, _ = cv2.connectedComponentsWithStats(vis, 8)
tiny = np.zeros(n1, bool); tiny[1:] = (st1[1:, 4] < (12 * AS) ** 2 * 0.6) & (np.maximum(st1[1:, 2], st1[1:, 3]) < 14 * AS)
whole = np.bincount(lab0.ravel(), minlength=n0)
# un fragment n'est retire que s'il provient d'un trait plus grand que lui (coupe) : les points et virgules du dessin restent
src_size = np.zeros(n1); 
for i in np.where(tiny)[0]:
    ys_, xs_ = np.where(lab1 == i); src_size[i] = whole[lab0[ys_[0], xs_[0]]]
tiny &= src_size > st1[:, 4] * 1.6
dropm = np.maximum(dropm, cv2.dilate(tiny[lab1].astype(np.uint8), np.ones((5, 5), np.uint8)).astype(np.float32))
top_cut = np.clip((UY - (NY(262.0) + 2.0)) / 2.0, 0, 1)                 # le dessin s'arrete sous les etiquettes LOUDNESS / SUSTAIN
corn_a = np.clip(lay * 0.95, 0, 1) * (1 - cut) * (1 - dropm) * on_top * top_cut
cream = np.array([214.0, 212.0, 160.0])                     # #dfdfa5 un peu patine, avec le grain de la peinture
gr_ = img - cv2.GaussianBlur(img, (0, 0), 2.0)
light = 1.0 + 0.05 * np.clip(1 - (UX - PX0) / PW * 0.6 - (UY - PY0) / PH * 0.6, -1, 1)
corn_col = cream * light[..., None] + 0.35 * gr_
img = img * (1 - corn_a[..., None]) + corn_col * corn_a[..., None]
img += (fbm((H, W), [1], [0.9], 22) * body)[..., None]

# ------------------------------------------------------------------ 4. commandes (photos du kit)
print('potards, inverseurs, LED, footswitch...')
def occlusion(mask, strength=1.0):
    contact = cv2.GaussianBlur(mask, (0, 0), 3.0 * AS)
    soft = cv2.GaussianBlur(mask, (0, 0), 14.0 * AS)
    o = np.clip(contact * 0.55 + soft * 0.9, 0, 0.5) * strength * (1.0 - mask)
    img[...] = img * (1.0 - o * body)[..., None]
_m = np.zeros((H, W), np.float32)
for (x, y, _, R, _ly) in KNOBS: _m = np.maximum(_m, np.clip((R - np.hypot(UX - x, UY - y)) * AS, 0, 1))
for c in (LED, OCT_LED): _m = np.maximum(_m, np.clip((LED_LENS * 1.07 - np.hypot(UX - c[0], UY - c[1])) * AS, 0, 1) * 0.6)
_fs_tmp = PC.footswitch_sprite(FS_FLATS, AS)[0]
_n = _fs_tmp.shape[0]; _x0 = int(round((FS[0] - _n / AS / 2) * AS)); _y0 = int(round((FS[1] - _n / AS / 2) * AS))
_fa = np.zeros((H, W), np.float32); _fa[_y0:_y0 + _n, _x0:_x0 + _n] = np.clip((_fs_tmp[..., 3] / 255.0 - 0.86) / 0.14, 0, 1)
_m = np.maximum(_m, _fa)
occlusion(_m)
for (x, y, _, R, _ly) in KNOBS:
    KN, KS = PC.knob_sprite(R, AS)
    over(img, KN, (x - KS / 2) * AS, (y - KS / 2) * AS)
LD, LDON, LS = PC.led_sprites(LED_LENS, AS, colour='yellow')
for c in (LED, OCT_LED): over(img, LD, (c[0] - LS / 2) * AS, (c[1] - LS / 2) * AS)
Image.fromarray(np.clip(LDON, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'led_on.png'))
FSPR, FSREFL, FS_W, FS_BTN = PC.footswitch_sprite(FS_FLATS, AS, refl_colour=(255.0, 205.0, 60.0))
over(img, FSPR, (FS[0] - FS_W / 2) * AS, (FS[1] - FS_W / 2) * AS)
Image.fromarray(FSREFL, 'RGBA').save(os.path.join(ROOT, 'fs_refl.png'))
# inverseurs : sprites dessines par le plugin (haut / milieu / bas), recales sur la rondelle -> pas dans le fond
TFR = TP.aligned_frames(TOG_R, TOG_S, AS * 2)              # 4 px par unite : net sur les ecrans HiDPI
strip = np.concatenate(TFR, 0)
Image.fromarray(strip, 'RGBA').save(os.path.join(ROOT, 'toggle_strip.png'), optimize=True)

# ------------------------------------------------------------------ 5. embases jack (photo du kit) : OUT a gauche, IN a droite
alpha = body.copy()
for side in (-1, 1):
    j, jw_u, jh_u = PC.jack_sprite(JACK_LEN, AS, side)
    jh, jw = j.shape[:2]
    jw2 = int(math.floor(JACK_PROT * AS))
    if jw2 != jw:
        pm_ = j.copy(); pm_[..., :3] *= pm_[..., 3:4] / 255.0
        pm_ = cv2.resize(pm_, (jw2, jh), interpolation=cv2.INTER_AREA)
        a_ = np.maximum(pm_[..., 3:4], 1e-3); pm_[..., :3] = np.where(pm_[..., 3:4] > 0.5, pm_[..., :3] * 255.0 / a_, 0)
        j = pm_; jw = jw2
    x0 = int(round((PX0 * AS - jw) if side < 0 else ((PX0 + PW) * AS)))
    y0 = int(round(JACK_Y * AS - jh / 2))
    occ = np.zeros((H, W), np.float32); occ[y0:y0 + jh, x0:x0 + jw] = j[..., 3] / 255.0
    occ = np.clip(cv2.GaussianBlur(occ, (0, 0), 3.0 * AS) * 0.9 + cv2.GaussianBlur(occ, (0, 0), 18.0 * AS) * 1.6, 0, 0.62)
    occ *= 0.2 if side > 0 else 0.7
    img *= (1.0 - occ * body)[..., None]
    over(img, j, x0, y0)
    ja = np.zeros((H, W), np.float32); ja[y0:y0 + jh, x0:x0 + jw] = j[..., 3] / 255.0
    alpha = np.maximum(alpha, ja)

# ------------------------------------------------------------------ 6. sauvegarde du fond
def to_out(a):
    x0, x1 = CROP_X0 * AS, (CROP_X0 + CROP_W) * AS
    y0, y1 = PY0 * AS, (PY0 + PH) * AS
    M = np.float32([[OUT_W / (x1 - x0), 0, -x0 * OUT_W / (x1 - x0)], [0, OUT_H / (y1 - y0), -y0 * OUT_H / (y1 - y0)]])
    return cv2.warpAffine(a, M, (OUT_W, OUT_H), flags=cv2.INTER_AREA if OUT_H < (y1 - y0) else cv2.INTER_CUBIC)
out_rgb = np.clip(to_out(img), 0, 255); out_a = np.clip(to_out(alpha), 0, 1)
Image.fromarray(np.dstack([out_rgb, out_a * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'background.png'), optimize=True)

# ------------------------------------------------------------------ 7. lumieres (calques alpha : seule la lumiere ajoutee)
GR = 110.0
def glow(c, fn, col=(1.0, 0.72, 0.10)):
    x0p, y0p = round((c[0] - GR) * AS), round((c[1] - GR) * AS)
    size = round(2 * GR * AS)
    reg = img[y0p:y0p + size, x0p:x0p + size].astype(np.float64)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    d = np.hypot((xx + 0.5) / AS - GR, (yy + 0.5) / AS - GR)
    h1, h2 = 8.0, 30.0
    E = 1.1 * h1 ** 3 / (d ** 2 + h1 ** 2) ** 1.5 + 0.176 * h2 ** 3 / (d ** 2 + h2 ** 2) ** 1.5
    E *= np.clip((GR - d) / 30.0, 0, 1)
    E[d < LED_LENS * 1.07] *= 0.0
    lum = reg.mean(2, keepdims=True)
    lit = reg + (reg * 2.0 + 12.0 * np.clip(lum / 40.0, 0.3, 1.6)) * E[..., None] * np.array(col)
    add = lit - reg
    a = np.clip(np.abs(add).max(-1) / 12.0, 0, 1)
    rgb = np.where(a[..., None] > 1e-3, reg + add / np.maximum(a, 1e-3)[..., None], reg)
    Image.fromarray(np.dstack([np.clip(rgb, 0, 255), a * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, fn))
    return (x0p / AS, y0p / AS, size / AS, size / AS)
GLR = glow(LED, 'glow_yellow.png')
GLO = glow(OCT_LED, 'glow_yellow_oct.png')
# halo vert de PICKLE sur la facade (les lettres elles-memes sont vectorielles, dessinees en vert par le plugin)
ys_, xs_ = np.where(pk_ink > 0.3)
pb = (xs_.min() / AS - 24, ys_.min() / AS - 24, (xs_.max() - xs_.min()) / AS + 48, (ys_.max() - ys_.min()) / AS + 48)
px0, py0, pw2, ph2 = int(pb[0] * AS), int(pb[1] * AS), int(pb[2] * AS), int(pb[3] * AS)
m_ = pk_ink[py0:py0 + ph2, px0:px0 + pw2]
reg = img[py0:py0 + ph2, px0:px0 + pw2].astype(np.float64)
h1, h2 = cv2.GaussianBlur(m_, (0, 0), 3.0 * AS), cv2.GaussianBlur(m_, (0, 0), 12.0 * AS)
lit = reg * (1 + (0.55 * h1 + 0.75 * h2)[..., None] * np.array([0.25, 1.0, 0.2])) + (0.30 * h1)[..., None] * np.array([80, 255, 60])
add = np.clip(lit, 0, 255) - reg
a = np.clip(np.abs(add).max(-1) / 40.0, 0, 1) * (1 - np.clip(m_ * 1.2, 0, 1))
rgb = np.where(a[..., None] > 1e-3, reg + add / np.maximum(a, 1e-3)[..., None], reg)
Image.fromarray(np.dstack([np.clip(rgb, 0, 255), a * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'pickle_glow.png'))
PKR = (px0 / AS, py0 / AS, pw2 / AS, ph2 / AS)

# ------------------------------------------------------------------ 8. Layout.h (repere du plugin : 617,5 x 900 unites)
def f(v): return f"{v:.2f}f"
def X(v): return (v - CROP_X0) * SC
def Y(v): return (v - PY0) * SC
def PT(p): return "{ %s, %s }" % (f(X(p[0])), f(Y(p[1])))
def RC(r): return "{ %s, %s, %s, %s }" % (f(X(r[0])), f(Y(r[1])), f(r[2] * SC), f(r[3] * SC))
fk = {'medium': 0, 'bold': 1, 'light': 2}
json.dump({"frame": [OUT_W / 2, OUT_H / 2], "knobs": [[X(k[0]), Y(k[1]), k[3] * SC] for k in KNOBS],
           "toggles": [[X(t[0]), Y(t[1])] for t in TOGGLES], "octave": [X(OCT[0]), Y(OCT[1])], "togR": TOG_R * SC,
           "led": [X(LED[0]), Y(LED[1])], "octLed": [X(OCT_LED[0]), Y(OCT_LED[1])], "fs": [X(FS[0]), Y(FS[1])],
           "status": [X(STATUS[0]), Y(STATUS[1]), STATUS[2] * SC, STATUS[3] * SC],
           "preset": [X(PRESET[0]), Y(PRESET[1]), PRESET[2] * SC, PRESET[3] * SC], "jackY": Y(JACK_Y),
           "jackL": X(PX0 - JACK_PROT / 2), "jackR": X(PX0 + PW + JACK_PROT / 2),
           "pickle": [X(PKR[0]), Y(PKR[1]), PKR[2] * SC, PKR[3] * SC]},
          open(os.path.join(ROOT, 'tools', 'layout.json'), 'w'), indent=1)
with open(os.path.join(ROOT, 'Layout.h'), 'w') as fo:
    fo.write(f"// genere par tools/make_art.py - ne pas modifier a la main (repere {OUT_W / 2} x {OUT_H / 2} = fond {OUT_W} x {OUT_H} px / 2)\n#pragma once\n#include <juce_graphics/juce_graphics.h>\nnamespace layout\n{{\n")
    fo.write(f"    constexpr float frameW = {f(OUT_W / 2)}, frameH = {f(OUT_H / 2)};   // pedale plein cadre (jacks aux bords gauche / droit)\n")
    fo.write("    // ordre : loudness, sustain, filter\n")
    fo.write("    const juce::Point<float> knobPos[3] = { " + ", ".join(PT(k[:2]) for k in KNOBS) + " };\n")
    fo.write("    const float knobR[3] = { " + ", ".join(f(k[3] * SC) for k in KNOBS) + " };\n")
    fo.write(f"    const juce::Point<float> crunchPos {PT(TOGGLES[0][:2])}, scoopPos {PT(TOGGLES[1][:2])}, octavePos {PT(OCT)};\n")
    fo.write(f"    const float togS = {f(TOG_S * SC)}, togR = {f(TOG_R * SC)};   // sprite d'inverseur (cote) / rondelle\n")
    fo.write(f"    const juce::Point<float> ledPos {PT(LED)}, octLedPos {PT(OCT_LED)}; const float ledS = {f(LS * SC)};\n")
    fo.write(f"    const juce::Rectangle<float> glowRect {RC(GLR)}, octGlowRect {RC(GLO)};\n")
    fo.write(f"    const juce::Point<float> fsPos {PT(FS)}; const float footR = {f(FS_BTN * SC)}, fsS = {f(FS_W * SC)};   // bouton / sprite\n")
    fo.write(f"    const juce::Rectangle<float> statusRect {RC(STATUS)}, presetRect {RC(PRESET)};\n")
    fo.write(f"    const float statusFont = {f(26.0 * SC)}, presetFont = {f(17.5 * SC)};\n")
    fo.write(f"    const juce::Rectangle<float> boxRect {RC((PX0, PY0, PW, PH))}, pickleGlowRect {RC(PKR)};\n")
    fo.write(f"    const float jackY = {f(Y(JACK_Y))};\n")
    fo.write("    // serigraphie vectorielle : texte, police (0 medium, 1 bold, 2 light), taille em, espacement, centre, rotation (degres)\n")
    fo.write("    struct Txt { const char* s; int font; float em, track, x, y, rot; };\n")
    fo.write("    const Txt texts[] = {\n" + ",\n".join(f'        {{ "{t[0]}", {fk[t[1]]}, {f(t[2] * SC)}, {f(t[3] * SC)}, {f(X(t[4]))}, {f(Y(t[5]))}, {f(t[6])} }}' for t in VTEXT) + "\n    };\n")
    t = PICKLE_TXT
    fo.write(f'    const Txt pickleText {{ "{t[0]}", {fk[t[1]]}, {f(t[2] * SC)}, {f(t[3] * SC)}, {f(X(t[4]))}, {f(Y(t[5]))}, {f(t[6])} }};   // s\'allume en vert\n')
    fo.write("    struct Tick { float x0, y0, x1, y1, w; };\n")
    fo.write("    const Tick ticks[] = { " + ", ".join(f"{{ {f(X(d[0]))}, {f(Y(d[1]))}, {f(X(d[2]))}, {f(Y(d[3]))}, {f(d[4] * SC)} }}" for d in VTICKS) + " };\n")
    fo.write(f"    const juce::Point<float> logoPos {PT(LOGO_C)}; const float logoW = {f(LOGO_W * SC)};\n")
    fo.write("}\n")

# ------------------------------------------------------------------ 9. apercu brut (sans la serigraphie ni les inverseurs : voir tools/render)
prev = img.copy()
over(prev, np.asarray(Image.open(os.path.join(ROOT, 'glow_yellow.png'))).astype(np.float32), GLR[0] * AS, GLR[1] * AS)
over(prev, LDON, (LED[0] - LS / 2) * AS, (LED[1] - LS / 2) * AS)
pv = np.clip(to_out(prev), 0, 255); bgc = np.array([14.0, 14.0, 15.0])
pv = pv * out_a[..., None] + bgc * (1 - out_a[..., None])
os.makedirs(os.path.join(ROOT, 'design'), exist_ok=True)
Image.fromarray(pv.astype(np.uint8)).save(os.path.join(ROOT, 'design', 'preview_art.png'))
print('ok', 'boitier %.1f x %.1f unites, PICKLE glow' % (PW * SC, PH * SC), [round(v, 1) for v in PKR])
