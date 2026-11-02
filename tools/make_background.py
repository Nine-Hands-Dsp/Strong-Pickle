"""
Strong Pickle (Studio Wiga) - fond du plugin, design original (aucune photo de la pedale d'origine).

Boitier compact facon « Smalls » (pedale simple), peinture vert cornichon metallisee a paillettes,
relief de « verrues » de cornichon, inscriptions creme, logo Studio Wiga.
Une seule lumiere pour toute la pedale : en haut a gauche (ombres portees vers le bas a droite).

Sorties (cadre 1520 x 1916 = 2 x 760 x 958, cadrage Echoflex / Doorn) :
  background.jpg      fond (pedale sans les pieces mobiles : reperes des potards, inverseurs, LED allumee)
  toggle_strip.png    inverseur a levier du kit Studio Wiga (FreakOut), 3 images carrees : bas, intermediaire, haut
  glow_blue.png       calque d'eclairage de la facade par la LED bleue (alpha : seule la lumiere ajoutee)
  Layout.h            reperes en unites 760 x 958
  tools/layout.json   memes reperes (notice)

Usage : python3 tools/make_background.py
"""
import os, json, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
AS = os.path.join(ROOT, 'assets')
FONTS = os.path.join(AS, 'fonts')
PX = 2
W2, H2 = 760 * PX, 958 * PX

L = np.array([-0.46, -0.62, 0.64]); L /= np.linalg.norm(L)       # vers la lumiere (haut gauche)
V = np.array([0.0, 0.0, 1.0])
HV = (L + V) / np.linalg.norm(L + V)
rng = np.random.default_rng(41)

# ------------------------------------------------------------------ reperes (unites 760 x 958)
PEDAL = (130.0, 22.0, 630.0, 938.0); PR = 30.0                     # boitier, rayon des angles
KNOB_R = 46.0                                                      # FILTER
KNOB_RS = {'loudness': 60.0, 'sustain': 60.0, 'filter': KNOB_R}     # v1.0 : gros potards du haut, comme sur la Swollen Pickle
KNOBS = {'loudness': (258.0, 160.0), 'sustain': (502.0, 160.0), 'filter': (380.0, 350.0)}
TOGGLES = {'crunch': (240.0, 352.0), 'scoop': (520.0, 352.0)}
TOG_SCALE = 0.86                                                   # kit FreakOut -> Strong Pickle
LED = (380.0, 690.0); LED_R = 7.6
FOOT = (380.0, 800.0); FOOT_FLATS = 104.0                          # largeur entre plats de l'ecrou
STATUS = (196.0, 448.0, 368.0, 48.0)
PRESET = (186.0, 502.0, 388.0, 34.0)
JACK_Y = 480.0                                                     # jacks au milieu de la hauteur du boitier
GLOW = (LED[0] - 128.0, LED[1] - 112.0, 256.0, 256.0)

U = lambda v: v * PX
yy, xx = np.mgrid[0:H2, 0:W2].astype(np.float32)
ux, uy = (xx + 0.5) / PX, (yy + 0.5) / PX

def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)

def rrect_sdf(x, y, x0, y0, x1, y1, r):
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2 - r, (y1 - y0) / 2 - r
    qx, qy = np.abs(x - cx) - hx, np.abs(y - cy) - hy
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r

def normals(h, scale_px):
    gy, gx = np.gradient(h, 1.0 / scale_px)
    n = np.dstack([-gx, -gy, np.ones_like(h)])
    return n / np.linalg.norm(n, axis=2, keepdims=True)

def blur(a, s_units):
    return cv2.GaussianBlur(a, (0, 0), s_units * PX)

# ------------------------------------------------------------------ 1. fond (plan de travail sombre)
bg = np.zeros((H2, W2, 3), np.float32)
r = np.hypot((ux - 380) / 520, (uy - 470) / 640)
base = 13 + 12 * np.clip(1 - r, 0, 1) ** 1.5
bg[:] = np.dstack([base * 0.92, base * 1.0, base * 0.95])
bg += rng.normal(0, 1.1, (H2, W2, 1)).astype(np.float32)

