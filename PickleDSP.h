#pragma once
//==============================================================================
// STRONG PICKLE (Studio Wiga) - moteur audio, sans dependance JUCE (testable hors plugin).
//
// Reproduction de la Way Huge Swollen Pickle Jumbo Fuzz MkIII (WM41, « Smalls »),
// une fuzz de la famille Big Muff (4 etages a transistors + tone stack passif) :
//
//  IN -> etage d'entree (emetteur commun, ~ +12 dB, saturation douce asymetrique)
//     -> [sur-echantillonnage x4]
//     -> etage d'ecretage 1 : SUSTAIN regle le gain (18 a 62 dB au total, fiche Way Huge),
//                             diodes en contre-reaction (compression logarithmique),
//                             condensateur de contre-reaction (coupe les aigus avant l'ecretage),
//                             charge du condensateur de liaison par la jonction base-emetteur
//                             (decalage de polarisation : « sputter » en fin de note)
//     -> etage d'ecretage 2 : idem, gain fixe
//          CRUNCH : bascule l'intensite de compression : bas = classique (diodes franches,
//                   fuzz lisse et compressee), haut = diodes freinees (resistance serie) :
//                   fuzz plus ouverte, plus granuleuse (l'etage atteint ses rails), plus forte
//     -> [decimation]
//     -> tone stack FILTER (passe-bas / passe-haut melanges : sombre et gras <-> brillant et fin,
//        creux des mediums au centre)
//          SCOOP : bas = voix classique (creux modere), haut = creux des mediums plus profond
//     -> etage de recuperation -> LOUDNESS (enorme reserve de volume) -> OUT
//
// Graves : condensateurs de liaison plus gros qu'une Big Muff (le « Jumbo » de la Pickle).
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
    struct Params
    {
        float loudness = 5.0f, sustain = 5.0f, filter = 5.0f;   // 0..10 comme sur la pedale
        bool  crunch = false, scoop = false;                    // inverseurs : false = bas (Swollen Pickle classique)
        bool  active = true;
        int   gate = 0;                                         // ajout Strong Pickle : 0 = off (defaut), 1 leger, 2 moyen, 3 fort
    };

    // Reglages du circuit (unites : volts, la guitare arrive autour de 0,1 a 0,5 V crete)
    struct Voice
    {
        float vd, rs, railP, railN;    // diodes (echelle du coude), resistance serie (CRUNCH), rails du transistor
        float kc;                      // vitesse de charge du condensateur de liaison (sputter)
    };
    inline const Voice& stageVoice (bool crunch)
    {
        static const Voice v[2] =
        {
            { 0.105f, 0.000f, 2.6f, 2.1f, 1.0f },    // CRUNCH bas : classique, diodes franches, compression forte
            { 0.130f, 0.300f, 1.6f, 1.2f, 1.0f },    // CRUNCH haut : diodes freinees, l'etage va jusqu'a ses rails
        };
        return v[crunch ? 1 : 0];
    }

    // etage d'ecretage a transistor (emetteur commun, diodes tete-beche en contre-reaction)
    struct ClipStage
    {
        OnePole fbLP;        // condensateur de contre-reaction : moins de gain dans les aigus
        OnePole couple;      // condensateur de liaison vers l'etage suivant (gros : graves « jumbo »)
        float vc = 0.0f;     // charge du condensateur d'entree (rectification base-emetteur)
        float kc = 0.0f, leak = 1.0f;
        float fbFloor = 0.15f;

        void prepare (double srO, double fbHz, double coupleHz, double chargeMs, double leakMs, float floor)
        {
            fbLP.setLP (srO, fbHz);
            couple.setLP (srO, coupleHz);
            kc   = (float) (1.0 - std::exp (-1.0 / (chargeMs * 0.001 * srO)));
            leak = (float) std::exp (-1.0 / (leakMs * 0.001 * srO));
            fbFloor = floor;
            reset();
        }
        void reset() { fbLP.reset(); couple.reset(); vc = 0.0f; }

        // x : tension sur la base ; g : gain lineaire de l'etage ; sp : intensite du « sputter » (0..1)
        inline float process (float x, float g, const Voice& v, float sp) noexcept
        {
            // la jonction base-emetteur conduit sur les cretes positives et charge le condensateur d'entree :
            // le point de repos glisse, la fin des notes « crachote » (sputter) avant de se couper
            float e = x - vc;
            constexpr float vbe = 0.012f;
            if (e > vbe) vc += (e - vbe) * kc * v.kc * sp;
            vc *= leak;
            e = x - vc;

            // gain (plus faible dans les aigus : condensateur de contre-reaction)
            const float lp = fbLP.lp (e);
            const float u = g * (lp + (e - lp) * fbFloor);

            // diodes en contre-reaction : compression logarithmique (asinh) ; CRUNCH ajoute une resistance serie
            float y = v.vd * std::asinh (u / v.vd) + v.rs * u;
            // rails du transistor (asymetriques)
            y = y >= 0.0f ? v.railP * std::tanh (y / v.railP) : v.railN * std::tanh (y / v.railN);
            // etage inverseur, puis condensateur de liaison
            return couple.hp (-y);
        }
    };

    //--------------------------------------------------------------------------
    class Channel
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate; srO = sr * Oversampler4::factor;
            os.design();
            inCouple.setLP (sr, 22.0);              // condensateur d'entree (gros : graves pleins)
            inLP.lowpass (sr, 9000.0, 0.6);         // etage d'entree : bande passante
            st1.prepare (srO, 2200.0, 18.0, 0.9, 320.0, 0.30f);
            st2.prepare (srO, 2800.0, 18.0, 1.2, 200.0, 0.32f);
            post1.lowpass (srO, 9000.0, 0.62);
            post2.lowpass (srO, 11000.0, 0.55);
            recCouple.setLP (sr, 16.0);
            outDC.setLP (sr, 8.0);
            outFizz.lowpass (sr, 11000.0, 0.6);
            jumbo.peak (sr, 95.0, 0.8, 2.5);        // « jumbo » : grave plus gros qu'une Big Muff
            reset();
        }
        void reset()
        {
            os.reset(); st1.reset(); st2.reset();
            for (auto* b : { &inLP, &post1, &post2, &toneBP, &outFizz, &jumbo }) b->reset();
            inCouple.reset(); toneLP.reset(); toneHP.reset(); recCouple.reset(); outDC.reset();
        }

        void updateTone (float filter01, float midFill)
        {
            toneLP.setLP (sr, 470.0);               // branche passe-bas du tone stack
            toneHP.setLP (sr, 1150.0);              // branche passe-haut
            t = filter01;
            // mediums : remplissage (classique) ou creux plus profond (SCOOP)
            const double fc = 760.0, q = 0.75;
            const double w = 2 * pi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            toneBP.set (al, 0.0, -al, 1 + al, -2 * c, 1 - al);    // passe-bande 0 dB au centre
            fill = midFill;
        }

        // un echantillon ; atten : SUSTAIN (lineaire) ; sp : sputter ; recGain : gain de l'etage de sortie
        inline float process (float x, const Voice& v, float atten, float g1, float g2, float sp) noexcept
        {
            // etage d'entree : ~ +12 dB, saturation douce asymetrique du transistor
            float s = inCouple.hp (x) * 4.0f;
            s = s >= 0.0f ? 2.4f * std::tanh (s / 2.4f) : 1.9f * std::tanh (s / 1.9f);
            s = inLP.process (s) * atten;           // SUSTAIN

            float up[Oversampler4::factor];
            os.up (s, up);
            for (int i = 0; i < Oversampler4::factor; ++i)
            {
                float y = st1.process (up[i], g1, v, sp);
                y = st2.process (y, g2, v, sp * 0.6f);
                up[i] = post2.process (post1.process (y));
            }
            float y = os.down (up);

            // tone stack (FILTER) : passe-bas <-> passe-haut, creux au centre ; SCOOP regle la profondeur
            const float lo = toneLP.lp (y);
            const float hi = y - toneHP.lp (y);
            float tone = lo * (1.0f - t) + hi * t + fill * toneBP.process (y);

            // etage de recuperation (gain fixe, transistor)
            float r = recCouple.hp (tone) * 5.0f;
            r = r >= 0.0f ? 3.2f * std::tanh (r / 3.2f) : 2.8f * std::tanh (r / 2.8f);
            r = jumbo.process (outFizz.process (r));
            return outDC.hp (r);
        }

    private:
        double sr = 48000.0, srO = 192000.0;
        Oversampler4 os;
        ClipStage st1, st2;
        OnePole inCouple, toneLP, toneHP, recCouple, outDC;
        Biquad inLP, post1, post2, toneBP, outFizz, jumbo;
        float t = 0.5f, fill = 0.0f;
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
            for (auto* s : { &sLoud, &sSus, &sFil, &sCrunch, &sScoop })
                s->setTime (sr, 14.0);
            sActive.setTime (sr, 3.0);
            sVoice.setTime (sr, 5.0);
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

        // SUSTAIN : gain de 18 dB (mini) a 62 dB (maxi) au total (fiche Way Huge) ; le gain monte vite
        // en debut de course (fuzz lisse vers 8 - 10 h, crachotements vers midi, mur de fuzz au-dela)
        static float sustainDb (float knob)
        {
            const float s = clampf (knob / 10.0f, 0.0f, 1.0f);
            return 44.0f * (1.0f - std::pow (1.0f - s, 1.8f));
        }
        // LOUDNESS : 0 = coupe, enorme reserve de volume en haut de course
        static float loudnessDb (float knob)
        {
            const float l = clampf (knob / 10.0f, 0.0f, 1.0f);
            return l <= 0.0f ? -90.0f : -60.0f + 72.0f * std::pow (l, 0.45f);
        }
        // FILTER : position du melange passe-bas / passe-haut (course legerement logarithmique, comme le potard)
        static float filterPos (float knob)
        {
            const float f = clampf (knob / 10.0f, 0.0f, 1.0f);
            return 0.5f + 0.5f * std::sin ((f - 0.5f) * (float) pi * 0.92f) / std::sin (0.5f * (float) pi * 0.92f);
        }

        void process (float* const* io, int numSamples, const Params& p)
        {
            if (first)
            {
                sLoud.reset (p.loudness); sSus.reset (p.sustain); sFil.reset (p.filter);
                sCrunch.reset (p.crunch ? 1.0f : 0.0f); sScoop.reset (p.scoop ? 1.0f : 0.0f);
                sActive.reset (p.active ? 1.0f : 0.0f); sVoice.reset (1.0f);
                curCrunch = p.crunch; first = false;
                refresh();
            }

            for (int i = 0; i < numSamples; ++i)
            {
                // inverseur CRUNCH : court fondu pour eviter le clic (la voix des diodes change d'un coup)
                if (p.crunch != curCrunch)
                {
                    const float m = sVoice.next (0.0f);
                    if (m < 0.002f) { curCrunch = p.crunch; sVoice.reset (0.0f); }
                }
                else
                    sVoice.next (1.0f);

                const Voice& v = stageVoice (curCrunch);
                const float ld  = sLoud.next (p.loudness);
                const float su  = sSus.next (p.sustain);
                sFil.next (p.filter);
                sScoop.next (p.scoop ? 1.0f : 0.0f);
                const float cr  = sCrunch.next (p.crunch ? 1.0f : 0.0f);
                const float act = sActive.next (p.active ? 1.0f : 0.0f);
                if ((counter++ & 15) == 0) refresh();

                const float sDb = sustainDb (su);
                const float atten = dbToGain (sDb - 44.0f);
                // crachotements plus marques au milieu de la course (au-dela, le gain les recouvre)
                const float s01 = clampf (su / 10.0f, 0.0f, 1.0f);
                const float sp = 0.35f + 0.65f * std::sin (s01 * (float) pi);
                const float out = dbToGain (loudnessDb (ld) + outTrim - crunchTrim * cr) * sVoice.s2;

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
                    const float wet = ch[c].process (x, v, atten, g1, g2, sp) * out * gg;
                    float* dl = dryLine[c];
                    dl[dryPos] = x;
                    const float dry = dl[(dryPos + dryLen - dryDelay) % dryLen];
                    io[c][i] = safety (dry + (wet - dry) * act);
                }
                dryPos = (dryPos + 1) % dryLen;
            }
        }

        float outTrim = -7.2f;      // calibrage : SUSTAIN / FILTER / LOUDNESS a midi ~ niveau du bypass (tests/test_dsp.cpp)
        float crunchTrim = 2.0f;    // CRUNCH haut : ~ +5 dB par rapport a CRUNCH bas (plus ouvert, nettement plus fort)
        float g1 = 100.0f;          // etage 1 : 40 dB (le SUSTAIN attenue avant)
        float g2 = 10.0f;           // etage 2 : 20 dB

    private:
        void refresh()
        {
            // SCOOP bas : mediums un peu remplis (voix classique) ; haut : creux plus profond
            const float midFill = 0.02f - 0.20f * sScoop.s2;
            for (int c = 0; c < nCh; ++c)
                ch[c].updateTone (filterPos (sFil.s2), midFill);
        }

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
        Smooth2 sLoud, sSus, sFil, sCrunch, sScoop, sActive, sVoice;
        bool curCrunch = false;
        bool first = true;
        unsigned counter = 0;
    };
}
