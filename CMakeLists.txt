# Build recipe for the "Sky Reverb" plugin.
# CMake reads this file and works out how to compile everything.

cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Oldest macOS the plugin runs on")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build for both Apple Silicon and Intel Macs")
project(SkyReverb VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download the JUCE audio framework automatically (no manual install needed).
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# The plugin formats to build: VST3 everywhere, plus AU (Logic, GarageBand) on Macs.
set(SKY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SKY_FORMATS AU)
endif()

juce_add_plugin(SkyReverb
    COMPANY_NAME             "Kevin"
    PLUGIN_MANUFACTURER_CODE Kvn1
    PLUGIN_CODE              Sky1
    PRODUCT_NAME             "Sky Reverb"
    FORMATS                  ${SKY_FORMATS}
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    VST3_CATEGORIES          Fx Reverb
    AU_MAIN_TYPE             kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(SkyReverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/dsp/ReverbEngine.cpp)

target_compile_definitions(SkyReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SkyReverb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional sound test (not part of the plugin): renders every mode and checks for
# silence, runaway volume and broken numbers. Turn on with -DSKY_BUILD_TESTS=ON.
option(SKY_BUILD_TESTS "Build the offline sound test" OFF)
if(SKY_BUILD_TESTS)
    add_executable(SkyReverbTest tests/EngineTest.cpp Source/dsp/ReverbEngine.cpp)
endif()
