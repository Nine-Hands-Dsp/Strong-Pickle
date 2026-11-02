#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"
#include "WigaWindowSize.h"

using namespace juce;

namespace
{
    // Positions dans le repere de reference 760 x 958 (fond 1520 x 1916), generees par tools/make_background.py
    const Point<float>* const knobPos = layout::knobPos;
    const float* const knobRs = layout::knobR;
    const Point<float> crunchPos = layout::crunchPos, scoopPos = layout::scoopPos, octavePos = layout::octavePos;
    const float togHalf = layout::togHalf;
    const Point<float> ledPos = layout::ledPos;
    const float ledR = layout::ledR;
    const Point<float> octLedPos = layout::octLedPos;      // petite LED d'octave (verte), sous OCTAVE
    const float octLedR = layout::octLedR;
    const Colour octCore (0xffeaffd6), octMid (0xff5fe23e), octDeep (0xff1d6512);
    const Rectangle<float> glowRect = layout::glowRect;
    const Rectangle<float> pickleRect = layout::pickleRect;    // « PICKLE » s'allume en vert avec l'effet
    const Point<float> footPos = layout::footPos;
    const float footR = layout::footR;
    const Rectangle<float> statusRect = layout::statusRect;
    const Rectangle<float> presetRect = layout::presetRect;

    const float rotStart = MathConstants<float>::pi * (7.0f / 6.0f);    // -150 degres (7 h)
    const float rotEnd   = MathConstants<float>::pi * (17.0f / 6.0f);   // +150 degres (5 h)

    // LED bleue (comme la pedale d'origine)
    const Colour ledCore (0xfffff8d8), ledMid (0xffffc21a), ledDeep (0xff8a5600);   // LED jaune
    const Colour inkStatus (0xffd6e8b6);     // texte de l'ecran du visuel
    const Colour inkPreset (0xffdae0be);     // texte de la barre de presets du visuel

    const char* const valueIds[] = { "loudness", "sustain", "filter", "crunch", "scoop", "active", "gate", "octave" };
    constexpr int numValueIds = 8;
}

//==============================================================================
// POTARDS : repere blanc (trait) peint sur le dessus, lumiere fixe en haut a gauche
//==============================================================================
bool PickleKnob::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= getWidth() * 0.5f;
}

void PickleKnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                              float pos, float startAngle, float endAngle, Slider&)
{
    const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float R = (float) jmin (w, h) * 0.5f;
    const float a = startAngle + pos * (endAngle - startAngle);
    const Point<float> dir (std::sin (a), -std::cos (a));
    ignoreUnused (dir);
    // repere du visuel : barrette claire aux bouts arrondis, de 16 % a 81 % du rayon du moletage
    const float r0 = 0.16f * R, r1 = 0.81f * R, bw = 0.18f * R;
    Path p; p.addRoundedRectangle (-bw * 0.5f, -r1, bw, r1 - r0, bw * 0.30f);
    const auto tr = AffineTransform::rotation (a).translated (c);
    // peinture un peu plus claire tournee vers la lumiere (haut gauche), ombre tres legere en bas a droite
    const float lightFacing = 0.5f + 0.5f * std::cos (a + MathConstants<float>::pi * 0.25f);
    g.setColour (Colours::black.withAlpha (0.40f));
    g.fillPath (p, tr.translated (R * 0.010f, R * 0.014f));
    g.setColour (Colour (0xffdedfd9).interpolatedWith (Colours::white, 0.35f * lightFacing));
    g.fillPath (p, tr);
}

//==============================================================================
// INVERSEUR (2 positions : 0 = bas, 1 = haut)
//==============================================================================
LeverSwitch::LeverSwitch (RangedAudioParameter& p, bool threePos)
    : three (threePos),
      attachment (p, [this] (float v)
                  {
                      const int nf = frameOf (v);
                      if (nf != frame)
                      {
                          const bool jump = std::abs (nf - frame) == 2;
                          frame = targetFrame = nf;
                          shownFrame = jump ? 1 : nf;                  // bas <-> haut : position intermediaire ~55 ms
                          transitionUntil = Time::getMillisecondCounterHiRes() + 55.0;
                          repaint();
                      }
                  })
{
    attachment.sendInitialUpdate();
    shownFrame = targetFrame = frame;
    setMouseCursor (MouseCursor::PointingHandCursor);
}

