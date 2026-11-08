#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <utility>

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // valori in unita' reali (scelte = indice, bool = 0/1)
};

const std::vector<FactoryPreset>& getFactoryPresets();

// Riporta tutti i parametri al valore iniziale e poi applica quelli del preset (notificando l'host)
void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset);
