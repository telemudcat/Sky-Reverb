#include "Presets.h"
#include "ModeParams.h"

namespace
{
    const juce::String presetExtension = ".skypreset";
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
{
    currentName = getFactoryPresets().front().name;
}

const std::vector<PresetManager::Factory>& PresetManager::getFactoryPresets()
{
    using namespace sky;
    // Values are in the same units the knobs show (seconds, ms, 0-100). Choice knobs use their position
    // (0 = first choice). Anything not listed goes back to its default.
    static const std::vector<Factory> list {
        { "Init Hall",          Hall,        {} },
        { "Small Studio",       Room,        { { "decay", 0.8f }, { "predelay", 5.0f }, { "tone", 55.0f }, { "mod", 30.0f }, { "mix", 25.0f }, { "room_size", 30.0f } } },
        { "Drum Room",          Room,        { { "decay", 1.2f }, { "predelay", 8.0f }, { "tone", 60.0f }, { "mix", 30.0f }, { "room_size", 55.0f }, { "room_early", 85.0f } } },
        { "Concert Hall",       Hall,        { { "decay", 3.2f }, { "predelay", 30.0f }, { "tone", 45.0f }, { "mix", 35.0f }, { "hall_size", 70.0f } } },
        { "Cathedral",          Hall,        { { "decay", 8.0f }, { "predelay", 60.0f }, { "tone", 40.0f }, { "mod", 60.0f }, { "mix", 40.0f }, { "hall_size", 100.0f }, { "hall_early", 25.0f } } },
        { "Vocal Plate",        Plate,       { { "decay", 2.2f }, { "predelay", 40.0f }, { "tone", 62.0f }, { "mix", 30.0f }, { "plate_size", 50.0f } } },
        { "Snare Plate",        Plate,       { { "decay", 1.4f }, { "predelay", 10.0f }, { "tone", 70.0f }, { "mix", 30.0f }, { "plate_size", 35.0f } } },
        { "Surf Tank",          Spring,      { { "decay", 2.5f }, { "predelay", 0.0f }, { "tone", 55.0f }, { "mix", 45.0f }, { "spring_springs", 2.0f }, { "spring_dwell", 75.0f }, { "spring_tension", 40.0f } } },
        { "Amp Spring",         Spring,      { { "decay", 1.6f }, { "predelay", 0.0f }, { "tone", 50.0f }, { "mix", 30.0f }, { "spring_springs", 1.0f }, { "spring_dwell", 40.0f }, { "spring_tension", 60.0f } } },
        { "Violin Swell",       Swell,       { { "decay", 4.0f }, { "predelay", 20.0f }, { "mix", 50.0f }, { "swell_rise", 1.2f }, { "swell_type", 0.0f } } },
        { "Ambient Fade In",    Swell,       { { "decay", 6.0f }, { "predelay", 40.0f }, { "tone", 45.0f }, { "mix", 55.0f }, { "swell_rise", 2.5f }, { "swell_type", 1.0f } } },
        { "Blooming Pad",       Bloom,       { { "decay", 6.0f }, { "predelay", 20.0f }, { "mix", 45.0f }, { "bloom_length", 60.0f }, { "bloom_feedback", 70.0f } } },
        { "Slow Flower",        Bloom,       { { "decay", 10.0f }, { "predelay", 30.0f }, { "tone", 40.0f }, { "mix", 45.0f }, { "bloom_length", 90.0f }, { "bloom_feedback", 55.0f } } },
        { "Endless Cloud",      Cloud,       { { "decay", 12.0f }, { "predelay", 40.0f }, { "mod", 60.0f }, { "mix", 50.0f }, { "cloud_diffusion", 80.0f } } },
        { "Soft Haze",          Cloud,       { { "decay", 5.0f }, { "predelay", 20.0f }, { "tone", 40.0f }, { "mix", 40.0f }, { "cloud_diffusion", 40.0f } } },
        { "Choir Ahh",          Chorale,     { { "decay", 6.0f }, { "predelay", 30.0f }, { "mix", 45.0f }, { "chorale_vowel", 0.0f }, { "chorale_voice", 0.0f }, { "chorale_blend", 60.0f } } },
        { "Angel Ooh",          Chorale,     { { "decay", 8.0f }, { "predelay", 30.0f }, { "tone", 55.0f }, { "mix", 50.0f }, { "chorale_vowel", 4.0f }, { "chorale_voice", 1.0f }, { "chorale_blend", 75.0f } } },
        { "Heavenly Octave",    Shimmer,     { { "decay", 8.0f }, { "predelay", 40.0f }, { "tone", 55.0f }, { "mix", 45.0f }, { "shimmer_pitch", 0.0f }, { "shimmer_amount", 55.0f } } },
        { "Fifth Heaven",       Shimmer,     { { "decay", 7.0f }, { "predelay", 30.0f }, { "mix", 45.0f }, { "shimmer_pitch", 2.0f }, { "shimmer_amount", 45.0f } } },
        { "Low Shimmer",        Shimmer,     { { "decay", 5.0f }, { "predelay", 20.0f }, { "tone", 40.0f }, { "lowend", 60.0f }, { "mix", 40.0f }, { "shimmer_pitch", 4.0f }, { "shimmer_amount", 40.0f } } },
        { "Tape Echo Verb",     Magneto,     { { "decay", 2.5f }, { "predelay", 0.0f }, { "mix", 40.0f }, { "magneto_time", 380.0f }, { "magneto_heads", 2.0f }, { "magneto_repeats", 45.0f } } },
        { "Slapback Hall",      Magneto,     { { "decay", 1.8f }, { "predelay", 0.0f }, { "mod", 30.0f }, { "mix", 35.0f }, { "magneto_time", 120.0f }, { "magneto_heads", 0.0f }, { "magneto_repeats", 15.0f } } },
        { "Reverse Swell",      NonLinear,   { { "decay", 4.0f }, { "predelay", 0.0f }, { "mix", 45.0f }, { "nl_shape", 0.0f }, { "nl_diffusion", 70.0f } } },
        { "80s Gated Snare",    NonLinear,   { { "decay", 1.2f }, { "predelay", 0.0f }, { "tone", 60.0f }, { "mix", 35.0f }, { "nl_shape", 1.0f }, { "nl_diffusion", 85.0f } } },
        { "Studio Walls",       Reflections, { { "decay", 0.6f }, { "predelay", 0.0f }, { "mix", 30.0f }, { "refl_shape", 0.0f }, { "refl_position", 40.0f } } },
        { "Long Tunnel",        Reflections, { { "decay", 2.0f }, { "predelay", 0.0f }, { "mix", 35.0f }, { "refl_shape", 3.0f }, { "refl_position", 70.0f } } },
    };
    return list;
}

bool PresetManager::isPerformanceParam (const juce::String& id)
{
    return id == "input" || id == "output" || id == "bypass" || id == "hold";
}

juce::File PresetManager::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sky Reverb Presets");
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : getUserFolder().findChildFiles (juce::File::findFiles, false, "*" + presetExtension))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

