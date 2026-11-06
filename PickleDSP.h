#pragma once
//==============================================================================
// STRONG PICKLE (NINE HANDS | DSP) - moteur audio, sans dependance JUCE (testable hors plugin).
//
// Reproduction de la Way Huge Swollen Pickle Jumbo Fuzz MkIII (WM41, « Smalls »),
// une fuzz de la famille Big Muff (4 etages a transistors + tone stack passif) :
//
// v1.1 : modele du circuit au niveau des composants (PickleCircuit.h), resolu a chaque echantillon :
//
//  IN -> [sur-echantillonnage x4]
//     -> Q1 etage d'entree (emetteur commun, ~ +20 dB, Miller 33 pF)
//     -> SUSTAIN (diviseur : 18 a 62 dB de gain au total, fiche Way Huge)
//     -> Q2, Q3 etages d'ecretage : 2N3904, contre-reaction 470 k, Miller 330 pF (v1.2.1, plus de definition),
//        diodes 1N914 tete-beche en serie avec 1 uF (collecteur -> base) ;
//        liaisons 150 / 100 nF sur 10 k (v1.2.1) : le grave est retire avant l'ecretage (fuzz definie, pas de bave)
//          CRUNCH : bas = diodes directes (Swollen Pickle classique) ; haut = 8,2 k en serie avec les diodes :
//                   l'etage monte jusqu'a ses rails, fuzz plus ouverte, plus granuleuse et plus forte
//     -> tone stack passif FILTER (39 k / 10 nF passe-bas, 4 nF / 22 k passe-haut, pot 100 k)
//          SCOOP : bas = valeurs classiques (creux d'environ 6 dB) ; haut = creux plus profond et plus large
//     -> Q4 etage de sortie -> [decimation] -> LOUDNESS -> OUT
//
// v1.3 : OCTAVE (inverseur ajoute, d'apres la Fulltone Ultimate Octave / Foxx Tone Machine) :
//     entre Q1 et SUSTAIN, un dephaseur (collecteur / emetteur, gains 1 et 0,78 : desequilibre comme
//     sur un vrai circuit, une part du fondamental reste) attaque deux diodes Schottky sur une charge commune : redressement double alternance
//     -> la frequence double (octave au-dessus), puis la fuzz l'ecrete. Calcule a 4x (le redressement cree
//     des harmoniques aigues). Seuil des diodes : fins de notes crachotantes, typiques de ces pedales.
//     v1.4 : inverseur 3 positions comme l'Ultimate Octave : haut = Fat (liaison 15 nF), milieu = off,
//     bas = Bright (liaison 1 nF : octave plus fine et plus percante).
//
// Le grain, la compression, la dissymetrie et les crachotements de fin de note viennent du circuit
// (polarisation des transistors, charge des condensateurs de 1 uF des diodes), sans courbe ajoutee.
//==============================================================================

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

namespace pickle
{
    constexpr double pi = 3.14159265358979323846;

    inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

