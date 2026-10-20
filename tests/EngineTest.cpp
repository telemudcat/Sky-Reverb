// Offline sound test for the reverb engine. Runs every mode through a set of situations and prints
// loudness numbers, so problems (silence, runaway volume, broken numbers) show up without listening.
#include "../Source/dsp/ReverbEngine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>

using namespace sky;

namespace
{
    EngineSettings defaults (int mode)
    {
        EngineSettings s;
        s.mode = mode;
        const auto& spec = getModes()[(size_t) mode];
        for (size_t i = 0; i < spec.params.size(); ++i)
            s.p[i] = spec.params[i].defaultValue;
        return s;
    }

    struct Stats { float peak = 0, rms = 0; bool bad = false; };

    // Runs `seconds` of audio. Input is noise for the first `burst` seconds, then silence.
    Stats run (ReverbEngine& e, const EngineSettings& s, double fs, float seconds, float burst,
               float from = 0.0f, float to = 1.0e9f, bool continuous = false)
    {
        std::mt19937 rng (1);
        std::normal_distribution<float> nd (0.0f, 0.2f);
        const int block = 256;
        std::vector<float> inL (block), inR (block), wL (block), wR (block), dg (block);
        Stats st;
        double sum = 0; long count = 0;
        const long total = (long) (seconds * fs);
        for (long pos = 0; pos < total; pos += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = (double) (pos + i) / fs;
                const bool on = continuous || t < burst;
                inL[(size_t) i] = on ? nd (rng) : 0.0f;
                inR[(size_t) i] = on ? nd (rng) : 0.0f;
            }
            e.process (inL.data(), inR.data(), wL.data(), wR.data(), dg.data(), block, s);
            for (int i = 0; i < block; ++i)
            {
                const double t = (double) (pos + i) / fs;
                for (float v : { wL[(size_t) i], wR[(size_t) i] })
                {
                    if (! std::isfinite (v)) st.bad = true;
                    if (t >= from && t < to)
                    {
                        st.peak = std::max (st.peak, std::abs (v));
                        sum += (double) v * v; ++count;
                    }
                }
            }
        }
        st.rms = count ? (float) std::sqrt (sum / (double) count) : 0.0f;
        return st;
    }

    float db (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }
}

int main()
{
    int problems = 0;
    for (double fs : { 44100.0 })
    {
        std::printf ("\n=== %.0f Hz ===\n%-12s %9s %9s %9s %9s %9s %9s %8s\n", fs, "mode", "noiseRMS", "peak",
                     "tail1s", "tail4s", "infinite", "freeze", "infPeak");
        for (int m = 0; m < NumModes; ++m)
        {
            auto s = defaults (m);
            ReverbEngine e;
            e.prepare (fs);

            // Steady noise: how loud is the reverb compared to the input (input RMS is -14 dB)?
            const auto steady = run (e, s, fs, 4.0f, 4.0f, 2.0f, 4.0f);

            // Burst then silence: does the tail decay?
            e.reset();
            const auto t1 = run (e, s, fs, 2.0f, 0.5f, 1.4f, 2.0f);
            e.reset();
            const auto t4 = run (e, s, fs, 8.0f, 0.5f, 7.0f, 8.0f);

            // Infinite with endless input for 20 s must stay bounded.
            e.reset();
            auto inf = s; inf.hold = true;
            const auto i20 = run (e, inf, fs, 20.0f, 0.0f, 15.0f, 20.0f, true);

            // Freeze: play 1 s, freeze, silence; tail must survive 10 s later.
            e.reset();
            run (e, s, fs, 1.0f, 1.0f);
            auto fr = s; fr.hold = true; fr.freeze = true;
            const auto f10 = run (e, fr, fs, 10.0f, 0.0f, 9.0f, 10.0f);

            const bool bad = steady.bad || t1.bad || t4.bad || i20.bad || f10.bad
                             || steady.rms < 0.01f || i20.peak > 1.45f || f10.rms < 0.005f;
            problems += bad ? 1 : 0;
            std::printf ("%-12s %8.1fdB %8.2f %8.1fdB %8.1fdB %8.1fdB %8.1fdB %8.2f %s\n", getModes()[(size_t) m].longName,
                         db (steady.rms), steady.peak, db (t1.rms), db (t4.rms), db (i20.rms), db (f10.rms), i20.peak,
                         bad ? "<-- PROBLEM" : "");
        }

        // Random knob torture: every mode, random settings, mode changes mid-stream.
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        ReverbEngine e;
        e.prepare (fs);
        float worst = 0.0f;
        bool anyBad = false;
        for (int round = 0; round < 60; ++round)
        {
            EngineSettings s;
            s.mode = (int) (u (rng) * NumModes) % NumModes;
            s.decayNorm = u (rng);
            s.decaySeconds = 0.2f + 24.8f * s.decayNorm * s.decayNorm;
            s.predelayMs = 500.0f * u (rng);
            s.tone = u (rng); s.mod = u (rng); s.lowEnd = u (rng);
            s.hold = u (rng) < 0.2f; s.freeze = u (rng) < 0.5f;
            const auto& spec = getModes()[(size_t) s.mode];
            for (size_t k = 0; k < spec.params.size(); ++k)
            {
                const auto& ps = spec.params[k];
                s.p[k] = ps.minValue + (ps.maxValue - ps.minValue) * u (rng);
                if (! ps.choices.empty()) s.p[k] = std::round (s.p[k]);
            }
            const auto st = run (e, s, fs, 1.5f, 1.0f);
            worst = std::max (worst, st.peak);
            anyBad = anyBad || st.bad;
        }
        std::printf ("random settings: worst peak %.2f %s\n", worst, anyBad ? "<-- BROKEN NUMBERS" : "ok");
        problems += anyBad ? 1 : 0;
    }

    std::printf ("\n%s\n", problems == 0 ? "ALL GOOD" : "PROBLEMS FOUND");
    return problems == 0 ? 0 : 1;
}
