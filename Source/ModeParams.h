#pragma once

#include <array>
#include <vector>

// The 12 reverb modes, and the extra knobs each one shows in the black parameter window.
// This single table is used both for the plugin's parameter list and for the screen.
namespace sky
{
    enum Mode
    {
        Room = 0, Hall, Plate, Spring, Swell, Bloom, Cloud, Chorale, Shimmer, Magneto, NonLinear, Reflections,
        NumModes
    };

    struct ModeParamSpec
    {
        const char* id;
        const char* name;
        float minValue, maxValue, defaultValue;
        const char* unit;                    // shown after the number, e.g. "ms"
        std::vector<const char*> choices;    // if not empty, this knob clicks between named settings
    };

    struct ModeSpec
    {
        const char* name;       // as printed next to the mode dial
        const char* longName;   // used in menus
        std::vector<ModeParamSpec> params;
    };

    constexpr int maxModeParams = 3;

    inline const std::array<ModeSpec, NumModes>& getModes()
    {
        static const std::array<ModeSpec, NumModes> modes {{
            { "ROOM", "Room", {
                { "room_size",      "SIZE",       0.0f, 100.0f, 45.0f, "", {} },
                { "room_diffusion", "DIFFUSION",  0.0f, 100.0f, 70.0f, "", {} },
                { "room_early",     "EARLY LEVEL",0.0f, 100.0f, 60.0f, "", {} } } },
            { "HALL", "Hall", {
                { "hall_size",      "SIZE",       0.0f, 100.0f, 55.0f, "", {} },
                { "hall_early",     "EARLY LEVEL",0.0f, 100.0f, 40.0f, "", {} },
                { "hall_diffusion", "DIFFUSION",  0.0f, 100.0f, 70.0f, "", {} } } },
            { "PLATE", "Plate", {
                { "plate_size",     "SIZE",       0.0f, 100.0f, 55.0f, "", {} },
                { "plate_diffusion","DIFFUSION",  0.0f, 100.0f, 80.0f, "", {} } } },
            { "SPRING", "Spring", {
                { "spring_springs", "SPRINGS",    0.0f, 2.0f, 2.0f, "", { "1", "2", "3" } },
                { "spring_dwell",   "DWELL",      0.0f, 100.0f, 50.0f, "", {} },
                { "spring_tension", "TENSION",    0.0f, 100.0f, 50.0f, "", {} } } },
            { "SWELL", "Swell", {
                { "swell_rise",     "RISE TIME",  0.05f, 4.0f, 0.9f, "s", {} },
                { "swell_type",     "SWELL",      0.0f, 1.0f, 0.0f, "", { "REVERB", "ALL" } } } },
            { "BLOOM", "Bloom", {
                { "bloom_length",   "LENGTH",     0.0f, 100.0f, 50.0f, "", {} },
                { "bloom_feedback", "FEEDBACK",   0.0f, 100.0f, 50.0f, "", {} } } },
            { "CLOUD", "Cloud", {
                { "cloud_diffusion","DIFFUSION",  0.0f, 100.0f, 50.0f, "", {} } } },
            { "CHORALE", "Chorale", {
                { "chorale_vowel",  "VOWEL",      0.0f, 4.0f, 0.0f, "", { "AH", "EH", "EE", "OH", "OO" } },
                { "chorale_voice",  "VOICE",      0.0f, 1.0f, 0.0f, "", { "LOW", "HIGH" } },
                { "chorale_blend",  "VOICE MIX",  0.0f, 100.0f, 60.0f, "", {} } } },
            { "SHIMMER", "Shimmer", {
                { "shimmer_pitch",  "PITCH",      0.0f, 5.0f, 0.0f, "",
                    { "+OCT", "+5TH", "+OCT +5TH", "+2 OCT", "-OCT", "-OCT +OCT" } },
                { "shimmer_amount", "AMOUNT",     0.0f, 100.0f, 50.0f, "", {} } } },
            { "MAGNETO", "Magneto", {
                { "magneto_time",   "TIME",       60.0f, 1000.0f, 350.0f, "ms", {} },
                { "magneto_heads",  "HEADS",      0.0f, 3.0f, 3.0f, "", { "1", "2", "3", "4" } },
                { "magneto_repeats","REPEATS",    0.0f, 100.0f, 40.0f, "", {} } } },
            { "NON-LINEAR", "Non-Linear", {
                { "nl_shape",       "SHAPE",      0.0f, 3.0f, 0.0f, "", { "REVERSE", "GATED", "SWOOSH", "RAMP" } },
                { "nl_diffusion",   "DIFFUSION",  0.0f, 100.0f, 70.0f, "", {} } } },
            { "REFLECTIONS", "Reflections", {
                { "refl_shape",     "ROOM",       0.0f, 3.0f, 0.0f, "", { "STUDIO", "CHAMBER", "WIDE", "TUNNEL" } },
                { "refl_position",  "POSITION",   0.0f, 100.0f, 50.0f, "", {} } } },
        }};
        return modes;
    }
}
