#include "OrbitLookAndFeel.h"

using namespace OrbitColours;

OrbitLookAndFeel::OrbitLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, purple.withAlpha (0.4f));
    setColour (juce::ComboBox::backgroundColourId, knobBody);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, cyan);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, purple.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, cyan);
    setColour (juce::TextButton::buttonColourId, knobBody);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0181822));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, purple.withAlpha (0.6f));
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf0181822));
    setColour (juce::BubbleComponent::outlineColourId, cyan.withAlpha (0.7f));
    setColour (juce::TextEditor::backgroundColourId, knobBody);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, purple);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, purple);
}

juce::Font OrbitLookAndFeel::font (float height, bool bold)
{
    auto opts = juce::FontOptions (height);
    return juce::Font (bold ? opts.withStyle ("Bold") : opts);
}

juce::Font OrbitLookAndFeel::getComboBoxFont (juce::ComboBox&)        { return font (12.5f, true); }
juce::Font OrbitLookAndFeel::getLabelFont (juce::Label& l)            { return font (juce::jmax (11.0f, (float) l.getHeight() * 0.62f)); }
juce::Font OrbitLookAndFeel::getPopupMenuFont()                       { return font (14.0f); }
juce::Font OrbitLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (14.0f, h * 0.5f), true); }

void OrbitLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float trackW = juce::jmax (3.0f, radius * 0.11f);
    const float arcR   = radius - trackW * 0.5f;
    const bool  bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const float angle   = startAngle + pos * (endAngle - startAngle);
    const bool  active  = slider.isMouseOverOrDragging();

    // traccia di fondo
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (OrbitColours::track);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // arco del valore con gradiente viola -> ciano e alone
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        juce::ColourGradient grad (purple, centre.x - radius, centre.y, cyan, centre.x + radius, centre.y, false);
        grad.addColour (0.5, blue);
        g.setGradientFill (grad);
        g.setOpacity (active ? 0.35f : 0.2f);
        g.strokePath (value, juce::PathStrokeType (trackW * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setOpacity (1.0f);
        g.strokePath (value, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // corpo del knob
    const float bodyR = radius - trackW * 2.2f;
    juce::ColourGradient body (juce::Colour (0xff262633), centre.x, centre.y - bodyR,
                               juce::Colour (0xff101016), centre.x, centre.y + bodyR, false);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (active ? purple.withAlpha (0.8f) : panelEdge);
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

    // indicatore
    const auto tip = centre.getPointOnCircumference (bodyR * 0.78f, angle);
    const auto in  = centre.getPointOnCircumference (bodyR * 0.30f, angle);
    g.setColour (cyan);
    g.drawLine ({ in, tip }, juce::jmax (2.0f, radius * 0.06f));
    g.setColour (cyan.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (tip));
}

void OrbitLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const float corner = r.getHeight() * 0.5f;

    if (on)
    {
        juce::ColourGradient grad (purple, r.getX(), r.getY(), cyan, r.getRight(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, corner);
        g.setColour (cyan.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (1.0f), corner + 1.0f, 2.0f);
    }
    else
    {
        g.setColour (knobBody);
        g.fillRoundedRectangle (r, corner);
        g.setColour (highlighted ? purple.withAlpha (0.7f) : panelEdge);
        g.drawRoundedRectangle (r, corner, 1.2f);
    }

    g.setColour (on ? juce::Colours::white : textDim);
    g.setFont (font (juce::jmin (14.0f, r.getHeight() * 0.48f), true));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1);
}

void OrbitLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? purple.withAlpha (0.5f) : knobBody);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (highlighted ? purple : panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
}

void OrbitLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (knobBody);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver (true) ? purple : panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    juce::Path arrow;
    const float ax = (float) width - 16.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (cyan);
    g.fillPath (arrow);
}

void OrbitLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                                   const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf0181822));
    g.fillRoundedRectangle (body, 6.0f);
    g.setColour (cyan.withAlpha (0.7f));
    g.drawRoundedRectangle (body, 6.0f, 1.2f);
}
