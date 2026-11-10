#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"
#include "WigaWindowSize.h"

using namespace juce;

namespace
{
    // repere 617,5 x 900 (fond 1235 x 1800 / 2), positions generees par tools/make_art.py dans Layout.h
    // Barre de presets et ligne d'etat AU MEME ENDROIT (regle des skills, nov. 2026) : une seule pastille,
    // centree sur l'ancien bloc ecran + barre, meme largeur (800 px du fond).
    const float barH = 38.0f, barFont = 23.0f;     // texte agrandi (presets et ligne d'etat a la meme taille)
    const Rectangle<float> barRect { layout::statusRect.getX(),
                                     (layout::statusRect.getY() + layout::presetRect.getBottom()) * 0.5f - barH * 0.5f,
                                     layout::statusRect.getWidth(), barH };
    const Rectangle<float> statusRect = barRect;
    const Rectangle<float> presetRect = barRect;

    const float rotStart = MathConstants<float>::pi * (7.0f / 6.0f);    // -150 degres (7 h)
    const float rotEnd   = MathConstants<float>::pi * (17.0f / 6.0f);   // +150 degres (5 h)

    const Colour inkCream (0xffe6e4c6);     // serigraphie creme (comme le cornichon #dfdfa5, un peu plus claire)
    const Colour pickleLit (0xffa8f08e), pickleEdge (0xff58c83e);     // PICKLE allume : vert clair, bord sature
    const Colour ledYellow (0xffffc21a);
    const Colour screenInk (0xffd6e8b6);     // texte de l'ecran et de la barre de presets
    const Colour panel (0xff0a140a);        // ecran et barre : vert tres sombre, semi-transparent (le cornichon se devine)
    const Colour menuAccent (0xff8ee06a);   // pastille des en-tetes de menu (vert PICKLE)

    const char* const valueIds[] = { "loudness", "sustain", "filter", "crunch", "scoop", "active", "gate", "octave" };
    constexpr int numValueIds = 8;
}

//==============================================================================
// POTARDS : repere creme arrondi (de 0,42 a 0,90 du rayon), lumiere fixe en haut a gauche
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
    const Point<float> p0 = c + dir * (0.42f * R), p1 = c + dir * (0.86f * R);
    const float lw = jmax (1.4f, 0.062f * R);
    Path line; line.startNewSubPath (p0); line.lineTo (p1);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.strokePath (line, PathStrokeType (lw * 1.15f, PathStrokeType::curved, PathStrokeType::rounded),
                  AffineTransform::translation (R * 0.010f, R * 0.014f));
    const float lightFacing = 0.5f + 0.5f * std::cos (a + MathConstants<float>::pi * 0.25f);
    g.setColour (Colour (0xfff2f2ee).withMultipliedBrightness (0.92f + 0.08f * lightFacing));
    g.strokePath (line, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
// INVERSEUR
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
                          shownFrame = jump ? 1 : nf;                  // bas <-> haut : position du milieu ~55 ms
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
    if (shownFrame != targetFrame && now >= transitionUntil) { shownFrame = targetFrame; repaint(); }
}

void LeverSwitch::paint (Graphics& g)
{
    if (! strip.isValid()) return;
    const int fs = strip.getWidth();
    const int row = 2 - shownFrame;                         // bande : haut / milieu / bas
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    // meme rectangle de destination pour les trois images : la rondelle et l'ecrou ne bougent pas
    g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, row * fs, fs, fs);
}

bool LeverSwitch::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = (float) getWidth() * (layout::togR / layout::togS);
    return std::abs ((float) x - c.x) <= r * 1.15f && std::abs ((float) y - c.y) <= r * 1.6f;
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
    moveTo (e.y < getHeight() / 2 ? frame + 1 : frame - 1); // moitie haute = un cran vers le haut
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
    return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * (layout::footR * 1.35f / layout::fsS);
}