    //--------------------------------------------------------------------------
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        {
            b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
        }
        void lowpass (double sr, double fc, double q = 0.7071)
        {
            fc = std::min (std::max (fc, 10.0), sr * 0.45);
            const double w = 2 * pi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void highpass (double sr, double fc, double q = 0.7071)
        {
            fc = std::min (std::max (fc, 5.0), sr * 0.45);
            const double w = 2 * pi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void peak (double sr, double fc, double q, double db)
        {
            fc = std::min (std::max (fc, 10.0), sr * 0.45);
            const double A = std::pow (10.0, db / 40.0);
            const double w = 2 * pi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void lowShelf (double sr, double fc, double db, double s = 0.8)
        {
            fc = std::min (std::max (fc, 10.0), sr * 0.45);
            const double A = std::pow (10.0, db / 40.0);
            const double w = 2 * pi * fc / sr, c = std::cos (w), sn = std::sin (w);
            const double al = sn / 2 * std::sqrt ((A + 1 / A) * (1 / s - 1) + 2), sa = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                 (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        }
        void highShelf (double sr, double fc, double db, double s = 0.8)
        {
            fc = std::min (std::max (fc, 10.0), sr * 0.45);
            const double A = std::pow (10.0, db / 40.0);
            const double w = 2 * pi * fc / sr, c = std::cos (w), sn = std::sin (w);
            const double al = sn / 2 * std::sqrt ((A + 1 / A) * (1 / s - 1) + 2), sa = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                 (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
        }
        inline float process (float xf) noexcept
        {
            const double x = xf;
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
        void reset() { z1 = z2 = 0; }
    };

    struct OnePole   // passe-bas / passe-haut du 1er ordre
    {
        float a = 0, z = 0;
        void setLP (double sr, double fc) { a = (float) std::exp (-2 * pi * fc / sr); }
        inline float lp (float x) noexcept { z = x + a * (z - x); return z; }
        inline float hp (float x) noexcept { z = x + a * (z - x); return x - z; }
        void reset() { z = 0; }
    };

    // Lissage a 2 poles (deux 1er ordre en cascade) : pas de marche d'escalier
    struct Smooth2
    {
        float c = 0.01f, s1 = 0, s2 = 0;
        void setTime (double sr, double ms) { c = (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr))); }
        void reset (float v) { s1 = s2 = v; }
        inline float next (float target) noexcept { s1 += (target - s1) * c; s2 += (s1 - s2) * c; return s2; }
        bool settled (float target) const { return std::abs (s2 - target) < 1.0e-5f && std::abs (s1 - target) < 1.0e-5f; }
    };

    //--------------------------------------------------------------------------
    // Sur-echantillonnage x4 : filtre RIF a phase lineaire (sinc fenetre de Kaiser)
    // en polyphase pour la montee et la descente.
    struct Oversampler4
    {
        static constexpr int factor = 4;
        static constexpr int taps = 128;          // multiple de 4
        float h[taps] {};
        float hist[taps * 2] {};                  // ligne a retard (entree x4) doublee pour lecture contigue
        int   pos = 0;
        float upHist[taps / factor * 2] {};
        int   upPos = 0;

        static double bessel0 (double x)
        {
            double s = 1, t = 1;
            for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
            return s;
        }

        void design()
        {
            const double fc = 0.5 / factor * 0.90;   // coupure ~ 0,45 x Fs d'origine
            const double beta = 8.6;
            double sum = 0;
            for (int n = 0; n < taps; ++n)
            {
                const double m = n - (taps - 1) * 0.5;
                const double x = 2 * fc * m;
                const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double r = 2.0 * n / (taps - 1) - 1.0;
                const double w = bessel0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / bessel0 (beta);
                h[n] = (float) (2 * fc * sinc * w);
                sum += h[n];
            }
            for (auto& v : h) v = (float) (v / sum);
            reset();
        }
        void reset()
        {
            std::fill (std::begin (hist), std::end (hist), 0.0f);
            std::fill (std::begin (upHist), std::end (upHist), 0.0f);
            pos = upPos = 0;
        }
        // latence totale (montee + descente) en echantillons a la frequence d'origine
        static double latency() { return (taps - 1) / (double) factor; }

        // 1 echantillon -> 4 echantillons
        inline void up (float x, float* out) noexcept
        {
            constexpr int L = taps / factor;
            upPos = (upPos + L - 1) % L;
            upHist[upPos] = x; upHist[upPos + L] = x;
            const float* hp = upHist + upPos;
            for (int ph = 0; ph < factor; ++ph)
            {
                float acc = 0;
                for (int k = 0; k < L; ++k)
                    acc += h[k * factor + ph] * hp[k];
                out[ph] = acc * factor;
            }
        }
        // 4 echantillons -> 1 echantillon
        inline float down (const float* in) noexcept
        {
            for (int i = 0; i < factor; ++i)
            {
                pos = (pos + taps - 1) % taps;
                hist[pos] = in[i]; hist[pos + taps] = in[i];
            }
            const float* hp = hist + pos;
            float acc = 0;
            for (int k = 0; k < taps; ++k)
                acc += h[k] * hp[k];
            return acc;
        }
    };

    //--------------------------------------------------------------------------

    //--------------------------------------------------------------------------
}   // namespace pickle (outils)

#include "PickleCircuit.h"
namespace pickle
{
    struct Params
    {
        float loudness = 5.0f, sustain = 5.0f, filter = 5.0f;   // 0..10 comme sur la pedale
        bool  crunch = false, scoop = false;                    // inverseurs : false = bas (Swollen Pickle classique)
        int   octave = 0;                                       // ajout Strong Pickle : 0 off, 1 Fat, 2 Bright (Fulltone Ultimate Octave)
        bool  active = true;
        int   gate = 0;                                         // ajout Strong Pickle : 0 = off (defaut), 1 leger, 2 moyen, 3 fort
    };

    // valeurs des composants
    inline StageValues boosterValues() { return { 82e3, 1e-6, 1.8e6, 33e-12, 15e3, 470, false, 1e-6, 9.0 }; }
    inline StageValues clipValues (double cin) { return { 10e3, cin, 470e3, 330e-12, 15e3, 150, true, 1e-6, 9.0 }; }
    inline StageValues outputValues() { return { 180e3, 100e-9, 1.8e6, 22e-12, 15e3, 1000, false, 1e-6, 9.0 }; }

    //--------------------------------------------------------------------------
    class Channel
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate; srO = sr * Oversampler4::factor;
            os.design();
            q1.prepare (boosterValues(), sr);          // etage d'entree : presque lineaire, a la frequence du projet
            q2.prepare (clipValues (150e-9), srO);
            q3.prepare (clipValues (100e-9), srO);
            q4.prepare (outputValues(), sr);           // etage de sortie : idem, apres la decimation
            tone.prepare (srO);
            dc1.setLP (sr, 4.0); dc3.setLP (srO, 4.0); dcOut.setLP (sr, 6.0);
            dcOct.setLP (srO, 12.0); octLp.lowpass (srO, 9000.0, 0.7);
            aa.lowpass (sr, 14000.0, 0.6);
            // le circuit part de son point de repos : les composantes continues sont retirees des le debut
            dc1.z = (float) q1.collector(); dc3.z = (float) q3.collector();
            q4c = q4.collector();
            os.reset();
        }

        void setCrunch (bool on)
        {
            const double r = on ? 8.2e3 : 1.0;
            q2.rs = r; q3.rs = r;
        }
        void setTone (double pos, bool scoop)
        {
            if (scoop) { tone.rLp = 39e3; tone.cLp = 22e-9; tone.cHp = 2.2e-9; tone.rHp = 22e3; }
            else       { tone.rLp = 39e3; tone.cLp = 10e-9; tone.cHp = 4e-9;   tone.rHp = 22e3; }
            tone.setPosition (pos);
        }
        // OCTAVE : 0 off, 1 Fat, 2 Bright. Inverseur Fat / Bright de l'Ultimate Octave : condensateur de liaison
        // apres le redresseur 15 nF (Fat) ou 1 nF (Bright) sur ~100 k -> passe-haut a ~106 Hz ou ~1,6 kHz.
        void setOctave (int mode)
        {
            octave = mode > 0;
            dcOct.setLP (srO, mode == 2 ? 1.0 / (2 * pi * 100e3 * 1e-9) : 1.0 / (2 * pi * 100e3 * 15e-9));
            dcOct.z = 0.0f; octLp.z1 = octLp.z2 = 0.0;
        }

        // une diode Schottky sur la charge commune : demi-alternance au genou doux (k)
        static inline float diodeHalf (float u) noexcept
        {
            constexpr float k = 0.05f;
            return 0.5f * (u + std::sqrt (u * u + k * k)) - 0.5f * k;
        }
        // redresseur double alternance : dephaseur + 2 diodes Schottky (genou doux, seuil vf)
        static inline float octaveUp (float v) noexcept
        {
            constexpr float g1 = 1.0f, g2 = 0.78f, vf = 0.10f;
            return diodeHalf (g1 * v - vf) + diodeHalf (-g2 * v - vf);
        }

        // un echantillon : x en volts (guitare ~ 0,1 a 0,5 V crete) ; atten : SUSTAIN (lineaire)
        inline float process (float x, float atten) noexcept
        {
            const double v1 = q1.process (x);
            float up[Oversampler4::factor];
            if (! octave)
            {
                os.up (dc1.hp ((float) v1) * atten, up);              // SUSTAIN (diviseur apres liaison)
            }
            else
            {
                os.up (dc1.hp ((float) v1), up);
                for (int i = 0; i < Oversampler4::factor; ++i)       // OCTAVE : redressement, liaison, SUSTAIN
                    up[i] = (float) octLp.process (dcOct.hp (octaveUp (up[i]))) * (octGain * atten);
            }
            for (int i = 0; i < Oversampler4::factor; ++i)
            {
                const double v2 = q2.process (up[i]);
                const double v3 = q3.process (v2);
                up[i] = (float) tone.process (dc3.hp ((float) v3));
            }
            const float t = os.down (up);
            const double v4 = q4.process (aa.process (t));
            return dcOut.hp ((float) (v4 - q4c));
        }

    private:
        double sr = 48000.0, srO = 192000.0, q4c = 4.5;
        bool octave = false;
        static constexpr float octGain = 2.4f;     // le redressement perd du niveau : remis au niveau de la fuzz sans octave
        OnePole dcOct;
        Biquad octLp;
        Oversampler4 os;
        TransistorStage q1, q2, q3, q4;
        ToneStack tone;
        OnePole dc1, dc3, dcOut;
        Biquad aa;
    };

    //--------------------------------------------------------------------------
    class Engine
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            nCh = std::min (maxChannels, std::max (1, numChannels));
            for (auto& c : ch) c.prepare (sr);
            for (auto* s : { &sLoud, &sSus, &sFil })
                s->setTime (sr, 14.0);
            sActive.setTime (sr, 3.0);
            sVoice.setTime (sr, 4.0);
            first = true;
            counter = 0;
            gateDetHP.highpass (sr, 110.0, 0.707);
            gateEnv = 0.0f; gateGain = 1.0f; gateOpen = true; gateHold = 0;
            gateRelEnv = (float) std::exp (-1.0 / (0.030 * sr));
            gateAtt    = (float) (1.0 - std::exp (-1.0 / (0.0008 * sr)));
            gateRel    = (float) (1.0 - std::exp (-1.0 / (0.150 * sr)));
            gateHoldLen = (int) (0.080 * sr);
            std::memset (dryLine, 0, sizeof (dryLine));
            dryPos = 0;
            dryDelay = latencyInt();
        }

