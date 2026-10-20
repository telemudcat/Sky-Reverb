#include "PluginEditor.h"

namespace
{
    const juce::Colour blueTop       { 0xff2b93d6 };
    const juce::Colour blueBottom    { 0xff1a6ead };
    const juce::Colour barTop        { 0xff2e2e30 };
    const juce::Colour barBottom     { 0xff1c1c1e };
    const juce::Colour lcdGreen      { 0xff4dff4d };
    const juce::Colour ledGreen      { 0xff7dff3c };
    const juce::Colour ledOff        { 0xff0f4f78 };
    const juce::Colour textWhite     { 0xfff7f9fb };
    const juce::Colour windowTop     { 0xff1d1d1f };
    const juce::Colour windowBottom  { 0xff0e0e0f };

    constexpr float knobStart = juce::MathConstants<float>::twoPi - 2.4f;
    constexpr float knobEnd   = juce::MathConstants<float>::twoPi + 2.4f;

    const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

    juce::Point<float> onCircle (juce::Point<float> c, float r, float angle)
    {
        return c + juce::Point<float> (r * std::sin (angle), -r * std::cos (angle));
    }

    juce::Font labelFont (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

    juce::Font valueFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
    }

    // A black, slightly shiny knob with a ridged edge and a white pointer line.
    void drawBlackKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle, bool triangle)
    {
        auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);

        // Shadow on the panel.
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (body.translated (r * 0.08f, r * 0.16f).expanded (r * 0.04f));

        // Ridged outer ring.
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3c3f), c.x, body.getY(),
                                                 juce::Colour (0xff060606), c.x, body.getBottom(), false));
        g.fillEllipse (body);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        for (int i = 0; i < 48; ++i)
        {
            const float a = (float) i / 48.0f * juce::MathConstants<float>::twoPi;
            g.drawLine ({ onCircle (c, r * 0.84f, a), onCircle (c, r * 0.99f, a) }, 0.8f);
        }

        // Smooth top with a soft highlight.
        auto top = body.reduced (r * 0.17f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a2d), c.x, top.getY(),
                                                 juce::Colour (0xff0b0b0c), c.x, top.getBottom(), false));
        g.fillEllipse (top);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), c.x - r * 0.25f, top.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), c.x, c.y, true));
        g.fillEllipse (top);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (top, 0.8f);

        g.setColour (textWhite);
        if (triangle)
        {
            juce::Path tri;
            tri.addTriangle (0.0f, -r * 0.62f, -r * 0.2f, -r * 0.2f, r * 0.2f, -r * 0.2f);
            g.fillPath (tri, juce::AffineTransform::rotation (angle).translated (c));
        }
        else
        {
            g.drawLine ({ onCircle (c, r * 0.18f, angle), onCircle (c, r * 0.8f, angle) }, juce::jmax (1.5f, r * 0.09f));
        }
    }
}

//==============================================================================
SkyLookAndFeel::SkyLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1e1e20));
    setColour (juce::PopupMenu::textColourId, textWhite);
    setColour (juce::PopupMenu::headerTextColourId, lcdGreen);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, blueBottom);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, lcdGreen);
    setColour (juce::ComboBox::arrowColourId, lcdGreen);
    setColour (juce::TextButton::textColourOffId, textWhite);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffb340));
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1e1e20));
    setColour (juce::AlertWindow::textColourId, textWhite);
    setColour (juce::AlertWindow::outlineColourId, blueTop);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::black);
    setColour (juce::TextEditor::textColourId, lcdGreen);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff555555));
    setColour (juce::TextEditor::focusedOutlineColourId, blueTop);
}

void SkyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                       float startAngle, float endAngle, juce::Slider&)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    drawBlackKnob (g, area.getCentre(), r, startAngle + sliderPos * (endAngle - startAngle), false);
}

void SkyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                           bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff151515 : (highlighted ? 0xff4a4a4d : 0xff3a3a3d)), 0.0f, r.getY(),
                                             juce::Colour (down ? 0xff202020 : 0xff222224), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (r.getX() + 3.0f, r.getY() + 1.0f, r.getRight() - 3.0f, r.getY() + 1.0f, 1.0f);
}

void SkyLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    g.setFont (labelFont (juce::jmin (13.0f, (float) b.getHeight() * 0.55f)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void SkyLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 12.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (lcdGreen);
    g.fillPath (arrow);
}

juce::Font SkyLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return valueFont (juce::jmin (14.0f, (float) box.getHeight() * 0.6f));
}

void SkyLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 1, box.getWidth() - 18, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

juce::Font SkyLookAndFeel::getPopupMenuFont() { return labelFont (15.0f); }

//==============================================================================
LabeledKnob::LabeledKnob()
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (knobStart, knobEnd, true);
    slider.setMouseDragSensitivity (220);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void LabeledKnob::attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& newName)
{
    attachment.reset();
    name = newName;
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64));
    }
    repaint();
}

void LabeledKnob::detach()
{
    attachment.reset();
    name.clear();
}

void LabeledKnob::resized()
{
    auto b = getLocalBounds();
    if (style == Style::Mini)
    {
        slider.setBounds (b.removeFromLeft (b.getHeight()));
        return;
    }
    const int valueH = style == Style::Big ? 18 : 16;
    const int nameH = style == Style::Big ? 22 : 18;
    b.removeFromTop (valueH);
    b.removeFromBottom (nameH);
    const int side = juce::jmin (b.getWidth(), b.getHeight());
    slider.setBounds (b.withSizeKeepingCentre (side, side));
}

void LabeledKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    if (style == Style::Mini)
    {
        b.removeFromLeft (b.getHeight() + 4);
        g.setColour (textWhite);
        g.setFont (labelFont (12.0f));
        g.drawText (name, b, juce::Justification::centredLeft);
        return;
    }

    const bool big = style == Style::Big;
    g.setColour (textWhite);
    g.setFont (valueFont (big ? 14.0f : 13.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), b.removeFromTop (big ? 18 : 16), juce::Justification::centred);
    g.setFont (labelFont (big ? 15.0f : 13.0f));
    g.drawText (name, b.removeFromBottom (big ? 22 : 18), juce::Justification::centred);
}

//==============================================================================
ModeSelector::ModeSelector (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v) { mode = juce::jlimit (0, sky::NumModes - 1, juce::roundToInt (v)); repaint(); })
{
    attachment.sendInitialUpdate();
    setTooltip ("Reverb mode: drag the knob, scroll, or click a name");
}

float ModeSelector::angleFor (int index) const
{
    const float spread = juce::degreesToRadians (150.0f);
    return -spread + (float) index * (2.0f * spread / (float) (sky::NumModes - 1));
}

juce::Point<float> ModeSelector::centre() const
{
    return { 112.0f, getHeight() * 0.5f + 6.0f };
}

juce::Rectangle<float> ModeSelector::labelBounds (int index) const
{
    const float a = angleFor (index);
    const auto p = onCircle (centre(), 50.0f, a);
    const float w = 92.0f, h = 16.0f;
    const float s = std::sin (a);
    if (std::abs (s) < 0.3f) // the two at the top
        return { p.x - w * 0.5f + s * 60.0f, p.y - h * 0.5f - 4.0f, w, h };
    return s < 0.0f ? juce::Rectangle<float> (p.x - w, p.y - h * 0.5f, w, h)
                    : juce::Rectangle<float> (p.x, p.y - h * 0.5f, w, h);
}

void ModeSelector::paint (juce::Graphics& g)
{
    const auto c = centre();
    const auto& modes = sky::getModes();

    for (int i = 0; i < sky::NumModes; ++i)
    {
        const float a = angleFor (i);
        const bool on = i == mode;

        // Little lights around the knob.
        const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre (onCircle (c, 36.0f, a));
        if (on)
        {
            g.setColour (ledGreen.withAlpha (0.35f));
            g.fillEllipse (led.expanded (3.5f));
        }
        g.setColour (on ? ledGreen : ledOff);
        g.fillEllipse (led);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (led, 0.8f);

        // Mode names.
        auto lb = labelBounds (i);
        const float s = std::sin (a);
        const auto just = std::abs (s) < 0.3f ? juce::Justification::centred
                                               : (s < 0.0f ? juce::Justification::centredRight : juce::Justification::centredLeft);
        g.setColour (on ? textWhite : textWhite.withAlpha (0.78f));
        g.setFont (labelFont (on ? 13.0f : 12.0f));
        g.drawText (modes[(size_t) i].name, lb.reduced (2.0f, 0.0f), just);
    }

    drawBlackKnob (g, c, 25.0f, angleFor (mode), true);
}

