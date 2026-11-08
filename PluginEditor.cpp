#include "PluginEditor.h"
#include "Presets.h"

using namespace OrbitColours;
using namespace ParamID;

OrbitReverbEditor::OrbitReverbEditor (OrbitReverbProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      size       (p.apvts, ParamID::size,  "Size",      "Dimensione dello spazio: da una piccola stanza a una sala enorme."),
      decay      (p.apvts, ParamID::decay, "Decay",     "Durata della coda (RT60): tempo in cui il riverbero scende di 60 dB."),
      preDelay   (p.apvts, ParamID::preDelay, "Pre-Delay", "Ritardo prima che inizi il riverbero: separa la voce dalla coda."),
      attack     (p.apvts, ParamID::attack, "Attack",   "Tempo con cui l'energia del riverbero cresce dopo l'inizio di un suono. "
                                                        "Valori lunghi = riverbero che si gonfia (swell). 0 = immediato."),
      release    (p.apvts, ParamID::release, "Release", "Dissolvenza aggiuntiva della coda quando il suono si ferma. "
                                                        "Al massimo (Natural) la coda segue solo il Decay. In modo GATED e' la lunghezza del gate."),
      diffusion  (p.apvts, ParamID::diffusion, "Diffusion", "Densita' delle riflessioni: bassa = echi distinti, alta = coda liscia."),
      damping    (p.apvts, ParamID::damping, "Damping",  "Assorbimento degli acuti nel tempo: la coda diventa piu' scura man mano che decade."),
      lowCut     (p.apvts, ParamID::lowCut, "Low Cut",   "Toglie le basse frequenze dal riverbero (non dal dry)."),
      highCut    (p.apvts, ParamID::highCut, "High Cut", "Attenua le alte frequenze del riverbero."),
      modulation (p.apvts, ParamID::modulation, "Modulation", "Movimento delle linee di ritardo: coda piu' viva e meno metallica."),
      orbitDisplay (p, orbitButton),
      rate       (p.apvts, orbitRate, "Rate",       "Velocita' dell'orbita in Hz (con SYNC attivo si usa la divisione musicale)."),
      depth      (p.apvts, orbitDepth, "Depth",     "Intensita' del movimento, da leggero a estremo."),
      orbitWidth (p.apvts, ParamID::orbitWidth, "Width", "Quanto lontano va il riverbero verso sinistra e destra."),
      phase      (p.apvts, orbitPhase, "Phase",     "Sfasamento tra lato sinistro e destro del riverbero: 0 = si muovono insieme, 180 = si incrociano."),
      center     (p.apvts, orbitCenter, "Center",   "Posizione centrale attorno a cui ruota il riverbero.", true),
      orbitMix   (p.apvts, ParamID::orbitMix, "Orbit Mix", "Quanta parte del riverbero viene messa in movimento."),
      offset     (p.apvts, orbitOffset, "Offset",   "Sposta il punto di partenza del movimento (fase iniziale)."),
      dry        (p.apvts, ParamID::dry, "Dry",       "Livello del segnale originale (mai elaborato)."),
      wet        (p.apvts, ParamID::wet, "Wet",       "Livello del riverbero."),
      output     (p.apvts, ParamID::output, "Output", "Volume finale."),
      ducking    (p.apvts, ParamID::ducking, "Ducking", "Abbassa il riverbero mentre il suono originale e' presente: lascia spazio alla voce."),
      width      (p.apvts, ParamID::width, "Width",   "Ampiezza stereo del riverbero (100 % = naturale, 200 % = extra largo).")
{
    setLookAndFeel (&lnf);

    for (auto* c : std::initializer_list<juce::Component*> { &spacePanel, &tonePanel, &orbitPanel, &mixPanel })
        addAndMakeVisible (c);

    for (auto* k : { &size, &decay, &preDelay, &attack, &release, &diffusion, &damping, &lowCut, &highCut, &modulation,
                     &rate, &depth, &orbitWidth, &phase, &center, &orbitMix, &offset, &dry, &wet, &output, &ducking, &width })
        addAndMakeVisible (k);

    addAndMakeVisible (orbitButton);
    addAndMakeVisible (orbitDisplay);
    orbitAtt = std::make_unique<ButtonAttachment> (p.apvts, orbitOn, orbitButton);

    setupCombo (direction, directionLabel, OrbitChoices::directions, "DIRECTION",
                "Verso del movimento: da sinistra a destra, da destra a sinistra, oppure alternato (inverte il verso a ogni giro).");
    setupCombo (shape, shapeLabel, OrbitChoices::shapes, "SHAPE",
                "SINE = morbido, TRIANGLE = movimento costante, ORBITAL = rotazione 3D con il riverbero che passa dietro la testa.");
    setupCombo (division, divisionLabel, OrbitChoices::divisions, "DIVISION",
                "Durata di un giro completo quando SYNC e' attivo, a tempo con il BPM del progetto.");
    directionAtt = std::make_unique<ComboAttachment> (p.apvts, orbitDirection, direction);
    shapeAtt     = std::make_unique<ComboAttachment> (p.apvts, orbitShape, shape);
    divisionAtt  = std::make_unique<ComboAttachment> (p.apvts, orbitDivision, division);

    sync.setTooltip ("Sincronizza l'orbita al BPM della DAW usando la divisione scelta.");
    addAndMakeVisible (sync);
    syncAtt = std::make_unique<ButtonAttachment> (p.apvts, orbitSync, sync);

    freeze.setTooltip ("FREEZE: congela la coda attuale e la mantiene all'infinito. Il nuovo audio non entra nel riverbero.");
    addAndMakeVisible (freeze);
    freezeAtt = std::make_unique<ButtonAttachment> (p.apvts, ParamID::freeze, freeze);

    setupCombo (modeBox, modeLabel, Modes::names, "MODE",
                "CLEAN naturale, LUSH largo e morbido, DARK scuro, GATED taglio netto, SHIMMER ottava sopra, "
                "INFINITE coda lunghissima, ORBITAL movimento immersivo sempre attivo.");
    modeAtt = std::make_unique<ComboAttachment> (p.apvts, ParamID::mode, modeBox);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, textDim);
    presetLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (presetLabel);
    presetBox.setTooltip ("Preset di fabbrica e preset salvati da te.");
    addAndMakeVisible (presetBox);
    refreshPresetList();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < 1000)
            processor.setCurrentProgram (id - 1);
        else if (juce::isPositiveAndBelow (id - 1000, userPresetFiles.size()))
            processor.loadUserPreset (userPresetFiles[id - 1000]);
    };

    saveButton.setTooltip ("Salva le impostazioni attuali come preset personale.");
    addAndMakeVisible (saveButton);
    saveButton.onClick = [this]
    {
        auto* w = new juce::AlertWindow ("Salva preset", "Nome del preset:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", processor.getCurrentPresetName());
        w->addButton ("SALVA", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("ANNULLA", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<OrbitReverbEditor> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
        {
            if (result == 1 && safe != nullptr)
            {
                const auto name = w->getTextEditorContents ("name");
                if (safe->processor.saveUserPreset (name))
                    safe->refreshPresetList();
                else
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                            "ORBIT REVERB", "Impossibile salvare il preset.");
            }
        }), true);
    };

    setSize (1120, 660);
}

