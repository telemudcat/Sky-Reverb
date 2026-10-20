#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

// Small sound-processing building blocks that the reverb modes are made from.
namespace sky::dsp
{
    constexpr float pi = 3.14159265358979f;
    constexpr float twoPi = 6.28318530717959f;

    inline float onePoleCoeff (float cutoffHz, float sampleRate)
    {
        return 1.0f - std::exp (-twoPi * std::min (cutoffHz, sampleRate * 0.45f) / sampleRate);
    }

    // Gentle limiter: leaves normal levels alone and rounds off anything above about 1.0.
    inline float softClip (float x)
    {
        constexpr float knee = 0.9f;
        const float a = std::abs (x);
        if (a <= knee)
            return x;
        const float over = a - knee;
        const float y = knee + over / (1.0f + over * 2.5f);
        return x < 0.0f ? -y : y;
    }

    // A circular "tape loop" of samples that can be read back at any delay, including in-between samples.
    class DelayLine
    {
    public:
        void prepare (int maxDelaySamples)
        {
            int size = 1;
            while (size < maxDelaySamples + 8)
                size <<= 1;
            buffer.assign ((size_t) size, 0.0f);
            mask = size - 1;
            writePos = 0;
        }

        void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }
        int maxDelay() const { return mask - 4; }

        void write (float x)
        {
            buffer[(size_t) writePos] = x;
            writePos = (writePos + 1) & mask;
        }

        // Sample from `d` samples ago (1 = the last one written). Read before writing.
        float at (int d) const { return buffer[(size_t) ((writePos - d) & mask)]; }

        // Smooth in-between read (cubic), for delays that move (chorus-like modulation).
        float read (float d) const
        {
            d = std::clamp (d, 2.0f, (float) maxDelay());
            const int i = (int) d;
            const float f = d - (float) i;
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

        // Cheaper straight-line in-between read.
        float readLinear (float d) const
        {
            d = std::clamp (d, 1.0f, (float) maxDelay());
            const int i = (int) d;
            const float f = d - (float) i;
            return at (i) + f * (at (i + 1) - at (i));
        }

    private:
        std::vector<float> buffer;
        int mask = 0, writePos = 0;
    };

    // Allpass "smearer": spreads a sound out in time without changing its tone. Chains of these make
    // a click turn into a dense wash, which is what "diffusion" means in reverbs.
    class Allpass
    {
    public:
        void prepare (int maxDelaySamples) { line.prepare (maxDelaySamples + 4); }
        void clear() { line.clear(); }
        void setDelay (float samples) { target = std::clamp (samples, 2.0f, (float) line.maxDelay() - 2.0f); }
        void snap() { current = target; }

        float process (float x, float g, float mod = 0.0f)
        {
            current += (target - current) * 0.0015f;
            const float z = line.read (current + mod);
            const float u = x + g * z;
            line.write (u);
            return z - g * u;
        }

    private:
        DelayLine line;
        float target = 10.0f, current = 10.0f;
    };

    // One-pole filter: a soft tone control. low() keeps the bass side, high() the treble side.
    struct OnePole
    {
        float z = 0.0f;
        float low (float x, float a) { z += a * (x - z); return z; }
        float high (float x, float a) { return x - low (x, a); }
        void clear() { z = 0.0f; }
    };

    // Standard "biquad" filter used for shelves (bass/treble tilt) and the vowel formants.
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void clear() { z1 = z2 = 0.0f; }

        void setLowShelf (float fs, float freq, float gainDb)  { setShelf (fs, freq, gainDb, false); }
        void setHighShelf (float fs, float freq, float gainDb) { setShelf (fs, freq, gainDb, true); }

        void setBandPass (float fs, float freq, float q)
        {
            const float w = twoPi * std::min (freq, fs * 0.45f) / fs;
            const float alpha = std::sin (w) / (2.0f * q);
            const float a0 = 1.0f + alpha;
            b0 = alpha / a0; b1 = 0.0f; b2 = -alpha / a0;
            a1 = -2.0f * std::cos (w) / a0; a2 = (1.0f - alpha) / a0;
        }