void ModeSelector::setMode (int m)
{
    m = juce::jlimit (0, sky::NumModes - 1, m);
    if (m != mode)
        attachment.setValueAsCompleteGesture ((float) m);
}

void ModeSelector::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    dragStartMode = mode;
    for (int i = 0; i < sky::NumModes; ++i)
        if (labelBounds (i).expanded (2.0f).contains (e.position)
            || juce::Rectangle<float> (14.0f, 14.0f).withCentre (onCircle (centre(), 36.0f, angleFor (i))).contains (e.position))
        {
            setMode (i);
            return;
        }
    dragging = e.position.getDistanceFrom (centre()) < 32.0f;
}

void ModeSelector::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    const float delta = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 16.0f;
    setMode (dragStartMode + (int) std::round (delta));
}

void ModeSelector::mouseUp (const juce::MouseEvent&) { dragging = false; }

void ModeSelector::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    wheelAccum += w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    if (std::abs (wheelAccum) >= 0.1f)
    {
        setMode (mode + (wheelAccum > 0.0f ? 1 : -1));
        wheelAccum = 0.0f;
    }
}

//==============================================================================
void LeverSwitch::paintButton (juce::Graphics& g, bool, bool)
{
    auto r = getLocalBounds().toFloat();
    auto slot = r.withSizeKeepingCentre (r.getWidth() * 0.7f, r.getHeight() - 2.0f);
    g.setColour (juce::Colour (0xff0b0b0b));
    g.fillRoundedRectangle (slot, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (slot, 3.0f, 0.8f);

    const bool down = getToggleState(); // down = FREEZE
    auto lever = slot.reduced (2.0f).withHeight (slot.getHeight() * 0.48f);
    if (down)
        lever.setY (slot.getBottom() - 2.0f - lever.getHeight());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e9e9), lever.getX(), 0.0f,
                                             juce::Colour (0xff7a7a7a), lever.getRight(), 0.0f, false));
    g.fillRoundedRectangle (lever, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int i = 1; i < 4; ++i)
    {
        const float y = lever.getY() + lever.getHeight() * (float) i / 4.0f;
        g.drawLine (lever.getX() + 2.0f, y, lever.getRight() - 2.0f, y, 0.8f);
    }
}

void HoldButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (1.0f, 2.0f), 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (highlighted ? 0xff3a3a3a : 0xff2a2a2a), 0.0f, r.getY(),
                                             juce::Colour (0xff0a0a0a), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (down ? r.reduced (1.0f) : r, 3.0f);
    g.setColour (on ? ledGreen : juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (r, 3.0f, on ? 1.6f : 0.8f);
    if (on)
    {
        g.setColour (ledGreen.withAlpha (0.25f));
        g.fillRoundedRectangle (r.reduced (3.0f), 2.0f);
    }
}

void LockButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool locked = getToggleState();
    g.setColour (locked ? textWhite : textWhite.withAlpha (highlighted ? 0.8f : 0.55f));

    auto body = r.withTrimmedTop (r.getHeight() * 0.45f);
    g.fillRoundedRectangle (body, 1.5f);
    juce::Path shackle;
    const float w = body.getWidth() * 0.6f;
    const float left = body.getCentreX() - w * 0.5f;
    const float lift = locked ? 0.0f : r.getHeight() * 0.18f;
    shackle.startNewSubPath (left, body.getY());
    shackle.lineTo (left, r.getY() + w * 0.5f - lift);
    shackle.addArc (left, r.getY() - lift, w, w, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
    shackle.lineTo (left + w, locked ? body.getY() : body.getY() - lift);
    g.strokePath (shackle, juce::PathStrokeType (1.6f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre (body.getCentre()));
}

void PresetDisplay::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (highlighted ? 0xff6a6a6a : 0xff4a4a4a));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (lcdGreen);
    g.setFont (valueFont (14.0f));
    g.drawText (text.toUpperCase(), r.withTrimmedRight (16.0f).reduced (6.0f, 0.0f), juce::Justification::centred, true);

    juce::Path arrow;
    const float ax = r.getRight() - 11.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.fillPath (arrow);
}

