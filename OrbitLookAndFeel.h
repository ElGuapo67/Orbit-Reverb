#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace OrbitColours
{
    inline const juce::Colour background { 0xff0a0a0f };
    inline const juce::Colour panel      { 0xff12121a };
    inline const juce::Colour panelEdge  { 0xff23232f };
    inline const juce::Colour knobBody   { 0xff1a1a24 };
    inline const juce::Colour track      { 0xff2a2a38 };
    inline const juce::Colour purple     { 0xff8b5cf6 };
    inline const juce::Colour blue       { 0xff3b82f6 };
    inline const juce::Colour cyan       { 0xff22d3ee };
    inline const juce::Colour text       { 0xffe5e7eb };
    inline const juce::Colour textDim    { 0xff8b8fa3 };
}

class OrbitLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OrbitLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;

    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                     const juce::Rectangle<float>& body) override;

    static juce::Font font (float height, bool bold = false);
};