int PresetManager::getNumPresets() const
{
    return (int) getFactoryPresets().size() + getUserPresetNames().size();
}

juce::String PresetManager::getPresetName (int index) const
{
    const int numFactory = (int) getFactoryPresets().size();
    if (index < numFactory)
        return getFactoryPresets()[(size_t) juce::jmax (0, index)].name;
    return getUserPresetNames()[index - numFactory];
}

bool PresetManager::isUserPreset (int index) const
{
    return index >= (int) getFactoryPresets().size();
}

void PresetManager::setParam (const juce::String& id, float realValue)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void PresetManager::loadPreset (int index, bool keepMix)
{
    const int numFactory = (int) getFactoryPresets().size();
    auto shouldSkip = [keepMix] (const juce::String& id)
    {
        return isPerformanceParam (id) || (keepMix && id == "mix");
    };

    if (index < numFactory)
    {
        const auto& preset = getFactoryPresets()[(size_t) juce::jmax (0, index)];

        // Everything back to default first, so presets always sound the same.
        for (auto* p : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! shouldSkip (ranged->getParameterID()))
                {
                    ranged->beginChangeGesture();
                    ranged->setValueNotifyingHost (ranged->getDefaultValue());
                    ranged->endChangeGesture();
                }

        setParam ("mode", (float) preset.mode);
        for (const auto& [id, value] : preset.values)
            if (! shouldSkip (id))
                setParam (id, value);

        setCurrent (index, preset.name);
        return;
    }

    const auto name = getPresetName (index);
    auto xml = juce::XmlDocument::parse (getUserFolder().getChildFile (name + presetExtension));
    if (xml == nullptr)
        return;

    for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = child->getStringAttribute ("id");
        if (! shouldSkip (id))
            setParam (id, (float) child->getDoubleAttribute ("value"));
    }
    setCurrent (index, name);
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    auto folder = getUserFolder();
    if (! folder.createDirectory())
        return false;

    juce::XmlElement xml ("SKYPRESET");
    for (auto* p : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isPerformanceParam (ranged->getParameterID()))
            {
                auto* child = xml.createNewChildElement ("PARAM");
                child->setAttribute ("id", ranged->getParameterID());
                child->setAttribute ("value", ranged->convertFrom0to1 (ranged->getValue()));
            }

    if (! xml.writeTo (folder.getChildFile (name + presetExtension)))
        return false;

    const int index = (int) getFactoryPresets().size() + getUserPresetNames().indexOf (name);
    setCurrent (index, name);
    return true;
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    const bool ok = getUserFolder().getChildFile (name + presetExtension).deleteFile();
    if (ok && name == currentName)
        setCurrent (0, getFactoryPresets().front().name);
    return ok;
}