//==============================================================================
SkyPanel::SkyPanel (SkyReverbProcessor& p)
    : processor (p), modeSelector (*p.params.getParameter ("mode"))
{
    setLookAndFeel (&look);
    auto& state = processor.params;

    const std::pair<LabeledKnob*, std::pair<const char*, const char*>> mainKnobs[] = {
        { &decay,    { "decay",    "DECAY" } },
        { &predelay, { "predelay", "PRE-DELAY" } },
        { &tone,     { "tone",     "TONE" } },
        { &mod,      { "mod",      "MOD" } },
        { &lowEnd,   { "lowend",   "LOW END" } },
        { &mix,      { "mix",      "MIX" } } };
    for (auto& [knob, ids] : mainKnobs)
    {
        knob->attach (state, ids.first, ids.second);
        addAndMakeVisible (*knob);
    }

    input.setStyle (LabeledKnob::Style::Mini);
    output.setStyle (LabeledKnob::Style::Mini);
    input.attach (state, "input", "INPUT");
    output.attach (state, "output", "OUTPUT");
    addAndMakeVisible (input);
    addAndMakeVisible (output);

    for (auto& k : modeKnobs)
    {
        k.setStyle (LabeledKnob::Style::Small);
        addChildComponent (k);
    }

    addAndMakeVisible (modeSelector);

    holdType.setTooltip ("Up: INFINITE (keeps adding what you play). Down: FREEZE (locks the sound, play on top)");
    hold.setTooltip ("HOLD: sustain the reverb forever");
    addAndMakeVisible (holdType);
    addAndMakeVisible (hold);
    holdTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "holdmode", holdType);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "hold", hold);

    mixLock.setTooltip ("Lock Mix: keep your Mix setting when changing presets");
    mixLock.setToggleState (processor.isMixLocked(), juce::dontSendNotification);
    mixLock.onClick = [this] { processor.setMixLocked (mixLock.getToggleState()); };
    addAndMakeVisible (mixLock);

    bypass.setClickingTogglesState (true);
    bypass.setTooltip ("Bypass: turn the reverb off (the tail rings out naturally)");
    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "bypass", bypass);

    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (+1); };
    save.onClick = [this] { savePreset(); };
    presetDisplay.onClick = [this] { showPresetMenu(); };
    save.setTooltip ("Save the current sound as your own preset");
    for (auto* c : std::initializer_list<juce::Component*> { &prevPreset, &nextPreset, &save, &presetDisplay })
        addAndMakeVisible (c);

    for (int i = 0; i < (int) std::size (scales); ++i)
        sizeBox.addItem (juce::String (juce::roundToInt (scales[i] * 100.0f)) + "%", i + 1);
    const float current = processor.getUiScale();
    for (int i = 0; i < (int) std::size (scales); ++i)
        if (std::abs (scales[i] - current) < 0.01f)
            sizeBox.setSelectedId (i + 1, juce::dontSendNotification);
    sizeBox.onChange = [this]
    {
        const int id = sizeBox.getSelectedId();
        if (id > 0 && onScaleChange)
            onScaleChange (scales[id - 1]);
    };
    addAndMakeVisible (sizeBox);

    makeBrushedTexture();
    rebuildModeKnobs (modeSelector.getMode());
    presetDisplay.text = processor.presets.getCurrentName();
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

SkyPanel::~SkyPanel()
{
    setLookAndFeel (nullptr);
}

