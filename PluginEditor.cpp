#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"

using namespace juce;

namespace
{
    // Positions dans le repere de reference 760 x 958 (fond 1520 x 1916), generees par tools/make_background.py
    const Point<float>* const knobPos = layout::knobPos;
    const float knobR = layout::knobR;
    const Point<float> crunchPos = layout::crunchPos, scoopPos = layout::scoopPos;
    const float togHalf = layout::togHalf;
    const Point<float> ledPos = layout::ledPos;
    const float ledR = layout::ledR;
    const Rectangle<float> glowRect = layout::glowRect;
    const Point<float> footPos = layout::footPos;
    const float footR = layout::footR;
    const Rectangle<float> statusRect = layout::statusRect;
    const Rectangle<float> presetRect = layout::presetRect;

    const float rotStart = MathConstants<float>::pi * (7.0f / 6.0f);    // -150 degres (7 h)
    const float rotEnd   = MathConstants<float>::pi * (17.0f / 6.0f);   // +150 degres (5 h)

    // LED bleue (comme la pedale d'origine)
    const Colour ledCore (0xffe6f4ff), ledMid (0xff2f86ff), ledDeep (0xff0a2a8c);
    const Colour inkCream (0xffece2be);
    const Colour panelDark (0xff0b140a);

    const char* const valueIds[] = { "loudness", "sustain", "filter", "crunch", "scoop", "active", "gate" };
    constexpr int numValueIds = 7;
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
    const Point<float> p0 = c + dir * (0.20f * R), p1 = c + dir * (0.74f * R);
    const float lw = 0.085f * R;
    // peinture un peu plus claire tournee vers la lumiere, ombre tres legere en bas a droite
    const float lightFacing = 0.5f + 0.5f * std::cos (a + MathConstants<float>::pi * 0.25f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawLine ({ p0.translated (R * 0.012f, R * 0.016f), p1.translated (R * 0.012f, R * 0.016f) }, lw);
    Path p; p.addLineSegment ({ p0, p1 }, lw);
    g.setColour (Colour::greyLevel (0.88f + 0.10f * lightFacing));
    g.strokePath (p, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
// INVERSEUR (2 positions : 0 = bas, 1 = haut)
//==============================================================================
LeverSwitch::LeverSwitch (RangedAudioParameter& p)
    : attachment (p, [this] (float v)
                  {
                      const int np = v > 0.5f ? 1 : 0;
                      if (np != pos)
                      {
                          pos = np;
                          targetFrame = pos == 0 ? 0 : 2;
                          shownFrame = 1;                              // position intermediaire ~55 ms
                          transitionUntil = Time::getMillisecondCounterHiRes() + 55.0;
                          repaint();
                      }
                  })
{
    attachment.sendInitialUpdate();
    shownFrame = targetFrame = pos == 0 ? 0 : 2;
    setMouseCursor (MouseCursor::PointingHandCursor);
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

void LeverSwitch::moveTo (int np)
{
    np = jlimit (0, 1, np);
    if (np == pos) return;
    attachment.setValueAsCompleteGesture ((float) np);
    if (onUserChange) onUserChange();
}

void LeverSwitch::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    moveTo (1 - pos);
}

void LeverSwitch::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) > 0.0f) moveTo (w.deltaY > 0 ? 1 : 0);
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
      footswitch   (*p.apvts.getParameter ("active"))
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    glowBlue   = ImageCache::getFromMemory (BinaryData::glow_blue_png,  BinaryData::glow_blue_pngSize);
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
    for (auto* l : { &crunchSwitch, &scoopSwitch })
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

    // Taille par defaut = taille minimale (impossible de reduire en dessous), proportions bloquees
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* cons = getConstrainer())
        cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);                      // anti-gel : l'hote ne redessine jamais ce qui est derriere la fenetre
    setSize (defaultW, defaultH);
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
    glowBlueS = (gpx >= glowBlue.getWidth()) ? glowBlue : glowBlue.rescaled (gpx, gpx, Graphics::highResamplingQuality);
    repaint();
}

void PickleAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici ; les limites du constrainer suffisent
    for (int i = 0; i < PickleAudioProcessor::numKnobs; ++i)
        knobs[i].setBounds (scaled ({ knobPos[i].x - knobR, knobPos[i].y - knobR, knobR * 2.0f, knobR * 2.0f }));
    auto lever = [this] (LeverSwitch& l, Point<float> c)
    {
        l.setBounds (scaled ({ c.x - togHalf, c.y - togHalf, togHalf * 2.0f, togHalf * 2.0f }));
    };
    lever (crunchSwitch, crunchPos);
    lever (scoopSwitch, scoopPos);
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
    if (glowBlueS.isValid())
    {
        g.setOpacity (level);
        g.drawImage (glowBlueS, glowRect);
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

void PickleAudioProcessorEditor::drawStatus (Graphics& g)
{
    const float a = statusAlpha;
    if (a <= 0.0f) return;
    g.setColour (panelDark.withAlpha (0.92f * a));
    g.fillRoundedRectangle (statusRect, 8.0f);
    g.setColour (inkCream.withAlpha (0.40f * a));
    g.drawRoundedRectangle (statusRect.reduced (0.75f), 9.0f, 1.3f);

    const bool longLine = lastLine1.length() > 18;
    g.setFont (Font (FontOptions (labelFace).withHeight (longLine ? 22.0f : 34.0f)));
    g.setColour (inkCream.withAlpha (a));
    g.drawFittedText (lastLine1, statusRect.reduced (10.0f, 0.0f).toNearestInt(), Justification::centred, longLine ? 2 : 1, 0.75f);
}

void PickleAudioProcessorEditor::drawPresetBar (Graphics& g)
{
    const auto r = presetRect;
    g.setColour (panelDark.withAlpha (0.82f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (inkCream.withAlpha (hoverZone != 0 ? 0.62f : 0.30f));
    g.drawRoundedRectangle (r.reduced (0.6f), r.getHeight() * 0.5f, 1.1f);

    const float aw = 34.0f;
    auto arrow = [&] (Rectangle<float> ar, bool left, bool hot)
    {
        Path p;
        const auto c = ar.getCentre();
        const float s = 6.5f;
        if (left) p.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.8f, c.y);
        else      p.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.8f, c.y);
        g.setColour (inkCream.withAlpha (hot ? 1.0f : 0.62f));
        g.fillPath (p);
    };
    arrow (r.withWidth (aw), true, hoverZone == 1);
    arrow (r.withTrimmedLeft (r.getWidth() - aw), false, hoverZone == 3);

    g.setFont (Font (FontOptions (labelFace).withHeight (23.0f)));
    g.setColour ((midiFlash > 0 ? Colour (0xff9fd0ff) : inkCream).withAlpha (hoverZone == 2 ? 1.0f : 0.88f));
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

    const float s = (float) getHeight() / (float) baseH;
    g.addTransform (AffineTransform::scale (s));
    drawLed (g);
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
    sizes.addItem (1, "60 % (default)", true, getHeight() == defaultH);
    sizes.addItem (2, "75 %",  true, getHeight() == roundToInt (baseH * 0.75f));
    sizes.addItem (3, "100 %", true, getHeight() == baseH);
    menu.addSubMenu ("Window size", sizes);
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
    midi.addItem (1201, "Reset MIDI to defaults (CC 20-25)");
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
            if      (r == 1) ed.setScaledHeight (defaultH);
            else if (r == 2) ed.setScaledHeight (roundToInt (baseH * 0.75f));
            else if (r == 3) ed.setScaledHeight (baseH);
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
