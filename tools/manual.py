# STRONG PICKLE (NINE HANDS | DSP) - notice PDF bilingue (anglais puis francais), illustree avec le rendu reel du plugin.
# Usage : python3 tools/manual.py [<dossier des captures (tools/render)>] <fichier PDF de sortie>
#   avec captures : recompose les images et les copie dans tools/manual_assets/ ;
#   sans captures : reprend les images composees de tools/manual_assets/ (extraites de la notice precedente).
import os, sys, json
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, CondPageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = sys.argv[1] if len(sys.argv) > 2 else None
OUT = sys.argv[-1]
VERSION = "1.5.0"
TMP = os.path.dirname(os.path.abspath(OUT))
FONTS = os.path.join(ROOT, "assets", "fonts")

for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold"),
             ("Title", "BarlowCondensed-ExtraBold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FONTS, f + ".ttf")))

ACC = colors.HexColor("#3E9A2E"); ACC_L = colors.HexColor("#8EDB6C")      # vert de la pedale / PICKLE allume
DARK = colors.HexColor("#17201A"); GREY = colors.HexColor("#5F6A62"); LIGHT = colors.HexColor("#EEF3E8")
LAY = json.load(open(os.path.join(ROOT, "tools", "layout.json")))
import subprocess
LOGO_PNG = os.path.join(os.path.dirname(os.path.abspath(OUT)), "nine_hands_logo_blanc.png")
subprocess.run(["rsvg-convert", "-w", "1200", os.path.join(ROOT, "NineHandsLogo.svg"), "-o", LOGO_PNG], check=True)

ASSETS = os.path.join(ROOT, "tools", "manual_assets")
NAMES = ["pickle_annotated.png", "pickle_switches.png", "pickle_onoff.png", "pickle_bar.png", "pickle_octave_sw.png", "pickle_cover_cutout.png"]
if SHOTS:
    def shot(name): return Image.open(os.path.join(SHOTS, name)).convert("RGB")
    src = shot("pickle_status.png")
    k = src.height / 900.0                          # repere du plugin v1.5 : 617,5 x 900 unites (fond 1235 x 1800 / 2)

    # ---------- capture annotee ----------
    kn, tg = LAY["knobs"], LAY["toggles"]
    st_, pr_ = LAY["status"], LAY["preset"]
    marks = [  # numero, x, y (unites du plugin)
        (1, kn[0][0] - 70, kn[0][1] - 62), (2, kn[1][0] + 70, kn[1][1] - 62), (3, kn[2][0] + 64, kn[2][1] - 44),
        (4, tg[0][0] - 42, tg[0][1] - 20), (5, tg[1][0] + 42, tg[1][1] - 20),
        (6, st_[0] - 4, st_[1] + st_[3] * 0.5), (7, pr_[0] - 4, pr_[1] + pr_[3] * 0.5), (8, LAY["pickle"][0] + 34, LAY["pickle"][1] + 46),
        (9, LAY["led"][0] - 30, LAY["led"][1]), (10, LAY["fs"][0] + 70, LAY["fs"][1]),
        (11, 40, LAY["jackY"] - 50), (12, 577, LAY["jackY"] - 50), (13, LAY["octave"][0] + 34, LAY["octave"][1] + 34), (14, LAY["octLed"][0], LAY["octLed"][1] - 26)]
    ann = src.copy(); dr = ImageDraw.Draw(ann)
    fnt = ImageFont.truetype(os.path.join(FONTS, "Poppins-Bold.ttf"), 34)
    for n, x, y in marks:
        cx, cy, r = x * k, y * k, 27
        dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(62, 154, 46), width=5)
        dr.text((cx, cy + 1), str(n), font=fnt, fill=(20, 32, 22), anchor="mm")
    annotated = os.path.join(TMP, "pickle_annotated.png"); ann.save(annotated)

    def crop_u(im, box): return im.crop(tuple(int(v * k) for v in box))
    gap = 18
    def strip(parts, path):
        s = Image.new("RGB", (sum(p.width for p in parts) + gap * (len(parts) - 1), max(p.height for p in parts)), (255, 255, 255))
        x = 0
        for p in parts: s.paste(p, (x, 0)); x += p.width + gap
        s.save(path); return path

    # inverseurs : les deux en bas (classique), CRUNCH haut, SCOOP haut
    box_sw = (95, 275, 525, 412)
    sw_img = strip([crop_u(shot(n), box_sw) for n in ("pickle_status.png", "pickle_crunch.png", "pickle_scoop.png")], os.path.join(TMP, "pickle_switches.png"))
    # effet actif / bypass : PICKLE vert + LED jaunes
    box_on = (165, 525, 540, 690)
    on_img = strip([crop_u(shot("pickle_bypass.png"), box_on), crop_u(src, box_on)], os.path.join(TMP, "pickle_onoff.png"))
    # ecran + barre de presets
    bar_img = os.path.join(TMP, "pickle_bar.png"); crop_u(src, (st_[0] - 8, st_[1] - 8, st_[0] + st_[2] + 8, pr_[1] + pr_[3] + 8)).save(bar_img)
    # inverseur OCTAVE : milieu (off), haut (Fat), bas (Bright)
    ox, oy = LAY["octave"]
    box_oc = (ox - 72, oy - 54, ox + 108, oy + 54)
    oc_img = strip([crop_u(src, box_oc), crop_u(shot("pickle_octave.png"), box_oc), crop_u(shot("pickle_octave_bright.png"), box_oc)], os.path.join(TMP, "pickle_octave_sw.png"))

    # ---------- couverture : pedale detouree avec le canal alpha du fond (boitier + jacks), reflet au sol, halo vert tres doux ----------
    bg_a = Image.open(os.path.join(ROOT, "background.png")).split()[3].resize(src.size, Image.LANCZOS)
    msk = np.asarray(bg_a)
    cut = Image.new("RGBA", src.size, (0, 0, 0, 0)); cut.paste(src, (0, 0), Image.fromarray(msk))
    cut = cut.crop(cut.getbbox())
    cw, ch = cut.size
    pad = int(cw * 0.16)
    canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
    yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
    gcx, gcy = canvas.width / 2, pad + ch * 0.45
    glow = np.exp(-(((xx - gcx) / (cw * 0.62)) ** 2 + ((yy - gcy) / (ch * 0.48)) ** 2)) * 30
    edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
    glow *= edge ** 2
    halo = np.dstack([glow * 0.35, glow * 0.95, glow * 0.30, np.full_like(glow, 255)]).astype(np.uint8)
    canvas = Image.alpha_composite(canvas, Image.fromarray(halo, "RGBA"))
    refl = cut.transpose(Image.FLIP_TOP_BOTTOM)
    ra = np.array(refl).astype(np.float64)
    fade = np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22
    ra[..., 3] *= fade[:, None]
    canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
    canvas.alpha_composite(cut, (pad, pad))
    canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
    cover = os.path.join(TMP, "pickle_cover_cutout.png"); canvas.convert("RGB").save(cover)
    import shutil
    os.makedirs(ASSETS, exist_ok=True)
    for n in NAMES: shutil.copy(os.path.join(TMP, n), os.path.join(ASSETS, n))
