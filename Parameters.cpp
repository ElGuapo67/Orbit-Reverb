#include "Parameters.h"

namespace
{
    using Float = juce::AudioParameterFloat;
    using Attr  = juce::AudioParameterFloatAttributes;

    juce::String pct (float v, int)  { return juce::String (juce::roundToInt (v)) + " %"; }
    juce::String ms  (float v, int)  { return v < 1000.0f ? juce::String (v, v < 10.0f ? 1 : 0) + " ms"
                                                         : juce::String (v / 1000.0f, 2) + " s"; }
    juce::String sec (float v, int)  { return juce::String (v, v < 10.0f ? 2 : 1) + " s"; }
    juce::String hz  (float v, int)  { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz"
                                                         : juce::String (v / 1000.0f, 1) + " kHz"; }
    juce::String deg (float v, int)  { return juce::String (juce::roundToInt (v)) + " deg"; }
    juce::String db  (float v, int)  { return juce::String (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }

    std::unique_ptr<Float> make (const char* id, const char* name, juce::NormalisableRange<float> r, float def,
                                 juce::String (*fmt) (float, int))
    {
        return std::make_unique<Float> (juce::ParameterID { id, 1 }, name, r, def,
                                        Attr().withStringFromValueFunction (fmt));
    }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::NormalisableRange<float> linear (float lo, float hi) { return { lo, hi, 0.1f }; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // ---------- SPACE ----------
    p.push_back (make (size,      "Size",      linear (0.0f, 100.0f), 50.0f, pct));
    p.push_back (make (decay,     "Decay",     skewed (0.1f, 60.0f, 3.0f), 2.5f, sec));
    p.push_back (make (preDelay,  "Pre-Delay", skewed (0.0f, 500.0f, 60.0f), 20.0f, ms));
    p.push_back (make (attack,    "Attack",    skewed (0.0f, 2000.0f, 250.0f), 0.0f, ms));
    p.push_back (std::make_unique<Float> (juce::ParameterID { release, 1 }, "Release",
                 skewed (20.0f, kReleaseInfinite, 800.0f), kReleaseInfinite,
                 Attr().withStringFromValueFunction ([] (float v, int)
                 {
                     return v >= kReleaseInfinite - 1.0f ? juce::String ("Natural") : ms (v, 0);
                 })));
    p.push_back (make (diffusion, "Diffusion", linear (0.0f, 100.0f), 75.0f, pct));

    // ---------- TONE ----------
    p.push_back (make (damping,    "Damping",    linear (0.0f, 100.0f), 40.0f, pct));
    p.push_back (make (lowCut,     "Low Cut",    skewed (20.0f, 1000.0f, 150.0f), 120.0f, hz));
    p.push_back (make (highCut,    "High Cut",   skewed (1000.0f, 20000.0f, 6000.0f), 12000.0f, hz));
    p.push_back (make (modulation, "Modulation", linear (0.0f, 100.0f), 25.0f, pct));

    // ---------- MIX ----------
    p.push_back (make (dry,     "Dry",     linear (0.0f, 100.0f), 100.0f, pct));
    p.push_back (make (wet,     "Wet",     linear (0.0f, 100.0f), 30.0f, pct));
    p.push_back (make (output,  "Output",  linear (-24.0f, 12.0f), 0.0f, db));
    p.push_back (make (ducking, "Ducking", linear (0.0f, 100.0f), 0.0f, pct));
    p.push_back (make (width,   "Width",   linear (0.0f, 200.0f), 100.0f, pct));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { freeze, 1 }, "Freeze", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { mode, 1 }, "Mode", Modes::names, 0));

    // ---------- ORBIT ----------
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { orbitOn, 1 }, "Orbit", false));
    p.push_back (std::make_unique<Float> (juce::ParameterID { orbitRate, 1 }, "Orbit Rate",
                 skewed (0.02f, 10.0f, 0.6f), 0.25f,
                 Attr().withStringFromValueFunction ([] (float v, int)
                 { return juce::String (v, v < 1.0f ? 2 : 1) + " Hz"; })));
    p.push_back (make (orbitDepth,  "Orbit Depth",   linear (0.0f, 100.0f), 70.0f, pct));
    p.push_back (make (orbitWidth,  "Orbit Width",   linear (0.0f, 100.0f), 80.0f, pct));
    p.push_back (make (orbitPhase,  "Orbit Phase",   linear (0.0f, 180.0f), 0.0f, deg));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { orbitDirection, 1 }, "Orbit Direction",
                                                               OrbitChoices::directions, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { orbitShape, 1 }, "Orbit Shape",
                                                               OrbitChoices::shapes, 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { orbitSync, 1 }, "Orbit Sync", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { orbitDivision, 1 }, "Orbit Division",
                                                               OrbitChoices::divisions, 2));
    p.push_back (std::make_unique<Float> (juce::ParameterID { orbitCenter, 1 }, "Orbit Center",
                 linear (-100.0f, 100.0f), 0.0f,
                 Attr().withStringFromValueFunction ([] (float v, int)
                 {
                     if (std::abs (v) < 0.5f) return juce::String ("C");
                     return juce::String (v < 0.0f ? "L " : "R ") + juce::String (juce::roundToInt (std::abs (v)));
                 })));
    p.push_back (make (orbitMix,    "Orbit Mix",     linear (0.0f, 100.0f), 100.0f, pct));
    p.push_back (make (orbitOffset, "Motion Offset", linear (0.0f, 360.0f), 0.0f, deg));

    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypass, 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}