int LeverSwitch::frameOf (float v) const
{
    if (! three) return v > 0.5f ? 2 : 0;
    const int i = roundToInt (v);
    return i == 1 ? 2 : (i == 2 ? 0 : 1);                   // Fat = haut, Bright = bas, Off = milieu
}

float LeverSwitch::valueOf (int f) const
{
    if (! three) return f >= 1 ? 1.0f : 0.0f;
    return f == 2 ? 1.0f : (f == 0 ? 2.0f : 0.0f);
}

void LeverSwitch::tick (double now)
{
    if (shownFrame != targetFrame && now >= transitionUntil)
    {
        shownFrame = targetFrame;
        repaint();
    }
}

void LeverSwitch::paint (Graphics& g)
{
    if (! strip.isValid()) return;
    const int fs = strip.getWidth();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, shownFrame * fs, fs, fs);
}

void LeverSwitch::moveTo (int nf)
{
    nf = jlimit (0, 2, nf);
    if (nf == frame) return;
    attachment.setValueAsCompleteGesture (valueOf (nf));
    if (onUserChange) onUserChange();
}

void LeverSwitch::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    if (! three) { moveTo (frame == 0 ? 2 : 0); return; }
    // 3 positions : clic dans la moitie haute = un cran vers le haut, moitie basse = un cran vers le bas
    moveTo (e.y < getHeight() / 2 ? frame + 1 : frame - 1);
}

void LeverSwitch::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) <= 0.0f) return;
    if (! three) moveTo (w.deltaY > 0 ? 2 : 0);
    else         moveTo (frame + (w.deltaY > 0 ? 1 : -1));
}

//==============================================================================
// FOOTSWITCH (fonctionnement d'Echoflex)
//==============================================================================
FootSwitchArea::FootSwitchArea (RangedAudioParameter& p)
    : attachment (p, [this] (float v) { value = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool FootSwitchArea::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * 0.5f;
}

void FootSwitchArea::paint (Graphics& g)
{
    if (pressed)
    {
        // bouton assombri tant qu'on appuie (disque noir a 25 %, rayon footR x 0,54)
        const auto c = getLocalBounds().toFloat().getCentre();
        const float r = (float) getWidth() * 0.5f * 0.54f;
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
}

void FootSwitchArea::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    pressed = true;
    repaint();
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);    // bascule des l'appui
    if (onUserChange) onUserChange();
}

void FootSwitchArea::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    pressed = false;
    repaint();
}

