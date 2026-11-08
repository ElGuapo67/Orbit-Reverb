#include "OrbitWidgets.h"
#include "OrbitLookAndFeel.h"
#include "../PluginProcessor.h"

using namespace OrbitColours;

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
            const juce::String& title, const juce::String& tooltip, bool bipolar)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setPopupDisplayEnabled (true, true, nullptr, 1500);
    slider.setTooltip (tooltip);
    slider.setDoubleClickReturnValue (true, state.getParameter (paramId)->convertFrom0to1 (
                                                 state.getParameter (paramId)->getDefaultValue()));
    slider.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (slider);

    label.setText (title.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, textDim);
    label.setTooltip (tooltip);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
}

void Knob::resized()
{
    // blocco etichetta + knob centrato verticalmente: niente spazi vuoti tra nome e knob
    auto r = getLocalBounds();
    const int sliderH = juce::jmin (r.getHeight() - 16, r.getWidth() + 18);
    r = r.withSizeKeepingCentre (r.getWidth(), sliderH + 16);
    label.setBounds (r.removeFromTop (16));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 16);
    slider.setBounds (r);
}

//==============================================================================
void SectionPanel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    juce::ColourGradient bg (juce::Colour (0xff15151f), r.getX(), r.getY(),
                             juce::Colour (0xff0f0f16), r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (accent ? purple.withAlpha (0.55f) : panelEdge);
    g.drawRoundedRectangle (r, 12.0f, accent ? 1.6f : 1.0f);

    auto titleArea = getLocalBounds().reduced (16, 10).removeFromTop (18);
    g.setColour (accent ? cyan : text);
    g.setFont (OrbitLookAndFeel::font (14.0f, true));
    g.drawText (title, titleArea, juce::Justification::centredLeft);

    // linea d'accento sotto il titolo
    const float tw = juce::GlyphArrangement::getStringWidth (OrbitLookAndFeel::font (14.0f, true), title);
    juce::ColourGradient line (purple, (float) titleArea.getX(), 0.0f, cyan, (float) titleArea.getX() + tw, 0.0f, false);
    g.setGradientFill (line);
    g.fillRect (juce::Rectangle<float> ((float) titleArea.getX(), (float) titleArea.getBottom() + 2.0f, tw, 2.0f));
}

//==============================================================================
OrbitButton::OrbitButton() : juce::Button ("ORBIT")
{
    setClickingTogglesState (true);
    setTooltip ("ORBIT: fa ruotare il riverbero intorno all'ascoltatore. Agisce solo sul segnale wet: "
                "il suono originale resta fermo al centro.");
}

void OrbitButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const float size = juce::jmin (b.getWidth(), b.getHeight()) - 8.0f;
    const auto  r = juce::Rectangle<float> (size, size).withCentre (b.getCentre());
    const auto  c = r.getCentre();
    const bool  on = getToggleState() || forced;
    const float ringR = size * 0.5f - 6.0f;

    // alone
    if (on)
        for (int i = 4; i > 0; --i)
        {
            g.setColour (purple.withAlpha (0.05f * (float) i));
            g.fillEllipse (r.reduced ((float) (4 - i) * -2.0f + 2.0f));
        }

    // corpo
    juce::ColourGradient body (juce::Colour (0xff22222e), c.x, r.getY(), juce::Colour (0xff0c0c12), c.x, r.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (r.reduced (8.0f));

    // anello
    juce::Path ring;
    ring.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
    if (on)
    {
        const auto p1 = c.getPointOnCircumference (ringR, phase);
        const auto p2 = c.getPointOnCircumference (ringR, phase + juce::MathConstants<float>::pi);
        juce::ColourGradient grad (cyan, p1.x, p1.y, purple, p2.x, p2.y, false);
        grad.addColour (0.5, blue);
        g.setGradientFill (grad);
        g.strokePath (ring, juce::PathStrokeType (5.0f));
    }
    else
    {
        g.setColour (highlighted ? purple.withAlpha (0.6f) : track);
        g.strokePath (ring, juce::PathStrokeType (4.0f));
    }

    // satellite
    if (on)
    {
        const auto sat = c.getPointOnCircumference (ringR, phase);
        g.setColour (cyan.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (sat));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (sat));
    }

    g.setColour (on ? juce::Colours::white : textDim);
    g.setFont (OrbitLookAndFeel::font (size * 0.16f, true));
    g.drawText ("ORBIT", r.withTrimmedBottom (size * 0.12f), juce::Justification::centred);
    g.setFont (OrbitLookAndFeel::font (size * 0.08f, true));
    g.setColour (on ? cyan : textDim);
    const juce::String sub = getToggleState() ? "ON" : (forced ? "MODE ORBITAL" : "OFF");
    g.drawText (sub, r.withTrimmedTop (size * 0.28f), juce::Justification::centred);

    if (down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillEllipse (r.reduced (8.0f));
    }
}

//==============================================================================
OrbitDisplay::OrbitDisplay (OrbitReverbProcessor& p, OrbitButton& b) : processor (p), button (b)
{
    setTooltip ("Vista dall'alto: il punto e' il riverbero che gira intorno alla testa (in alto = davanti). "
                "La barra mostra la posizione stereo istantanea.");
    startTimerHz (60);
}

