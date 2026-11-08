#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    std::atomic<float>* raw (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        auto* p = s.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    }
}

OrbitReverbProcessor::OrbitReverbProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ORBIT_REVERB", createParameterLayout())
{
    using namespace ParamID;
    pSize = raw (apvts, size);           pDecay = raw (apvts, decay);         pPreDelay = raw (apvts, preDelay);
    pAttack = raw (apvts, attack);       pRelease = raw (apvts, release);     pDiffusion = raw (apvts, diffusion);
    pDamping = raw (apvts, damping);     pLowCut = raw (apvts, lowCut);       pHighCut = raw (apvts, highCut);
    pModulation = raw (apvts, modulation);
    pDry = raw (apvts, dry);             pWet = raw (apvts, wet);             pOutput = raw (apvts, output);
    pDucking = raw (apvts, ducking);     pWidth = raw (apvts, width);         pFreeze = raw (apvts, freeze);
    pMode = raw (apvts, mode);           pBypass = raw (apvts, bypass);
    pOrbitOn = raw (apvts, orbitOn);     pOrbitRate = raw (apvts, orbitRate); pOrbitDepth = raw (apvts, orbitDepth);
    pOrbitWidth = raw (apvts, orbitWidth); pOrbitPhase = raw (apvts, orbitPhase);
    pOrbitDirection = raw (apvts, orbitDirection); pOrbitShape = raw (apvts, orbitShape);
    pOrbitSync = raw (apvts, orbitSync); pOrbitDivision = raw (apvts, orbitDivision);
    pOrbitCenter = raw (apvts, orbitCenter); pOrbitMix = raw (apvts, orbitMix); pOrbitOffset = raw (apvts, orbitOffset);
    bypassParam = apvts.getParameter (bypass);
}

bool OrbitReverbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk && in.size() <= out.size();     // mono>mono, mono>stereo, stereo>stereo
}

void OrbitReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const float sr = (float) sampleRate;

    engine.prepare (sampleRate);
    orbitMotion.prepare (sampleRate);
    dynamics.prepare (sr);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };
    lowCutFilter.prepare (spec);
    highCutFilter.prepare (spec);
    lowCutFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCutFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowCutFilter.setResonance (0.7071f);
    highCutFilter.setResonance (0.7071f);

    for (auto* s : { &dryGain, &wetGain, &outGain, &wetWidth })
        s->reset (sampleRate, 0.03);
    bypassMix.reset (sampleRate, 0.02);

    lowCutLog.setTime (0.05f, sr);
    highCutLog.setTime (0.05f, sr);

    reset();
}

void OrbitReverbProcessor::reset()
{
    engine.reset();
    orbitMotion.reset();
    dynamics.reset();
    lowCutFilter.reset();
    highCutFilter.reset();

    dryGain.setCurrentAndTargetValue (pDry->load() * 0.01f);
    wetGain.setCurrentAndTargetValue (pWet->load() * 0.01f);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    wetWidth.setCurrentAndTargetValue (pWidth->load() * 0.01f);
    bypassMix.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    lowCutLog.reset (std::log (pLowCut->load()));
    highCutLog.reset (std::log (pHighCut->load()));
    updateFilters (true);
}

bool OrbitReverbProcessor::isOrbitActive() const
{
    return pOrbitOn->load() > 0.5f || (int) pMode->load() == Modes::Orbital;
}

double OrbitReverbProcessor::getTailLengthSeconds() const
{
    if (pFreeze->load() > 0.5f || (int) pMode->load() == Modes::Infinite)
        return 60.0;
    return juce::jmin (60.0, (double) pDecay->load() * 1.3 + pPreDelay->load() * 0.001 + 0.5);
}

void OrbitReverbProcessor::updateFilters (bool force)
{
    const float lc = std::exp (lowCutLog.current), hc = std::exp (highCutLog.current);
    const float nyq = 0.45f * (float) currentSampleRate;
    if (force || ++filterCounter >= 16)
    {
        filterCounter = 0;
        lowCutFilter.setCutoffFrequency (juce::jlimit (10.0f, nyq, lc));
        highCutFilter.setCutoffFrequency (juce::jlimit (200.0f, nyq, hc));
    }
}

void OrbitReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numSamples == 0 || numOut == 0)
        return;

    // ---------------- parametri efficaci (MODE) ----------------
    const int mode = juce::jlimit (0, 6, (int) pMode->load());

    orbit::ReverbSettings rs;
    rs.size       = pSize->load() * 0.01f;
    rs.decaySec   = pDecay->load();
    rs.preDelayMs = pPreDelay->load();
    rs.diffusion  = pDiffusion->load() * 0.01f;
    rs.damping    = pDamping->load() * 0.01f;
    rs.modulation = pModulation->load() * 0.01f;
    rs.erLevel    = 0.3f;
    rs.shimmer    = 0.0f;
    rs.freeze     = pFreeze->load() > 0.5f;

    float highCutHz = pHighCut->load(), widthMul = 1.0f, orbitDepthMul = 1.0f, haas = 1.0f;
    bool gated = false, orbitForced = false;

    switch (mode)
    {
        case Modes::Clean:    rs.erLevel = 0.35f; rs.modulation *= 0.6f; break;
        case Modes::Lush:     rs.erLevel = 0.12f; rs.modulation = juce::jmin (1.0f, rs.modulation * 1.8f + 0.15f);
                              rs.diffusion = juce::jmin (1.0f, rs.diffusion + 0.2f); rs.decaySec *= 1.25f;
                              widthMul = 1.2f; break;
        case Modes::Dark:     rs.erLevel = 0.25f; rs.damping = 0.55f + 0.45f * rs.damping;
                              highCutHz = juce::jmin (highCutHz, 4500.0f); break;
        case Modes::Gated:    rs.erLevel = 0.5f; rs.decaySec = juce::jmax (rs.decaySec, 1.5f);
                              rs.diffusion = 1.0f; gated = true; break;
        case Modes::Shimmer:  rs.erLevel = 0.1f; rs.shimmer = 0.55f; rs.modulation = juce::jmax (rs.modulation, 0.3f);
                              rs.damping *= 0.6f; break;
        case Modes::Infinite: rs.erLevel = 0.05f; rs.decaySec = juce::jlimit (40.0f, 180.0f, rs.decaySec * 6.0f);
                              rs.damping *= 0.5f; rs.modulation = juce::jmax (rs.modulation, 0.3f); break;
        case Modes::Orbital:  rs.erLevel = 0.15f; rs.modulation = juce::jmin (1.0f, rs.modulation * 1.2f);
                              orbitForced = true; orbitDepthMul = 1.35f; haas = 1.6f; widthMul = 1.15f; break;
        default: break;
    }

    engine.setSettings (rs);
    dynamics.setParams (pAttack->load(), pRelease->load(), gated, pDucking->load() * 0.01f);

    // ---------------- ORBIT + trasporto della DAW ----------------
    orbit::OrbitSettings os;
    os.enabled   = pOrbitOn->load() > 0.5f || orbitForced;
    os.rateHz    = pOrbitRate->load();
    os.depth     = juce::jmin (1.0f, pOrbitDepth->load() * 0.01f * orbitDepthMul);
    os.width     = pOrbitWidth->load() * 0.01f;
    os.phaseDeg  = pOrbitPhase->load();
    os.direction = (int) pOrbitDirection->load();
    os.shape     = (int) pOrbitShape->load();
    os.sync      = pOrbitSync->load() > 0.5f;
    os.divisionBeats = OrbitChoices::divisionBeats[juce::jlimit (0, 7, (int) pOrbitDivision->load())];
    os.center    = pOrbitCenter->load() * 0.01f;
    os.mix       = pOrbitMix->load() * 0.01f;
    os.offsetDeg = pOrbitOffset->load();
    os.haasScale = haas;

    orbit::TransportInfo transport;
    if (auto* playHead = getPlayHead())
    {
        if (const auto pos = playHead->getPosition())
        {
            if (const auto bpm = pos->getBpm())         { transport.bpm = *bpm; transport.hasBpm = true; }
            if (const auto ppq = pos->getPpqPosition()) { transport.ppq = *ppq; transport.hasPpq = true; }
            transport.playing = pos->getIsPlaying();
        }
    }
    orbitMotion.setSettings (os, transport, numSamples);

    // ---------------- target smussati ----------------
    dryGain.setTargetValue (pDry->load() * 0.01f);
    wetGain.setTargetValue (pWet->load() * 0.01f);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    wetWidth.setTargetValue (pWidth->load() * 0.01f * widthMul);
    bypassMix.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    lowCutLog.target  = std::log (juce::jlimit (20.0f, 1000.0f, pLowCut->load()));
    highCutLog.target = std::log (juce::jlimit (1000.0f, 20000.0f, highCutHz));

    // ---------------- elaborazione per campione ----------------
    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const float l = inL[n];
        const float r = inR[n];

        float feed = 1.0f, wetEnv = 1.0f;
        dynamics.process (juce::jmax (std::abs (l), std::abs (r)), feed, wetEnv);

        float wl = 0.0f, wr = 0.0f;
        engine.processSample (l, r, feed, wl, wr);

        lowCutLog.next(); highCutLog.next();
        updateFilters (false);
        wl = highCutFilter.processSample (0, lowCutFilter.processSample (0, wl));
        wr = highCutFilter.processSample (1, lowCutFilter.processSample (1, wr));

        // larghezza stereo del wet (M/S)
        const float w = wetWidth.getNextValue();
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * w;
        wl = mid + side;
        wr = mid - side;

        // ORBIT: solo sul segnale wet
        orbitMotion.processSample (wl, wr);

        const float wg = wetGain.getNextValue() * wetEnv;
        const float dg = dryGain.getNextValue();
        const float og = outGain.getNextValue();
        const float bp = bypassMix.getNextValue();

        float oL = (l * dg + wl * wg) * og;
        float oR = (r * dg + wr * wg) * og;
        oL += bp * (l - oL);
        oR += bp * (r - oR);

        if (outR != nullptr) { outL[n] = oL; outR[n] = oR; }
        else                 { outL[n] = 0.5f * (oL + oR); }
    }
}