        static double latencySamples() { return Oversampler4::latency(); }
        static int latencyInt() { return (int) std::lround (Oversampler4::latency()); }

        // SUSTAIN : diviseur devant le 1er etage d'ecretage ; gain total 18 dB (mini) a 62 dB (maxi)
        static float sustainDb (float knob)
        {
            const float s = clampf (knob / 10.0f, 0.0f, 1.0f);
            return -46.0f + 44.0f * (1.0f - std::pow (1.0f - s, 1.8f));
        }
        // LOUDNESS : 0 = coupe, enorme reserve de volume en haut de course
        static float loudnessDb (float knob)
        {
            const float l = clampf (knob / 10.0f, 0.0f, 1.0f);
            return l <= 0.0f ? -90.0f : -60.0f + 72.0f * std::pow (l, 0.45f);
        }
        // FILTER : position du curseur (potentiometre lineaire)
        static float filterPos (float knob) { return clampf (knob / 10.0f, 0.0f, 1.0f); }

        void process (float* const* io, int numSamples, const Params& p)
        {
            if (first)
            {
                sLoud.reset (p.loudness); sSus.reset (p.sustain); sFil.reset (p.filter);
                sActive.reset (p.active ? 1.0f : 0.0f); sVoice.reset (1.0f);
                curCrunch = p.crunch; curScoop = p.scoop; curOct = p.octave; first = false;
                for (auto& c : ch) { c.setCrunch (curCrunch); c.setTone (filterPos (p.filter), curScoop); c.setOctave (curOct); }
            }

            for (int i = 0; i < numSamples; ++i)
            {
                // inverseurs : court fondu (le circuit change d'un coup, comme un vrai inverseur, sans clic)
                if (p.crunch != curCrunch || p.scoop != curScoop || p.octave != curOct)
                {
                    const float m = sVoice.next (0.0f);
                    if (m < 0.002f)
                    {
                        curCrunch = p.crunch; curScoop = p.scoop; curOct = p.octave; sVoice.reset (0.0f);
                        for (auto& c : ch) { c.setCrunch (curCrunch); c.setTone (filterPos (sFil.s2), curScoop); c.setOctave (curOct); }
                    }
                }
                else
                    sVoice.next (1.0f);

                const float ld  = sLoud.next (p.loudness);
                const float su  = sSus.next (p.sustain);
                const float fi  = sFil.next (p.filter);
                const float act = sActive.next (p.active ? 1.0f : 0.0f);
                if ((counter++ & 15) == 0)
                    for (int c = 0; c < nCh; ++c) ch[c].setTone (filterPos (fi), curScoop);

                const float atten = dbToGain (sustainDb (su));
                const float out = dbToGain (loudnessDb (ld) + outTrim + (curCrunch ? crunchTrim : 0.0f) + (curOct == 1 ? octTrim : (curOct == 2 ? octTrimBright : 0.0f))) * sVoice.s2;

                float gg = 1.0f;
                if (p.gate > 0)
                {
                    const float mono = nCh > 1 ? 0.5f * (io[0][i] + io[1][i]) : io[0][i];
                    const float d = std::abs ((float) gateDetHP.process (mono));
                    gateEnv = std::max (d, gateEnv * gateRelEnv);
                    static const float openDb[4] = { -200.0f, -74.0f, -66.0f, -56.0f };
                    const float thrOpen = dbToGain (openDb[std::min (3, p.gate)]), thrClose = thrOpen * 0.5f;
                    if (gateEnv > thrOpen)       { gateOpen = true; gateHold = gateHoldLen; }
                    else if (gateEnv < thrClose) { if (gateHold > 0) --gateHold; else gateOpen = false; }
                    const float tg = gateOpen ? 1.0f : 0.0f;
                    gateGain += (tg - gateGain) * (tg > gateGain ? gateAtt : gateRel);
                    gg = gateGain * gateGain;
                }
                else { gateGain = 1.0f; gateOpen = true; }

                for (int c = 0; c < nCh; ++c)
                {
                    const float x = io[c][i];
                    const float wet = ch[c].process (x, atten) * out * gg;
                    float* dl = dryLine[c];
                    dl[dryPos] = x;
                    const float dry = dl[(dryPos + dryLen - dryDelay) % dryLen];
                    io[c][i] = safety (dry + (wet - dry) * act);
                }
                dryPos = (dryPos + 1) % dryLen;
            }
        }