else:
    annotated, sw_img, on_img, bar_img, oc_img, cover = [os.path.join(ASSETS, n) for n in NAMES]

def img(path, width):
    im = Image.open(path); h = width * im.height / im.width
    return RLImage(path, width=width, height=h)

# ---------- styles ----------
st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=24, textColor=ACC, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=13, leading=17, textColor=DARK, spaceAfter=4, spaceBefore=10, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=5),
    "tip": ParagraphStyle("tip", fontName="Pop", fontSize=9.0, leading=12.4, textColor=DARK, spaceAfter=3),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
    "cells": ParagraphStyle("cells", fontName="Pop", fontSize=7.6, leading=9.6, textColor=DARK),
    "cellbs": ParagraphStyle("cellbs", fontName="PopM", fontSize=7.6, leading=9.6, textColor=DARK),
    "cover": ParagraphStyle("cover", fontName="Title", fontSize=50, leading=56, textColor=colors.white, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="Pop", fontSize=12, leading=16, textColor=colors.HexColor("#BDC4B8"), alignment=TA_CENTER),
    "coverlang": ParagraphStyle("coverlang", fontName="PopM", fontSize=8.5, leading=11, textColor=ACC_L),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items, s="p"): return [P("&bull;&nbsp;&nbsp;" + i, s) for i in items]
