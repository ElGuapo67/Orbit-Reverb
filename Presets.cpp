#include "Presets.h"
#include "Parameters.h"

using namespace ParamID;

const std::vector<FactoryPreset>& getFactoryPresets()
{
    // Modi: 0 CLEAN, 1 LUSH, 2 DARK, 3 GATED, 4 SHIMMER, 5 INFINITE, 6 ORBITAL
    // Divisioni: 0 4/1, 1 2/1, 2 1/1, 3 1/2, 4 1/4, 5 1/8, 6 1/8D, 7 1/16
    // Forme: 0 SINE, 1 TRIANGLE, 2 ORBITAL     Direzioni: 0 L>R, 1 R>L, 2 ALTERNATE
    static const std::vector<FactoryPreset> presets
    {
        { "Vocal Space", { { mode, 0 }, { size, 35 }, { decay, 1.6f }, { preDelay, 30 }, { diffusion, 70 },
                           { damping, 45 }, { lowCut, 200 }, { highCut, 9000 }, { modulation, 20 },
                           { width, 110 }, { wet, 25 }, { ducking, 35 } } },

        { "Wide Trap Vocal", { { mode, 1 }, { size, 55 }, { decay, 2.8f }, { preDelay, 45 }, { diffusion, 80 },
                               { damping, 40 }, { lowCut, 250 }, { highCut, 10000 }, { modulation, 35 },
                               { width, 160 }, { wet, 30 }, { ducking, 50 },
                               { orbitOn, 1 }, { orbitSync, 1 }, { orbitDivision, 3 }, { orbitDepth, 40 },
                               { orbitWidth, 70 }, { orbitMix, 60 } } },

        { "Drill Atmosphere", { { mode, 2 }, { size, 75 }, { decay, 4.5f }, { preDelay, 20 }, { diffusion, 85 },
                                { damping, 60 }, { lowCut, 300 }, { highCut, 6000 }, { modulation, 30 },
                                { width, 140 }, { wet, 35 }, { ducking, 30 },
                                { orbitOn, 1 }, { orbitSync, 1 }, { orbitDivision, 2 }, { orbitDepth, 50 } } },

        { "Dark Hall", { { mode, 2 }, { size, 85 }, { decay, 6.0f }, { preDelay, 40 }, { diffusion, 85 },
                         { damping, 70 }, { lowCut, 150 }, { highCut, 5000 }, { modulation, 25 },
                         { width, 130 }, { wet, 35 } } },

        { "Infinite Pad", { { mode, 5 }, { size, 90 }, { decay, 30 }, { preDelay, 10 }, { diffusion, 90 },
                            { damping, 35 }, { lowCut, 200 }, { highCut, 12000 }, { modulation, 50 },
                            { attack, 300 }, { width, 150 }, { dry, 80 }, { wet, 50 } } },

        { "Dreamy R&B", { { mode, 1 }, { size, 65 }, { decay, 3.5f }, { preDelay, 60 }, { diffusion, 85 },
                          { damping, 40 }, { lowCut, 220 }, { highCut, 9000 }, { modulation, 45 },
                          { width, 150 }, { wet, 32 }, { ducking, 40 },
                          { orbitOn, 1 }, { orbitRate, 0.15f }, { orbitDepth, 35 }, { orbitMix, 50 } } },

        { "Psychedelic Orbit", { { mode, 6 }, { size, 70 }, { decay, 5.0f }, { preDelay, 25 }, { diffusion, 85 },
                                 { damping, 35 }, { lowCut, 180 }, { highCut, 12000 }, { modulation, 60 },
                                 { wet, 45 }, { orbitOn, 1 }, { orbitRate, 0.5f }, { orbitDepth, 100 },
                                 { orbitWidth, 100 }, { orbitShape, 2 }, { orbitDirection, 2 },
                                 { orbitPhase, 90 }, { orbitMix, 100 } } },

        { "Fast Stereo Bounce", { { mode, 0 }, { size, 40 }, { decay, 1.8f }, { preDelay, 15 }, { diffusion, 75 },
                                  { damping, 35 }, { lowCut, 200 }, { highCut, 11000 }, { wet, 35 },
                                  { orbitOn, 1 }, { orbitSync, 1 }, { orbitDivision, 5 }, { orbitDepth, 90 },
                                  { orbitWidth, 100 }, { orbitShape, 1 } } },

        { "Cinematic Huge Hall", { { mode, 1 }, { size, 100 }, { decay, 9.0f }, { preDelay, 80 }, { diffusion, 95 },
                                   { damping, 40 }, { lowCut, 120 }, { highCut, 11000 }, { modulation, 35 },
                                   { width, 170 }, { wet, 40 } } },

        { "Short Plate", { { mode, 0 }, { size, 20 }, { decay, 1.1f }, { preDelay, 5 }, { diffusion, 100 },
                           { damping, 20 }, { lowCut, 150 }, { highCut, 14000 }, { modulation, 15 }, { wet, 25 } } },

        { "Shimmer Cloud", { { mode, 4 }, { size, 85 }, { decay, 8.0f }, { preDelay, 40 }, { diffusion, 90 },
                             { damping, 30 }, { lowCut, 400 }, { highCut, 14000 }, { modulation, 45 },
                             { width, 160 }, { wet, 40 }, { orbitOn, 1 }, { orbitRate, 0.1f }, { orbitDepth, 40 } } },

        // Swell in stile "reverse": nessuna inversione del nastro. L'ATTACK lungo fa crescere l'energia del
        // riverbero dopo ogni attacco (inviluppo reale), il pre-delay lungo lo separa dal dry,
        // e il RELEASE corto lo chiude: il risultato percepito e' simile a una coda invertita.
        { "Reverse-Style Swell", { { mode, 1 }, { size, 75 }, { decay, 4.0f }, { preDelay, 120 }, { diffusion, 90 },
                                   { damping, 45 }, { lowCut, 250 }, { highCut, 10000 }, { modulation, 30 },
                                   { attack, 1200 }, { release, 900 }, { width, 140 }, { wet, 45 } } },
    };
    return presets;
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset)
{
    for (auto* p : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr || ranged->getParameterID() == bypass)
            continue;

        float normalised = ranged->getDefaultValue();
        for (const auto& [id, value] : preset.values)
            if (id == ranged->getParameterID())
                normalised = ranged->convertTo0to1 (value);

        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (normalised);
        ranged->endChangeGesture();
    }
}