void FootSwitchArea::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (ledLevel > 0.003f && reflection.isValid())
    {
        g.setOpacity (ledLevel);
        g.drawImage (reflection, b);
        g.setOpacity (1.0f);
    }
    if (pressed)
    {
        const float r = b.getWidth() * (layout::footR / layout::fsS) * 0.98f;
        const auto c = b.getCentre();
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
}

void FootSwitchArea::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    pressed = true;
    repaint();
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
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
    background = ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
    glow       = ImageCache::getFromMemory (BinaryData::glow_yellow_png, BinaryData::glow_yellow_pngSize);
    glowOct    = ImageCache::getFromMemory (BinaryData::glow_yellow_oct_png, BinaryData::glow_yellow_oct_pngSize);
    pickleGlow = ImageCache::getFromMemory (BinaryData::pickle_glow_png, BinaryData::pickle_glow_pngSize);
    ledOn      = ImageCache::getFromMemory (BinaryData::led_on_png, BinaryData::led_on_pngSize);
    footswitch.reflection = ImageCache::getFromMemory (BinaryData::fs_refl_png, BinaryData::fs_refl_pngSize);
    const auto strip = ImageCache::getFromMemory (BinaryData::toggle_strip_png, BinaryData::toggle_strip_pngSize);
    labelFace   = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
    boldFace    = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
    lightFace   = Typeface::createSystemTypefaceFor (BinaryData::PoppinsLight_ttf, BinaryData::PoppinsLight_ttfSize);
    regularFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf, BinaryData::PoppinsRegular_ttfSize);
    menuLnf.f = { regularFace, labelFace, boldFace };
    if (auto xml = XmlDocument::parse (String::fromUTF8 (BinaryData::NineHandsLogo_svg, BinaryData::NineHandsLogo_svgSize)))
        logo = Drawable::createFromSVG (*xml);
    buildPrint();

    // index des commandes = learnDef du processeur (0-2 potards, 3 crunch, 4 scoop, 5 footswitch, 6 octave)
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
    footswitch.onUserChange = [this] { proc.currentPreset = -1; };
    addAndMakeVisible (footswitch);

    for (int i = 0; i < numValueIds; ++i) lastValues[i] = proc.raw (valueIds[i]);
    lastPresetCounter = proc.presetCounter.load();
    lastMidiCount = proc.midiCounter.load();
    ledLevel = proc.rawAt (PickleAudioProcessor::rActive) > 0.5f ? 1.0f : 0.0f;
    octLedLevel = (ledLevel > 0.5f && roundToInt (proc.rawAt (PickleAudioProcessor::rOctave)) != 0) ? 1.0f : 0.0f;
    footswitch.ledLevel = ledLevel;

    // Taille par defaut = taille minimale (impossible de reduire en dessous), proportions bloquees
    const int defaultW = roundToInt ((float) defaultH * baseW / baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, roundToInt ((float) maxH * baseW / baseH), maxH);
    if (auto* cons = getConstrainer())
        cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);
    setSize (defaultW, defaultH);
    setScaledHeight (sizeHeight (WigaWindowSize::defaultIndex (JucePlugin_Name, numSizes)));
    rebuildCaches();
    startTimerHz (30);
}

PickleAudioProcessorEditor::~PickleAudioProcessorEditor()
{
    PopupMenu::dismissAllActiveMenus();
    stopTimer();
    for (auto& s : knobs) { s.removeMouseListener (&rightClickFwd); s.setLookAndFeel (nullptr); }
}

//==============================================================================
int PickleAudioProcessorEditor::sizeHeight (int idx)
{
    const int h[numSizes] = { (int) defaultH, roundToInt (refH * 0.75f), (int) refH };
    return h[jlimit (0, numSizes - 1, idx)];
}

void PickleAudioProcessorEditor::setScaledHeight (int h)
{
    h = jlimit ((int) defaultH, (int) maxH, h);
    setSize (roundToInt ((float) h * baseW / baseH), h);
}

Rectangle<int> PickleAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    const float s = (float) getHeight() / baseH;
    return r.transformedBy (AffineTransform::scale (s)).getSmallestIntegerContainer();
}

