// Strong Pickle : tests du moteur audio hors plugin (g++ -O2 -std=c++17 tests/test_dsp.cpp -I.)
#include "PickleDSP.h"
#include <cstdio>
#include <chrono>
using namespace pickle;
static const double SR = 48000.0;

// guitare synthetique (Karplus-Strong) : accord / note grave, crete ~0.3
static std::vector<float> guitar (double secs, float peak = 0.3f)
{
    std::vector<float> out ((size_t) (secs * SR), 0.0f); int dummy = 0; (void) dummy;
    const double f[] = { 82.4, 123.5, 164.8 };
    unsigned rng = 1;
    for (double fr : f)
    {
        int N = (int) (SR / fr);
        std::vector<float> buf (N);
        for (auto& b : buf) { rng = rng * 1664525u + 1013904223u; b = ((rng >> 9) / 8388608.0f - 1.0f); }
        int idx = 0;
        for (size_t i = 0; i < out.size(); ++i)
        {
            float a = buf[idx], b2 = buf[(idx + 1) % N];
            buf[idx] = 0.4962f * (a + b2);
            out[i] += a; idx = (idx + 1) % N;
        }
    }
    float m = 0; for (auto v : out) m = std::max (m, std::abs (v));
    for (auto& v : out) v *= peak / m;
    return out;
}
static double rmsDb (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += v[i] * (double) v[i];
    return 10 * std::log10 (s / (b - a) + 1e-30);
}
static std::vector<float> run (Params p, const std::vector<float>& in)
{
    Engine e; e.prepare (SR, 1);
    std::vector<float> buf = in;
    float* io[1] = { buf.data() };
    for (size_t pos = 0; pos < buf.size(); pos += 256)
    {
        float* b[1] = { buf.data() + pos };
        e.process (b, (int) std::min<size_t> (256, buf.size() - pos), p);
    }
    (void) io;
    return buf;
}
static double sineGain (Params p, double f, float amp)
{
    std::vector<float> in ((size_t) (SR * 0.6));
    for (size_t i = 0; i < in.size(); ++i) in[i] = amp * (float) std::sin (2 * pi * f * i / SR);
    auto o = run (p, in);
    // composante a la frequence du test (correlation) : insensible au bruit et a la derive lente
    double cs = 0, sn = 0; size_t a = in.size() / 2;
    for (size_t i = a; i < in.size(); ++i) { const double ph = 2 * pi * f * (double) i / SR; cs += o[i] * std::cos (ph); sn += o[i] * std::sin (ph); }
    const double ampOut = 2.0 * std::sqrt (cs * cs + sn * sn) / (double) (in.size() - a);
    return 20 * std::log10 (ampOut / amp + 1e-30);
}