void SkyPanel::makeBrushedTexture()
{
    // Brushed metal: a blue gradient covered in thousands of faint horizontal scratches.
    const int w = baseWidth * 2, h = (baseHeight - 44) * 2;
    brushed = juce::Image (juce::Image::RGB, w, h, true);
    juce::Graphics g (brushed);
    g.setGradientFill (juce::ColourGradient (blueTop, 0.0f, 0.0f, blueBottom, 0.0f, (float) h, false));
    g.fillAll();

    juce::Random rng (2024);
    for (int i = 0; i < 9000; ++i)
    {
        const float y = rng.nextFloat() * (float) h;
        const float x = rng.nextFloat() * (float) w;
        const float len = 40.0f + rng.nextFloat() * 400.0f;
        const bool light = rng.nextBool();
        g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f + rng.nextFloat() * 0.05f));
        g.drawHorizontalLine ((int) y, x, x + len);
    }

    // A soft sheen across the middle, like light catching the metal.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.0f), 0.0f, 0.0f,
                                             juce::Colours::white.withAlpha (0.0f), (float) w, 0.0f, false));
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), 0.0f, 0.0f, juce::Colours::white.withAlpha (0.0f), (float) w, 0.0f, false);
    sheen.addColour (0.45, juce::Colours::white.withAlpha (0.09f));
    g.setGradientFill (sheen);
    g.fillAll();
}

void SkyPanel::rebuildModeKnobs (int m)
{
    shownMode = m;
    const auto& spec = sky::getModes()[(size_t) m];
    numModeKnobs = (int) spec.params.size();
    for (int i = 0; i < sky::maxModeParams; ++i)
    {
        auto& k = modeKnobs[(size_t) i];
        if (i < numModeKnobs)
        {
            k.attach (processor.params, spec.params[(size_t) i].id, spec.params[(size_t) i].name);
            k.setVisible (true);
        }
        else
        {
            k.detach();
            k.setVisible (false);
        }
    }
    resized();
    repaint (paramWindow);
}

void SkyPanel::stepPreset (int delta)
{
    auto& pm = processor.presets;
    const int count = pm.getNumPresets();
    const int next = (pm.getCurrentIndex() + delta + count) % count;
    pm.loadPreset (next, processor.isMixLocked());
    presetDisplay.text = pm.getCurrentName();
    presetDisplay.repaint();
}

void SkyPanel::showPresetMenu()
{
    auto& pm = processor.presets;
    juce::PopupMenu menu;
    const auto& factory = PresetManager::getFactoryPresets();

    menu.addSectionHeader ("FACTORY");
    for (int m = 0; m < sky::NumModes; ++m)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) factory.size(); ++i)
            if (factory[(size_t) i].mode == m)
                sub.addItem (i + 1, factory[(size_t) i].name, true, pm.getCurrentIndex() == i);
        menu.addSubMenu (sky::getModes()[(size_t) m].longName, sub);
    }

    const auto user = pm.getUserPresetNames();
    menu.addSectionHeader ("YOUR PRESETS");
    if (user.isEmpty())
        menu.addItem (-1, "(none yet - press SAVE)", false);
    for (int i = 0; i < user.size(); ++i)
    {
        const int index = (int) factory.size() + i;
        menu.addItem (index + 1, user[i], true, pm.getCurrentIndex() == index);
    }

    menu.addSeparator();
    const bool canDelete = pm.isUserPreset (pm.getCurrentIndex());
    menu.addItem (10001, "Delete \"" + pm.getCurrentName() + "\"", canDelete);
    menu.addItem (10002, "Open presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetDisplay),
        [this] (int result)
        {
            auto& presets = processor.presets;
            if (result == 10001)
            {
                presets.deleteUserPreset (presets.getCurrentName());
            }
            else if (result == 10002)
            {
                auto folder = PresetManager::getUserFolder();
                folder.createDirectory();
                folder.startAsProcess();
            }
            else if (result > 0)
            {
                presets.loadPreset (result - 1, processor.isMixLocked());
            }
            presetDisplay.text = presets.getCurrentName();
            presetDisplay.repaint();
        });
}

void SkyPanel::savePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&look);
    const auto current = processor.presets.getCurrentName();
    saveDialog->addTextEditor ("name", processor.presets.isUserPreset (processor.presets.getCurrentIndex()) ? current : current + " (mine)");
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveDialog != nullptr)
        {
            const auto name = saveDialog->getTextEditorContents ("name");
            if (! processor.presets.saveUserPreset (name))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save",
                                                        "The preset couldn't be saved. Try a different name.");
            presetDisplay.text = processor.presets.getCurrentName();
            presetDisplay.repaint();
        }
        saveDialog.reset();
    }), false);
}