//==============================================================================
// EDITEUR
//==============================================================================
PickleAudioProcessorEditor::PickleAudioProcessorEditor (PickleAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      crunchSwitch (*p.apvts.getParameter ("crunch")),
      scoopSwitch  (*p.apvts.getParameter ("scoop")),
      octaveSwitch (*p.apvts.getParameter ("octave"), true),
      footswitch   (*p.apvts.getParameter ("active"))
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    glowYellow = ImageCache::getFromMemory (BinaryData::glow_yellow_png, BinaryData::glow_yellow_pngSize);
    pickleGlow = ImageCache::getFromMemory (BinaryData::pickle_glow_png, BinaryData::pickle_glow_pngSize);
    // v1.2.3 : fond « PICKLE allume » = le fond lui-meme avec les lettres vertes posees au pixel pres (fond a 2 px par unite).
    // Les deux fonds sont mis a l'echelle de la meme facon et dessines au meme endroit : aucun decalage possible,
    // a toutes les tailles de fenetre et a toutes les echelles d'affichage.
    if (background.isValid() && pickleGlow.isValid())
    {
        backgroundLit = background.createCopy();
        Graphics gl (backgroundLit);
        gl.drawImageAt (pickleGlow, roundToInt (pickleRect.getX() * 2.0f), roundToInt (pickleRect.getY() * 2.0f));
    }
    const auto strip = ImageCache::getFromMemory (BinaryData::toggle_strip_png, BinaryData::toggle_strip_pngSize);
    labelFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);

    rightClickFwd.callback = [this] (Component* c)
    {
        int idx = -1;
        for (int i = 0; i < PickleAudioProcessor::numKnobs; ++i)
            if (c == &knobs[i]) idx = i;
        showContextMenu (idx);
    };

    for (int i = 0; i < PickleAudioProcessor::numKnobs; ++i)
    {
        auto& s = knobs[i];
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (rotStart, rotEnd, true);
        s.setMouseDragSensitivity (220);
        s.setDoubleClickReturnValue (true, 5.0);
        s.setLookAndFeel (&lnf);
        s.setMouseCursor (MouseCursor::PointingHandCursor);
        s.addMouseListener (&rightClickFwd, false);
        s.onDragStart = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (s);
        knobAtt[i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, PickleAudioProcessor::knobId (i), s);
    }

    crunchSwitch.onRightClick = [this] { showContextMenu (3); };
    scoopSwitch.onRightClick  = [this] { showContextMenu (4); };
    octaveSwitch.onRightClick = [this] { showContextMenu (6); };
    for (auto* l : { &crunchSwitch, &scoopSwitch, &octaveSwitch })
    {
        l->strip = strip;
        l->onUserChange = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (*l);
    }
    footswitch.onRightClick = [this] { showContextMenu (5); };
    addAndMakeVisible (footswitch);

    for (int i = 0; i < numValueIds; ++i)
        lastValues[i] = proc.raw (valueIds[i]);
    lastPresetCounter = proc.presetCounter.load();
    lastMidiCount = proc.midiCounter.load();
    ledLevel = proc.rawAt (PickleAudioProcessor::rActive) > 0.5f ? 1.0f : 0.0f;
    octLedLevel = (ledLevel > 0.5f && roundToInt (proc.rawAt (PickleAudioProcessor::rOctave)) != 0) ? 1.0f : 0.0f;

    // Taille par defaut = taille minimale (impossible de reduire en dessous), proportions bloquees
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* cons = getConstrainer())
        cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);                      // anti-gel : l'hote ne redessine jamais ce qui est derriere la fenetre
    setSize (defaultW, defaultH);
    // taille des nouvelles fenetres : celle choisie dans le menu « Default size (new windows) »
    setScaledHeight (sizeHeight (WigaWindowSize::defaultIndex (JucePlugin_Name, numSizes)));
    rebuildCaches();
    startTimerHz (30);
}

PickleAudioProcessorEditor::~PickleAudioProcessorEditor()
{
    stopTimer();
    for (auto& s : knobs)
    {
        s.removeMouseListener (&rightClickFwd);
        s.setLookAndFeel (nullptr);
    }
}

//==============================================================================
int PickleAudioProcessorEditor::sizeHeight (int idx)
{
    const int h[numSizes] = { (int) defaultH, roundToInt (baseH * 0.75f), (int) baseH };
    return h[jlimit (0, numSizes - 1, idx)];
}

void PickleAudioProcessorEditor::setScaledHeight (int h)
{
    h = jmax ((int) defaultH, h);
    setSize (roundToInt ((float) h * (float) baseW / (float) baseH), h);
}

Rectangle<int> PickleAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    const float s = (float) getHeight() / (float) baseH;
    return r.transformedBy (AffineTransform::scale (s)).getSmallestIntegerContainer();
}

float PickleAudioProcessorEditor::physicalScale() const
{
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

void PickleAudioProcessorEditor::showStatus()
{
    statusUntil = Time::getMillisecondCounterHiRes() + 2500.0;
}

//==============================================================================
// Caches d'affichage : fond et calque lumineux deja a la taille de la fenetre (reconstruits par le timer)
//==============================================================================
void PickleAudioProcessorEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f) sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid()) return;

    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    bgScaled = (pw >= background.getWidth()) ? background : background.rescaled (pw, ph, Graphics::highResamplingQuality);

    const float s = (float) h / (float) baseH;
    const int gpx = roundToInt (glowRect.getWidth() * s * sc);
    glowYellowS = (gpx >= glowYellow.getWidth()) ? glowYellow : glowYellow.rescaled (gpx, gpx, Graphics::highResamplingQuality);
    const int pkw = roundToInt (pickleRect.getWidth() * s * sc), pkh = roundToInt (pickleRect.getHeight() * s * sc);
    bgLitScaled = ! backgroundLit.isValid() ? Image() : ((pw >= backgroundLit.getWidth()) ? backgroundLit : backgroundLit.rescaled (pw, ph, Graphics::highResamplingQuality));
    ignoreUnused (pkw, pkh);
    repaint();
}

void PickleAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici ; les limites du constrainer suffisent
    for (int i = 0; i < PickleAudioProcessor::numKnobs; ++i)
        knobs[i].setBounds (scaled ({ knobPos[i].x - knobRs[i], knobPos[i].y - knobRs[i], knobRs[i] * 2.0f, knobRs[i] * 2.0f }));
    auto lever = [this] (LeverSwitch& l, Point<float> c)
    {
        l.setBounds (scaled ({ c.x - togHalf, c.y - togHalf, togHalf * 2.0f, togHalf * 2.0f }));
    };
    lever (crunchSwitch, crunchPos);
    lever (scoopSwitch, scoopPos);
    lever (octaveSwitch, octavePos);
    footswitch.setBounds (scaled ({ footPos.x - footR, footPos.y - footR, footR * 2.0f, footR * 2.0f }));
    resizeSettle = 4;
}

//==============================================================================
// Anti-gel v2 (Studio Wiga) :
//  1. moteur de rendu logiciel force sur Windows, applique a chaque nouvelle fenetre hote (peer)
//     et reverifie toutes les secondes, jamais en boucle ;
//  2. aucun calcul lourd dans paint() : caches reconstruits par le timer seulement ;
//  3. chien de garde : redessin demande non servi en 1,5 s -> moteur de rendu reapplique, tout redessine ;
//  4. cote audio : aucun appel a l'hote ni allocation depuis le thread audio (voir PluginProcessor).
//==============================================================================
void PickleAudioProcessorEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw)
            peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void PickleAudioProcessorEditor::parentHierarchyChanged() { useSoftwareRenderer(); }
void PickleAudioProcessorEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void PickleAudioProcessorEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

//==============================================================================
String PickleAudioProcessorEditor::buildStatusLine() const
{
    const int i = lastChanged;
    if (i < 0) return {};
    const String id = valueIds[i];
    const float v = proc.raw (valueIds[i]);
    if (i < 3)            return id + "  " + String (roundToInt (v * 10.0f)) + " %";
    if (id == "crunch")   return v > 0.5f ? "crunch up (open)" : "crunch down (classic)";
    if (id == "scoop")    return v > 0.5f ? "scoop up (deep cut)" : "scoop down (classic)";
    if (id == "octave")
    {
        const int o = roundToInt (v);
        return o == 1 ? "octave up fat" : (o == 2 ? "octave up bright" : "octave off");
    }
    if (id == "gate")     return "noise gate " + PickleAudioProcessor::gateNames()[jlimit (0, 3, roundToInt (v))].toLowerCase();
    return v > 0.5f ? "effect on" : "bypass";
}

String PickleAudioProcessorEditor::buildPresetLine() const
{
    const int cp = proc.currentPreset.load();
    String s;
    if (cp >= 0 && cp < PickleAudioProcessor::getNumFactoryPresets())
        s << "P" << (cp + 1) << "  " << PickleAudioProcessor::getFactoryPreset (cp).name;
    else if (cp > 100) s << "User " << (cp - 100);
    else               s << "Edited";
    if (proc.rawAt (PickleAudioProcessor::rActive) < 0.5f) s << "  (bypass)";
    return s;
}