void OrbitDisplay::timerCallback()
{
    const auto& om = processor.getOrbitMotion();
    const bool active = processor.isOrbitActive();
    activeAmt += ((active ? 1.0f : 0.0f) - activeAmt) * 0.15f;

    phase = om.uiPhase.load (std::memory_order_relaxed);
    pan   = om.uiPosition.load (std::memory_order_relaxed);
    z     = om.uiDepthZ.load (std::memory_order_relaxed);
    rate  = om.uiRateHz.load (std::memory_order_relaxed);

    trail[(size_t) trailHead] = { std::sin (phase), -std::cos (phase) };
    trailHead = (trailHead + 1) % (int) trail.size();

    const bool orbitParam = processor.apvts.getRawParameterValue ("orbit_on")->load() > 0.5f;
    button.setForcedByMode (active && ! orbitParam);
    button.setAnimationPhase (active ? phase : 0.0f, pan);
    repaint();
}

void OrbitDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff0c0c12));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    auto inner = r.reduced (10.0f);
    auto bottom = inner.removeFromBottom (40.0f);
    const float radius = juce::jmin (inner.getWidth(), inner.getHeight()) * 0.5f - 6.0f;
    const auto c = inner.getCentre();

    // orbita
    g.setColour (track);
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c), 1.0f);
    g.setColour (purple.withAlpha (0.15f + 0.45f * activeAmt));
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c), 2.0f);

    // testa dell'ascoltatore (naso verso l'alto = davanti)
    const float hr = radius * 0.22f;
    g.setColour (juce::Colour (0xff2b2b3a));
    g.fillEllipse (juce::Rectangle<float> (hr * 2.0f, hr * 2.0f).withCentre (c));
    juce::Path nose;
    nose.addTriangle (c.x - hr * 0.3f, c.y - hr * 0.9f, c.x + hr * 0.3f, c.y - hr * 0.9f, c.x, c.y - hr * 1.35f);
    g.fillPath (nose);
    g.setColour (textDim);
    g.setFont (OrbitLookAndFeel::font (11.0f, true));
    g.drawText ("L", juce::Rectangle<float> (16.0f, 14.0f).withCentre ({ c.x - hr - 12.0f, c.y }), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (16.0f, 14.0f).withCentre ({ c.x + hr + 12.0f, c.y }), juce::Justification::centred);

    // scia + satellite
    if (activeAmt > 0.02f)
    {
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const size_t idx = ((size_t) trailHead + i) % trail.size();
            const float age = (float) i / (float) trail.size();
            const auto p = c + trail[idx] * radius;
            g.setColour (cyan.withAlpha (0.35f * age * activeAmt));
            g.fillEllipse (juce::Rectangle<float> (3.0f + 4.0f * age, 3.0f + 4.0f * age).withCentre (p));
        }
        const auto dot = c + juce::Point<float> (std::sin (phase), -std::cos (phase)) * radius;
        const float front = 0.75f + 0.25f * z;          // piu' grande davanti, piu' piccolo dietro
        g.setColour (cyan.withAlpha (0.3f * activeAmt));
        g.fillEllipse (juce::Rectangle<float> (26.0f * front, 26.0f * front).withCentre (dot));
        g.setColour (juce::Colours::white.withAlpha (activeAmt));
        g.fillEllipse (juce::Rectangle<float> (11.0f * front, 11.0f * front).withCentre (dot));
    }

    // barra della posizione stereo istantanea
    auto bar = bottom.removeFromTop (14.0f).reduced (14.0f, 4.0f);
    g.setColour (track);
    g.fillRoundedRectangle (bar, 3.0f);
    const float px = bar.getX() + (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * 0.5f * bar.getWidth();
    juce::ColourGradient grad (purple, bar.getX(), 0.0f, cyan, bar.getRight(), 0.0f, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (juce::Rectangle<float> (6.0f, bar.getHeight() + 8.0f).withCentre ({ px, bar.getCentreY() }), 3.0f);
    g.setColour (textDim);
    g.setFont (OrbitLookAndFeel::font (10.0f, true));
    g.drawText ("L", juce::Rectangle<float> (12.0f, 14.0f).withCentre ({ bar.getX() - 8.0f, bar.getCentreY() }), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (12.0f, 14.0f).withCentre ({ bar.getRight() + 8.0f, bar.getCentreY() }), juce::Justification::centred);

    // testi
    const int panPct = juce::roundToInt (std::abs (pan) * 100.0f);
    const juce::String posText = panPct < 2 ? juce::String ("POS C") : juce::String ("POS ") + (pan < 0 ? "L " : "R ") + juce::String (panPct);
    const juce::String rateText = activeAmt > 0.5f ? "RATE " + juce::String (rate, rate < 1.0f ? 2 : 1) + " Hz" : juce::String ("ORBIT OFF");
    g.setFont (OrbitLookAndFeel::font (12.0f, true));
    g.setColour (activeAmt > 0.5f ? cyan : textDim);
    g.drawText (rateText, bottom.reduced (6.0f, 0.0f), juce::Justification::centredLeft);
    g.setColour (text);
    g.drawText (posText, bottom.reduced (6.0f, 0.0f), juce::Justification::centredRight);
}