# ------------------------------------------------------------------ 2. boitier : forme, bord arrondi, ombre portee
x0, y0, x1, y1 = PEDAL
sd = rrect_sdf(ux, uy, x0, y0, x1, y1, PR)                       # < 0 a l'interieur
inside = np.clip(0.5 - sd * PX, 0, 1)

# ombre portee (douce + contact), vers le bas a droite
sh_far = blur((rrect_sdf(ux - 13, uy - 19, x0, y0, x1, y1, PR) < 0).astype(np.float32), 13)
sh_near = blur((rrect_sdf(ux - 3, uy - 5, x0, y0, x1, y1, PR) < 0).astype(np.float32), 3.5)
bg *= (1 - 0.62 * sh_far[..., None]) * (1 - 0.45 * sh_near[..., None])

# relief : bord arrondi de la fonderie (rayon 9 unites) ; facade lisse (pas de verrues)
BEV = 9.0
d_in = np.clip(-sd, 0, None)
h = BEV * np.sqrt(np.clip(1 - np.clip(1 - d_in / BEV, 0, 1) ** 2, 0, 1))

# zones degagees (commandes, inscriptions, ecran, footswitch, logo) : pas de verrues
clear = np.zeros((H2, W2), np.float32)
def clear_disk(c, rr):
    global clear
    clear = np.maximum(clear, (np.hypot(ux - c[0], uy - c[1]) < rr).astype(np.float32))
def clear_rect(x, y, w, hh, pad=8):
    global clear
    clear = np.maximum(clear, ((ux > x - pad) & (ux < x + w + pad) & (uy > y - pad) & (uy < y + hh + pad)).astype(np.float32))
for k_, c in KNOBS.items(): clear_disk(c, KNOB_RS[k_] + 16)
for c in TOGGLES.values(): clear_disk(c, 34)
clear_disk(LED, 24); clear_disk(FOOT, 74)
clear_rect(STATUS[0], STATUS[1], STATUS[2], PRESET[1] + PRESET[3] - STATUS[1], 10)
for lab in ((262, 238), (498, 238), (380, 420), (240, 420), (520, 420)): clear_rect(lab[0] - 60, lab[1] - 14, 120, 28, 4)
clear_rect(190, 548, 380, 104, 6)          # nom
clear_rect(250, 890, 260, 34, 4)           # logo
clear_rect(140, JACK_Y - 14, 52, 28, 2); clear_rect(568, JACK_Y - 14, 52, 28, 2)
clear = blur(clear, 6)

bumps = np.zeros((H2, W2), np.float32)   # v1.0 : texture cornichon en relief retiree, facade lisse
N = normals(h, PX)