        void setHighPass (float fs, float freq, float q = 0.707f)
        {
            const float w = twoPi * std::min (freq, fs * 0.45f) / fs;
            const float alpha = std::sin (w) / (2.0f * q), c = std::cos (w);
            const float a0 = 1.0f + alpha;
            b0 = (1.0f + c) * 0.5f / a0; b1 = -(1.0f + c) / a0; b2 = b0;
            a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
        }

        void setLowPass (float fs, float freq, float q = 0.707f)
        {
            const float w = twoPi * std::min (freq, fs * 0.45f) / fs;
            const float alpha = std::sin (w) / (2.0f * q), c = std::cos (w);
            const float a0 = 1.0f + alpha;
            b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0;
            a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
        }

    private:
        void setShelf (float fs, float freq, float gainDb, bool high)
        {
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w = twoPi * std::min (freq, fs * 0.45f) / fs;
            const float c = std::cos (w), s = std::sin (w);
            const float alpha = s / 2.0f * std::sqrt (2.0f);
            const float sq = 2.0f * std::sqrt (A) * alpha;
            float a0;
            if (high)
            {
                a0 = (A + 1) - (A - 1) * c + sq;
                b0 = A * ((A + 1) + (A - 1) * c + sq) / a0;
                b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
                b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
                a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
                a2 = ((A + 1) - (A - 1) * c - sq) / a0;
            }
            else
            {
                a0 = (A + 1) + (A - 1) * c + sq;
                b0 = A * ((A + 1) - (A - 1) * c + sq) / a0;
                b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
                b2 = A * ((A + 1) - (A - 1) * c - sq) / a0;
                a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
                a2 = ((A + 1) + (A - 1) * c - sq) / a0;
            }
        }
    };

    // Slow sine wave used to gently wobble delay times (the "Mod" knob).
    struct Lfo
    {
        float phase = 0.0f;
        float next (float inc)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            return std::sin (twoPi * phase);
        }
    };

    // Tiny fast random number generator.
    struct Noise
    {
        uint32_t state = 22222;
        float next() // -1 .. 1
        {
            state = state * 1664525u + 1013904223u;
            return (float) (int32_t) state * (1.0f / 2147483648.0f);
        }
    };

    // Pitch shifter: two read heads sweep through a short delay and cross-fade, which raises or lowers
    // the pitch without changing speed. Used for Shimmer.
    class PitchShifter
    {
    public:
        void prepare (float fs)
        {
            window = 0.06f * fs;
            line.prepare ((int) (window * 2.0f) + 16);
        }
        void clear() { line.clear(); phase = 0.0f; }

        float process (float x, float ratio)
        {
            line.write (x);
            phase += (1.0f - ratio) / window;
            phase -= std::floor (phase);
            float p2 = phase + 0.5f;
            if (p2 >= 1.0f) p2 -= 1.0f;
            const float g1 = std::sin (pi * phase), g2 = std::sin (pi * p2);
            return g1 * line.readLinear (2.0f + phase * window) + g2 * line.readLinear (2.0f + p2 * window);
        }

    private:
        DelayLine line;
        float window = 2000.0f, phase = 0.0f;
    };

    // The heart of most modes: 8 delay lines that feed back into each other through a mixing matrix
    // (a "feedback delay network"). It turns a sound into a long, dense, smooth tail.
    class Fdn
    {
    public:
        static constexpr int N = 8;

        void prepare (float sampleRate, float maxSeconds)
        {
            fs = sampleRate;
            for (auto& l : lines)
                l.prepare ((int) (maxSeconds * fs) + 64);
            for (int i = 0; i < N; ++i)
                lfos[i].phase = (float) i / (float) N;
            primed = false;
            clear();
        }

        void clear()
        {
            for (int i = 0; i < N; ++i)
            {
                lines[i].clear();
                lowState[i].clear();
                highState[i].clear();
            }
            primed = false;
        }

        // lengthSeconds: average delay of the lines (bigger = bigger space).
        // rt60: seconds for the tail to fade by 60 dB. lowMult/highMult: how much longer/shorter bass and
        // treble ring compared to the middle. infinite: no fading at all.
        void configure (float lengthSeconds, float rt60, float lowMult, float highMult,
                        float lowXoverHz, float highXoverHz, float modDepthSeconds, float modRateHz, bool infinite)
        {
            static constexpr float ratios[N] = { 0.555f, 0.627f, 0.706f, 0.781f, 0.862f, 0.948f, 1.033f, 1.117f };
            const float maxLen = (float) lines[0].maxDelay() - 8.0f;
            for (int i = 0; i < N; ++i)
            {
                targetLen[i] = std::clamp (lengthSeconds * ratios[i] * fs, 16.0f, maxLen - modDepthSeconds * fs - 4.0f);
                if (! primed)
                    currentLen[i] = targetLen[i];
            }
            primed = true;

            modDepth = modDepthSeconds * fs;
            modInc = modRateHz / fs;
            lowA = onePoleCoeff (lowXoverHz, fs);
            highA = onePoleCoeff (highXoverHz, fs);
            frozen = infinite;

            for (int i = 0; i < N; ++i)
            {
                if (infinite)
                {
                    gLow[i] = gMid[i] = gHigh[i] = 1.0f;
                    continue;
                }
                const float len = targetLen[i];
                auto gainFor = [&] (float rt) { return std::pow (10.0f, -3.0f * len / (std::max (rt, 0.05f) * fs)); };
                gMid[i] = gainFor (rt60);
                gLow[i] = gainFor (rt60 * lowMult);
                gHigh[i] = gainFor (rt60 * highMult);
            }
        }

        // Directly set how much each pass keeps (used where a knob sets "feedback" rather than seconds).
        void setFeedback (float g)
        {
            for (int i = 0; i < N; ++i)
                gLow[i] = gMid[i] = gHigh[i] = g;
        }

        void process (float inL, float inR, float& outL, float& outR)
        {
            float y[N];
            for (int i = 0; i < N; ++i)
            {
                currentLen[i] += (targetLen[i] - currentLen[i]) * 0.0004f;
                const float m = modDepth > 0.0f ? modDepth * (1.0f + lfos[i].next (modInc * modSpread[i])) : 0.0f;
                y[i] = lines[i].read (currentLen[i] + m);
            }

            // Bass / middle / treble each fade at their own speed (that's what Tone and Low End shape).
            float z[N];
            for (int i = 0; i < N; ++i)
            {
                const float low = lowState[i].low (y[i], lowA);
                const float notHigh = highState[i].low (y[i], highA);
                const float high = y[i] - notHigh;
                const float mid = notHigh - low;
                z[i] = gLow[i] * low + gMid[i] * mid + gHigh[i] * high;
            }

            hadamard (z);

            static constexpr float inSign[N] = { 1, 1, -1, 1, 1, -1, -1, 1 };
            for (int i = 0; i < N; ++i)
            {
                const float in = (i & 1) ? inR : inL;
                float v = z[i] + in * inSign[i] * 0.5f;
                if (frozen)
                    v = softClip (v);
                lines[i].write (v);
            }

            outL = (y[0] - y[1] + y[2] + y[3] - y[4] + y[5] - y[6] + y[7]) * 0.35f;
            outR = (y[0] + y[1] - y[2] + y[3] + y[4] + y[5] - y[6] - y[7]) * 0.35f;
        }

        float energy() const
        {
            float e = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                const float v = lines[i].at (1);
                e += v * v;
            }
            return e;
        }

    private:
        static void hadamard (float* x)
        {
            for (int h = 1; h < N; h <<= 1)
                for (int i = 0; i < N; i += h * 2)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = x[j], b = x[j + h];
                        x[j] = a + b;
                        x[j + h] = a - b;
                    }
            constexpr float norm = 0.35355339f; // 1 / sqrt(8): keeps the energy the same
            for (int i = 0; i < N; ++i)
                x[i] *= norm;
        }

        float fs = 44100.0f;
        DelayLine lines[N];
        OnePole lowState[N], highState[N];
        Lfo lfos[N];
        float modSpread[N] = { 1.0f, 1.13f, 0.87f, 1.29f, 0.71f, 1.07f, 0.93f, 1.21f };
        float targetLen[N] {}, currentLen[N] {};
        float gLow[N] {}, gMid[N] {}, gHigh[N] {};
        float modDepth = 0.0f, modInc = 0.0f, lowA = 0.05f, highA = 0.3f;
        bool frozen = false, primed = false;
    };
}
