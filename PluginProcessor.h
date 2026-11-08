#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "dsp/ReverbEngine.h"
#include "dsp/OrbitMotion.h"
#include "dsp/WetDynamics.h"

class OrbitReverbProcessor : public juce::AudioProcessor
{
public:
    OrbitReverbProcessor();
    ~OrbitReverbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ORBIT REVERB"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // ---- preset utente ----
    static juce::File getUserPresetFolder();
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String getCurrentPresetName() const { return currentPresetName; }

    // Valori di sola lettura per la GUI (animazione ORBIT)
    const orbit::OrbitMotion& getOrbitMotion() const { return orbitMotion; }
    bool isOrbitActive() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    orbit::ReverbEngine engine;
    orbit::OrbitMotion  orbitMotion;
    orbit::WetDynamics  dynamics;
    juce::dsp::StateVariableTPTFilter<float> lowCutFilter, highCutFilter;

    juce::SmoothedValue<float> dryGain, wetGain, outGain, wetWidth, bypassMix;
    orbit::Smooth lowCutLog, highCutLog;
    int filterCounter = 0;
    double currentSampleRate = 48000.0;

    int currentProgram = 0;
    juce::String currentPresetName { "Default" };

    // puntatori ai valori dei parametri (letture atomiche nel thread audio)
    std::atomic<float> *pSize, *pDecay, *pPreDelay, *pAttack, *pRelease, *pDiffusion,
                       *pDamping, *pLowCut, *pHighCut, *pModulation,
                       *pDry, *pWet, *pOutput, *pDucking, *pWidth, *pFreeze, *pMode, *pBypass,
                       *pOrbitOn, *pOrbitRate, *pOrbitDepth, *pOrbitWidth, *pOrbitPhase, *pOrbitDirection,
                       *pOrbitShape, *pOrbitSync, *pOrbitDivision, *pOrbitCenter, *pOrbitMix, *pOrbitOffset;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    void updateFilters (bool force);
    void syncParametersToTree (const juce::ValueTree& tree, bool includeBypass);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitReverbProcessor)
};
