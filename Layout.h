// genere par tools/make_background.py - ne pas modifier a la main (repere 760 x 958)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    const juce::Point<float> knobPos[3] = { { 262.00f, 168.00f }, { 498.00f, 168.00f }, { 380.00f, 350.00f } };
    const float knobR = 46.00f;
    const juce::Point<float> crunchPos { 240.00f, 352.00f }, scoopPos { 520.00f, 352.00f };
    const float togHalf = 40.75f;
    const juce::Point<float> ledPos { 380.00f, 690.00f }; const float ledR = 7.60f;
    const juce::Point<float> footPos { 380.00f, 800.00f }; const float footR = 53.61f;
    const juce::Rectangle<float> glowRect { 252.00f, 578.00f, 256.00f, 256.00f };
    const juce::Rectangle<float> statusRect { 196.00f, 448.00f, 368.00f, 48.00f };
    const juce::Rectangle<float> presetRect { 186.00f, 502.00f, 388.00f, 34.00f };
}