void SkyPanel::timerCallback()
{
    const int m = modeSelector.getMode();
    if (m != shownMode)
        rebuildModeKnobs (m);

    if (presetDisplay.text != processor.presets.getCurrentName())
    {
        presetDisplay.text = processor.presets.getCurrentName();
        presetDisplay.repaint();
    }

    // The INFINITE / FREEZE / HOLD words light up with their switches.
    const bool holdState = hold.getToggleState(), typeState = holdType.getToggleState();
    if (holdState != shownHold || typeState != shownFreeze)
    {
        shownHold = holdState;
        shownFreeze = typeState;
        repaint (holdType.getBounds().getUnion (hold.getBounds()).expanded (90, 10));
    }

    const float in = processor.inputPeak.load(), out = processor.outputPeak.load();
    if (std::abs (in - shownIn) > 0.01f || std::abs (out - shownOut) > 0.01f)
    {
        shownIn = in;
        shownOut = out;
        repaint (inputLed.expanded (4));
        repaint (outputLed.expanded (4));
    }
}

void SkyPanel::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();

    // Dark top bar.
    auto bar = full.withHeight (44.0f);
    g.setGradientFill (juce::ColourGradient (barTop, 0.0f, 0.0f, barBottom, 0.0f, bar.getBottom(), false));
    g.fillRect (bar);
    g.setColour (juce::Colours::black);
    g.drawLine (0.0f, bar.getBottom() - 0.5f, full.getRight(), bar.getBottom() - 0.5f, 1.0f);

    // Level lights next to INPUT and OUTPUT (green, turning red when too loud).
    auto drawLevel = [&] (juce::Rectangle<int> r, float level)
    {
        const auto rf = r.toFloat();
        const auto colour = level > 0.98f ? juce::Colour (0xffff3b30) : ledGreen;
        const float bright = juce::jlimit (0.0f, 1.0f, std::sqrt (level) * 1.4f);
        g.setColour (juce::Colour (0xff102010));
        g.fillEllipse (rf);
        g.setColour (colour.withAlpha (bright));
        g.fillEllipse (rf);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (rf, 0.8f);
    };
    drawLevel (inputLed, shownIn);
    drawLevel (outputLed, shownOut);

    g.setColour (textWhite.withAlpha (0.8f));
    g.setFont (labelFont (11.0f));
    g.drawText ("SIZE", sizeBox.getBounds().translated (-42, 0).withWidth (38), juce::Justification::centredRight);

    // Blue brushed panel.
    auto panel = full.withTrimmedTop (44.0f);
    g.drawImage (brushed, panel);
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawLine (0.0f, panel.getY() + 0.5f, full.getRight(), panel.getY() + 0.5f, 1.0f);

    // Logo.
    {
        g.setColour (textWhite);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText ("kevin", 38, 54, 120, 16, juce::Justification::centredLeft);

        juce::GlyphArrangement logo;
        logo.addLineOfText (juce::Font (juce::FontOptions (40.0f, juce::Font::bold | juce::Font::italic)), "Sky Reverb", 0.0f, 0.0f);
        juce::Path text;
        logo.createPath (text);
        const auto tb = text.getBounds();
        text.applyTransform (juce::AffineTransform::translation (-tb.getX(), -tb.getY())
                                 .followedBy (juce::AffineTransform::scale (175.0f / tb.getWidth(), 0.95f))
                                 .followedBy (juce::AffineTransform::shear (-0.12f, 0.0f))
                                 .translated (42.0f, 72.0f));
        g.setColour (juce::Colour (0xff0b3d63));
        g.strokePath (text, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved), juce::AffineTransform::translation (1.0f, 2.0f));
        g.setColour (textWhite);
        g.fillPath (text);
    }

    // The black parameter window.
    {
        const auto w = paramWindow.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (w.translated (0.0f, 2.0f).expanded (1.0f), 6.0f);
        g.setGradientFill (juce::ColourGradient (windowTop, 0.0f, w.getY(), windowBottom, 0.0f, w.getBottom(), false));
        g.fillRoundedRectangle (w, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (w.reduced (0.5f), 6.0f, 1.0f);
    }

    // INFINITE / FREEZE and HOLD markings.
    {
        const bool freeze = holdType.getToggleState();
        auto sw = holdType.getBounds().toFloat();
        g.setFont (labelFont (12.0f));
        g.setColour (freeze ? textWhite.withAlpha (0.6f) : textWhite);
        g.drawText ("INFINITE", juce::Rectangle<float> (sw.getRight() + 6.0f, sw.getY() - 1.0f, 80.0f, 14.0f), juce::Justification::centredLeft);
        g.setColour (freeze ? textWhite : textWhite.withAlpha (0.6f));
        g.drawText ("FREEZE", juce::Rectangle<float> (sw.getRight() + 6.0f, sw.getBottom() - 13.0f, 80.0f, 14.0f), juce::Justification::centredLeft);

        auto hb = hold.getBounds().toFloat();
        g.setColour (hold.getToggleState() ? ledGreen : textWhite.withAlpha (0.85f));
        g.drawText ("HOLD", juce::Rectangle<float> (hb.getRight() + 6.0f, hb.getY(), 60.0f, hb.getHeight()), juce::Justification::centredLeft);

        // Dotted line joining the switch to the button.
        g.setColour (textWhite.withAlpha (0.6f));
        const float x = sw.getX() - 8.0f;
        for (float y = sw.getY() + 4.0f; y < hb.getCentreY(); y += 4.0f)
            g.fillRect (x, y, 1.2f, 1.8f);
        for (float xx = x; xx < sw.getX() - 2.0f; xx += 4.0f)
        {
            g.fillRect (xx, sw.getY() + 4.0f, 1.8f, 1.2f);
            g.fillRect (xx, hb.getCentreY(), 1.8f, 1.2f);
        }
    }
}

