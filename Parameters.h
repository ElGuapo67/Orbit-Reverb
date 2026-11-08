#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Identificatori di tutti i parametri (stabili: non cambiarli, servono per automazione e preset)
namespace ParamID
{
    inline constexpr auto size = "size", decay = "decay", preDelay = "predelay", attack = "attack",
                          release = "release", diffusion = "diffusion";
    inline constexpr auto damping = "damping", lowCut = "lowcut", highCut = "highcut", modulation = "modulation";
    inline constexpr auto dry = "dry", wet = "wet", output = "output", ducking = "ducking", freeze = "freeze",
                          width = "width", mode = "mode", bypass = "bypass";
    inline constexpr auto orbitOn = "orbit_on", orbitRate = "orbit_rate", orbitDepth = "orbit_depth",
                          orbitWidth = "orbit_width", orbitPhase = "orbit_phase", orbitDirection = "orbit_direction",
                          orbitShape = "orbit_shape", orbitSync = "orbit_sync", orbitDivision = "orbit_division",
                          orbitCenter = "orbit_center", orbitMix = "orbit_mix", orbitOffset = "orbit_offset";
}

namespace Modes
{
    enum Mode { Clean = 0, Lush, Dark, Gated, Shimmer, Infinite, Orbital };
    inline const juce::StringArray names { "CLEAN", "LUSH", "DARK", "GATED", "SHIMMER", "INFINITE", "ORBITAL" };
}

namespace OrbitChoices
{
    enum Direction { LeftToRight = 0, RightToLeft, Alternate };
    enum Shape { Sine = 0, Triangle, Orbital };
    inline const juce::StringArray directions { "L > R", "R > L", "ALTERNATE" };
    inline const juce::StringArray shapes     { "SINE", "TRIANGLE", "ORBITAL" };
    inline const juce::StringArray divisions  { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/8 D", "1/16" };
    // lunghezza di un giro completo in quarti (beat) per ogni divisione
    inline constexpr double divisionBeats[] { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.75, 0.25 };
}

// Release al valore massimo = nessuna dissolvenza aggiuntiva (coda naturale)
inline constexpr float kReleaseInfinite = 10000.0f;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
