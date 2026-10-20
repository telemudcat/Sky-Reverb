#include "ReverbEngine.h"

namespace sky
{
    using namespace dsp;

    namespace
    {
        // Loudness trim per mode, so switching modes doesn't jump in volume (measured with the sound test).
        constexpr float modeTrim[NumModes] = {
            0.9f, // Room
            1.3f, // Hall
            1.05f, // Plate
            0.5f, // Spring
            1.25f, // Swell
            2.4f, // Bloom
            1.8f, // Cloud
            2.2f, // Chorale
            1.3f, // Shimmer
            2.4f, // Magneto
            2.0f, // Non-Linear
            1.25f, // Reflections
        };

        // Vowel formants (the resonances that make a voice sound like "ah" or "oo"), in Hz.
        constexpr float formantsLow[5][3]  = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                                               { 570, 840, 2410 },  { 300, 870, 2240 } };
        constexpr float formantsHigh[5][3] = { { 850, 1220, 2810 }, { 610, 2330, 2990 }, { 310, 2790, 3310 },
                                               { 590, 920, 2710 },  { 370, 950, 2670 } };

        constexpr float springLengths[3] = { 0.0331f, 0.0413f, 0.0469f };

        float decayGain (float delaySeconds, float rt60)
        {
            return std::pow (10.0f, -3.0f * delaySeconds / std::max (rt60, 0.05f));
        }
    }

    void ReverbEngine::prepare (double sampleRate)
    {
        fs = (float) sampleRate;

        preDelayL.prepare ((int) (fs * 1.05f));
        preDelayR.prepare ((int) (fs * 1.05f));
        for (int i = 0; i < maxDiffusers; ++i)
        {
            diffL[i].prepare ((int) (fs * 0.06f));
            diffR[i].prepare ((int) (fs * 0.06f));
        }
        fdn.prepare (fs, 0.5f);
        bloomFdn.prepare (fs, 0.4f);
        earlyLine.prepare ((int) (fs * 0.4f));

        for (int i = 0; i < maxSprings; ++i)
        {
            springs[(size_t) i].loop.prepare ((int) (fs * 0.06f));
            springs[(size_t) i].lfo.phase = 0.3f * (float) i;
        }
        springHp.setHighPass (fs, 110.0f);
        springLpL.setLowPass (fs, 4800.0f);
        springLpR.setLowPass (fs, 4800.0f);

        shifterA.prepare (fs);
        shifterB.prepare (fs);
        shimmerHp.setHighPass (fs, 250.0f);
        shimmerLp.setLowPass (fs, 7000.0f);

        tape.prepare ((int) (fs * 1.1f));
        tapeHp.setHighPass (fs, 140.0f);
        tapeLp.setLowPass (fs, 3200.0f);

        nlLine.prepare ((int) (fs * 1.6f));
        for (int i = 0; i < 4; ++i)
        {
            nlDiffL[i].prepare ((int) (fs * 0.03f));
            nlDiffR[i].prepare ((int) (fs * 0.03f));
        }

        dcL.setHighPass (fs, 22.0f);
        dcR.setHighPass (fs, 22.0f);

        current.mode = activeMode;
        reset();
    }

    void ReverbEngine::reset()
    {
        preDelayL.clear(); preDelayR.clear();
        for (int i = 0; i < maxDiffusers; ++i) { diffL[i].clear(); diffR[i].clear(); }
        fdn.clear();
        bloomFdn.clear();
        earlyLine.clear();
        earlyKey = -1.0f;
        for (auto& t : earlyTaps)
            t.delay = 0.0f;

        for (auto& s : springs)
        {
            std::fill (std::begin (s.apState), std::end (s.apState), 0.0f);
            s.loop.clear();
            s.damp.clear();
            s.out = 0.0f;
        }
        springHp.clear(); springLpL.clear(); springLpR.clear();

        envFast = envSlow = swellGain = 0.0f;
        swellStage = 0; silentSamples = 0; retriggerHold = 0;

        for (int i = 0; i < 3; ++i) { formantL[i].clear(); formantR[i].clear(); }
        formantKey = -1.0f;

        shifterA.clear(); shifterB.clear(); shimmerHp.clear(); shimmerLp.clear();
        lastWetMono = 0.0f;

        tape.clear(); tapeHp.clear(); tapeLp.clear(); echoToneL.clear(); echoToneR.clear();

        nlLine.clear();
        for (int i = 0; i < 4; ++i) { nlDiffL[i].clear(); nlDiffR[i].clear(); }
        nlShape = -1;

        lowShelfL.clear(); lowShelfR.clear(); highShelfL.clear(); highShelfR.clear(); dcL.clear(); dcR.clear();
        holdMix = 0.0f;
        injectScale = 1.0f;
        outEnv = 0.0f;
    }

    //==============================================================================
    void ReverbEngine::buildRoomReflections (float width, float length, float height, float listenerPos,
                                             float absorb, float scaleDelay)
    {
        const float sx = width * 0.5f, sy = length * 0.22f, sz = 1.5f;
        const float lx = width * 0.4f, ly = length * (0.4f + 0.5f * listenerPos), lz = 1.6f;
        const float direct = std::max (1.0f, std::hypot (sx - lx, sy - ly, sz - lz));

        struct Candidate { float delay, gain, pan; };
        std::vector<Candidate> found;

        auto image = [] (int n, float size, float pos)
        {
            return (n % 2 == 0) ? (float) n * size + pos : (float) n * size + (size - pos);
        };

        for (int i = -2; i <= 2; ++i)
            for (int j = -2; j <= 2; ++j)
                for (int k = -1; k <= 1; ++k)
                {
                    const int order = std::abs (i) + std::abs (j) + std::abs (k);
                    if (order == 0 || order > 3)
                        continue;
                    const float ix = image (i, width, sx), iy = image (j, length, sy), iz = image (k, height, sz);
                    const float dx = ix - lx, dy = iy - ly, dz = iz - lz;
                    const float dist = std::sqrt (dx * dx + dy * dy + dz * dz);
                    const float delay = (dist - direct) / 343.0f * scaleDelay;
                    if (delay < 0.0007f || delay > 0.37f)
                        continue;
                    const float gain = (direct / dist) * std::pow (absorb, (float) order);
                    const float pan = dx / std::max (0.001f, std::sqrt (dx * dx + dy * dy));
                    found.push_back ({ delay, gain, pan });
                }

        std::sort (found.begin(), found.end(), [] (const Candidate& a, const Candidate& b) { return a.gain > b.gain; });
        numEarlyTaps = std::min ((int) found.size(), maxTaps);

        float energy = 0.0f;
        for (int t = 0; t < numEarlyTaps; ++t)
            energy += found[(size_t) t].gain * found[(size_t) t].gain;
        const float norm = energy > 0.0f ? 0.9f / std::sqrt (energy) : 0.0f;

        for (int t = 0; t < numEarlyTaps; ++t)
        {
            auto& tap = earlyTaps[(size_t) t];
            const auto& c = found[(size_t) t];
            tap.targetDelay = c.delay * fs;
            if (tap.delay <= 0.0f)
                tap.delay = tap.targetDelay;
            tap.gainL = c.gain * norm * std::sqrt (0.5f * (1.0f - 0.9f * c.pan)) * 1.414f;
            tap.gainR = c.gain * norm * std::sqrt (0.5f * (1.0f + 0.9f * c.pan)) * 1.414f;
        }
    }

    void ReverbEngine::buildNonLinearTaps (int shape)
    {
        Noise rng;
        rng.state = 9871;
        float energy = 0.0f;

        for (int k = 0; k < maxTaps; ++k)
        {
            const float pos = std::clamp (((float) k + 0.5f + 0.7f * rng.next() * 0.5f) / (float) maxTaps, 0.002f, 1.0f);
            float env = 1.0f;
            switch (shape)
            {
                case 0: env = 0.04f + pos * pos * 1.2f; break;                                  // Reverse: grows, then stops
                case 1: env = pos > 0.9f ? (1.0f - pos) * 10.0f : 1.0f; break;                  // Gated: flat, then stops
                case 2: env = pos < 0.6f ? std::pow (pos / 0.6f, 1.6f)                          // Swoosh: rises then falls
                                         : std::pow ((1.0f - pos) / 0.4f, 0.8f); break;
                default: env = 0.2f + 0.8f * pos; break;                                        // Ramp: gentle rise
            }
            const float sign = rng.next() >= 0.0f ? 1.0f : -1.0f;
            const float pan = 0.5f + 0.45f * rng.next();
            auto& t = nlTaps[(size_t) k];
            t.targetDelay = pos;
            t.gainL = env * sign * std::cos (pan * pi * 0.5f);
            t.gainR = env * sign * std::sin (pan * pi * 0.5f) * (k % 3 == 0 ? -1.0f : 1.0f);
            energy += 0.5f * (t.gainL * t.gainL + t.gainR * t.gainR);
        }

        const float norm = 1.0f / std::sqrt (std::max (energy, 1.0e-6f));
        for (auto& t : nlTaps)
        {
            t.gainL *= norm;
            t.gainR *= norm;
        }
        nlShape = shape;
    }

    //==============================================================================
    void ReverbEngine::configure (const EngineSettings& s)
    {
        const float hiMult = 0.1f * std::pow (10.0f, s.tone);           // treble rings 10% .. 100% as long
        const float loMult = std::pow (2.0f, (s.lowEnd - 0.5f) * 2.0f);  // bass rings half .. twice as long
        const float modAmt = s.mod * 2.0f;
        const float rt = s.decaySeconds;
        const bool infinite = s.hold;

        // Tone and Low End also tilt the overall colour of the reverb.
        const float lowDb = s.lowEnd < 0.5f ? -24.0f * (0.5f - s.lowEnd) : 12.0f * (s.lowEnd - 0.5f);
        const float highDb = s.tone < 0.5f ? -18.0f * (0.5f - s.tone) : 10.0f * (s.tone - 0.5f);
        lowShelfL.setLowShelf (fs, 220.0f, lowDb);   lowShelfR.setLowShelf (fs, 220.0f, lowDb);
        highShelfL.setHighShelf (fs, 2500.0f, highDb); highShelfR.setHighShelf (fs, 2500.0f, highDb);

        preDelayTarget = std::clamp (s.predelayMs * 0.001f * fs, 1.0f, fs);

        setup = ModeSetup();
        setup.outputGain = modeTrim[activeMode];

        auto setDiffusers = [this] (std::initializer_list<float> ms, float scale)
        {
            int i = 0;
            for (float m : ms)
            {
                diffL[i].setDelay (m * 0.001f * scale * fs);
                diffR[i].setDelay (m * 0.001f * scale * 1.07f * fs);
                ++i;
            }
            setup.numDiffusers = i;
        };

        auto mainFdn = [&] (float len, float highXover, float modSeconds, float modRate, float hiScale = 1.0f)
        {
            fdn.configure (len, rt, loMult, std::min (1.0f, hiMult * hiScale), 250.0f, highXover,
                           modSeconds * modAmt, modRate, infinite);
        };

        const float* p = s.p;
        switch (activeMode)
        {
            case Room:
            {
                const float size = p[0] * 0.01f;
                mainFdn (0.012f + 0.05f * size, 4000.0f, 0.0003f, 0.8f);
                setDiffusers ({ 4.1f, 5.9f, 8.3f, 12.7f }, 0.4f + size);
                setup.diffusion = 0.45f + 0.3f * p[1] * 0.01f;
                setup.earlyLevel = p[2] * 0.012f;
                const float key = 1000.0f + p[0];
                if (std::abs (key - earlyKey) > 0.001f)
                {
                    const float w = 3.0f + 9.0f * size;
                    buildRoomReflections (w, w * 1.3f, 2.5f + 2.0f * size, 0.6f, 0.72f, 1.0f);
                    earlyKey = key;
                }
                break;
            }
            case Hall:
            {
                const float size = p[0] * 0.01f;
                mainFdn (0.045f + 0.09f * size, 3000.0f, 0.0007f, 0.5f);
                setDiffusers ({ 4.7f, 6.9f, 10.3f, 14.9f }, 0.7f + size);
                setup.diffusion = 0.45f + 0.3f * p[2] * 0.01f;
                setup.earlyLevel = p[1] * 0.012f;
                const float key = 2000.0f + p[0];
                if (std::abs (key - earlyKey) > 0.001f)
                {
                    const float w = 14.0f + 20.0f * size;
                    buildRoomReflections (w, w * 1.6f, 10.0f + 8.0f * size, 0.5f, 0.82f, 1.0f);
                    earlyKey = key;
                }
                break;
            }
            case Plate:
            {
                const float size = p[0] * 0.01f;
                mainFdn (0.025f + 0.05f * size, 6000.0f, 0.0005f, 1.0f, 1.6f);
                setDiffusers ({ 1.1f, 1.7f, 2.6f, 3.9f, 5.3f, 7.7f }, 0.6f + 0.8f * size);
                setup.diffusion = 0.55f + 0.2f * p[1] * 0.01f;
                break;
            }
            case Spring:
                setup.useFdn = false;
                setup.numDiffusers = 0;
                mainFdn (0.08f, 4000.0f, 0.0006f, 0.4f);
                break;
            case Swell:
                mainFdn (0.08f, 3000.0f, 0.0007f, 0.5f);
                setDiffusers ({ 4.7f, 6.9f, 10.3f, 14.9f }, 1.2f);
                setup.diffusion = 0.65f;
                break;
            case Bloom:
            {
                mainFdn (0.1f, 3000.0f, 0.0008f, 0.4f);
                setDiffusers ({ 4.7f, 6.9f, 10.3f, 14.9f }, 1.4f);
                setup.diffusion = 0.7f;
                bloomFdn.configure (0.05f + 0.25f * p[0] * 0.01f, 1.0f, 1.0f, 0.6f, 250.0f, 3000.0f,
                                    0.0005f * modAmt, 0.35f, false);
                bloomFdn.setFeedback (0.3f + 0.62f * p[1] * 0.01f);
                break;
            }
            case Cloud:
            {
                mainFdn (0.17f, 2500.0f, 0.0015f, 0.25f);
                const float d = p[0] * 0.01f;
                setDiffusers ({ 7.1f, 11.3f, 15.7f, 19.9f, 24.1f, 29.3f, 33.7f, 41.9f }, 1.0f);
                setup.numDiffusers = 4 + (int) std::round (d * 4.0f);
                setup.diffusion = 0.35f + 0.4f * d;
                break;
            }
            case Chorale:
            {
                mainFdn (0.07f, 3500.0f, 0.0025f, 1.4f);
                setDiffusers ({ 4.7f, 6.9f, 10.3f, 14.9f }, 1.0f);
                setup.diffusion = 0.65f;
                const int vowel = std::clamp ((int) std::round (p[0]), 0, 4);
                const bool high = p[1] >= 0.5f;
                const float key = (float) vowel + (high ? 10.0f : 0.0f);
                if (std::abs (key - formantKey) > 0.001f)
                {
                    const auto& f = high ? formantsHigh[vowel] : formantsLow[vowel];
                    static constexpr float q[3] = { 7.0f, 9.0f, 11.0f };
                    for (int i = 0; i < 3; ++i)
                    {
                        formantL[i].setBandPass (fs, f[i], q[i]);
                        formantR[i].setBandPass (fs, f[i] * 1.015f, q[i]);
                    }
                    formantKey = key;
                }
                break;
            }
            case Shimmer:
                mainFdn (0.08f, 4000.0f, 0.0008f, 0.4f);
                setDiffusers ({ 4.7f, 6.9f, 10.3f, 14.9f }, 1.1f);
                setup.diffusion = 0.65f;
                break;
            case Magneto:
                mainFdn (0.05f, 3000.0f, 0.0005f, 0.5f);
                setDiffusers ({ 4.1f, 5.9f, 8.3f, 12.7f }, 1.0f);
                setup.diffusion = 0.55f;
                setup.fdnLevel = 0.8f;
                break;
            case NonLinear:
            {
                setup.useFdn = false;
                setup.numDiffusers = 0;
                mainFdn (0.08f, 4000.0f, 0.0006f, 0.4f);
                const int shape = std::clamp ((int) std::round (p[0]), 0, 3);
                if (shape != nlShape)
                    buildNonLinearTaps (shape);
                const float g = 0.2f + 0.5f * p[1] * 0.01f;
                static constexpr float ms[4] = { 3.3f, 5.1f, 7.9f, 11.3f };
                for (int i = 0; i < 4; ++i)
                {
                    nlDiffL[i].setDelay (ms[i] * 0.001f * fs);
                    nlDiffR[i].setDelay (ms[i] * 0.00107f * fs);
                }
                setup.diffusion = g;
                break;
            }
            case Reflections:
            default:
            {
                static constexpr float shapes[4][4] = {   // width, length, height, absorb
                    { 6.0f, 8.0f, 3.0f, 0.72f },          // Studio
                    { 10.0f, 14.0f, 6.0f, 0.86f },        // Chamber
                    { 26.0f, 11.0f, 8.0f, 0.8f },         // Wide
                    { 4.0f, 55.0f, 4.0f, 0.9f } };        // Tunnel
                const int shape = std::clamp ((int) std::round (p[0]), 0, 3);
                const auto& sh = shapes[shape];
                mainFdn (0.02f + 0.0015f * (sh[0] + sh[1]), 3500.0f, 0.0004f, 0.6f);
                setup.numDiffusers = 0;
                setup.fdnLevel = 0.35f;
                setup.earlyLevel = 1.1f;
                const float key = 3000.0f + (float) shape * 200.0f + p[1];
                if (std::abs (key - earlyKey) > 0.001f)
                {
                    buildRoomReflections (sh[0], sh[1], sh[2], p[1] * 0.01f, sh[3], 1.0f);
                    earlyKey = key;
                }
                break;
            }
        }
    }

    //==============================================================================
    void ReverbEngine::updateSwell (float level)
    {
        const float attack = 0.3f;
        const float release = 1.0f - std::exp (-1.0f / (0.03f * fs));
        const float slow = 1.0f - std::exp (-1.0f / (0.25f * fs));
        const float threshold = 0.004f;

        envFast += (level - envFast) * (level > envFast ? attack : release);
        envSlow += (envFast - envSlow) * slow;

        if (envFast < threshold)
        {
            if (++silentSamples > (int) (0.08f * fs))
                swellStage = 0;
        }
        else
        {
            silentSamples = 0;
        }

        if (retriggerHold > 0)
            --retriggerHold;

        const float rise = std::max (0.05f, current.p[0]);
        switch (swellStage)
        {
            case 0: // waiting for a note: stay silent
                swellGain = std::max (0.0f, swellGain - 1.0f / (0.01f * fs));
                if (envFast >= threshold)
                    swellStage = 1;
                break;
            case 1: // fading in
                if (envFast > envSlow * 2.5f && retriggerHold == 0 && swellGain > 0.3f)
                {
                    swellStage = 2;
                    retriggerHold = (int) (0.12f * fs);
                }
                swellGain = std::min (1.0f, swellGain + 1.0f / (rise * fs));
                break;
            default: // a new note: quickly duck, then fade in again
                swellGain -= 1.0f / (0.006f * fs);
                if (swellGain <= 0.0f)
                {
                    swellGain = 0.0f;
                    swellStage = 1;
                }
                break;
        }
    }

    float ReverbEngine::processSpring (float x, int index, float a, float feedback, float dampA, float modAmt)
    {
        auto& sp = springs[(size_t) index];
        float v = x + sp.out * feedback;

        // A long chain of allpasses makes high notes arrive later than low ones: the spring "boing".
        for (int k = 0; k < springStages; ++k)
        {
            const float y = a * v + sp.apState[k];
            sp.apState[k] = v - a * y;
            v = y;
        }

        const float m = sp.lfo.next (0.37f / fs) * modAmt * 0.0004f * fs;
        const float r = sp.loop.read (springLengths[index] * fs + m + 2.0f);
        sp.loop.write (softClip (v));
        sp.out = sp.damp.low (r, dampA);
        return sp.out;
    }

    //==============================================================================
    void ReverbEngine::process (const float* inL, const float* inR, float* wetL, float* wetR, float* dryGain,
                                int numSamples, const EngineSettings& settings)
    {
        const bool switching = settings.mode != activeMode;
        if (! switching)
            current = settings;
        else
            current.hold = settings.hold, current.freeze = settings.freeze;

        configure (current);

        const auto& s = current;
        const float* p = s.p;
        const float fadeStep = 1.0f / (0.03f * fs);
        const float envCoeff = 1.0f - std::exp (-1.0f / (0.05f * fs));
        const bool hold = s.hold;
        const bool freeze = hold && s.freeze;

        // Per-mode values that stay the same for the whole block.
        const float springTension = p[2] * 0.01f;
        const int numSprings = std::clamp ((int) std::round (p[0]), 0, 2) + 1;
        const float springDrive = 1.0f + p[1] * 0.08f;
        const float springRt = std::clamp (s.decaySeconds, 0.25f, 10.0f);
        const float springDamp = onePoleCoeff (2500.0f + 6000.0f * s.tone, fs);

        float shimmerRatioA = 2.0f, shimmerRatioB = 0.0f;
        switch ((int) std::round (p[0]))
        {
            case 1: shimmerRatioA = 1.4983f; break;
            case 2: shimmerRatioA = 2.0f; shimmerRatioB = 1.4983f; break;
            case 3: shimmerRatioA = 4.0f; break;
            case 4: shimmerRatioA = 0.5f; break;
            case 5: shimmerRatioA = 0.5f; shimmerRatioB = 2.0f; break;
            default: break;
        }
        const float shimmerAmount = p[1] * 0.01f;

        const float magnetoTarget = std::clamp (p[0], 60.0f, 1000.0f) * 0.001f;
        const int heads = std::clamp ((int) std::round (p[1]), 0, 3) + 1;
        const float repeats = 0.92f * p[2] * 0.01f;

        const float nlTarget = 0.08f + 1.35f * s.decayNorm;
        const float choraleBlend = p[2] * 0.01f;
        const bool swellAll = p[1] >= 0.5f;

        for (int i = 0; i < numSamples; ++i)
        {
            modeFade = switching ? std::max (0.0f, modeFade - fadeStep) : std::min (1.0f, modeFade + fadeStep);

            float xL = inL[i], xR = inR[i];
            dryGain[i] = 1.0f;

            float swell = 1.0f;
            if (activeMode == Swell)
            {
                updateSwell (std::max (std::abs (xL), std::abs (xR)));
                swell = swellGain * swellGain;
                if (swellAll)
                    dryGain[i] = swell;
            }

            inputFade += ((freeze ? 0.0f : 1.0f) - inputFade) * 0.002f;
            const float inScale = inputFade * modeFade * swell;
            xL *= inScale;
            xR *= inScale;

            // Pre-delay: a gap before the reverb starts.
            preDelaySamples += (preDelayTarget - preDelaySamples) * 0.0005f;
            const float dL = preDelayL.readLinear (preDelaySamples);
            const float dR = preDelayR.readLinear (preDelaySamples);
            preDelayL.write (xL);
            preDelayR.write (xR);

            // Early reflections: the first few echoes off the walls.
            float eL = 0.0f, eR = 0.0f;
            if (setup.earlyLevel > 0.0f)
            {
                for (int t = 0; t < numEarlyTaps; ++t)
                {
                    auto& tap = earlyTaps[(size_t) t];
                    tap.delay += (tap.targetDelay - tap.delay) * 0.001f;
                    const float v = earlyLine.readLinear (tap.delay);
                    eL += v * tap.gainL;
                    eR += v * tap.gainR;
                }
                earlyLine.write ((dL + dR) * 0.5f);
            }

            float fdnInL = dL, fdnInR = dR;
            float nativeL = 0.0f, nativeR = 0.0f;   // sound made by a mode's own special part

            switch (activeMode)
            {
                case Spring:
                {
                    const float x = springHp.process ((dL + dR) * 0.5f) * springDrive;
                    const float driven = std::tanh (x) / std::sqrt (springDrive);
                    float o[3] {};
                    for (int k = 0; k < numSprings; ++k)
                    {
                        const float a = (0.35f + 0.4f * springTension) * (0.95f + 0.05f * (float) k);
                        const float fb = decayGain (springLengths[k] + (float) springStages * 0.5f / fs, springRt) * 0.97f;
                        o[k] = processSpring (driven, k, a, fb, springDamp, s.mod * 2.0f);
                    }
                    if (numSprings == 1)      { nativeL = o[0]; nativeR = o[0]; }
                    else if (numSprings == 2) { nativeL = o[0]; nativeR = o[1]; }
                    else                      { nativeL = o[0] + 0.6f * o[2]; nativeR = o[1] + 0.6f * o[2]; }
                    nativeL = springLpL.process (nativeL) * 1.4f;
                    nativeR = springLpR.process (nativeR) * 1.4f;
                    break;
                }
                case Magneto:
                {
                    tapeTime += (magnetoTarget - tapeTime) * 0.0002f;
                    const float wobble = (wow.next (0.55f / fs) * 0.0018f + flutter.next (6.3f / fs) * 0.00025f) * s.mod * 2.0f;
                    float sumL = 0.0f, sumR = 0.0f, last = 0.0f;
                    for (int h = 1; h <= heads; ++h)
                    {
                        const float d = (tapeTime * (float) h / (float) heads + wobble) * fs;
                        const float v = tape.read (d);
                        if (h % 2 == 1) { sumL += v; sumR += 0.55f * v; }
                        else            { sumR += v; sumL += 0.55f * v; }
                        last = v;
                    }
                    const float fb = tapeLp.process (tapeHp.process (last)) * repeats;
                    tape.write (softClip ((dL + dR) * 0.5f + fb));
                    const float headNorm = 1.0f / std::sqrt ((float) heads);
                    const float toneA = onePoleCoeff (4500.0f, fs);
                    nativeL = echoToneL.low (sumL * headNorm, toneA);
                    nativeR = echoToneR.low (sumR * headNorm, toneA);
                    fdnInL = nativeL * 0.8f + dL * 0.25f;
                    fdnInR = nativeR * 0.8f + dR * 0.25f;
                    nativeL *= 0.7f;
                    nativeR *= 0.7f;
                    break;
                }
                case NonLinear:
                {
                    nlLength += (nlTarget - nlLength) * 0.0003f;
                    const float len = nlLength * fs;
                    float l = 0.0f, r = 0.0f;
                    for (int t = 0; t < maxTaps; ++t)
                    {
                        const auto& tap = nlTaps[(size_t) t];
                        const float v = nlLine.readLinear (tap.targetDelay * len + 1.0f);
                        l += v * tap.gainL;
                        r += v * tap.gainR;
                    }
                    nlLine.write ((dL + dR) * 0.5f);
                    for (int k = 0; k < 4; ++k)
                    {
                        l = nlDiffL[k].process (l, setup.diffusion);
                        r = nlDiffR[k].process (r, setup.diffusion);
                    }
                    nativeL = l * 0.8f;
                    nativeR = r * 0.8f;
                    break;
                }
                default:
                    break;
            }

            // Diffusers smear the input into a dense cloud before it enters the main tail.
            for (int k = 0; k < setup.numDiffusers; ++k)
            {
                fdnInL = diffL[k].process (fdnInL, setup.diffusion);
                fdnInR = diffR[k].process (fdnInR, setup.diffusion);
            }

            if (activeMode == Bloom)
            {
                float bL, bR;
                bloomFdn.process (fdnInL, fdnInR, bL, bR);
                fdnInL = 0.25f * fdnInL + 0.9f * bL;
                fdnInR = 0.25f * fdnInR + 0.9f * bR;
            }

            float shimmerOut = 0.0f;
            if (activeMode == Shimmer)
            {
                // Take the reverb's own sound, shift it up an octave (or other interval) and feed it back in,
                // so each pass climbs higher: the classic shimmer "halo".
                const float src = shimmerLp.process (shimmerHp.process (lastWetMono));
                float shifted = shifterA.process (src, shimmerRatioA);
                if (shimmerRatioB > 0.0f)
                    shifted = 0.65f * (shifted + shifterB.process (src, shimmerRatioB));
                shimmerOut = shifted * shimmerAmount;
                fdnInL += shimmerOut * 0.85f;
                fdnInR += shimmerOut * 0.85f;
            }

            // Main tail.
            float fL, fR;
            if (setup.useFdn)
            {
                fdn.process (fdnInL * injectScale, fdnInR * injectScale, fL, fR);
                fL *= setup.fdnLevel;
                fR *= setup.fdnLevel;
            }
            else
            {
                // Modes without a long tail of their own (Spring, Non-Linear) get one while HOLD is on.
                holdMix += ((hold ? 1.0f : 0.0f) - holdMix) * (hold ? 0.0008f : 0.00004f);
                fdn.process (nativeL * holdMix * injectScale, nativeR * holdMix * injectScale, fL, fR);
                nativeL *= 1.0f - holdMix;
                nativeR *= 1.0f - holdMix;
                fL *= std::min (1.0f, holdMix * 3.0f);
                fR *= std::min (1.0f, holdMix * 3.0f);
            }

            lastWetMono = (fL + fR) * 0.5f;

            float outL = fL + nativeL + eL * setup.earlyLevel;
            float outR = fR + nativeR + eR * setup.earlyLevel;

            // Automatic level control. While HOLD is on, the tail never fades, so we let in only as much
            // new sound as keeps it at a comfortable level. Otherwise it only steps in if the tail gets
            // extremely loud (e.g. Shimmer feeding back on itself).
            outEnv += ((outL * outL + outR * outR) * 0.5f * setup.outputGain * setup.outputGain - outEnv) * envCoeff;
            if ((i & 31) == 0)
            {
                const float want = hold ? std::clamp (2.0f - std::sqrt (outEnv) / 0.12f, 0.0f, 1.0f)
                                        : std::min (1.0f, 1.4f / (std::sqrt (fdn.energy()) + 1.0e-6f));
                injectScale += (want - injectScale) * (want < injectScale ? 0.5f : 0.02f);
            }

            if (activeMode == Chorale)
            {
                static constexpr float formantGain[3] = { 1.0f, 0.7f, 0.45f };
                float vL = 0.0f, vR = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    vL += formantL[k].process (outL) * formantGain[k];
                    vR += formantR[k].process (outR) * formantGain[k];
                }
                outL = outL * (1.0f - choraleBlend) + vL * 2.6f * choraleBlend;
                outR = outR * (1.0f - choraleBlend) + vR * 2.6f * choraleBlend;
            }

            if (activeMode == Shimmer)
            {
                outL += shimmerOut * 0.25f;
                outR += shimmerOut * 0.25f;
            }

            outL = highShelfL.process (lowShelfL.process (dcL.process (outL)));
            outR = highShelfR.process (lowShelfR.process (dcR.process (outR)));

            const float g = setup.outputGain * modeFade;
            wetL[i] = softClip (outL * g * 0.6f) * 1.4f;
            wetR[i] = softClip (outR * g * 0.6f) * 1.4f;
        }

        if (switching && modeFade <= 0.0f)
        {
            activeMode = settings.mode;
            reset();
            current = settings;
        }
    }
}
