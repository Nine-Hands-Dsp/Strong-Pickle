#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Potards : le chapeau noir (et ses reflets) fait partie du fond et reste fixe ;
// seul le repere blanc tourne.
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
// Inverseur a levier du kit Studio Wiga (3 images : bas, intermediaire ~55 ms, haut)
//==============================================================================
class LeverSwitch : public juce::Component
{
public:
    explicit LeverSwitch (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void tick (double nowMs);
    juce::Image strip;
    std::function<void()> onRightClick, onUserChange;

private:
    void moveTo (int newPos);
    juce::ParameterAttachment attachment;
    int pos = 0, shownFrame = 0, targetFrame = 0;
    double transitionUntil = 0.0;
};

//==============================================================================
// Footswitch (reference Echoflex) : bascule a l'appui, bouton assombri tant qu'on appuie.
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

private:
    bool value = false, pressed = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
struct RightClickForwarder : public juce::MouseListener
{
    std::function<void (juce::Component*)> callback;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && callback) callback (e.eventComponent);
    }
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

    // Repere de reference (100 %) et taille par defaut = taille minimale (identique a Echoflex / Doorn)
    static constexpr int baseW = 760;
    static constexpr int baseH = 958;
    static constexpr int defaultH = 493;

    void setScaledHeight (int h);
    static constexpr int numSizes = 3;          // 60 % (defaut d'origine) / 75 % / 100 %
    static int sizeHeight (int idx);
    void showStatus();

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void showContextMenu (int learnIdx = -1);
    void handleMidiMenu (int r);
    void showPresetMenu();
    void stepPreset (int delta);
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    void drawLed (juce::Graphics&);
    void drawStatus (juce::Graphics&);
    void drawPresetBar (juce::Graphics&);

    PickleAudioProcessor& proc;
    PickleKnobLookAndFeel lnf;
    RightClickForwarder rightClickFwd;

    juce::Image background, bgScaled, glowYellow, glowYellowS, pickleGlow, pickleGlowS, backgroundLit, bgLitScaled;
    int cacheW = 0, cacheH = 0;
    float cacheScale = 0.0f;
    int resizeSettle = 0;
    // anti-gel v2 : suivi de la fenetre hote, de l'echelle et des redessins
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;
    void askRepaint (juce::Rectangle<int> r);
    void visibilityChanged() override;
    juce::Typeface::Ptr labelFace;

    PickleKnob knobs[PickleAudioProcessor::numKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt[PickleAudioProcessor::numKnobs];
    LeverSwitch crunchSwitch, scoopSwitch;
    FootSwitchArea footswitch;

    float ledLevel = 0.0f, drawnLed = -1.0f;
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
