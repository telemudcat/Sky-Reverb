#pragma once

#include "PluginProcessor.h"

// Draws knobs, buttons and menus in the look of a blue hardware reverb pedal.
class SkyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
};

// A knob with its value printed above it and its name below, like on the pedal.
class LabeledKnob : public juce::Component
{
public:
    enum class Style { Big, Small, Mini };

    LabeledKnob();
    void attach (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name);
    void detach();
    void setStyle (Style s) { style = s; resized(); }
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String name;
    Style style = Style::Big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// The big 12-position mode dial with its ring of lights and mode names.
class ModeSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModeSelector (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    int getMode() const { return mode; }

private:
    float angleFor (int index) const;
    juce::Point<float> centre() const;
    juce::Rectangle<float> labelBounds (int index) const;
    void setMode (int m);

    juce::ParameterAttachment attachment;
    int mode = 1, dragStartMode = 1;
    float wheelAccum = 0.0f;
    bool dragging = false;
};

// Vertical flip switch for INFINITE (up) / FREEZE (down).
class LeverSwitch : public juce::Button
{
public:
    LeverSwitch() : juce::Button ("Infinite / Freeze") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// The square HOLD button with a light in it.
class HoldButton : public juce::Button
{
public:
    HoldButton() : juce::Button ("Hold") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Small padlock next to Mix: when locked, changing presets keeps your Mix setting.
class LockButton : public juce::Button
{
public:
    LockButton() : juce::Button ("Mix lock") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Green-on-black preset name display. Click it for the preset menu.
class PresetDisplay : public juce::Button
{
public:
    PresetDisplay() : juce::Button ("Presets") {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::String text;
};

// Everything you see, drawn at a fixed "100%" size and then scaled up or down by the SIZE menu.
class SkyPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseWidth = 820, baseHeight = 330;

    explicit SkyPanel (SkyReverbProcessor&);
    ~SkyPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (float)> onScaleChange;

private:
    void timerCallback() override;
    void rebuildModeKnobs (int mode);
    void showPresetMenu();
    void stepPreset (int delta);
    void savePreset();
    void makeBrushedTexture();

    SkyReverbProcessor& processor;
    SkyLookAndFeel look;

    LabeledKnob input, output;
    LabeledKnob decay, predelay, tone, mod, lowEnd, mix;
    std::array<LabeledKnob, sky::maxModeParams> modeKnobs;
    int shownMode = -1, numModeKnobs = 0;

    ModeSelector modeSelector;
    LeverSwitch holdType;
    HoldButton hold;
    LockButton mixLock;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdTypeAttachment, holdAttachment, bypassAttachment;

    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, save { "SAVE" }, bypass { "BYPASS" };
    PresetDisplay presetDisplay;
    juce::ComboBox sizeBox;

    juce::Image brushed;
    juce::Rectangle<int> paramWindow, inputLed, outputLed;
    float shownIn = 0.0f, shownOut = 0.0f;
    bool shownHold = false, shownFreeze = false;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkyPanel)
};

// The plugin window. It just holds the panel and resizes it.
class SkyReverbEditor : public juce::AudioProcessorEditor
{
public:
    explicit SkyReverbEditor (SkyReverbProcessor&);
    void resized() override;

private:
    void applyScale (float);

    SkyReverbProcessor& processor;
    SkyPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkyReverbEditor)
};