# peinture vert cornichon metallisee : couleur de base + paillettes dorees + leger peau d'orange
base_col = np.array([66, 150, 34], np.float32)     # vert vif metallise, proche de la Swollen Pickle
dark_col = np.array([38, 98, 22], np.float32)
# degrade : centre un peu plus clair, bords plus sombres (peinture plus fine sur les aretes)
cg = np.clip(1 - np.hypot((ux - 380) / 300, (uy - 470) / 520), 0, 1)
col = dark_col + (base_col - dark_col) * (0.55 + 0.45 * cg)[..., None]
col = col * (1 + 0.10 * bumps[..., None] / 1.6)                    # verrues un peu plus claires (peinture plus epaisse)
flake = rng.random((H2, W2)).astype(np.float32)
flake = np.where(flake > 0.985, rng.uniform(0.5, 1.0, (H2, W2)).astype(np.float32), 0.0)
orange = blur(rng.normal(0, 1, (H2, W2)).astype(np.float32), 1.2) * 0.35
Nl = N + np.dstack([np.zeros_like(orange), orange, np.zeros_like(orange)]) * 0.15
Nl /= np.linalg.norm(Nl, axis=2, keepdims=True)
ndl = np.clip((Nl * L).sum(-1), 0, 1)
ndh = np.clip((Nl * HV).sum(-1), 0, 1)
# grande tache de lumiere (lampe en haut a gauche) sur la facade
lamp = np.exp(-(((ux - 250) / 330) ** 2 + ((uy - 230) / 420) ** 2))
diffuse = 0.28 + 0.95 * ndl
spec = 0.16 * ndh ** 30 * (0.6 + 0.8 * lamp) + 0.55 * ndh ** 220
face = col * diffuse[..., None] * (0.86 + 0.28 * lamp[..., None])
face += spec[..., None] * np.array([214, 230, 190], np.float32)
face += (flake * (0.35 + 0.9 * ndh ** 8) * (0.5 + lamp))[..., None] * np.array([150, 150, 70], np.float32)
# arete du bord : liseré clair cote lumiere, sombre cote ombre (fonderie)
edge = smooth(0, 2.5, d_in) * (1 - smooth(2.5, 9, d_in))
face *= (1 - 0.18 * edge[..., None] * (1 - ndl[..., None]))

img = bg * (1 - inside[..., None]) + face * inside[..., None]

# ------------------------------------------------------------------ 3. jacks du kit (tranches) : OUT a gauche, IN a droite
def paste_rgba(dst, im, x, y):
    a = np.asarray(im).astype(np.float32)
    hh, ww = a.shape[:2]
    xa, ya = int(round(x)), int(round(y))
    sub = dst[ya:ya + hh, xa:xa + ww]
    al = a[..., 3:4] / 255
    dst[ya:ya + hh, xa:xa + ww] = sub * (1 - al) + a[..., :3] * al

JK = 0.62
jo = Image.open(os.path.join(AS, 'kit', 'jack_out_gauche.png')).convert('RGBA')
ji = Image.open(os.path.join(AS, 'kit', 'jack_in_droite.png')).convert('RGBA')
jo = jo.resize((round(jo.width * JK), round(jo.height * JK)), Image.LANCZOS)
ji = ji.resize((round(ji.width * JK), round(ji.height * JK)), Image.LANCZOS)
# ombre des jacks sur le plan de travail
for im, xr, side in ((jo, U(x0) - jo.width + 3, 'L'), (ji, U(x1) - 3, 'R')):
    a = np.zeros((H2, W2), np.float32)
    al = np.asarray(im)[..., 3].astype(np.float32) / 255
    yt = U(JACK_Y) - im.height / 2
    a[int(yt) + 30:int(yt) + 30 + im.height, int(xr) + 20:int(xr) + 20 + im.width] = al
    img *= (1 - 0.55 * blur(a, 7)[..., None])
    paste_rgba(img, im, xr, yt)

# ------------------------------------------------------------------ 4. inscriptions (encre creme serigraphiee)
ink = np.array([236, 226, 190], np.float32)
def text_layer(items):
    lay = Image.new('L', (W2, H2), 0); d = ImageDraw.Draw(lay)
    for txt, (cx, cy), font, track in items:
        f = ImageFont.truetype(os.path.join(FONTS, font), int(U(track[0])))
        sp = U(track[1])
        widths = [d.textlength(ch, font=f) for ch in txt]
        tw = sum(widths) + sp * (len(txt) - 1)
        x = U(cx) - tw / 2
        asc, desc = f.getmetrics()
        for ch, w_ in zip(txt, widths):
            d.text((x, U(cy) - (asc - desc * 0.2) * 0.62), ch, font=f, fill=255)
            x += w_ + sp
    return np.asarray(lay).astype(np.float32) / 255