int main()
{
    Params p;
    // 1. gain petits signaux (1 kHz, -110 dBV) : SUSTAIN 0 -> 10 doit couvrir ~44 dB (18 -> 62 dB)
    p.loudness = 5; p.filter = 5;
    p.sustain = 0;  double g0 = sineGain (p, 1000, 0.00003f);
    p.sustain = 10; double g10 = sineGain (p, 1000, 0.00003f);
    printf ("petits signaux : sustain 0 = %.1f dB, sustain 10 = %.1f dB, ecart %.1f dB (cible 44)\n", g0, g10, g10 - g0);
    for (float s : { 2.5f, 5.0f, 7.5f }) { p.sustain = s; printf ("   sustain %.1f : %.1f dB\n", s, sineGain (p, 1000, 0.00003f)); }

    // 2. niveau avec une guitare (accord, crete 0,3) : loudness midi ~ niveau du bypass
    auto g = guitar (3.0);
    const double inDb = rmsDb (g, 0, g.size());
    printf ("guitare : %.1f dBFS RMS\n", inDb);
    for (int cr = 0; cr < 2; ++cr)
        for (float s : { 1.5f, 3.0f, 5.0f, 7.5f, 10.0f })
        {
            Params q; q.sustain = s; q.crunch = cr; q.loudness = 5; q.filter = 5;
            auto o = run (q, g);
            float pk = 0; bool bad = false;
            for (auto v : o) { pk = std::max (pk, std::abs (v)); if (! std::isfinite (v)) bad = true; }
            printf ("  crunch %d sustain %4.1f : %+5.1f dB / bypass, crete %.2f %s\n", cr, s, rmsDb (o, 0, o.size()) - inDb, pk, bad ? "NaN!" : "");
        }
    // 3. scoop : reponse (fuzz forte -> spectre) ; tone : grave / aigu selon FILTER
    for (int sc = 0; sc < 2; ++sc)
        for (float f : { 0.0f, 5.0f, 10.0f })
        {
            Params q; q.sustain = 0; q.filter = f; q.scoop = sc; q.loudness = 5;
            printf ("  scoop %d filter %4.1f : 100 Hz %+5.1f  400 Hz %+5.1f  800 Hz %+5.1f  2 kHz %+5.1f  5 kHz %+5.1f\n", sc, f,
                    sineGain (q, 100, 0.00003f), sineGain (q, 400, 0.00003f), sineGain (q, 800, 0.00003f), sineGain (q, 2000, 0.00003f), sineGain (q, 5000, 0.00003f));
        }
    // 3b. OCTAVE : note seule 196 Hz -> l'octave (392 Hz) domine, niveau proche de la fuzz sans octave
    {
        std::vector<float> in ((size_t) (SR * 0.6));
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.1f * (float) std::sin (2 * pi * 196.0 * i / SR);
        auto comp = [&] (const std::vector<float>& o, double f)
        {
            double cs = 0, sn = 0; size_t a = o.size() / 2;
            for (size_t i = a; i < o.size(); ++i) { const double ph = 2 * pi * f * (double) i / SR; cs += o[i] * std::cos (ph); sn += o[i] * std::sin (ph); }
            return 20 * std::log10 (2.0 * std::sqrt (cs * cs + sn * sn) / (double) (o.size() - a) + 1e-12);
        };
        for (int oc = 0; oc < 2; ++oc)
        {
            Params q; q.sustain = 7.5f; q.filter = 6.5f; q.octave = oc;
            auto o = run (q, in);
            printf ("  octave %d : rms %+5.1f dB  196 Hz %+5.1f  392 Hz %+5.1f\n", oc, rmsDb (o, o.size() / 2, o.size()), comp (o, 196), comp (o, 392));
        }
    }
    // 4. bypass : sortie == entree retardee
    {
        Params q; q.active = false;
        auto o = run (q, g);
        int d = Engine::latencyInt(); double err = 0;
        for (size_t i = 2000; i < g.size(); ++i) err = std::max (err, (double) std::abs (o[i] - g[i - d]));
        printf ("bypass : erreur max %.2e (latence %d)\n", err, d);
    }
    // 5. sputter : enveloppe de fin de note (fraction de silence dans la queue) selon sustain
    for (float s : { 2.0f, 5.0f, 9.0f })
    {
        Params q; q.sustain = s; q.loudness = 5;
        auto o = run (q, g);
        size_t a = (size_t) (1.5 * SR), b = (size_t) (3.0 * SR); int quiet = 0, tot = 0;
        for (size_t i = a; i < b; i += 48) { double e = rmsDb (o, i, i + 48); tot++; if (e < -50) quiet++; }
        printf ("  queue sustain %.0f : %.0f %% de trous, niveau queue %.1f dB\n", s, 100.0 * quiet / tot, rmsDb (o, a, b));
    }
    // 6. charge CPU (stereo)
    {
        Engine e; e.prepare (SR, 2); Params q;
        std::vector<float> L (g), R (g);
        auto t0 = std::chrono::steady_clock::now();
        for (size_t pos = 0; pos + 512 <= L.size(); pos += 512) { float* b[2] = { L.data() + pos, R.data() + pos }; e.process (b, 512, q); }
        double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        printf ("CPU : %.1f %% d'un coeur (stereo 48 kHz)\n", 100.0 * ms / 3000.0);
    }
}