OrbitReverbEditor::~OrbitReverbEditor()
{
    setLookAndFeel (nullptr);
}

void OrbitReverbEditor::setupCombo (juce::ComboBox& box, juce::Label& label, const juce::StringArray& items,
                                    const juce::String& title, const juce::String& tip)
{
    box.addItemList (items, 1);
    box.setTooltip (tip);
    box.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (box);
    label.setText (title, juce::dontSendNotification);
    label.setColour (juce::Label::textColourId, textDim);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void OrbitReverbEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    const auto& factory = getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        presetBox.addItem (factory[(size_t) i].name, i + 1);

    userPresetFiles = processor.getUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), 1000 + i);
    }
    presetBox.setText (processor.getCurrentPresetName(), juce::dontSendNotification);
}

void OrbitReverbEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // bagliore di fondo
    juce::ColourGradient glow (purple.withAlpha (0.10f), (float) getWidth() * 0.5f, 120.0f,
                               juce::Colours::transparentBlack, (float) getWidth() * 0.5f, (float) getHeight(), true);
    g.setGradientFill (glow);
    g.fillRect (getLocalBounds());

    // titolo
    const juce::Rectangle<float> title (24.0f, 14.0f, 340.0f, 34.0f);
    juce::ColourGradient tg (purple, title.getX(), 0.0f, cyan, title.getX() + 260.0f, 0.0f, false);
    tg.addColour (0.5, blue);
    g.setGradientFill (tg);
    g.setFont (OrbitLookAndFeel::font (30.0f, true));
    g.drawText ("ORBIT REVERB", title, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (OrbitLookAndFeel::font (11.0f, true));
    g.drawText ("IMMERSIVE STEREO REVERB", juce::Rectangle<float> (26.0f, 46.0f, 300.0f, 14.0f), juce::Justification::centredLeft);
}

