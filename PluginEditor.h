#pragma once
#include "PluginProcessor.h"
#include "gui/OrbitLookAndFeel.h"
#include "gui/OrbitWidgets.h"

class OrbitReverbEditor : public juce::AudioProcessorEditor
{
public:
    explicit OrbitReverbEditor (OrbitReverbProcessor&);
    ~OrbitReverbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    OrbitReverbProcessor& processor;
    OrbitLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    SectionPanel spacePanel { "SPACE" }, tonePanel { "TONE" }, orbitPanel { "ORBIT MOTION", true }, mixPanel { "MIX" };

    // SPACE
    Knob size, decay, preDelay, attack, release, diffusion;
    // TONE
    Knob damping, lowCut, highCut, modulation;
    // ORBIT
    OrbitButton orbitButton;
    OrbitDisplay orbitDisplay;
    Knob rate, depth, orbitWidth, phase, center, orbitMix, offset;
    juce::ComboBox direction, shape, division;
    juce::ToggleButton sync { "SYNC" };
    juce::Label directionLabel, shapeLabel, divisionLabel;
    // MIX
    Knob dry, wet, output, ducking, width;
    juce::ToggleButton freeze { "FREEZE" };
    // HEADER
    juce::ComboBox modeBox, presetBox;
    juce::TextButton saveButton { "SAVE" };
    juce::Label modeLabel, presetLabel;
    juce::Array<juce::File> userPresetFiles;

    std::unique_ptr<ComboAttachment>  modeAtt, directionAtt, shapeAtt, divisionAtt;
    std::unique_ptr<ButtonAttachment> orbitAtt, syncAtt, freezeAtt;

    void refreshPresetList();
    void setupCombo (juce::ComboBox&, juce::Label&, const juce::StringArray&, const juce::String& title, const juce::String& tip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitReverbEditor)
};
