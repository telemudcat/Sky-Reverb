#pragma once

#include "DspBlocks.h"
#include "../ModeParams.h"
#include <array>

namespace sky
{
    // Everything the reverb needs to know from the knobs, gathered once per audio block.
    struct EngineSettings
    {
        int mode = Hall;
        float decaySeconds = 3.0f;
        float decayNorm = 0.5f;     // Decay knob position, 0 (fully left) to 1 (fully right)
        float predelayMs = 20.0f;
        float tone = 0.5f;          // 0..1
        float mod = 0.5f;           // 0..1
        float lowEnd = 0.5f;        // 0..1
        bool hold = false;          // the HOLD button
        bool freeze = false;        // switch set to FREEZE (otherwise INFINITE)
        float p[maxModeParams] {};  // the current mode's own knobs, in their real units
    };

    // All 12 reverb modes. Takes the (already gain-adjusted) input and produces only the reverb ("wet")
    // sound. dryGain is filled with how loud the untouched ("dry") sound should be, which only the
    // Swell mode changes.
    class ReverbEngine
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void process (const float* inL, const float* inR, float* wetL, float* wetR, float* dryGain,
                      int numSamples, const EngineSettings& settings);

    private:
        static constexpr int maxDiffusers = 8;
        static constexpr int maxTaps = 48;
        static constexpr int maxSprings = 3;
        static constexpr int springStages = 32;

        struct ModeSetup
        {
            bool useFdn = true;
            float fdnLevel = 1.0f;
            int numDiffusers = 4;
            float diffusion = 0.6f;
            float earlyLevel = 0.0f;
            float outputGain = 1.0f;
        };

        struct Tap { float targetDelay = 0, delay = 0, gainL = 0, gainR = 0; };

        struct SpringState
        {
            float apState[springStages] {};
            dsp::DelayLine loop;
            dsp::OnePole damp;
            dsp::Lfo lfo;
            float out = 0.0f;
        };

        void configure (const EngineSettings&);
        void buildRoomReflections (float width, float length, float height, float listenerPos,
                                   float absorb, float scaleDelay);
        void buildNonLinearTaps (int shape);
        float processSpring (float x, int springIndex, float a, float feedback, float dampA, float modAmt);
        void updateSwell (float level);

        float fs = 44100.0f;
        int activeMode = Hall;
        float modeFade = 1.0f;
        ModeSetup setup;
        EngineSettings current;

        // Shared parts.
        dsp::DelayLine preDelayL, preDelayR;
        float preDelaySamples = 0.0f, preDelayTarget = 0.0f;
        dsp::Allpass diffL[maxDiffusers], diffR[maxDiffusers];
        dsp::Fdn fdn;
        dsp::Biquad lowShelfL, lowShelfR, highShelfL, highShelfR, dcL, dcR;

        // Early reflections (Room, Hall, Reflections).
        dsp::DelayLine earlyLine;
        std::array<Tap, maxTaps> earlyTaps {};
        int numEarlyTaps = 0;
        float earlyKey = -1.0f;

        // Hold / Infinite / Freeze.
        float inputFade = 1.0f;     // falls to 0 while frozen so nothing new gets in
        float holdMix = 0.0f;       // for modes without the main tail: how much of the hold tail we hear
        float injectScale = 1.0f;   // automatic level control so Infinite never runs away
        float outEnv = 0.0f;        // recent loudness of the reverb

        // Spring.
        std::array<SpringState, maxSprings> springs;
        dsp::Biquad springHp, springLpL, springLpR;

        // Swell.
        float envFast = 0.0f, envSlow = 0.0f, swellGain = 0.0f;
        int swellStage = 0, silentSamples = 0, retriggerHold = 0;

        // Bloom.
        dsp::Fdn bloomFdn;

        // Chorale.
        dsp::Biquad formantL[3], formantR[3];
        float formantKey = -1.0f;

        // Shimmer.
        dsp::PitchShifter shifterA, shifterB;
        dsp::Biquad shimmerHp, shimmerLp;
        float lastWetMono = 0.0f;

        // Magneto (tape echo).
        dsp::DelayLine tape;
        dsp::Lfo wow, flutter;
        dsp::Biquad tapeHp, tapeLp;
        dsp::OnePole echoToneL, echoToneR;
        float tapeTime = 0.35f;

        // Non-Linear.
        dsp::DelayLine nlLine;
        std::array<Tap, maxTaps> nlTaps {};
        float nlLength = 0.5f;
        int nlShape = -1;
        dsp::Allpass nlDiffL[4], nlDiffR[4];

        dsp::Noise noise;
    };
}
