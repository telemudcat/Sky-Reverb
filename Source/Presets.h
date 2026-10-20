#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets (built in) and your own saved presets (files in Documents/Sky Reverb Presets).
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    struct Factory
    {
        juce::String name;
        int mode;
        std::vector<std::pair<juce::String, float>> values; // parameter id -> value in real units
    };

    static const std::vector<Factory>& getFactoryPresets();

    static juce::File getUserFolder();
    juce::StringArray getUserPresetNames() const;

    // One combined list: factory presets first, then yours.
    int getNumPresets() const;
    juce::String getPresetName (int index) const;
    bool isUserPreset (int index) const;
    void loadPreset (int index, bool keepMix);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);

    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentName() const { return currentName; }
    void setCurrent (int index, const juce::String& name) { currentIndex = index; currentName = name; }

    // Parameters a preset never touches (levels, bypass, the HOLD button).
    static bool isPerformanceParam (const juce::String& id);

private:
    void setParam (const juce::String& id, float realValue);

    juce::AudioProcessorValueTreeState& state;
    int currentIndex = 0;
    juce::String currentName;
};