float PickleAudioProcessorEditor::physicalScale() const
{
    if (forcedScale > 0.0f) return forcedScale;
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

void PickleAudioProcessorEditor::showStatus() { statusUntil = Time::getMillisecondCounterHiRes() + 2500.0; }

//==============================================================================
// Caches d'affichage (reconstruits par le timer seulement)
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
    const float s = (float) h / baseH;
    const int gpx = roundToInt (layout::glowRect.getWidth() * s * sc);
    glowS    = (gpx >= glow.getWidth())    ? glow    : glow.rescaled (gpx, gpx, Graphics::highResamplingQuality);
    glowOctS = (gpx >= glowOct.getWidth()) ? glowOct : glowOct.rescaled (gpx, gpx, Graphics::highResamplingQuality);
    repaint();
}

void PickleAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici ; les limites du constrainer suffisent
    for (int i = 0; i < PickleAudioProcessor::numKnobs; ++i)
    {
        const auto c = layout::knobPos[i]; const float r = layout::knobR[i];
        knobs[i].setBounds (scaled ({ c.x - r, c.y - r, r * 2.0f, r * 2.0f }));
    }
    // inverseurs : le sprite (rondelle + levier + ombre) est carre et centre sur la rondelle
    auto lever = [this] (LeverSwitch& l, Point<float> c)
    {
        const float hs = layout::togS * 0.5f;
        const float s = (float) getHeight() / baseH;
        // bornes en pixels entiers, carrees, centrees au demi-pixel pres : les trois images y sont dessinees a l'identique
        const int side = roundToInt (layout::togS * s);
        l.setBounds (roundToInt ((c.x - hs) * s), roundToInt ((c.y - hs) * s), side, side);
    };
    lever (crunchSwitch, layout::crunchPos);
    lever (scoopSwitch, layout::scoopPos);
    lever (octaveSwitch, layout::octavePos);
    const float fr = layout::fsS * 0.5f;
    footswitch.setBounds (scaled ({ layout::fsPos.x - fr, layout::fsPos.y - fr, fr * 2.0f, fr * 2.0f }));
    resizeSettle = 4;
}

//==============================================================================
// Anti-gel v2 (NINE HANDS | DSP)
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
        if (peer->getCurrentRenderingEngine() != sw) peer->setCurrentRenderingEngine (sw);
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
        repaintAskedMs = 0.0; lastPeer = nullptr;
        useSoftwareRenderer(); repaint();
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
    statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.30f);   // ~120 ms a l'apparition, ~300 ms au retour
    if (statusAlpha < 0.005f) statusAlpha = 0.0f;
    if (statusAlpha > 0.995f) statusAlpha = 1.0f;

    // LED jaune + PICKLE vert = effet actif ; LED d'octave = octave en service (Fat ou Bright) et effet actif
    const bool on = proc.rawAt (PickleAudioProcessor::rActive) > 0.5f;
    ledLevel += ((on ? 1.0f : 0.0f) - ledLevel) * (on ? 0.45f : 0.28f);
    if (ledLevel < 0.003f) ledLevel = 0.0f;
    if (ledLevel > 0.997f) ledLevel = 1.0f;
    if (std::abs (ledLevel - drawnLed) > 0.004f)
    {
        drawnLed = ledLevel;
        footswitch.ledLevel = ledLevel;
        askRepaint (scaled (layout::glowRect));
        askRepaint (scaled (layout::pickleGlowRect));
        footswitch.repaint();
    }
    const bool octOn = on && roundToInt (proc.rawAt (PickleAudioProcessor::rOctave)) != 0;
    octLedLevel += ((octOn ? 1.0f : 0.0f) - octLedLevel) * (octOn ? 0.45f : 0.28f);
    if (octLedLevel < 0.003f) octLedLevel = 0.0f;
    if (octLedLevel > 0.997f) octLedLevel = 1.0f;
    if (std::abs (octLedLevel - drawnOctLed) > 0.004f)
    {
        drawnOctLed = octLedLevel;
        askRepaint (scaled (layout::octGlowRect));
    }

    const auto l1 = (learnLineUntil > 0.0 && now < learnLineUntil + 800.0 ? learnLine : buildStatusLine()), pl = buildPresetLine();
    const bool textChanged = l1 != lastLine1;
    lastLine1 = l1;
    if (std::abs (statusAlpha - drawnStatus) > 0.003f || (statusAlpha > 0.0f && textChanged))
    {
        drawnStatus = statusAlpha;
        askRepaint (scaled (barRect.expanded (3.0f)));
    }
    if (pl != lastPreset || hoverZone != drawnHover || midiFlash > 0)
    {
        lastPreset = pl; drawnHover = hoverZone;
        askRepaint (scaled (barRect.expanded (3.0f)));
    }
}