labels = [('LOUDNESS', (258, 248), 'BarlowCondensed-SemiBold.ttf', (17, 1.6)),
          ('SUSTAIN', (502, 248), 'BarlowCondensed-SemiBold.ttf', (17, 1.6)),
          ('FILTER', (380, 418), 'BarlowCondensed-SemiBold.ttf', (17, 1.6)),
          ('CRUNCH', (240, 418), 'BarlowCondensed-SemiBold.ttf', (17, 1.6)),
          ('SCOOP', (520, 418), 'BarlowCondensed-SemiBold.ttf', (17, 1.6)),
          ('OUT', (166, JACK_Y), 'BarlowCondensed-SemiBold.ttf', (14, 1.2)),
          ('IN', (594, JACK_Y), 'BarlowCondensed-SemiBold.ttf', (14, 1.2)),
          ('STRONG', (380, 566), 'BarlowCondensed-Bold.ttf', (24, 9.0)),
          ('PICKLE', (380, 614), 'BarlowCondensed-ExtraBold.ttf', (66, 3.0)),
          ('FUZZ', (380, 657), 'BarlowCondensed-Medium.ttf', (13, 6.0))]
ta = text_layer(labels)
# encre : legerement mate, suit un peu l'eclairage
inkc = ink[None, None, :] * (0.80 + 0.22 * ndl[..., None]) * (0.92 + 0.10 * lamp[..., None])
img = img * (1 - ta[..., None]) + inkc * ta[..., None]

# petits ticks de course autour des potards (repere 7 h -> 5 h), encre creme
tick = Image.new('L', (W2, H2), 0); dt = ImageDraw.Draw(tick)
for kname, (cx, cy) in KNOBS.items():
    KR = KNOB_RS[kname]
    for k in range(11):
        a = math.radians(-150 + 30 * k)
        r0, r1 = KR + 6, KR + (12 if k in (0, 5, 10) else 9)
        p0 = (U(cx + r0 * math.sin(a)), U(cy - r0 * math.cos(a))); p1 = (U(cx + r1 * math.sin(a)), U(cy - r1 * math.cos(a)))
        dt.line([p0, p1], fill=255, width=int(U(1.6)))
tk = np.asarray(tick).astype(np.float32) / 255 * 0.85
img = img * (1 - tk[..., None]) + inkc * tk[..., None]

# logo Studio Wiga (blanc) en bas
logo = Image.open(os.path.join(AS, 'logo_studio_wiga_blanc.png')).convert('RGBA')
LW = int(U(190)); logo = logo.resize((LW, round(logo.height * LW / logo.width)), Image.LANCZOS)
la = np.asarray(logo).astype(np.float32) / 255
lx, ly = int(U(380) - LW / 2), int(U(906) - logo.height / 2)
reg = img[ly:ly + logo.height, lx:lx + LW]
img[ly:ly + logo.height, lx:lx + LW] = reg * (1 - la[..., 3:]) + (la[..., :3] * 255 * 0.93) * la[..., 3:]