//==============================================================================
int OrbitReverbProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

const juce::String OrbitReverbProcessor::getProgramName (int index)
{
    const auto& p = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void OrbitReverbProcessor::setCurrentProgram (int index)
{
    const auto& p = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) p.size()))
        return;
    currentProgram = index;
    currentPresetName = p[(size_t) index].name;
    applyFactoryPreset (apvts, p[(size_t) index]);
}

//==============================================================================
void OrbitReverbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OrbitReverbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPresetName = tree.getProperty ("presetName", "Default").toString();
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
            syncParametersToTree (tree, true);
        }
    }
}

// Riallinea ogni parametro al valore salvato e avvisa l'host. Necessario per i parametri a scatti
// (bool / scelte): se l'host aveva un valore normalizzato intermedio (es. 0.29 per un bool "spento"),
// replaceState non vede differenze e l'host resterebbe disallineato.
void OrbitReverbProcessor::syncParametersToTree (const juce::ValueTree& tree, bool includeBypass)
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (! includeBypass && ranged->getParameterID() == ParamID::bypass)
                continue;
            const auto child = tree.getChildWithProperty ("id", ranged->getParameterID());
            if (! child.isValid())
                continue;
            const float target = ranged->convertTo0to1 ((float) child.getProperty ("value"));
            if (std::abs (ranged->getValue() - target) > 1.0e-6f)
                ranged->setValueNotifyingHost (target);
        }
}

//==============================================================================
juce::File OrbitReverbProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("OrbitReverb").getChildFile ("Presets");
}

juce::Array<juce::File> OrbitReverbProcessor::getUserPresets() const
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.orbitpreset");
    files.sort();
    return files;
}

bool OrbitReverbProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;

    auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
        return false;

    auto state = apvts.copyState();
    state.setProperty ("presetName", name.trim(), nullptr);
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (folder.getChildFile (clean + ".orbitpreset")))
        return false;

    currentPresetName = name.trim();
    return true;
}

bool OrbitReverbProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    // applica i valori notificando l'host (automazione e undo della DAW restano coerenti); il bypass non si tocca
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (ranged->getParameterID() != ParamID::bypass)
                ranged->beginChangeGesture();
    syncParametersToTree (tree, false);
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (ranged->getParameterID() != ParamID::bypass)
                ranged->endChangeGesture();
    currentPresetName = tree.getProperty ("presetName", file.getFileNameWithoutExtension()).toString();
    return true;
}

//==============================================================================
juce::AudioProcessorEditor* OrbitReverbProcessor::createEditor()
{
    return new OrbitReverbEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrbitReverbProcessor();
}