//==============================================================================
void PickleAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }

    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f) rebuildCaches (sc);
    }

    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0;
        lastPeer = nullptr;
        useSoftwareRenderer();
        repaint();
    }

    crunchSwitch.tick (now);
    scoopSwitch.tick (now);
    octaveSwitch.tick (now);

    int changed = -1, numChanged = 0;
    for (int i = 0; i < numValueIds; ++i)
    {
        const float v = proc.raw (valueIds[i]);
        if (std::abs (v - lastValues[i]) > 1.0e-4f) { lastValues[i] = v; changed = i; ++numChanged; }
    }
    const int pcount = proc.presetCounter.load();
    if (pcount != lastPresetCounter) { lastPresetCounter = pcount; numChanged = 0; }
    if (numChanged == 1) { lastChanged = changed; showStatus(); learnLineUntil = 0.0; }
    for (int i = 0; i < PickleAudioProcessor::numKnobs; ++i)
        if (knobs[i].isMouseButtonDown()) { lastChanged = i; showStatus(); learnLineUntil = 0.0; }

    const int lt = proc.learnTarget.load();
    const int learned = proc.lastLearned.exchange (-1);
    if (learned >= 0)
    {
        learnLine = String (PickleAudioProcessor::learnDef (learned).name) + " = CC " + String (proc.ccMap[learned].load());
        learnLineUntil = now + 2000.0;
        statusUntil = jmax (statusUntil, learnLineUntil);
    }
    else if (lt >= 0)
    {
        learnLine = "MIDI LEARN: move a control for " + String (PickleAudioProcessor::learnDef (lt).name);
        learnLineUntil = now + 200.0;
        statusUntil = jmax (statusUntil, now + 200.0);
    }

    const int mc = proc.midiCounter.load();
    if (mc != lastMidiCount) { lastMidiCount = mc; midiFlash = 8; }
    if (midiFlash > 0) --midiFlash;

    const float target = now < statusUntil ? 1.0f : 0.0f;
    statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.12f);
    if (statusAlpha < 0.005f) statusAlpha = 0.0f;
    if (statusAlpha > 0.995f) statusAlpha = 1.0f;

    // LED bleue = effet actif ; fondu 0,45 a l'allumage / 0,28 a l'extinction
    const bool on = proc.rawAt (PickleAudioProcessor::rActive) > 0.5f;
    ledLevel += ((on ? 1.0f : 0.0f) - ledLevel) * (on ? 0.45f : 0.28f);
    if (ledLevel < 0.003f) ledLevel = 0.0f;
    if (ledLevel > 0.997f) ledLevel = 1.0f;
    if (std::abs (ledLevel - drawnLed) > 0.004f)
    {
        drawnLed = ledLevel;
        askRepaint (scaled (glowRect));
        askRepaint (scaled (pickleRect));
    }
    // LED d'octave : allumee quand l'octave est en service (Fat ou Bright) et l'effet actif ; meme fondu
    const bool octOn = on && roundToInt (proc.rawAt (PickleAudioProcessor::rOctave)) != 0;
    octLedLevel += ((octOn ? 1.0f : 0.0f) - octLedLevel) * (octOn ? 0.45f : 0.28f);
    if (octLedLevel < 0.003f) octLedLevel = 0.0f;
    if (octLedLevel > 0.997f) octLedLevel = 1.0f;
    if (std::abs (octLedLevel - drawnOctLed) > 0.004f)
    {
        drawnOctLed = octLedLevel;
        const float hr = octLedR * 4.0f;
        askRepaint (scaled ({ octLedPos.x - hr, octLedPos.y - hr, hr * 2.0f, hr * 2.0f }).expanded (2));
    }

    const auto l1 = (learnLineUntil > 0.0 && now < learnLineUntil + 800.0 ? learnLine : buildStatusLine()), pl = buildPresetLine();
    const bool textChanged = l1 != lastLine1;
    lastLine1 = l1;
    if (std::abs (statusAlpha - drawnStatus) > 0.003f || (statusAlpha > 0.0f && textChanged))
    {
        drawnStatus = statusAlpha;
        askRepaint (scaled (statusRect.expanded (4.0f)));
    }
    if (pl != lastPreset || hoverZone != drawnHover || midiFlash > 0)
    {
        lastPreset = pl; drawnHover = hoverZone;
        askRepaint (scaled (presetRect.expanded (3.0f)));
    }
}