void SkyPanel::resized()
{
    // Top bar.
    input.setBounds (12, 9, 90, 26);
    inputLed = { 70, 17, 9, 9 };
    output.setBounds (104, 9, 100, 26);
    outputLed = { 170, 17, 9, 9 };
    prevPreset.setBounds (232, 10, 24, 24);
    presetDisplay.setBounds (260, 10, 210, 24);
    nextPreset.setBounds (474, 10, 24, 24);
    save.setBounds (514, 10, 62, 24);
    bypass.setBounds (612, 10, 82, 24);
    sizeBox.setBounds (744, 10, 66, 24);

    // The labels for the Mini knobs sit to the right of the knob, so shift the lights after the text.
    inputLed.setX (input.getX() + 26 + 4 + 44);
    outputLed.setX (output.getX() + 26 + 4 + 54);

    // Big knob row.
    const int knobW = 88, knobH = 96, rowY = 50, firstX = 286, gap = 96;
    LabeledKnob* row[] = { &decay, &predelay, &tone, &mod, &lowEnd, &mix };
    for (int i = 0; i < 6; ++i)
        row[i]->setBounds (firstX + i * gap - knobW / 2, rowY, knobW, knobH);
    mixLock.setBounds (mix.getRight() - 14, rowY + knobH - 22, 14, 18);

    // Mode dial.
    modeSelector.setBounds (6, 150, 240, 170);

    // Parameter window with the current mode's own knobs.
    paramWindow = { 250, 164, 446, 146 };
    const int mk = 100;
    const int totalW = numModeKnobs * mk + (numModeKnobs - 1) * 20;
    int x = paramWindow.getCentreX() - totalW / 2;
    for (int i = 0; i < numModeKnobs; ++i, x += mk + 20)
        modeKnobs[(size_t) i].setBounds (x, paramWindow.getY() + 14, mk, 92);

    // Infinite / Freeze and Hold.
    holdType.setBounds (722, 180, 18, 46);
    hold.setBounds (716, 260, 30, 26);
}

//==============================================================================
SkyReverbEditor::SkyReverbEditor (SkyReverbProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p)
{
    addAndMakeVisible (panel);
    panel.onScaleChange = [this] (float s) { applyScale (s); };
    applyScale (processor.getUiScale());
}

void SkyReverbEditor::applyScale (float s)
{
    processor.setUiScale (s);
    panel.setTransform (juce::AffineTransform::scale (s));
    setSize (juce::roundToInt ((float) SkyPanel::baseWidth * s), juce::roundToInt ((float) SkyPanel::baseHeight * s));
}

void SkyReverbEditor::resized()
{
    panel.setBounds (0, 0, SkyPanel::baseWidth, SkyPanel::baseHeight);
}
