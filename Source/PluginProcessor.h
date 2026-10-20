#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ReverbEngine.h"
#include "Presets.h"

// The "brain" of the plugin: holds the knobs, the presets, and runs the reverb.
class SkyReverbProcessor : public juce::AudioProcessor
{
public:
    SkyReverbProcessor();
    ~SkyReverbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 25.0; }

    // Lets the DAW's own bypass button and the plugin's BYPASS button stay in sync.
    juce::AudioProcessorParameter* getBypassParameter() const override { return params.getParameter ("bypass"); }

    // Factory presets also show up in the DAW's own preset list ("programs" in plugin-speak).
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // Saving/loading everything with your song.
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState params;
    PresetManager presets;

    // Window size (1.0 = 100%) and the little padlock next to Mix. Saved with the song.
    float getUiScale() const;
    void setUiScale (float);
    bool isMixLocked() const;
    void setMixLocked (bool);

    // Recent peak levels for the little lights next to INPUT and OUTPUT.
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

private:
    sky::ReverbEngine engine;
    static constexpr int chunk = 512;
    std::vector<float> inL, inR, wetL, wetR, dryGain, rawL, rawR, bypassAmt, levelIn;

    juce::SmoothedValue<float> inGain, outGain, dryMix, wetMix, bypassFade;

    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* predelayParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* modParam = nullptr;
    std::atomic<float>* lowEndParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* holdModeParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::array<std::array<std::atomic<float>*, sky::maxModeParams>, sky::NumModes> modeParams {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkyReverbProcessor)
};