//==============================================================================
void PickleAudioProcessorEditor::drawLed (Graphics& g)
{
    const float level = ledLevel;
    if (level <= 0.003f) return;

    // la facade recoit la lumiere de la LED (calque precalcule : fond x eclairement x couleur, alpha)
    if (glowYellowS.isValid())
    {
        g.setOpacity (level);
        g.drawImage (glowYellowS, glowRect);
    }
    g.setOpacity (1.0f);

    const auto c = ledPos; const float r = ledR;
    ColourGradient halo (ledMid.withAlpha (0.22f * level), c.x, c.y, ledMid.withAlpha (0.0f), c.x + r * 2.6f, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (c.x - r * 2.6f, c.y - r * 2.6f, r * 5.2f, r * 5.2f);

    ColourGradient body (ledCore.withAlpha (level), c.x - r * 0.10f, c.y - r * 0.12f,
                         ledDeep.withAlpha (level), c.x + r * 1.05f, c.y + r * 0.2f, true);
    body.addColour (0.30, ledCore.interpolatedWith (ledMid, 0.55f).withAlpha (level));
    body.addColour (0.62, ledMid.withAlpha (level));
    g.setGradientFill (body);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    g.setColour (ledDeep.darker (0.6f).withAlpha (0.55f * level));
    g.drawEllipse (c.x - r + 0.6f, c.y - r + 0.6f, r * 2.0f - 1.2f, r * 2.0f - 1.2f, r * 0.12f);

    ColourGradient sp (Colours::white.withAlpha (0.75f * level), c.x - r * 0.30f, c.y - r * 0.42f,
                       Colours::white.withAlpha (0.0f), c.x - r * 0.30f + r * 0.32f, c.y - r * 0.42f, true);
    g.setGradientFill (sp);
    g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.70f, r * 0.64f, r * 0.52f);
}

void PickleAudioProcessorEditor::drawOctLed (Graphics& g)
{
    const float level = octLedLevel;
    if (level <= 0.003f) return;
    const auto c = octLedPos; const float r = octLedR;
    ColourGradient halo (octMid.withAlpha (0.30f * level), c.x, c.y, octMid.withAlpha (0.0f), c.x + r * 3.8f, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (c.x - r * 3.8f, c.y - r * 3.8f, r * 7.6f, r * 7.6f);
    ColourGradient body (octCore.withAlpha (level), c.x - r * 0.10f, c.y - r * 0.12f,
                         octDeep.withAlpha (level), c.x + r * 1.05f, c.y + r * 0.2f, true);
    body.addColour (0.30, octCore.interpolatedWith (octMid, 0.55f).withAlpha (level));
    body.addColour (0.62, octMid.withAlpha (level));
    g.setGradientFill (body);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    ColourGradient sp (Colours::white.withAlpha (0.75f * level), c.x - r * 0.30f, c.y - r * 0.42f,
                       Colours::white.withAlpha (0.0f), c.x - r * 0.30f + r * 0.32f, c.y - r * 0.42f, true);
    g.setGradientFill (sp);
    g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.70f, r * 0.64f, r * 0.52f);
}

void PickleAudioProcessorEditor::drawStatus (Graphics& g)
{
    const float a = statusAlpha;
    if (a <= 0.0f) return;
    // l'ecran (cadre et cases) fait partie du fond : seul le texte est dessine
    const bool longLine = lastLine1.length() > 18;
    g.setFont (Font (FontOptions (labelFace).withHeight (longLine ? 22.0f : 34.0f)));
    g.setColour (inkStatus.withAlpha (a));
    g.drawFittedText (lastLine1, statusRect.reduced (10.0f, 0.0f).toNearestInt(), Justification::centred, longLine ? 2 : 1, 0.75f);
}

