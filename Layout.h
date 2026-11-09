// genere par tools/make_art.py - ne pas modifier a la main (repere 617.5 x 900.0 = fond 1235 x 1800 px / 2)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    constexpr float frameW = 617.50f, frameH = 900.00f;   // pedale plein cadre (jacks aux bords gauche / droit)
    // ordre : loudness, sustain, filter
    const juce::Point<float> knobPos[3] = { { 165.36f, 138.25f }, { 449.26f, 136.59f }, { 308.75f, 328.64f } };
    const float knobR[3] = { 58.78f, 58.78f, 48.01f };
    const juce::Point<float> crunchPos { 145.11f, 332.58f }, scoopPos { 472.39f, 332.38f }, octavePos { 472.39f, 658.94f };
    const float togS = 85.83f, togR = 23.84f;   // sprite d'inverseur (cote) / rondelle
    const juce::Point<float> ledPos { 308.75f, 658.94f }, octLedPos { 421.54f, 658.94f }; const float ledS = 24.83f;
    const juce::Rectangle<float> glowRect { 197.51f, 547.58f, 222.47f, 222.47f }, octGlowRect { 310.27f, 547.58f, 222.47f, 222.47f };
    const juce::Point<float> fsPos { 308.75f, 769.04f }; const float footR = 36.13f, fsS = 181.19f;   // bouton / sprite
    const juce::Rectangle<float> statusRect { 108.75f, 428.81f, 400.00f, 47.19f }, presetRect { 108.75f, 484.27f, 400.00f, 32.28f };
    const float statusFont = 26.29f, presetFont = 17.70f;
    const juce::Rectangle<float> boxRect { 21.49f, 0.00f, 574.52f, 900.00f }, pickleGlowRect { 179.82f, 542.53f, 257.87f, 92.02f };
    const float jackY = 461.42f;
    // serigraphie vectorielle : texte, police (0 medium, 1 bold, 2 light), taille em, espacement, centre, rotation (degres)
    struct Txt { const char* s; int font; float em, track, x, y, rot; };
    const Txt texts[] = {
        { "LOUDNESS", 0, 17.19f, 2.22f, 165.36f, 228.87f, 0.00f },
        { "SUSTAIN", 0, 17.19f, 2.22f, 449.26f, 228.48f, 0.00f },
        { "FILTER", 0, 17.19f, 2.22f, 308.75f, 399.83f, 0.00f },
        { "CRUNCH", 0, 17.19f, 2.22f, 145.11f, 399.83f, 0.00f },
        { "SCOOP", 0, 17.19f, 2.22f, 472.39f, 399.44f, 0.00f },
        { "STRONG", 0, 22.25f, 9.10f, 308.75f, 547.15f, 0.00f },
        { "FUZZ", 0, 19.21f, 8.09f, 308.75f, 627.81f, 0.00f },
        { "FAT", 0, 12.64f, 1.62f, 472.39f, 609.27f, 0.00f },
        { "BRIGHT", 0, 12.64f, 1.62f, 472.39f, 705.63f, 0.00f },
        { "OCTAVE", 0, 12.64f, 1.62f, 532.86f, 658.94f, 0.00f },
        { "OUT", 0, 15.17f, 1.82f, 58.50f, 461.42f, 0.00f },
        { "IN", 0, 15.17f, 1.82f, 559.00f, 461.42f, 0.00f }
    };
    const Txt pickleText { "PICKLE", 1, 60.67f, 2.02f, 308.75f, 588.97f, 0.00f };   // s'allume en vert
    struct Tick { float x0, y0, x1, y1, w; };
    const Tick ticks[] = { { 132.73f, 194.75f, 129.06f, 201.11f, 2.12f }, { 108.85f, 170.87f, 102.49f, 174.54f, 2.12f }, { 100.11f, 138.25f, 92.77f, 138.25f, 2.12f }, { 108.85f, 105.63f, 102.49f, 101.95f, 2.12f }, { 132.73f, 81.75f, 129.06f, 75.38f, 2.12f }, { 165.36f, 73.00f, 165.36f, 65.66f, 2.12f }, { 197.98f, 81.75f, 201.65f, 75.38f, 2.12f }, { 221.86f, 105.63f, 228.22f, 101.95f, 2.12f }, { 230.60f, 138.25f, 237.95f, 138.25f, 2.12f }, { 221.86f, 170.87f, 228.22f, 174.54f, 2.12f }, { 197.98f, 194.75f, 201.65f, 201.11f, 2.12f }, { 416.63f, 193.09f, 412.96f, 199.46f, 2.12f }, { 392.75f, 169.21f, 386.39f, 172.88f, 2.12f }, { 384.01f, 136.59f, 376.67f, 136.59f, 2.12f }, { 392.75f, 103.97f, 386.39f, 100.29f, 2.12f }, { 416.63f, 80.09f, 412.96f, 73.72f, 2.12f }, { 449.26f, 71.35f, 449.26f, 64.00f, 2.12f }, { 481.88f, 80.09f, 485.55f, 73.72f, 2.12f }, { 505.76f, 103.97f, 512.12f, 100.29f, 2.12f }, { 514.50f, 136.59f, 521.85f, 136.59f, 2.12f }, { 505.76f, 169.21f, 512.12f, 172.88f, 2.12f }, { 481.88f, 193.09f, 485.55f, 199.46f, 2.12f }, { 282.10f, 374.79f, 279.10f, 379.99f, 1.73f }, { 262.60f, 355.28f, 257.40f, 358.29f, 1.73f }, { 255.46f, 328.64f, 249.46f, 328.64f, 1.73f }, { 262.60f, 301.99f, 257.40f, 298.99f, 1.73f }, { 282.10f, 282.49f, 279.10f, 277.29f, 1.73f }, { 308.75f, 275.35f, 308.75f, 269.35f, 1.73f }, { 335.40f, 282.49f, 338.40f, 277.29f, 1.73f }, { 354.90f, 301.99f, 360.10f, 298.99f, 1.73f }, { 362.04f, 328.64f, 368.04f, 328.64f, 1.73f }, { 354.90f, 355.28f, 360.10f, 358.29f, 1.73f }, { 335.40f, 374.79f, 338.40f, 379.99f, 1.73f } };
    const juce::Point<float> logoPos { 308.75f, 877.05f }; const float logoW = 192.13f;
}
