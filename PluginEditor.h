#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "Layout.h"
#include "NineHandsMenu.h"

//==============================================================================
// Potards (photo du kit, dans le fond : reflets fixes) ; seul le repere creme peint tourne.
//==============================================================================
class PickleKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
};

class PickleKnob : public juce::Slider
{
public:
    bool hitTest (int x, int y) override;
};

//==============================================================================
// Inverseur a levier (photos du kit : haut / milieu / bas, recalees sur la rondelle -> aucun decalage).
// 2 positions : image du milieu ~55 ms pendant la bascule. 3 positions (OCTAVE : haut Fat, milieu off, bas Bright) :
// passage haut <-> bas par le milieu ~55 ms. Les trois images sont dessinees exactement dans le meme rectangle.
//==============================================================================
class LeverSwitch : public juce::Component
{
public:
    explicit LeverSwitch (juce::RangedAudioParameter& p, bool threePos = false);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void tick (double nowMs);
    juce::Image strip;                       // haut / milieu / bas empiles (carres)
    std::function<void()> onRightClick, onUserChange;

private:
    int frameOf (float v) const;
    float valueOf (int frame) const;
    void moveTo (int newFrame);
    const bool three;
    juce::ParameterAttachment attachment;
    int frame = 0, shownFrame = 0, targetFrame = 0;     // positions : 0 = bas, 1 = milieu, 2 = haut
    double transitionUntil = 0.0;
};

//==============================================================================
// Footswitch (photo du kit, dans le fond) : fonctionnement d'Echoflex ; reflet jaune de la LED en fondu.
//==============================================================================
class FootSwitchArea : public juce::Component
{
public:
    explicit FootSwitchArea (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    std::function<void()> onRightClick, onUserChange;
    juce::Image reflection;
    float ledLevel = 0.0f;
private:
    bool value = false, pressed = false;
    juce::ParameterAttachment attachment;
};

struct RightClickForwarder : public juce::MouseListener
{
    std::function<void (juce::Component*)> callback;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && callback) callback (e.eventComponent); }
};

//==============================================================================
class PickleAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit PickleAudioProcessorEditor (PickleAudioProcessor&);
    ~PickleAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // Repere du fond (pedale plein cadre, image 1235 x 1800 px / 2). Hauteurs de fenetre des pedales simples :
    // taille par defaut = taille minimale 493 px (« 60 % »), 100 % = 958 px, jusqu'a 150 %.
    static constexpr float baseW = layout::frameW;
    static constexpr float baseH = layout::frameH;
    static constexpr int defaultH = 493;
    static constexpr int refH = 958;
    static constexpr int maxH = refH * 3 / 2;

    void setScaledHeight (int h);
    void forceRenderScale (float sc) { forcedScale = sc; rebuildCaches (sc); }   // captures (outil de rendu)
    static constexpr int numSizes = 3;          // 60 % (defaut) / 75 % / 100 %
    static int sizeHeight (int idx);
    void showStatus();
    void openMainMenu() { showContextMenu(); }  // outil de rendu (capture du menu)

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void visibilityChanged() override;
    void askRepaint (juce::Rectangle<int> r);
    void showContextMenu (int learnIdx = -1);
    void handleMidiMenu (int r);
    void showPresetMenu();
    void stepPreset (int delta);
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    void buildPrint();
    void drawPrint (juce::Graphics&);
    void drawGlows (juce::Graphics&);
    void drawLeds (juce::Graphics&);
    void drawStatus (juce::Graphics&);
    void drawPresetBar (juce::Graphics&);

    PickleAudioProcessor& proc;
    PickleKnobLookAndFeel lnf;
    NineHandsMenu::MenuLnf menuLnf;
    RightClickForwarder rightClickFwd;

    juce::Image background, bgScaled, glow, glowS, glowOct, glowOctS, pickleGlow, ledOn;
    int cacheW = 0, cacheH = 0;
    float cacheScale = 0.0f, forcedScale = 0.0f;
    int resizeSettle = 0;
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;
    juce::Typeface::Ptr labelFace, boldFace, lightFace, regularFace;
    juce::Path printPath, picklePath;      // serigraphie (unites du plugin) ; PICKLE a part (s'allume en vert)
    std::unique_ptr<juce::Drawable> logo;

    PickleKnob knobs[PickleAudioProcessor::numKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt[PickleAudioProcessor::numKnobs];
    LeverSwitch crunchSwitch, scoopSwitch, octaveSwitch;
    FootSwitchArea footswitch;

    float ledLevel = 0.0f, drawnLed = -1.0f;
    float octLedLevel = 0.0f, drawnOctLed = -1.0f;
    int lastChanged = -1;
    float statusAlpha = 0.0f, drawnStatus = -1.0f;
    double statusUntil = 0.0;
    juce::String lastLine1, lastPreset;
    juce::String learnLine;
    double learnLineUntil = 0.0;
    float lastValues[16];
    int lastPresetCounter = 0, lastMidiCount = 0, midiFlash = 0;
    int hoverZone = 0, drawnHover = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PickleAudioProcessorEditor)
};