void PickleAudioProcessorEditor::drawPresetBar (Graphics& g)
{
    const auto r = presetRect;
    // la barre et ses fleches font partie du fond : seuls le texte et le survol des fleches sont dessines
    const float aw = 34.0f;
    auto arrow = [&] (Rectangle<float> ar, bool left, bool hot)
    {
        Path p;
        const auto c = ar.getCentre();
        const float s = 6.5f;
        if (left) p.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.8f, c.y);
        else      p.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.8f, c.y);
        if (! hot) return;
        g.setColour (inkPreset.withAlpha (0.85f));
        g.fillPath (p);
    };
    arrow (r.withWidth (aw), true, hoverZone == 1);
    arrow (r.withTrimmedLeft (r.getWidth() - aw), false, hoverZone == 3);

    g.setFont (Font (FontOptions (labelFace).withHeight (23.0f)));
    g.setColour ((midiFlash > 0 ? Colour (0xffffd75a) : inkPreset).withAlpha (hoverZone == 2 ? 1.0f : 0.88f));
    g.drawFittedText (lastPreset, r.reduced (aw, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
}

void PickleAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::black);
    // anti-gel : aucun rescale ici, on dessine le cache existant (ou l'original en attendant le timer)
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (&bg == &background ? Graphics::highResamplingQuality : Graphics::lowResamplingQuality);
    if (bg.isValid())
        g.drawImage (bg, getLocalBounds().toFloat());
    // « PICKLE » allume : meme image de fond (version allumee), meme mise a l'echelle, limitee a la zone du mot
    if (ledLevel > 0.003f)
    {
        const Image& bl = bgLitScaled.isValid() ? bgLitScaled : backgroundLit;
        if (bl.isValid())
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (scaled (pickleRect));
            g.setOpacity (ledLevel);
            g.drawImage (bl, getLocalBounds().toFloat());
        }
    }

    const float s = (float) getHeight() / (float) baseH;
    g.addTransform (AffineTransform::scale (s));
    drawLed (g);
    drawOctLed (g);
    drawStatus (g);
    drawPresetBar (g);
}

//==============================================================================
// Souris : barre de presets (fleches = precedent / suivant, nom = menu deroulant), clic droit = menu
//==============================================================================
static int presetZoneAt (Point<float> p)
{
    if (! presetRect.contains (p)) return 0;
    if (p.x < presetRect.getX() + 34.0f) return 1;
    if (p.x > presetRect.getRight() - 34.0f) return 3;
    return 2;
}

void PickleAudioProcessorEditor::mouseMove (const MouseEvent& e)
{
    const float s = (float) getHeight() / (float) baseH;
    hoverZone = presetZoneAt (e.position / s);
    setMouseCursor (hoverZone != 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void PickleAudioProcessorEditor::mouseExit (const MouseEvent&) { hoverZone = 0; }

void PickleAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }
    const float s = (float) getHeight() / (float) baseH;
    switch (presetZoneAt (e.position / s))
    {
        case 1: stepPreset (-1); break;
        case 2: showPresetMenu(); break;
        case 3: stepPreset (1); break;
        default: break;
    }
}

void PickleAudioProcessorEditor::stepPreset (int delta)
{
    const int n = PickleAudioProcessor::getNumFactoryPresets();
    int cp = proc.currentPreset.load();
    if (cp < 0 || cp >= n) cp = delta > 0 ? -1 : 0;
    proc.applyFactoryPreset ((cp + delta + n) % n);
}

void PickleAudioProcessorEditor::showPresetMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Factory presets");
    for (int i = 0; i < PickleAudioProcessor::getNumFactoryPresets(); ++i)
        m.addItem (100 + i, String (i + 1) + ". " + PickleAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    m.addSeparator();
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    m.addSubMenu ("Load user preset", load);
    m.addSubMenu ("Save user preset", save);

    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (scaled (presetRect))),
        [safe = Component::SafePointer<PickleAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pr = safe->proc;
            if      (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
            safe->showStatus();
        });
}

void PickleAudioProcessorEditor::handleMidiMenu (int r)
{
    if (r >= 1000 && r < 1000 + PickleAudioProcessor::numLearn)      proc.learnTarget.store (r - 1000);
    else if (r >= 1100 && r < 1100 + PickleAudioProcessor::numLearn) proc.ccMap[r - 1100].store (-1);
    else if (r == 1200) proc.learnTarget.store (-1);
    else if (r == 1201) { proc.learnTarget.store (-1); proc.resetMidiMap(); }
}