//==============================================================================
// Serigraphie vectorielle : construite une fois en chemins (unites du plugin), remplie a l'echelle de la fenetre
//==============================================================================
static Path textPath (const layout::Txt& t, const Typeface::Ptr& face)
{
    const Font font (FontOptions (face).withPointHeight (t.em));
    const String s (t.s);
    float total = 0.0f;
    for (int i = 0; i < s.length(); ++i)
        total += GlyphArrangement::getStringWidth (font, String::charToString (s[i])) + (i > 0 ? t.track : 0.0f);
    const float base = (font.getAscent() - font.getDescent()) * 0.5f;
    GlyphArrangement ga;
    float x = -total * 0.5f;
    for (int i = 0; i < s.length(); ++i)
    {
        const String ch = String::charToString (s[i]);
        ga.addLineOfText (font, ch, x, base);
        x += GlyphArrangement::getStringWidth (font, ch) + t.track;
    }
    Path p; ga.createPath (p);
    p.applyTransform (AffineTransform::rotation (-t.rot * MathConstants<float>::pi / 180.0f).translated (t.x, t.y));
    return p;
}

void PickleAudioProcessorEditor::buildPrint()
{
    printPath.clear();
    auto faceOf = [this] (int f) { return f == 1 ? boldFace : (f == 2 ? lightFace : labelFace); };
    for (const auto& t : layout::texts) printPath.addPath (textPath (t, faceOf (t.font)));
    for (const auto& k : layout::ticks)
    {
        Path seg; seg.startNewSubPath (k.x0, k.y0); seg.lineTo (k.x1, k.y1);
        Path stroked;
        PathStrokeType (k.w, PathStrokeType::curved, PathStrokeType::rounded).createStrokedPath (stroked, seg);
        printPath.addPath (stroked);
    }
    picklePath = textPath (layout::pickleText, faceOf (layout::pickleText.font));
}

