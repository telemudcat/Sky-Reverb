#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v)); }

    juce::NormalisableRange<float> skewedRange (float lo, float hi, float centre, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SkyReverbProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbText = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 }, "Input",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    StringArray modeNames;
    for (const auto& m : sky::getModes())
        modeNames.add (m.longName);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeNames, (int) sky::Hall));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 }, "Decay", skewedRange (0.2f, 25.0f, 3.0f), 3.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return v < 10.0f ? String (v, 2) + "s" : String (v, 1) + "s"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "predelay", 1 }, "Pre-Delay", skewedRange (0.0f, 500.0f, 80.0f), 20.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "ms"; })));

    auto percent = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
    };
    percent ("tone", "Tone", 50.0f);
    percent ("mod", "Mod", 50.0f);
    percent ("lowend", "Low End", 50.0f);
    percent ("mix", "Mix", 35.0f);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "Hold", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "holdmode", 1 }, "Hold Type", StringArray { "Infinite", "Freeze" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));

    // Each mode's own knobs (the ones in the black window).
    for (const auto& mode : sky::getModes())
        for (const auto& p : mode.params)
        {
            const String name = String (mode.longName) + " " + String (p.name).toLowerCase();
            if (! p.choices.empty())
            {
                StringArray choices;
                for (auto* c : p.choices)
                    choices.add (c);
                layout.add (std::make_unique<AudioParameterChoice> (ParameterID { p.id, 1 }, name, choices, (int) p.defaultValue));
            }
            else
            {
                const String unit = p.unit;
                const bool seconds = unit == "s";
                layout.add (std::make_unique<AudioParameterFloat> (ParameterID { p.id, 1 }, name,
                    seconds ? skewedRange (p.minValue, p.maxValue, 1.0f) : NormalisableRange<float> (p.minValue, p.maxValue),
                    p.defaultValue,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([unit, seconds] (float v, int)
                        { return seconds ? String (v, 2) + unit : String (roundToInt (v)) + unit; })));
            }
        }

    return layout;
}

SkyReverbProcessor::SkyReverbProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "SKYREVERB", createLayout()),
      presets (params)
{
    inputParam = params.getRawParameterValue ("input");
    outputParam = params.getRawParameterValue ("output");
    modeParam = params.getRawParameterValue ("mode");
    decayParam = params.getRawParameterValue ("decay");
    predelayParam = params.getRawParameterValue ("predelay");
    toneParam = params.getRawParameterValue ("tone");
    modParam = params.getRawParameterValue ("mod");
    lowEndParam = params.getRawParameterValue ("lowend");
    mixParam = params.getRawParameterValue ("mix");
    holdParam = params.getRawParameterValue ("hold");
    holdModeParam = params.getRawParameterValue ("holdmode");
    bypassParam = params.getRawParameterValue ("bypass");

    for (int m = 0; m < sky::NumModes; ++m)
    {
        const auto& spec = sky::getModes()[(size_t) m];
        for (size_t k = 0; k < spec.params.size(); ++k)
            modeParams[(size_t) m][k] = params.getRawParameterValue (spec.params[k].id);
    }

    params.state.setProperty ("uiScale", 1.25f, nullptr);
    params.state.setProperty ("mixLock", false, nullptr);
}

bool SkyReverbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    // Mono in -> stereo out is allowed (a mono guitar track gets a wide stereo reverb).
    return in == juce::AudioChannelSet::mono() || in == out;
}

void SkyReverbProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    for (auto* v : { &inL, &inR, &wetL, &wetR, &dryGain, &rawL, &rawR, &bypassAmt, &levelIn })
        v->assign ((size_t) chunk, 0.0f);

    for (auto* s : { &inGain, &outGain, &dryMix, &wetMix, &bypassFade })
        s->reset (sampleRate, 0.03);
    inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputParam->load()));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    bypassFade.setCurrentAndTargetValue (bypassParam->load() > 0.5f ? 1.0f : 0.0f);
}

void SkyReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numIn == 0 || numOut == 0)
        return;

    // Read the knobs.
    sky::EngineSettings s;
    s.mode = juce::jlimit (0, sky::NumModes - 1, (int) modeParam->load());
    s.decaySeconds = decayParam->load();
    s.decayNorm = params.getParameter ("decay")->getValue();
    s.predelayMs = predelayParam->load();
    s.tone = toneParam->load() * 0.01f;
    s.mod = modParam->load() * 0.01f;
    s.lowEnd = lowEndParam->load() * 0.01f;
    s.hold = holdParam->load() > 0.5f;
    s.freeze = holdModeParam->load() > 0.5f;
    for (int k = 0; k < sky::maxModeParams; ++k)
        if (auto* p = modeParams[(size_t) s.mode][(size_t) k])
            s.p[k] = p->load();

    // Mix: the dry sound stays full until halfway, then fades as the reverb takes over.
    const float mix = mixParam->load() * 0.01f;
    dryMix.setTargetValue (juce::jmin (1.0f, 2.0f * (1.0f - mix)));
    wetMix.setTargetValue (juce::jmin (1.0f, 2.0f * mix));
    inGain.setTargetValue (juce::Decibels::decibelsToGain (inputParam->load()));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    bypassFade.setTargetValue (bypassParam->load() > 0.5f ? 1.0f : 0.0f);

    float* outLp = buffer.getWritePointer (0);
    float* outRp = buffer.getWritePointer (numOut > 1 ? 1 : 0);
    const float* srcL = buffer.getReadPointer (0);
    const float* srcR = buffer.getReadPointer (numIn > 1 ? 1 : 0);

    float inPeak = 0.0f, outPeak = 0.0f;

    for (int start = 0; start < numSamples; start += chunk)
    {
        const int n = juce::jmin (chunk, numSamples - start);

        // Copy the input in (with the Input level applied). While bypassed, nothing new goes into the
        // reverb, but whatever is still ringing carries on (a "trails" bypass, like the real pedal).
        for (int i = 0; i < n; ++i)
        {
            const float g = inGain.getNextValue();
            const float b = bypassFade.getNextValue();
            const float l = srcL[start + i], r = srcR[start + i];
            rawL[(size_t) i] = l;
            rawR[(size_t) i] = r;
            bypassAmt[(size_t) i] = b;
            levelIn[(size_t) i] = g;
            inL[(size_t) i] = l * g * (1.0f - b);
            inR[(size_t) i] = r * g * (1.0f - b);
            inPeak = juce::jmax (inPeak, std::abs (l * g), std::abs (r * g));
        }

        engine.process (inL.data(), inR.data(), wetL.data(), wetR.data(), dryGain.data(), n, s);

        const bool monoOut = numOut == 1;
        for (int i = 0; i < n; ++i)
        {
            const float b = bypassAmt[(size_t) i];
            const float og = outGain.getNextValue();
            const float dm = dryMix.getNextValue() * dryGain[(size_t) i];
            const float wm = wetMix.getNextValue();
            const float ig = levelIn[(size_t) i];

            const float l = (1.0f - b) * rawL[(size_t) i] * ig * dm * og + b * rawL[(size_t) i] + wetL[(size_t) i] * wm * og;
            const float r = (1.0f - b) * rawR[(size_t) i] * ig * dm * og + b * rawR[(size_t) i] + wetR[(size_t) i] * wm * og;

            if (monoOut)
                outLp[start + i] = 0.5f * (l + r);
            else
            {
                outLp[start + i] = l;
                outRp[start + i] = r;
            }
            outPeak = juce::jmax (outPeak, std::abs (l), std::abs (r));
        }
    }

    inputPeak = juce::jmax (inPeak, inputPeak.load() * 0.8f);
    outputPeak = juce::jmax (outPeak, outputPeak.load() * 0.8f);
}

//==============================================================================
int SkyReverbProcessor::getNumPrograms() { return (int) PresetManager::getFactoryPresets().size(); }
int SkyReverbProcessor::getCurrentProgram() { return juce::jmin (presets.getCurrentIndex(), getNumPrograms() - 1); }
void SkyReverbProcessor::setCurrentProgram (int index) { presets.loadPreset (index, isMixLocked()); }
const juce::String SkyReverbProcessor::getProgramName (int index) { return presets.getPresetName (index); }

float SkyReverbProcessor::getUiScale() const { return (float) params.state.getProperty ("uiScale", 1.25f); }
void SkyReverbProcessor::setUiScale (float s) { params.state.setProperty ("uiScale", s, nullptr); }
bool SkyReverbProcessor::isMixLocked() const { return (bool) params.state.getProperty ("mixLock", false); }
void SkyReverbProcessor::setMixLocked (bool b) { params.state.setProperty ("mixLock", b, nullptr); }

void SkyReverbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    params.state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    params.state.setProperty ("presetIndex", presets.getCurrentIndex(), nullptr);
    if (auto xml = params.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SkyReverbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (params.state.getType()))
        {
            params.replaceState (juce::ValueTree::fromXml (*xml));
            presets.setCurrent ((int) params.state.getProperty ("presetIndex", 0),
                                params.state.getProperty ("presetName", "Init Hall").toString());
        }
}

juce::AudioProcessorEditor* SkyReverbProcessor::createEditor() { return new SkyReverbEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SkyReverbProcessor(); }