void PickleAudioProcessorEditor::showContextMenu (int learnIdx)
{
    PopupMenu menu;
    auto ccText = [this] (int i)
    {
        const int cc = proc.ccMap[i].load();
        return cc >= 0 ? "CC " + String (cc) : String (CharPointer_UTF8 ("\xe2\x80\x94"));
    };
    const int lt = proc.learnTarget.load();
    if (learnIdx >= 0)
    {
        const String name = PickleAudioProcessor::learnDef (learnIdx).name;
        menu.addItem (1000 + learnIdx, "MIDI Learn: " + name + " (" + ccText (learnIdx) + ")", true, lt == learnIdx);
        menu.addItem (1100 + learnIdx, "Clear MIDI: " + name, proc.ccMap[learnIdx].load() >= 0);
        if (lt >= 0) menu.addItem (1200, "Cancel MIDI learn");
        menu.addSeparator();
    }

    PopupMenu sizes;
    {
        // Plugin size : taille de cette fenetre + taille par defaut des nouvelles fenetres (comme Octopus)
        const char* names[numSizes] = { "60 %", "75 %", "100 %" };
        const int def = WigaWindowSize::defaultIndex (JucePlugin_Name, numSizes);
        PopupMenu defSizes;
        for (int i = 0; i < numSizes; ++i)
        {
            sizes.addItem (1 + i, String (names[i]) + (i == def ? "  (default)" : ""), true, std::abs (getHeight() - sizeHeight (i)) <= 2);
            defSizes.addItem (20 + i, names[i], true, i == def);
        }
        sizes.addSeparator();
        sizes.addSubMenu ("Default size (new windows)", defSizes);
        menu.addSubMenu ("Plugin size", sizes);
    }
    menu.addSeparator();

    PopupMenu factory;
    for (int i = 0; i < PickleAudioProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, String (i + 1) + ". " + PickleAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    menu.addSubMenu ("Factory presets", factory);
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();

    PopupMenu gate;
    const int gNow = jlimit (0, 3, roundToInt (proc.rawAt (PickleAudioProcessor::rGate)));
    const char* gateHelp[4] = { "Off (default, like the original pedal)", "Light", "Medium", "Strong (noisy pickups / hum)" };
    for (int i = 0; i < 4; ++i) gate.addItem (50 + i, gateHelp[i], true, gNow == i);
    menu.addSubMenu ("Noise gate", gate);

    PopupMenu settings;
    settings.addItem (41, "MIDI footswitch latching (127 = on / 0 = off)", true, proc.rawAt (PickleAudioProcessor::rMidiFsMode) < 0.5f);
    settings.addItem (42, "MIDI footswitch momentary (toggle each press)", true, proc.rawAt (PickleAudioProcessor::rMidiFsMode) > 0.5f);
    menu.addSubMenu ("Settings", settings);

    PopupMenu midi;
    for (int i = 0; i < PickleAudioProcessor::numLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (PickleAudioProcessor::learnDef (i).name) + " (" + ccText (i) + ")",
                      true, lt == i);
    midi.addSeparator();
    if (lt >= 0) midi.addItem (1200, "Cancel MIDI learn");
    midi.addItem (1201, "Clear all MIDI assignments");
    midi.addSeparator();
    midi.addItem (910, "CC n+32 = fine (14-bit) for knobs on CC 0-31", false);
    midi.addItem (911, "Program Change 1-" + String (PickleAudioProcessor::getNumFactoryPresets()) + "  Factory presets", false);
    menu.addSubMenu ("MIDI", midi);
    menu.addSeparator();
    menu.addItem (999, "Strong Pickle fuzz v" + String (JucePlugin_VersionString) + " - STUDIO WIGA", false);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
        [safe = Component::SafePointer<PickleAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe;
            auto& pr = ed.proc;
            if      (r >= 1 && r < 1 + numSizes) ed.setScaledHeight (ed.sizeHeight (r - 1));
            else if (r >= 20 && r < 20 + numSizes)
            {
                WigaWindowSize::setDefaultIndex (JucePlugin_Name, r - 20);   // toutes les nouvelles fenetres de ce plugin
                ed.setScaledHeight (ed.sizeHeight (r - 20));                // et cette fenetre tout de suite
            }
            else if (r == 41) pr.setParamValue ("midiFsMode", 0.0f);
            else if (r == 42) pr.setParamValue ("midiFsMode", 1.0f);
            else if (r >= 50 && r <= 53) pr.setParamValue ("gate", (float) (r - 50));
            else if (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
            else if (r >= 1000) { ed.handleMidiMenu (r); return; }
            ed.showStatus();
        });
}