void PickleAudioProcessorEditor::drawPrint (Graphics& g)
{
    const float lv = ledLevel;
    g.setColour (inkCream);
    g.fillPath (printPath);
    g.setColour (inkCream.interpolatedWith (pickleLit, lv));
    g.fillPath (picklePath);
    if (lv > 0.003f)
    {
        const auto b = picklePath.getBounds();
        ColourGradient core (Colour (0xffe4ffd6).withAlpha (0.55f * lv), b.getCentreX(), b.getY() + b.getHeight() * 0.35f,
                             pickleEdge.withAlpha (0.0f), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (core);
        g.fillPath (picklePath);
        g.setColour (pickleEdge.withAlpha (0.55f * lv));
        g.strokePath (picklePath, PathStrokeType (0.9f));
    }
    if (logo != nullptr)
    {
        const auto vb = logo->getDrawableBounds();
        const float w = layout::logoW, h = w * vb.getHeight() / jmax (1.0f, vb.getWidth());
        logo->drawWithin (g, { layout::logoPos.x - w * 0.5f, layout::logoPos.y - h * 0.5f, w, h }, RectanglePlacement::centred, 0.92f);
    }
}

void PickleAudioProcessorEditor::drawGlows (Graphics& g)
{
    // lumieres sur la facade (calques alpha precalcules), SOUS la serigraphie et les lentilles : jamais de texte masque
    auto layer = [&] (float level, const Image& img, Rectangle<float> r)
    {
        if (level <= 0.003f || ! img.isValid()) return;
        g.setOpacity (level); g.drawImage (img, r); g.setOpacity (1.0f);
    };
    layer (ledLevel, glowS.isValid() ? glowS : glow, layout::glowRect);
    layer (octLedLevel, glowOctS.isValid() ? glowOctS : glowOct, layout::octGlowRect);
    layer (ledLevel, pickleGlow, layout::pickleGlowRect);              // halo vert de PICKLE
}

void PickleAudioProcessorEditor::drawLeds (Graphics& g)
{
    auto one = [&] (Point<float> c, float level)
    {
        if (level <= 0.003f) return;
        const float hs = layout::ledS * 0.5f;
        if (ledOn.isValid()) { g.setOpacity (level); g.drawImage (ledOn, { c.x - hs, c.y - hs, hs * 2.0f, hs * 2.0f }); }
        g.setOpacity (1.0f);
        const float hr = 20.0f;
        ColourGradient halo (ledYellow.withAlpha (0.28f * level), c.x, c.y, ledYellow.withAlpha (0.0f), c.x + hr, c.y, true);
        halo.addColour (0.4, ledYellow.withAlpha (0.07f * level));
        g.setGradientFill (halo);
        g.fillEllipse (c.x - hr, c.y - hr, hr * 2.0f, hr * 2.0f);
    };
    one (layout::ledPos, ledLevel);
    one (layout::octLedPos, octLedLevel);
}

void PickleAudioProcessorEditor::drawStatus (Graphics& g)
{
    // pastille unique : fond + contour (la ligne d'etat et la barre de presets s'y relaient en fondu croise)
    const auto r = barRect;
    g.setColour (panel.withAlpha (0.80f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Colour (0xff8fb07e).withAlpha (hoverZone != 0 && statusAlpha <= 0.0f ? 0.65f : 0.40f));
    g.drawRoundedRectangle (r.reduced (0.6f), r.getHeight() * 0.5f, 1.2f);
    const float a = statusAlpha;
    if (a <= 0.0f) return;
    g.setFont (Font (FontOptions (labelFace).withHeight (barFont)));
    g.setColour (screenInk.withAlpha (a));
    g.drawFittedText (lastLine1, r.reduced (r.getHeight() * 0.5f, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
}

void PickleAudioProcessorEditor::drawPresetBar (Graphics& g)
{
    const float a = 1.0f - statusAlpha;                 // la barre de presets revient quand la ligne d'etat s'efface
    if (a <= 0.0f) return;
    const auto r = barRect;
    const float aw = 34.0f;
    auto arrow = [&] (Rectangle<float> ar, bool left, bool hot)
    {
        Path p; const auto c = ar.getCentre(); const float sz = 5.5f;
        if (left) p.addTriangle (c.x + sz * 0.6f, c.y - sz, c.x + sz * 0.6f, c.y + sz, c.x - sz * 0.8f, c.y);
        else      p.addTriangle (c.x - sz * 0.6f, c.y - sz, c.x - sz * 0.6f, c.y + sz, c.x + sz * 0.8f, c.y);
        g.setColour (screenInk.withAlpha ((hot ? 1.0f : 0.6f) * a));
        g.fillPath (p);
    };
    arrow (r.withWidth (aw), true, hoverZone == 1);
    arrow (r.withTrimmedLeft (r.getWidth() - aw), false, hoverZone == 3);
    g.setFont (Font (FontOptions (labelFace).withHeight (barFont)));
    g.setColour ((midiFlash > 0 ? Colour (0xffc8ff9c) : screenInk).withAlpha ((hoverZone == 2 ? 1.0f : 0.92f) * a));
    g.drawFittedText (lastPreset, r.reduced (aw, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
}

void PickleAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colour (0xff111112));          // autour des jacks : le fond de l'image est transparent
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (&bg == &background ? Graphics::highResamplingQuality : Graphics::lowResamplingQuality);
    if (bg.isValid()) g.drawImage (bg, getLocalBounds().toFloat());

    const float s = (float) getHeight() / baseH;
    g.addTransform (AffineTransform::scale (s));
    g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
    drawStatus (g);
    drawPresetBar (g);
    drawGlows (g);
    drawPrint (g);
    drawLeds (g);
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
    const float s = (float) getHeight() / baseH;
    hoverZone = presetZoneAt (e.position / s);
    setMouseCursor (hoverZone != 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void PickleAudioProcessorEditor::mouseExit (const MouseEvent&) { hoverZone = 0; }

void PickleAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }
    const float s = (float) getHeight() / baseH;
    if (statusAlpha > 0.0f && barRect.contains (e.position / s))
    {   // la ligne d'etat ne bloque pas : un clic fait revenir la barre de presets
        statusUntil = 0.0; learnLineUntil = 0.0;
        return;
    }
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
    m.setLookAndFeel (&menuLnf);
    NineHandsMenu::addHeader (m, menuLnf.f, NineHandsMenu::icoPreset, menuAccent, "Presets", buildPresetLine());
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
    using namespace NineHandsMenu;
    PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    auto ccText = [this] (int i)
    {
        const int cc = proc.ccMap[i].load();
        return cc >= 0 ? "CC " + String (cc) : String ("not assigned");
    };
    const int lt = proc.learnTarget.load();
    if (learnIdx >= 0)
    {
        // menu d'une commande : en-tete (pastille + nom + assignation MIDI), puis MIDI Learn
        const String name = PickleAudioProcessor::learnDef (learnIdx).name;
        const int ico = learnIdx < 3 ? icoKnob : (learnIdx == 5 ? icoPedal : icoSwitch);
        addHeader (menu, menuLnf.f, ico, menuAccent, name, "MIDI: " + ccText (learnIdx));
        menu.addItem (1000 + learnIdx, "MIDI Learn: " + name, true, lt == learnIdx);
        menu.addItem (1100 + learnIdx, "Clear MIDI: " + name, proc.ccMap[learnIdx].load() >= 0);
        if (lt >= 0) menu.addItem (1200, "Cancel MIDI learn");
        menu.addSeparator();
    }
    else
        addHeader (menu, menuLnf.f, icoPedal, menuAccent, "Strong Pickle", "Jumbo fuzz  v" + String (JucePlugin_VersionString));

    PopupMenu sizes;
    {
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
    }
    menu.addSubMenu ("Plugin size", sizes);
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
    midi.addSectionHeader ("MIDI Learn");
    for (int i = 0; i < PickleAudioProcessor::numLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (PickleAudioProcessor::learnDef (i).name) + "  (" + ccText (i) + ")", true, lt == i);
    midi.addSeparator();
    if (lt >= 0) midi.addItem (1200, "Cancel MIDI learn");
    midi.addItem (1201, "Clear all MIDI assignments");
    midi.addSeparator();
    midi.addItem (910, "CC n+32 = fine (14-bit) for knobs on CC 0-31", false);
    midi.addItem (911, "Program Change 1-" + String (PickleAudioProcessor::getNumFactoryPresets()) + "  Factory presets", false);
    menu.addSubMenu ("MIDI", midi);
    menu.addSeparator();
    menu.addItem (999, "Strong Pickle v" + String (JucePlugin_VersionString) + "  NINE HANDS | DSP", false);

    menu.showMenuAsync (PopupMenu::Options().withMousePosition(),    // menu clic droit : toujours la ou l on clique
        [safe = Component::SafePointer<PickleAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe;
            auto& pr = ed.proc;
            if      (r >= 1 && r < 1 + numSizes) ed.setScaledHeight (ed.sizeHeight (r - 1));
            else if (r >= 20 && r < 20 + numSizes)
            {
                WigaWindowSize::setDefaultIndex (JucePlugin_Name, r - 20);
                ed.setScaledHeight (ed.sizeHeight (r - 20));
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