# ------------------------------------------------------------------ 5. potards (plastique noir, rendu 3D, sans repere)
SS = 3
def render_knob(R):
    n = int(math.ceil(U(R * 2 + 40) * SS / 2)) * 2
    k = PX * SS
    yk, xk = np.mgrid[0:n, 0:n].astype(np.float32)
    lx_, ly_ = (xk + 0.5) / k - n / k / 2, (yk + 0.5) / k - n / k / 2
    rr = np.hypot(lx_, ly_); th = np.arctan2(ly_, lx_)
    top = 0.80 * R
    hk = np.zeros_like(rr)
    hk = np.where(rr < top, 22.0 + 0.6 * (1 - (rr / top) ** 2), hk)                     # dessus tres legerement bombe
    ch = (rr >= top) & (rr < 0.87 * R)
    hk = np.where(ch, 22.0 - (rr - top) / (0.07 * R) * 4.0, hk)                         # chanfrein
    sk = (rr >= 0.87 * R) & (rr < R)
    knurl = 0.55 * (np.cos(th * 36) > 0.0)                                               # moletage
    hk = np.where(sk, 18.0 - (rr - 0.87 * R) / (0.13 * R) * 15.0 + knurl * (rr < 0.985 * R), hk)
    mask = (rr < R).astype(np.float32)
    mask = cv2.GaussianBlur(mask, (0, 0), 0.6)
    Nk = normals(hk, k)
    ndl_ = np.clip((Nk * L).sum(-1), 0, 1); ndh_ = np.clip((Nk * HV).sum(-1), 0, 1)
    albedo = 9.0
    # reflet conique du dessus (plastique tourne) : bande claire orientee vers la lumiere, fixe
    cone = np.where(rr < top, 0.5 + 0.5 * np.cos(2 * (th - math.atan2(-0.62, -0.46))), 0) * np.clip(rr / top, 0, 1) ** 0.7
    c = albedo * (0.4 + 1.6 * ndl_) + 120 * ndh_ ** 40 + 260 * ndh_ ** 400 + 26 * cone
    c = np.where(ch, c + 60 * ndl_ ** 3, c)                                              # arete du chanfrein qui accroche la lumiere
    rgb = np.dstack([c * 0.97, c * 1.0, c * 1.04])
    rgb = cv2.resize(rgb, (n // SS, n // SS), interpolation=cv2.INTER_AREA)
    mask = cv2.resize(mask, (n // SS, n // SS), interpolation=cv2.INTER_AREA)
    return rgb, mask

_kcache = {}
for kname, (cx, cy) in KNOBS.items():
    if KNOB_RS[kname] not in _kcache: _kcache[KNOB_RS[kname]] = render_knob(KNOB_RS[kname])
    krgb, kmask = _kcache[KNOB_RS[kname]]
    kn = krgb.shape[0]
    xa, ya = int(round(U(cx) - kn / 2)), int(round(U(cy) - kn / 2))
    # ombre portee (douce) + ombre de contact
    shadow = np.zeros((H2, W2), np.float32)
    shadow[ya + int(U(6)):ya + int(U(6)) + kn, xa + int(U(4)):xa + int(U(4)) + kn] = kmask
    s1 = blur(shadow, 5)
    shadow2 = np.zeros((H2, W2), np.float32); shadow2[ya + int(U(1.5)):ya + int(U(1.5)) + kn, xa + int(U(1)):xa + int(U(1)) + kn] = kmask
    img *= (1 - 0.55 * s1[..., None]) * (1 - 0.5 * blur(shadow2, 1.5)[..., None])
    sub = img[ya:ya + kn, xa:xa + kn]
    img[ya:ya + kn, xa:xa + kn] = sub * (1 - kmask[..., None]) + krgb * kmask[..., None]

# ------------------------------------------------------------------ 6. LED eteinte (bague chromee, lentille bleue sombre)
def render_led():
    R = LED_R * 1.55
    n = int(U(R * 2 + 12) * SS); k = PX * SS
    yk, xk = np.mgrid[0:n, 0:n].astype(np.float32)
    lx_, ly_ = (xk + 0.5) / k - n / k / 2, (yk + 0.5) / k - n / k / 2
    rr = np.hypot(lx_, ly_)
    hk = np.where(rr < LED_R, 2.5 + 2.0 * np.sqrt(np.clip(1 - (rr / LED_R) ** 2, 0, 1)), 0)
    ring = (rr >= LED_R) & (rr < R)
    hk = np.where(ring, 3.2 * np.sin(np.clip((rr - LED_R) / (R - LED_R), 0, 1) * math.pi), hk)
    Nk = normals(hk, k)
    ndl_ = np.clip((Nk * L).sum(-1), 0, 1); ndh_ = np.clip((Nk * HV).sum(-1), 0, 1)
    chrome = 40 + 150 * ndl_ ** 2 + 255 * ndh_ ** 60
    lens = np.dstack([14 + 40 * ndh_ ** 30, 20 + 46 * ndh_ ** 30, 42 + 70 * ndh_ ** 30]) + 230 * (ndh_ ** 500)[..., None]
    rgb = np.where(ring[..., None], np.dstack([chrome, chrome, chrome * 1.02]), lens)
    gorge = (rr >= LED_R - 0.9) & (rr < LED_R + 0.4)
    rgb = np.where(gorge[..., None], rgb * 0.25, rgb)
    mask = (rr < R).astype(np.float32)
    rgb = cv2.resize(rgb, (n // SS, n // SS), interpolation=cv2.INTER_AREA)
    mask = cv2.resize(mask, (n // SS, n // SS), interpolation=cv2.INTER_AREA)
    return rgb, mask
lrgb, lmask = render_led(); ln_ = lrgb.shape[0]
xa, ya = int(round(U(LED[0]) - ln_ / 2)), int(round(U(LED[1]) - ln_ / 2))
shadow = np.zeros((H2, W2), np.float32); shadow[ya + 4:ya + 4 + ln_, xa + 3:xa + 3 + ln_] = lmask
img *= (1 - 0.5 * blur(shadow, 2.0)[..., None])
img[ya:ya + ln_, xa:xa + ln_] = img[ya:ya + ln_, xa:xa + ln_] * (1 - lmask[..., None]) + lrgb * lmask[..., None]

# ------------------------------------------------------------------ 7. footswitch d'Echoflex (reference Studio Wiga)
fs = Image.open(os.path.join(HERE, 'fs_echoflex.png')).convert('RGBA')      # 200 unites (ombre comprise), 124 entre plats
fsk = FOOT_FLATS / 124.0
fs = fs.resize((round(fs.width * fsk), round(fs.height * fsk)), Image.LANCZOS)
paste_rgba(img, fs, U(FOOT[0]) - fs.width / 2, U(FOOT[1]) - fs.height / 2)
FOOT_R = 60.0 * 1.0654 * fsk          # rayon de la zone cliquable (meme rapport que Doorn)

# ------------------------------------------------------------------ 8. calque d'eclairage par la LED bleue
gx0, gy0, gw, gh = GLOW
sx, sy = int(U(gx0)), int(U(gy0)); sw, shh = int(U(gw)), int(U(gh))
regbg = img[sy:sy + shh, sx:sx + sw].copy()
gu = (np.mgrid[0:shh, 0:sw][1] + 0.5) / PX + gx0; gv = (np.mgrid[0:shh, 0:sw][0] + 0.5) / PX + gy0
d = np.hypot(gu - LED[0], gv - LED[1])
hsrc = 9.0                                                     # source a ~9 unites au-dessus de la facade
E = hsrc ** 3 / (d * d + hsrc * hsrc) ** 1.5                   # loi cos / d^2
E = 1.25 * E + 0.18 * np.exp(-(d / 70.0) ** 2)                 # tache serree + diffusion large et faible
blue = np.array([0.30, 0.62, 1.55], np.float32)
lit = regbg * (1 + E[..., None] * blue * 2.2) + (E[..., None] ** 2) * blue * 60
inl = inside[sy:sy + shh, sx:sx + sw]
lit = regbg + (lit - regbg) * inl[..., None]                   # le plan de travail n'est pas eclaire
add = np.clip(lit, 0, 255) - regbg
a = np.clip(add.max(-1) / 60.0, 0, 1)
a = np.maximum(a, np.clip(E * 0.9, 0, 1) * inl)
src = np.where(a[..., None] > 1e-3, regbg + add / np.maximum(a[..., None], 1e-3), regbg)
glow = np.dstack([np.clip(src, 0, 255), a[..., None] * 255]).astype(np.uint8)
Image.fromarray(glow, 'RGBA').resize((512, 512), Image.LANCZOS).save(os.path.join(ROOT, 'glow_blue.png'))

# ------------------------------------------------------------------ 9. inverseurs du kit -> bande de 3 images carrees
def tog(name):
    im = Image.open(os.path.join(AS, 'kit', name)).convert('RGBA')
    a = np.asarray(im).astype(np.float32)
    rows = np.arange(a.shape[0])[:, None]
    a[..., 3] *= np.clip((168 - rows) / 26.0, 0, 1)            # on garde l'inverseur et son ombre, pas les inscriptions du visuel d'origine
    if 'OFF' in name:
        a[..., 3] *= np.clip((rows - 30) / 10.0, 0, 1)          # levier en bas : rien au-dessus de la bague (reste du levier du visuel d'origine)
    im = Image.fromarray(a.astype(np.uint8), 'RGBA')
    F = 190; frame = Image.new('RGBA', (F, F), (0, 0, 0, 0))
    frame.alpha_composite(im.crop((0, 0, 186, 190)), (F // 2 - 93, F // 2 - 81))   # centre de la bague au centre de l'image
    return frame
frames = [tog('switch_levier_OFF.png'), tog('switch_levier_milieu.png'), tog('switch_levier_ON.png')]   # bas, intermediaire, haut
F = frames[0].width
TS = int(round(F * TOG_SCALE))
strip = Image.new('RGBA', (TS, TS * 3), (0, 0, 0, 0))
for i, f_ in enumerate(frames): strip.alpha_composite(f_.resize((TS, TS), Image.LANCZOS), (0, i * TS))
strip.save(os.path.join(ROOT, 'toggle_strip.png'))
TOG_HALF = TS / PX / 2

# ------------------------------------------------------------------ sorties
out = np.clip(img, 0, 255).astype(np.uint8)
Image.fromarray(out).save(os.path.join(ROOT, 'background.jpg'), quality=93, subsampling=0)

lay = dict(knobs=[list(KNOBS[k]) for k in ('loudness', 'sustain', 'filter')], knobR=[KNOB_RS[k] for k in ('loudness', 'sustain', 'filter')],
           toggles=[list(TOGGLES[k]) for k in ('crunch', 'scoop')], togHalf=TOG_HALF,
           led=list(LED), ledR=LED_R, foot=list(FOOT), footR=FOOT_R, glow=list(GLOW),
           status=list(STATUS), preset=list(PRESET), pedal=list(PEDAL))
json.dump(lay, open(os.path.join(HERE, 'layout.json'), 'w'), indent=1)
P = lambda p: '{ %.2ff, %.2ff }' % tuple(p)
R_ = lambda r: '{ %.2ff, %.2ff, %.2ff, %.2ff }' % tuple(r)
open(os.path.join(ROOT, 'Layout.h'), 'w').write(f"""// genere par tools/make_background.py - ne pas modifier a la main (repere 760 x 958)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{{
    const juce::Point<float> knobPos[3] = {{ {P(KNOBS['loudness'])}, {P(KNOBS['sustain'])}, {P(KNOBS['filter'])} }};
    const float knobR[3] = {{ {KNOB_RS['loudness']:.2f}f, {KNOB_RS['sustain']:.2f}f, {KNOB_RS['filter']:.2f}f }};
    const juce::Point<float> crunchPos {P(TOGGLES['crunch'])}, scoopPos {P(TOGGLES['scoop'])};
    const float togHalf = {TOG_HALF:.2f}f;
    const juce::Point<float> ledPos {P(LED)}; const float ledR = {LED_R:.2f}f;
    const juce::Point<float> footPos {P(FOOT)}; const float footR = {FOOT_R:.2f}f;
    const juce::Rectangle<float> glowRect {R_(GLOW)};
    const juce::Rectangle<float> statusRect {R_(STATUS)};
    const juce::Rectangle<float> presetRect {R_(PRESET)};
}}
""")
print('ok', out.shape, 'toggle', TS, 'footR', round(FOOT_R, 2))