void OrbitReverbEditor::resized()
{
    // ---- header ----
    modeLabel.setBounds (420, 22, 50, 26);
    modeBox.setBounds (472, 20, 150, 30);
    presetLabel.setBounds (650, 22, 60, 26);
    presetBox.setBounds (714, 20, 280, 30);
    saveButton.setBounds (1004, 20, 92, 30);

    // ---- pannelli ----
    spacePanel.setBounds (16, 76, 360, 330);
    tonePanel.setBounds (16, 416, 360, 228);
    orbitPanel.setBounds (390, 76, 420, 568);
    mixPanel.setBounds (824, 76, 280, 568);

    auto grid = [] (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps, int cols)
    {
        const int rows = ((int) comps.size() + cols - 1) / cols;
        const int w = area.getWidth() / cols, h = area.getHeight() / rows;
        int i = 0;
        for (auto* c : comps)
        {
            c->setBounds (area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h);
            c->setBounds (c->getBounds().reduced (4));
            ++i;
        }
    };

    auto offsetRect = [] (const SectionPanel& panel) { return panel.content().translated (panel.getX(), panel.getY()); };

    grid (offsetRect (spacePanel), { &size, &decay, &preDelay, &attack, &release, &diffusion }, 3);
    grid (offsetRect (tonePanel), { &damping, &lowCut, &highCut, &modulation }, 4);   // knob ridimensionati in Knob::resized

    // ---- ORBIT ----
    auto o = offsetRect (orbitPanel);
    auto top = o.removeFromTop (196);
    orbitButton.setBounds (top.removeFromLeft (196).reduced (4));
    orbitDisplay.setBounds (top.reduced (6, 2));

    o.removeFromTop (8);
    auto comboLabels = o.removeFromTop (16);
    auto combos = o.removeFromTop (30);
    const int widths[] { 112, 104, 84 };
    juce::Component* labels[] { &directionLabel, &shapeLabel, &divisionLabel };
    juce::Component* boxes[]  { &direction, &shape, &division };
    for (int i = 0; i < 3; ++i)
    {
        labels[i]->setBounds (comboLabels.removeFromLeft (widths[i]));
        boxes[i]->setBounds (combos.removeFromLeft (widths[i]).reduced (3, 0));
    }
    sync.setBounds (combos.reduced (3, 0));

    o.removeFromTop (10);
    auto row1 = o.removeFromTop (o.getHeight() / 2);
    grid (row1, { &rate, &depth, &orbitWidth, &phase }, 4);
    grid (o, { &center, &orbitMix, &offset }, 3);

    // ---- MIX ----
    auto m = offsetRect (mixPanel);
    auto freezeArea = m.removeFromBottom (54);
    grid (m, { &dry, &wet, &output, &ducking, &width }, 2);
    freeze.setBounds (freezeArea.reduced (20, 8));
}