        float outTrim = -3.0f;      // calibrage : SUSTAIN / FILTER / LOUDNESS a midi ~ +4,5 dB au-dessus du bypass (tests/test_dsp.cpp)
        float crunchTrim = 0.0f;    // CRUNCH haut : le circuit est naturellement plus fort
        float octTrim = 1.5f;       // OCTAVE Fat : meme volume percu qu'en fuzz seule (tests)
        float octTrimBright = 1.5f; // OCTAVE Bright

    private:
        static inline float safety (float x) noexcept
        {
            const float ax = std::abs (x);
            if (ax < 0.95f) return x;
            return std::copysign (0.95f + 0.05f * std::tanh ((ax - 0.95f) / 0.05f), x);
        }

        float gateEnv = 0.0f, gateGain = 1.0f, gateRelEnv = 0.999f, gateAtt = 0.03f, gateRel = 0.0002f;
        bool  gateOpen = true; int gateHold = 0, gateHoldLen = 3840;
        Biquad gateDetHP;

        double sr = 48000.0;
        int nCh = 2;
        static constexpr int dryLen = 64;
        float dryLine[maxChannels][dryLen] {};
        int dryPos = 0, dryDelay = 32;
        Channel ch[maxChannels];
        Smooth2 sLoud, sSus, sFil, sActive, sVoice;
        bool curCrunch = false, curScoop = false; int curOct = 0;
        bool first = true;
        unsigned counter = 0;
    };
}