def table(rows, widths, header=True, small=False):
    data = [[P(c, ("cellbs" if small else "cellb") if (header and r == 0) else ("cells" if small else "cell")) for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#D5DFCF")),
             ("TOPPADDING", (0, 0), (-1, -1), 2 if small else 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 2 if small else 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]
    t.setStyle(TableStyle(style)); return t

# ---------- donnees (identiques au code livre : PluginProcessor.cpp) ----------
FACTORY = [  # nom, loudness, sustain, filter, crunch, scoop, octave
    ("Classic Pickle",       5.0, 5.0, 5.0, 0, 0, 0), ("Siamese Dream",        5.0, 7.0, 5.5, 0, 0, 0),
    ("Smooth Low Fuzz",      6.0, 2.5, 5.0, 0, 0, 0), ("Sputter Zone",         5.5, 5.0, 5.5, 0, 0, 0),
    ("Open Crunch",          4.5, 6.0, 6.0, 1, 0, 0), ("Scooped Power Chords", 5.0, 7.0, 5.0, 0, 1, 0),
    ("Dark Wool",            6.0, 6.0, 2.0, 0, 0, 0), ("Bright Lead",          5.0, 8.5, 7.5, 1, 0, 0),
    ("Amp Pusher",           8.0, 1.5, 6.5, 1, 0, 0), ("Bass Pickle",          5.0, 4.0, 3.5, 0, 0, 0),
    ("Doom Sludge",          6.0, 10.0, 3.0, 0, 1, 0), ("7 Words",              5.0, 6.0, 6.0, 1, 0, 0),
    ("Octave Lead",          5.0, 7.5, 6.5, 0, 0, 1)]
FACT_DESC = {
 "en": ["Both switches down, all knobs at noon: the classic three-knob Swollen Pickle.",
        "The guitars of Smashing Pumpkins' <i>Siamese Dream</i>: Billy Corgan recorded almost all its fuzz through a late-'70s op-amp Big Muff, more compressed than the transistor versions. Sustain at 2 o'clock, tone just past noon, mids barely scooped, compressed fuzz, volume matched to your clean sound. Double the rhythm part and pan it hard left / right.",
        "Sustain around 9 o'clock: the smooth, round fuzz that sits below the sputter zone.",
        "Sustain at noon, where the original pedal sputters the most: notes break up as they die out.",
        "Crunch switch up: a more open, grainier and more dynamic fuzz, a little louder.",
        "Scoop switch up and plenty of sustain: deep mid cut for huge power chords.",
        "Filter turned down: thick, woolly, dark fuzz with a big low end.",
        "High sustain, bright filter and Crunch up: a lead that cuts through the mix.",
        "Little fuzz and a lot of volume, Crunch up: pushes an amp that is already overdriven, as metal producers do with the Swollen Pickle.",
        "Moderate sustain and a darker filter keep the low end of a bass (or a low-tuned guitar) round and full.",
        "Sustain full, scooped and dark: an endless wall for slow, heavy riffs.",
        "The intro riff of Deftones' <i>7 Words</i> (<i>Adrenaline</i>, 1995): choppy, chunky and aggressive, but crisp and precise, as reviews of the record describe it. Sustain just past noon, Filter slightly bright, Crunch up for a more open, articulate attack, mids kept (Scoop down). Play it palm-muted and tight, into a high-gain amp.",
        "Octave switch up (Fat), in the spirit of the Fulltone Ultimate Octave: high sustain and a slightly bright filter. Play single notes on the neck pickup, above the 12th fret, for the ring-modulator-like octave-up; chords turn into a gnarly, glitchy fuzz."],
 "fr": ["Les deux inverseurs en bas, tous les potards à midi : la Swollen Pickle classique à trois potards.",
        "Les guitares de <i>Siamese Dream</i> des Smashing Pumpkins : Billy Corgan a enregistré presque toutes les fuzz de l'album avec un Big Muff op-amp de la fin des années 70, plus compressé que les versions à transistors. Sustain vers 14 h, tone à peine au-dessus de midi, médiums juste un peu creusés, fuzz compressée, volume au niveau du son clair. Doublez la rythmique et panotez-la gauche / droite.",
        "Sustain vers 9 h : la fuzz lisse et ronde, juste avant la zone qui crachote.",
        "Sustain à midi, là où la pédale d'origine crachote le plus : les notes se cassent en s'éteignant.",
        "Inverseur Crunch en haut : une fuzz plus ouverte, plus granuleuse et plus dynamique, un peu plus forte.",
        "Inverseur Scoop en haut et beaucoup de sustain : médiums bien creusés pour des power chords énormes.",
        "Filter baissé : une fuzz épaisse, laineuse et sombre, avec un grave énorme.",
        "Beaucoup de sustain, filter brillant et Crunch en haut : un solo qui passe devant le mix.",
        "Peu de fuzz et beaucoup de volume, Crunch en haut : pour pousser un ampli déjà saturé, comme le font des producteurs metal avec la Swollen Pickle.",
        "Sustain modéré et filter plus sombre : le grave d'une basse (ou d'une guitare accordée bas) reste rond et plein.",
        "Sustain à fond, médiums creusés et son sombre : un mur sans fin pour les riffs lents et lourds.",
        "Le riff d'intro de <i>7 Words</i> de Deftones (<i>Adrenaline</i>, 1995) : haché, épais et agressif, mais net et précis, comme le décrivent les critiques de l'album. Sustain juste après midi, Filter un peu brillant, Crunch en haut pour une attaque plus ouverte et articulée, médiums gardés (Scoop en bas). Jouez-le en palm mute, bien serré, dans un ampli à fort gain.",
        "Inverseur Octave en haut (Fat), dans l'esprit de la Fulltone Ultimate Octave : beaucoup de sustain et un filter un peu brillant. Jouez des notes seules au micro manche, au-delà de la 12e case, pour l'octave au-dessus aux accents de modulateur en anneau ; les accords deviennent une fuzz rugueuse et chaotique."]}
def pc(v): return f"{round(v * 10)} %"
SW = {"en": ("down", "up"), "fr": ("bas", "haut")}
OC = {"en": ("off", "Fat", "Bright"), "fr": ("off", "Fat", "Bright")}

T = {
"en": dict(
 lang="ENGLISH", sub="Jumbo fuzz &nbsp;·&nbsp; VST3 for Windows and macOS", ver="User manual &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Introduction",
 intro=["<b>STRONG PICKLE</b> is a NINE HANDS | DSP plugin modelled on the <b>Way Huge Swollen Pickle Jumbo Fuzz MkIII</b> (WM41, \"Smalls\" series), a Big Muff-family fuzz known for its huge low end, its enormous volume reserve and its sputtery character halfway up the Sustain knob.",
        "Since version 1.1 the circuit is modelled <b>component by component</b> and solved at every sample (Newton's method on the node voltages, 4x oversampling): a 2N3904 input stage, the Sustain divider, two 2N3904 clipping stages (470 k feedback, 330 pF Miller capacitor, 1N914 diodes in series with a 1 µF capacitor from collector to base, 150 nF / 100 nF coupling into 10 k: the low end is trimmed before the clipping, so the fuzz stays defined instead of smearing), the passive Filter tone stack, the output stage and Loudness. Nothing is added on top: the gain, the square-wave grain, the asymmetry, the way notes decay and sputter all come from the transistors and capacitors themselves. The coupling and tone values give the low end of the \"jumbo\" Pickle voice.",
        "Strong Pickle adds an <b>OCTAVE</b> switch that the original pedal does not have, modelled on the octave-up circuit of the <b>Fulltone Ultimate Octave</b> (a descendant of the Foxx Tone Machine): a phase splitter drives two Schottky diodes that full-wave rectify the signal before the fuzz, with its Fat and Bright voicings."],
 inst_t="2. Installation",
 inst=[("Windows", "Download the <b>StrongPickle-VST3-Windows</b> artifact from the Actions tab of the GitHub repository Nine-Hands-Dsp/strong-pickle-, unzip it and copy the <b>Strong Pickle.vst3</b> folder into <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Download <b>StrongPickle-VST3-macOS</b>, unzip it and copy <b>Strong Pickle.vst3</b> into <b>~/Library/Audio/Plug-Ins/VST3</b>. If macOS blocks it, run <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Strong\\ Pickle.vst3</b> in the Terminal. The binary is universal (Apple Silicon and Intel)."),
       ("Reaper", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. The plugin appears as <b>Strong Pickle (NINE HANDS | DSP)</b>. To control it from MIDI footswitches and expression pedals, the track must receive MIDI from the controller (record-arm or input monitoring, MIDI input set to your device, e.g. the M-Vave Chocolate Plus).")],
 ov_t="3. Interface overview",
 ov_rows=[["#", "Control", "Role"],
   ["1", "LOUDNESS", "Output volume. Drag up / down, mouse wheel, double-click = noon, right-click = MIDI Learn (true for every control)."],
   ["2", "SUSTAIN", "Amount of fuzz (18 to 62 dB of gain)."],
   ["3", "FILTER", "Tone: dark and fat to the left, bright and thin to the right."],
   ["4", "CRUNCH switch", "Down = classic (smooth, compressed), up = open, grainier and louder."],
   ["5", "SCOOP switch", "Down = classic voice, up = deep mid cut."],
   ["6", "Screen", "Setting of the control you just moved; appears when something moves."],
   ["7", "Preset bar", "Arrows = previous / next factory preset, click on the name = preset menu."],
   ["8", "PICKLE", "Lights up green when the effect is on."],
   ["9", "LED", "Yellow = effect on, off = bypass."],
   ["10", "Footswitch", "Effect on / bypass."],
   ["11", "OUT", "Output jack."],
   ["12", "IN", "Input jack."],
   ["13", "OCTAVE switch", "3 positions, as on the Ultimate Octave: up = Fat, middle = off, down = Bright."],
   ["14", "Octave LED", "Yellow = octave in use (Fat or Bright) with the effect on."]],
 ctl_t="4. Controls in detail",
 ctl=[("SUSTAIN", "Sets the gain of the circuit, from <b>18 dB</b> fully down to <b>62 dB</b> fully up (the figures of the Way Huge spec sheet). It is a divider between the input stage and the first clipping stage. The gain rises quickly at the start of the travel: around 8 to 10 o'clock the fuzz is round and still dynamic; around noon the two clipping stages are fully driven (square-wave fuzz); further up it becomes a dense wall of sustain. The level follows your picking and the notes decay naturally, without pumping."),
      ("FILTER", "The passive tone stack of the Big Muff family, with its real values: a low-pass branch (39 k / 10 nF), a high-pass branch (4 nF / 22 k) and the 100 k Filter pot between them. Fully down, the low end swells and the highs roll off; fully up, the low end thins out and the grain comes forward; around noon the two branches meet and leave the typical mid dip."),
      ("LOUDNESS", "Audio taper: 0 = muted, noon about 4 to 5 dB above the bypassed signal with Sustain and Filter at noon, and up to +12 dB more at the top: the huge volume reserve of the pedal. A safety limiter keeps the output under 0 dBFS."),
      ("CRUNCH switch", "Changes the compression of the clipping stages. <b>Down</b> (classic): the diodes sit directly in the feedback loop and clamp firmly: dense, sustaining fuzz. <b>Up</b>: an 8.2 k resistor is put in series with the diodes, the stages swing much further before the diodes take over: a more open, grainier and more dynamic fuzz, with sharper peaks and about 2 to 3 dB more level."),
      ("SCOOP switch", "<b>Down</b>: the classic voice, a mid dip of about 5 dB around 800 Hz with Filter at noon. <b>Up</b>: the tone stack uses wider-spaced values (22 nF and 2.2 nF), for a deep mid cut of about 12 dB, for huge power chords. The original documents disagree on the direction of the Scoop switch; the plugin follows the Way Huge manual (Scoop cuts the mids)."),
      ("Both switches down", "This is the classic three-knob Swollen Pickle (preset 1, Classic Pickle)."),
      ("OCTAVE switch", "A 3-position switch with the two voicings of the Fulltone Ultimate Octave (its Fat / Bright toggle). <b>Middle</b>: no octave, the Swollen Pickle alone. <b>Up, FAT</b> and <b>down, BRIGHT</b>: the octave-up. Between the input stage and Sustain, a phase splitter (collector and emitter outputs, slightly unbalanced as on a real circuit) drives two Schottky diodes into a common load: both halves of the wave are folded upwards, so the pitch doubles, one octave up, then the fuzz clips it. The coupling capacitor after the rectifier makes the voicing: <b>Fat</b> = 15 nF, the full octave with its low end and a little of the original note; <b>Bright</b> = 1 nF, a thinner, more cutting octave where the original note almost disappears. The rectifier runs at 4x oversampling. It sounds best on single notes, neck pickup, above the 12th fret; low notes and chords give a rough, ring-modulated fuzz, and the diode threshold makes the end of the notes sputter, as on the hardware. The level is matched to the fuzz without octave. Click the upper half of the switch to move it up one step, the lower half to move it down.")],
 sw_cap="Switches: both down (classic), Crunch up, Scoop up.",
 oc_cap="Octave switch: middle (off), up (Fat), down (Bright).",
 fs_t="5. Footswitch, LED and noise gate",
 fs=["<b>Footswitch</b>: it switches as soon as you press it and the cap darkens while held. In bypass, the dry signal is passed through, time-aligned with the oversampling latency reported to the DAW.",
     "<b>LED and PICKLE</b>: when the effect is on, the LED lights up yellow (the panel around it catches the light) and the word <b>PICKLE</b> lights up green; both fade in and out together. The second yellow LED, left of the OCTAVE switch, lights up (with the same glow on the panel) when the octave is in use, Fat or Bright, and the effect is on.",
     "<b>Noise gate</b> (right-click &gt; Noise gate): <b>Off</b> by default, like the original pedal, or Light, Medium, Strong for noisy pickups. It opens instantly on the next note and lets notes fade smoothly. Acts on the effect only."],
 on_cap="Bypass, then effect on: PICKLE lit in green, yellow LED.",
 midi_t="6. MIDI",
 midi_intro=["Every control can be driven from a MIDI controller (footswitches, expression pedals, knobs). Strong Pickle listens on all MIDI channels. <b>No CC is assigned out of the box</b>: you choose each one with <b>MIDI Learn</b>, so the plugin never reacts to a controller you have not assigned yourself."],
 midi_hdr=["Control", "Assignment", "Type"],
 midi=[["Loudness", "MIDI Learn", "Continuous (0-127 = full travel)"], ["Sustain", "MIDI Learn", "Continuous"], ["Filter", "MIDI Learn", "Continuous"],
       ["Crunch", "MIDI Learn", "Switch: 64 or more = up"], ["Scoop", "MIDI Learn", "Switch: 64 or more = up"],
       ["Footswitch", "MIDI Learn", "Latching (127 = on / 0 = off) or momentary (toggle on each press)"],
       ["Octave", "MIDI Learn", "3 zones: 0-42 = off, 43-84 = Fat, 85-127 = Bright"]],
 learn=["<b>MIDI Learn</b>: right-click a control, choose <b>MIDI Learn: &lt;name&gt; (CC n)</b> (<b>—</b> when nothing is assigned), then move your expression pedal or press the MIDI button. The screen shows <b>MIDI LEARN: move a control for Sustain</b>, then confirms for 2 seconds, e.g. <b>Sustain = CC 64</b>. A CC already used by another control is moved to the new one. <b>Clear MIDI</b> removes an assignment; right-click &gt; MIDI &gt; <b>Clear all MIDI assignments</b> removes them all. The assignments are saved with the project. Older projects that still had the former factory map (CC 20 to 26, never changed) open with no assignment; a project in which you had changed at least one assignment keeps its assignments.",
        "<b>Expression pedals</b>: movements are never stepped. Strong Pickle ramps to each new value over the time between two messages; the real travel of your pedal is learnt automatically (a pedal sending only 2 to 116 still reaches exactly 0 and 100 %). For a knob on CC 0 to 31, CC n+32 is read as its fine 14-bit part.",
        "<b>MIDI footswitch</b>: latching (default) or momentary, right-click &gt; Settings. <b>Program Change 1 to 13</b> recall the factory presets. All parameters can also be automated in the DAW."],
 win_t="7. Window",
 win=["Strong Pickle uses the single pedal format of Echoflex and Doorn: it can never be made smaller than 60 %. Right-click &gt; <b>Plugin size</b>: <b>60 %</b>, <b>75 %</b> or <b>100 %</b> for this window, \"(default)\" marking the default size; <b>Default size (new windows)</b> chooses the size at which every new Strong Pickle window opens (one setting shared by all instances, saved on the computer) and resizes the current one at once. The size is not saved with the project: each time it is opened, the window comes back at the default size (60 % until you choose another one). You can also drag the bottom-right corner up to 150 % (proportions are kept).",
      "The <b>screen</b> shows the setting of the knob or switch you just moved (knobs in %, e.g. <i>sustain 64 %</i>, <i>crunch up (open)</i>); it appears when a setting moves and fades out after 2.5 s. It also shows the MIDI Learn messages. The <b>preset bar</b> under it always shows the current preset (or <i>Edited</i>) and the bypass state."],
 bar_cap="Screen and preset bar.",
 pre_t="8. Presets",
 pre=["Use the arrows of the preset bar, click its name, right-click anywhere, or use the preset menu of your DAW. Right-click &gt; <b>Save user preset</b> stores up to 9 presets of your own."],
 fac_hdr=["PC", "Preset", "Loud. / Sust. / Filter (%)", "Crunch · Scoop · Octave", "Description"],
 tips_t="9. Tips",
 tips=["In Reaper place Strong Pickle <b>before</b> your amp simulator (for example before Ampire, followed by Impulse for the cab), with a clean or slightly crunchy amp, as you would with the real pedal.",
       "For a 7-string in G or lower tunings, keep Sustain under noon and Filter slightly down: the low strings stay defined instead of sputtering. Above noon, palm mutes turn into a wall.",
       "Turn Loudness down before raising Sustain: the volume reserve is huge.",
       "Like many fuzzes, it reacts to your guitar volume knob: roll it back to clean up the fuzz."],
 tech_t="10. Specifications",
 tech=[["Format", "VST3 (Windows 64-bit, macOS universal)"], ["Channels", "Mono or stereo"],
       ["Latency", "32 samples (oversampling filter), reported to the DAW"], ["Gain", "18 to 62 dB (Sustain)"],
       ["Filter", "passive tone stack 39 k / 10 nF and 4 nF / 22 k, 100 k pot; Scoop: deeper mid cut (about −12 dB)"],
       ["Loudness", "muted to +12 dB; noon ≈ +4.5 dB over bypass"],
       ["Processing", "component-level circuit model (2N3904, 1N914; Octave: phase splitter and Schottky rectifier), 4x oversampling, smoothed parameters"], ["CPU", "about 12 to 25 % of one core at 48 kHz (stereo), depending on the computer"],
       ["MIDI", "all channels; MIDI Learn on the 7 controls (nothing assigned by default), 14-bit CC, Program Change 1-13"],
       ["Window", "Plugin size 60 % / 75 % / 100 %, default size of new windows to choose (60 % at first), up to 150 %, never smaller than 60 %; anti-freeze v2 under Windows"]]),
"fr": dict(
 lang="FRANÇAIS", sub="Fuzz jumbo &nbsp;·&nbsp; VST3 pour Windows et macOS", ver="Notice d'utilisation &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Présentation",
 intro=["<b>STRONG PICKLE</b> est un plugin NINE HANDS | DSP inspiré de la <b>Way Huge Swollen Pickle Jumbo Fuzz MkIII</b> (WM41, série « Smalls »), une fuzz de la famille Big Muff connue pour son grave énorme, sa réserve de volume considérable et ses crachotements à mi-course du potard Sustain.",
        "Depuis la version 1.1, le circuit est reproduit <b>composant par composant</b> et résolu à chaque échantillon (méthode de Newton sur les tensions des nœuds, sur-échantillonnage x4) : étage d'entrée à 2N3904, diviseur Sustain, deux étages d'écrêtage à 2N3904 (contre-réaction 470 k, condensateur Miller 330 pF, diodes 1N914 en série avec un condensateur de 1 µF entre collecteur et base, liaisons 150 nF / 100 nF sur 10 k : le grave est réduit avant l'écrêtage, la fuzz reste définie au lieu de baver), tone stack passif Filter, étage de sortie et Loudness. Rien n'est ajouté par-dessus : le gain, le grain carré, la dissymétrie, la façon dont les notes s'éteignent et crachotent viennent des transistors et des condensateurs eux-mêmes. Les valeurs de liaison et de tonalité donnent le grave de la voix « jumbo » de la Pickle.",
        "Strong Pickle ajoute un inverseur <b>OCTAVE</b> que la pédale d'origine n'a pas, reproduit d'après le circuit d'octave de la <b>Fulltone Ultimate Octave</b> (descendante de la Foxx Tone Machine) : un déphaseur attaque deux diodes Schottky qui redressent le signal en double alternance, avant la fuzz, avec ses deux voix Fat et Bright."],
 inst_t="2. Installation",
 inst=[("Windows", "Téléchargez l'artefact <b>StrongPickle-VST3-Windows</b> dans l'onglet Actions du dépôt GitHub Nine-Hands-Dsp/strong-pickle-, décompressez-le et copiez le dossier <b>Strong Pickle.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Téléchargez <b>StrongPickle-VST3-macOS</b>, décompressez-le et copiez <b>Strong Pickle.vst3</b> dans <b>~/Bibliothèque/Audio/Plug-Ins/VST3</b>. Si macOS le bloque, tapez <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Strong\\ Pickle.vst3</b> dans le Terminal. Le binaire est universel (Apple Silicon et Intel)."),
       ("Reaper", "Options &gt; Préférences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Le plugin apparaît sous le nom <b>Strong Pickle (NINE HANDS | DSP)</b>. Pour le piloter avec des footswitchs et pédales d'expression MIDI, la piste doit recevoir le MIDI du contrôleur (armement ou monitoring d'entrée, entrée MIDI réglée sur votre appareil, par exemple la M-Vave Chocolate Plus).")],
 ov_t="3. Vue d'ensemble de l'interface",
 ov_rows=[["#", "Commande", "Rôle"],
   ["1", "LOUDNESS", "Volume de sortie. Glisser vers le haut / le bas, molette, double-clic = midi, clic droit = MIDI Learn (valable pour toutes les commandes)."],
   ["2", "SUSTAIN", "Quantité de fuzz (18 à 62 dB de gain)."],
   ["3", "FILTER", "Tonalité : sombre et gras à gauche, brillant et fin à droite."],
   ["4", "Inverseur CRUNCH", "En bas = classique (lisse, compressé), en haut = ouvert, plus granuleux et plus fort."],
   ["5", "Inverseur SCOOP", "En bas = voix classique, en haut = médiums bien creusés."],
   ["6", "Écran", "Réglage de la commande que vous venez de bouger ; apparaît quand un réglage bouge."],
   ["7", "Barre de presets", "Flèches = preset usine précédent / suivant, clic sur le nom = menu des presets."],
   ["8", "PICKLE", "S'allume en vert quand l'effet est actif."],
   ["9", "LED", "Jaune = effet actif, éteinte = bypass."],
   ["10", "Footswitch", "Effet actif / bypass."],
   ["11", "OUT", "Jack de sortie."],
   ["12", "IN", "Jack d'entrée."],
   ["13", "Inverseur OCTAVE", "3 positions, comme sur l'Ultimate Octave : haut = Fat, milieu = sans octave, bas = Bright."],
   ["14", "LED d'octave", "Jaune = octave en service (Fat ou Bright), effet actif."]],
 ctl_t="4. Les commandes en détail",
 ctl=[("SUSTAIN", "Règle le gain du circuit, de <b>18 dB</b> au minimum à <b>62 dB</b> au maximum (les chiffres de la fiche Way Huge). C'est un diviseur entre l'étage d'entrée et le premier étage d'écrêtage. Le gain monte vite en début de course : vers 8 – 10 h, la fuzz est ronde et encore dynamique ; vers midi, les deux étages d'écrêtage sont pleinement attaqués (fuzz carrée) ; au-delà, c'est un mur de sustain dense. Le niveau suit votre jeu et les notes s'éteignent naturellement, sans pompage."),
      ("FILTER", "Le tone stack passif de la famille Big Muff, avec ses vraies valeurs : une branche passe-bas (39 k / 10 nF), une branche passe-haut (4 nF / 22 k) et le potentiomètre Filter de 100 k entre les deux. Tout à gauche, le grave gonfle et les aigus tombent ; tout à droite, le grave s'amincit et le grain ressort ; vers midi, les deux branches se rejoignent et laissent le creux de médiums typique."),
      ("LOUDNESS", "Courbe audio : 0 = coupé, midi environ 4 à 5 dB au-dessus du signal en bypass avec Sustain et Filter à midi, et jusqu'à +12 dB de plus en haut de course : l'énorme réserve de volume de la pédale. Un limiteur de sécurité garde la sortie sous 0 dBFS."),
      ("Inverseur CRUNCH", "Change la compression des étages d'écrêtage. <b>En bas</b> (classique) : les diodes sont directement dans la boucle de contre-réaction et limitent franchement : fuzz dense, beaucoup de sustain. <b>En haut</b> : une résistance de 8,2 k est mise en série avec les diodes, les étages montent beaucoup plus haut avant qu'elles n'agissent : une fuzz plus ouverte, plus granuleuse et plus dynamique, avec des crêtes plus franches et environ 2 à 3 dB de niveau en plus."),
      ("Inverseur SCOOP", "<b>En bas</b> : la voix classique, un creux d'environ 5 dB vers 800 Hz avec le Filter à midi. <b>En haut</b> : le tone stack prend des valeurs plus écartées (22 nF et 2,2 nF), pour un creux de médiums profond d'environ 12 dB, idéal pour des power chords énormes. Les documents d'origine se contredisent sur le sens de l'inverseur Scoop ; le plugin suit le manuel Way Huge (le Scoop creuse les médiums)."),
      ("Les deux inverseurs en bas", "C'est la Swollen Pickle classique à trois potards (preset 1, Classic Pickle)."),
      ("Inverseur OCTAVE", "Un inverseur à 3 positions avec les deux voix de la Fulltone Ultimate Octave (son inverseur Fat / Bright). <b>Milieu</b> : sans octave, la Swollen Pickle seule. <b>En haut, FAT</b> et <b>en bas, BRIGHT</b> : l'octave au-dessus. Entre l'étage d'entrée et le Sustain, un déphaseur (sorties collecteur et émetteur, légèrement déséquilibrées comme sur un vrai circuit) attaque deux diodes Schottky sur une charge commune : les deux alternances sont repliées vers le haut, la fréquence double (une octave au-dessus), puis la fuzz l'écrête. Le condensateur de liaison après le redresseur fait la voix : <b>Fat</b> = 15 nF, l'octave pleine avec son grave et un peu de la note d'origine ; <b>Bright</b> = 1 nF, une octave plus fine et plus perçante où la note d'origine disparaît presque. Le redresseur est calculé en sur-échantillonnage x4. Le résultat est le meilleur sur des notes seules, micro manche, au-delà de la 12e case ; les notes graves et les accords donnent une fuzz rugueuse aux accents de modulateur en anneau, et le seuil des diodes fait crachoter la fin des notes, comme sur la pédale. Le niveau est calé sur celui de la fuzz sans octave. Un clic dans la moitié haute de l'inverseur le monte d'un cran, dans la moitié basse le descend d'un cran.")],
 sw_cap="Inverseurs : les deux en bas (classique), Crunch en haut, Scoop en haut.",
 oc_cap="Inverseur Octave : milieu (sans octave), haut (Fat), bas (Bright).",
 fs_t="5. Footswitch, LED et noise gate",
 fs=["<b>Footswitch</b> : il bascule dès l'appui et le bouton s'assombrit tant qu'on appuie. En bypass, le signal sec passe, aligné dans le temps sur la latence du sur-échantillonnage annoncée au DAW.",
     "<b>LED et PICKLE</b> : quand l'effet est actif, la LED s'allume en jaune (la façade autour reçoit la lumière) et le mot <b>PICKLE</b> s'allume en vert ; les deux s'allument et s'éteignent ensemble, en fondu. La seconde LED jaune, à gauche de l'inverseur OCTAVE, s'allume (avec la même lumière sur la façade) quand l'octave est en service, Fat ou Bright, et que l'effet est actif.",
     "<b>Noise gate</b> (clic droit &gt; Noise gate) : <b>Off</b> par défaut, comme la pédale d'origine, ou Light, Medium, Strong pour des micros bruyants. Il s'ouvre instantanément à la note suivante et laisse les notes s'éteindre en douceur. N'agit que sur l'effet."],
 on_cap="Bypass, puis effet actif : PICKLE allumé en vert, LED jaune.",
 midi_t="6. MIDI",
 midi_intro=["Toutes les commandes se pilotent depuis un contrôleur MIDI (footswitchs, pédales d'expression, potards). Strong Pickle écoute tous les canaux MIDI. <b>Aucun CC n'est assigné d'office</b> : vous choisissez chacun avec le <b>MIDI Learn</b>, le plugin ne réagit donc jamais à un contrôleur que vous n'avez pas assigné vous-même."],
 midi_hdr=["Commande", "Assignation", "Type"],
 midi=[["Loudness", "MIDI Learn", "Continu (0-127 = toute la course)"], ["Sustain", "MIDI Learn", "Continu"], ["Filter", "MIDI Learn", "Continu"],
       ["Crunch", "MIDI Learn", "Bascule : 64 ou plus = en haut"], ["Scoop", "MIDI Learn", "Bascule : 64 ou plus = en haut"],
       ["Footswitch", "MIDI Learn", "À accrochage (127 = on / 0 = off) ou momentané (bascule à chaque appui)"],
       ["Octave", "MIDI Learn", "3 zones : 0-42 = off, 43-84 = Fat, 85-127 = Bright"]],
 learn=["<b>MIDI Learn</b> : clic droit sur une commande, choisissez <b>MIDI Learn: &lt;nom&gt; (CC n)</b> (<b>—</b> si rien n'est assigné), puis bougez la pédale d'expression ou appuyez sur le bouton MIDI. L'écran affiche <b>MIDI LEARN: move a control for Sustain</b>, puis confirme pendant 2 secondes, par ex. <b>Sustain = CC 64</b>. Un CC déjà utilisé par une autre commande passe à la nouvelle. <b>Clear MIDI</b> retire une assignation ; clic droit &gt; MIDI &gt; <b>Clear all MIDI assignments</b> les retire toutes. Les assignations sont sauvegardées avec le projet. Les anciens projets restés sur l'ancienne table d'usine (CC 20 à 26, jamais modifiée) s'ouvrent sans assignation ; un projet où vous aviez changé au moins une assignation garde ses assignations.",
        "<b>Pédales d'expression</b> : les mouvements ne sont jamais saccadés. Strong Pickle rejoint chaque nouvelle valeur par une rampe sur le temps mesuré entre deux messages ; la course réelle de votre pédale est apprise automatiquement (une pédale qui n'envoie que 2 à 116 atteint quand même exactement 0 et 100 %). Pour un potard sur un CC de 0 à 31, le CC n+32 est lu comme sa partie fine (14 bits).",
        "<b>Footswitch MIDI</b> : à accrochage (par défaut) ou momentané, clic droit &gt; Settings. <b>Program Change 1 à 13</b> : rappel des presets usine. Tous les paramètres sont aussi automatisables dans le DAW."],
 win_t="7. Fenêtre",
 win=["Strong Pickle utilise le format pédale simple d'Echoflex et de Doorn : elle ne peut jamais être réduite sous 60 %. Clic droit &gt; <b>Plugin size</b> : <b>60 %</b>, <b>75 %</b> ou <b>100 %</b> pour cette fenêtre, « (default) » indiquant la taille par défaut ; <b>Default size (new windows)</b> choisit la taille d'ouverture de toutes les nouvelles fenêtres de Strong Pickle (réglage commun à toutes les instances, enregistré sur l'ordinateur) et redimensionne aussitôt la fenêtre courante. La taille n'est pas sauvegardée avec le projet : à chaque ouverture, la fenêtre revient à la taille par défaut (60 % tant que vous n'en avez pas choisi une autre). Vous pouvez aussi tirer le coin en bas à droite jusqu'à 150 % (les proportions sont conservées).",
      "L'<b>écran</b> affiche le réglage du potard ou de l'inverseur que vous venez de bouger (potards en %, par ex. <i>sustain 64 %</i>, <i>crunch up (open)</i>) ; il apparaît quand un réglage bouge et s'efface au bout de 2,5 s. Il affiche aussi les messages du MIDI Learn. La <b>barre de presets</b> en dessous affiche en permanence le preset (ou <i>Edited</i>) et l'état du bypass."],
 bar_cap="Écran et barre de presets.",
 pre_t="8. Presets",
 pre=["Utilisez les flèches de la barre de presets, cliquez sur son nom, faites un clic droit n'importe où, ou passez par le menu de presets de votre DAW. Clic droit &gt; <b>Save user preset</b> enregistre jusqu'à 9 presets personnels."],
 fac_hdr=["PC", "Preset", "Loud. / Sust. / Filter (%)", "Crunch · Scoop · Octave", "Description"],
 tips_t="9. Conseils d'utilisation",
 tips=["Dans Reaper, placez Strong Pickle <b>avant</b> votre simulateur d'ampli (par exemple avant Ampire, suivi d'Impulse pour le baffle), avec un ampli clair ou légèrement crunch, comme avec la vraie pédale.",
       "Pour une 7 cordes en Sol ou des accordages plus bas, gardez le Sustain sous midi et le Filter un peu baissé : les cordes graves restent définies au lieu de crachoter. Au-delà de midi, les palm mutes deviennent un mur.",
       "Baissez le Loudness avant de monter le Sustain : la réserve de volume est énorme.",
       "Comme beaucoup de fuzz, elle réagit au potard de volume de la guitare : baissez-le pour éclaircir la fuzz."],
 tech_t="10. Caractéristiques techniques",
 tech=[["Format", "VST3 (Windows 64 bits, macOS universel)"], ["Canaux", "Mono ou stéréo"],
       ["Latence", "32 échantillons (filtre de sur-échantillonnage), annoncée au DAW"], ["Gain", "18 à 62 dB (Sustain)"],
       ["Filter", "tone stack passif 39 k / 10 nF et 4 nF / 22 k, pot 100 k ; Scoop : creux plus profond (environ −12 dB)"],
       ["Loudness", "coupé à +12 dB ; midi ≈ +4,5 dB au-dessus du bypass"],
       ["Traitement", "modèle du circuit composant par composant (2N3904, 1N914 ; Octave : déphaseur et redresseur Schottky), sur-échantillonnage x4, paramètres lissés"], ["Processeur", "environ 12 à 25 % d'un cœur à 48 kHz (stéréo), selon l'ordinateur"],
       ["MIDI", "tous les canaux ; MIDI Learn sur les 7 commandes (rien d'assigné par défaut), CC 14 bits, Program Change 1-13"],
       ["Fenêtre", "Plugin size 60 % / 75 % / 100 %, taille par défaut des nouvelles fenêtres au choix (60 % au départ), jusqu'à 150 %, jamais sous 60 % ; anti-gel v2 sous Windows"]]),
}

W = A4[0] - 36 * mm
def section(lang):
    t = T[lang]; f = []
    f.append(NextPageTemplate("p"))
    f.append(Spacer(1, 2 * mm)); f.append(P(t["lang"], "coverlang")); f.append(Spacer(1, 2 * mm))
    f.append(img(cover, W * 0.56)); f.append(Spacer(1, 1 * mm))
    f.append(P("S T R O N G &nbsp; P I C K L E", "cover")); f.append(P(t["sub"], "coversub"))
    f.append(Spacer(1, 2 * mm)); f.append(P(t["ver"], "coversub")); f.append(Spacer(1, 6 * mm))
    f.append(img(LOGO_PNG, 52 * mm))
    f.append(PageBreak())
    f.append(P(t["intro_t"], "h1")); f += [P(x) for x in t["intro"]]
    f.append(P(t["inst_t"], "h1"))
    for h, x in t["inst"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(PageBreak())
    f.append(P(t["ov_t"], "h1"))
    ov = Table([[[img(annotated, 92 * mm), Spacer(1, 5 * mm), img(on_img, 80 * mm), P(t["on_cap"], "small")],
                 table(t["ov_rows"], [9 * mm, 27 * mm, W - 96 * mm - 36 * mm])]], colWidths=[96 * mm, W - 96 * mm])
    ov.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
    f.append(ov); f.append(PageBreak())
    f.append(P(t["ctl_t"], "h1"))
    for h, x in t["ctl"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(KeepTogether([Spacer(1, 3 * mm), img(sw_img, W * 0.64), P(t["sw_cap"], "small")]))
    f.append(KeepTogether([Spacer(1, 2 * mm), img(oc_img, W * 0.40), P(t["oc_cap"], "small")]))
    f.append(P(t["fs_t"], "h1"))
    f += bullets(t["fs"])
    f.append(P(t["midi_t"], "h1")); f += [P(x) for x in t["midi_intro"]]
    f.append(KeepTogether([table([t["midi_hdr"]] + t["midi"], [36 * mm, 26 * mm, W - 62 * mm])]))
    f.append(Spacer(1, 2 * mm)); f += bullets(t["learn"])
    f.append(P(t["win_t"], "h1")); f.append(P(t["win"][0]))
    wt = Table([[[P(t["win"][1])], [img(bar_img, 66 * mm), P(t["bar_cap"], "small")]]], colWidths=[W - 70 * mm, 70 * mm])
    wt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (0, 0), 0)]))
    f.append(wt)
    f.append(P(t["pre_t"], "h1")); f += [P(x) for x in t["pre"]]
    rows = [t["fac_hdr"]]
    for i, (n, l, s, fi, cr, sc, oc) in enumerate(FACTORY):
        rows.append([str(i + 1), "<b>" + n + "</b>", f"{round(l * 10)} / {round(s * 10)} / {round(fi * 10)}", f"{SW[lang][cr]} · {SW[lang][sc]} · {OC[lang][oc]}", FACT_DESC[lang][i]])
    f.append(table(rows, [8 * mm, 26 * mm, 22 * mm, 24 * mm, W - 80 * mm], small=True))
    f.append(P(t["tips_t"], "h1")); f += bullets(t["tips"], "tip")
    f.append(KeepTogether([P(t["tech_t"], "h1"), table(t["tech"], [26 * mm, W - 26 * mm], header=False, small=True)])); f.append(Spacer(1, 4 * mm))
    f.append(PageBreak())
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Strong Pickle (NINE HANDS | DSP) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="STRONG PICKLE - Manual / Notice v" + VERSION, author="NINE HANDS | DSP")
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 3 * mm, A4[0], 3 * mm, stroke=0, fill=1); c.restoreState()
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
en = section("en"); fr = section("fr")
en[-1:] = [NextPageTemplate("cover"), PageBreak()]
doc.build(en + fr[:-1])
print(OUT)
