// genere par tools/make_background.py - ne pas modifier a la main (repere 760 x 958)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    const juce::Point<float> knobPos[3] = { { 255.83f, 161.67f }, { 501.67f, 160.00f }, { 380.00f, 353.33f } };
    const float knobR[3] = { 59.17f, 59.17f, 48.33f };      // rayon du moletage
    const juce::Point<float> crunchPos { 237.92f, 357.08f }, scoopPos { 521.67f, 356.67f };
    const float togHalf = 26.00f;
    const juce::Point<float> ledPos { 379.58f, 685.83f }; const float ledR = 6.60f;
    const juce::Point<float> footPos { 376.67f, 796.67f }; const float footR = 45.00f;
    const juce::Rectangle<float> glowRect { 251.58f, 557.83f, 256.00f, 256.00f };
    const juce::Rectangle<float> pickleRect { 261.50f, 566.50f, 240.00f, 96.50f };    // calque « PICKLE » allume (vert)
    const juce::Rectangle<float> statusRect { 195.00f, 454.17f, 371.67f, 47.50f };
    const juce::Rectangle<float> presetRect { 185.00f, 510.00f, 390.83f, 32.50f };
}
