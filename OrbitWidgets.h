#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class OrbitReverbProcessor;

//==============================================================================
// Knob con etichetta, valore numerico sempre visibile, popup durante il trascinamento e tooltip
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
          const juce::String& title, const juce::String& tooltip, bool bipolar = false);

    void resized() override;
    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// Pannello di sezione (SPACE / TONE / ORBIT / MIX)
class SectionPanel : public juce::Component
{
public:
    explicit SectionPanel (juce::String titleText, bool highlight = false) : title (std::move (titleText)), accent (highlight) {}
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (14).withTrimmedTop (26); }

private:
    juce::String title;
    bool accent;
};

//==============================================================================
// Il grande pulsante ORBIT: anello luminoso con un satellite che gira quando e' attivo
class OrbitButton : public juce::Button
{
public:
    OrbitButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void setAnimationPhase (float p, float pan) { phase = p; panPos = pan; repaint(); }
    void setForcedByMode (bool f) { forced = f; }

private:
    float phase = 0.0f, panPos = 0.0f;
    bool  forced = false;
};

//==============================================================================
// Display: vista dall'alto dell'ascoltatore con il riverbero in orbita, posizione L/R e velocita'.
// Legge solo valori atomici pubblicati dal DSP: nessuna elaborazione audio nella GUI.
class OrbitDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    OrbitDisplay (OrbitReverbProcessor&, OrbitButton&);
    ~OrbitDisplay() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    OrbitReverbProcessor& processor;
    OrbitButton& button;
    float phase = 0.0f, pan = 0.0f, z = 1.0f, rate = 0.0f, activeAmt = 0.0f;
    std::array<juce::Point<float>, 28> trail {};
    int trailHead = 0;
};
