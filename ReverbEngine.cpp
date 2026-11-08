#include "ReverbEngine.h"

namespace orbit
{
    namespace
    {
        constexpr std::array<float, 4> diffMsL { 4.77f, 3.59f, 12.73f, 9.30f };
        constexpr std::array<float, 4> diffMsR { 4.93f, 3.71f, 12.11f, 8.71f };

        float sizeToScale (float size) { return 0.3f + 1.7f * std::pow (std::clamp (size, 0.0f, 1.0f), 1.2f); }
    }

    void ReverbEngine::prepare (double sampleRate)
    {
        sr = (float) sampleRate;

        const int maxInput = (int) ((maxPreDelayMs + 56.0f * 1.6f + 5.0f) * 0.001f * sr);
        inputL.setMaxDelay (maxInput);
        inputR.setMaxDelay (maxInput);

        for (size_t k = 0; k < 4; ++k)
        {
            diffL[k].prepare ((int) (diffMsL[k] * 0.001f * sr));
            diffR[k].prepare ((int) (diffMsR[k] * 0.001f * sr));
        }

        const int maxLine = (int) ((baseMs.back() * maxSizeScale + 2.0f * maxModMs + 4.0f) * 0.001f * sr);
        for (auto& l : lines) l.setMaxDelay (maxLine);

        for (size_t i = 0; i < numLines; ++i)
            lfoInc[i] = kTwoPi * lfoHz[i] / sr;

        shifter.prepare (sr);
        shimmerHPa = lowpassCoeff (350.0f, sr);
        shimmerLPa = lowpassCoeff (7000.0f, sr);

        sizeScale.setTime (0.25f, sr);
        preDelaySamples.setTime (0.08f, sr);
        modDepthSamples.setTime (0.15f, sr);
        freezeAmt.setTime (0.06f, sr);
        shimmerAmt.setTime (0.2f, sr);
        erLevel.setTime (0.1f, sr);
        diffusionG.setTime (0.1f, sr);

        reset();
    }

    void ReverbEngine::reset()
    {
        inputL.clear(); inputR.clear();
        for (auto& d : diffL) d.clear();
        for (auto& d : diffR) d.clear();
        for (auto& l : lines) l.clear();
        for (auto& d : damp)  d.clear();
        shifter.clear();
        shimmerHP.clear(); shimmerLP.clear();

        for (size_t i = 0; i < numLines; ++i)
            lfoPhase[i] = kTwoPi * (float) i / (float) numLines;

        sizeScale.reset (sizeScale.target > 0.0f ? sizeScale.target : sizeToScale (0.5f));
        preDelaySamples.reset (preDelaySamples.target);
        modDepthSamples.reset (modDepthSamples.target);
        freezeAmt.reset (freezeAmt.target);
        shimmerAmt.reset (shimmerAmt.target);
        erLevel.reset (erLevel.target);
        diffusionG.reset (diffusionG.target);
        blockCounter = 0;
        updateGains();
    }

    void ReverbEngine::setSettings (const ReverbSettings& s)
    {
        sizeScale.target       = sizeToScale (s.size);
        preDelaySamples.target = 2.0f + std::clamp (s.preDelayMs, 0.0f, maxPreDelayMs) * 0.001f * sr;
        modDepthSamples.target = std::clamp (s.modulation, 0.0f, 1.0f) * maxModMs * 0.001f * sr;
        freezeAmt.target       = s.freeze ? 1.0f : 0.0f;
        shimmerAmt.target      = std::clamp (s.shimmer, 0.0f, 1.0f);
        erLevel.target         = std::clamp (s.erLevel, 0.0f, 1.0f);
        diffusionG.target      = 0.78f * std::clamp (s.diffusion, 0.0f, 1.0f);
        targetDecay            = std::clamp (s.decaySec, 0.05f, 200.0f);

        const float d = std::clamp (s.damping, 0.0f, 1.0f);
        dampCoeff = d < 0.001f ? 1.0f : lowpassCoeff (18000.0f * std::pow (0.08f, d), sr);
    }

    void ReverbEngine::updateGains() noexcept
    {
        const float fz = freezeAmt.current;
        for (size_t i = 0; i < numLines; ++i)
        {
            const float len = lineLength ((int) i) + modDepthSamples.current;
            float g = std::pow (10.0f, -3.0f * len / (sr * targetDecay));   // RT60: -60 dB dopo decay secondi
            g *= 1.0f - 0.25f * shimmerAmt.current;                          // margine di stabilita' con lo shimmer
            g += (1.0f - g) * fz;                                             // FREEZE -> loop senza perdite
            gains[i] = std::min (g, 1.0f);
        }
    }

    void ReverbEngine::processSample (float inL, float inR, float feed, float& outLeft, float& outRight) noexcept
    {
        const float scale = sizeScale.next();
        const float pd    = preDelaySamples.next();
        const float md    = modDepthSamples.next();
        const float fz    = freezeAmt.next();
        const float sh    = shimmerAmt.next() * (1.0f - fz);
        const float er    = erLevel.next();
        const float dg    = diffusionG.next();

        if (++blockCounter >= 32) { blockCounter = 0; updateGains(); }

        // ---- ingresso (escluso durante FREEZE) ----
        const float gate = feed * (1.0f - fz);
        inputL.push (inL * gate);
        inputR.push (inR * gate);

        float xL = inputL.tap (pd);
        float xR = inputR.tap (pd);

        // ---- early reflections ----
        const float erScale = (0.4f + 0.6f * scale) * 0.001f * sr;
        float eL = 0.0f, eR = 0.0f;
        for (size_t k = 0; k < erGain.size(); ++k)
        {
            eL += erGain[k] * inputL.tap (pd + erMsL[k] * erScale);
            eR += erGain[k] * inputR.tap (pd + erMsR[k] * erScale);
        }

        // ---- diffusione ----
        for (size_t k = 0; k < 4; ++k)
        {
            xL = diffL[k].process (xL, dg);
            xR = diffR[k].process (xR, dg);
        }

        // ---- FDN ----
        std::array<float, numLines> o;
        float sum = 0.0f;
        const float a = dampCoeff + (1.0f - dampCoeff) * fz;      // in FREEZE niente damping

        for (size_t i = 0; i < numLines; ++i)
        {
            lfoPhase[i] += lfoInc[i];
            if (lfoPhase[i] >= kTwoPi) lfoPhase[i] -= kTwoPi;

            const float len = baseMs[i] * 0.001f * sr * scale + md * (1.0f + std::sin (lfoPhase[i])) + 2.0f;
            o[i] = damp[i].process (lines[i].tap (len), a);
            sum += o[i];
        }

        // shimmer: +12 semitoni sulla somma delle linee, reiniettato nel feedback
        const float shimIn = shimmerHP.process (sum * 0.25f, shimmerHPa);
        const float shimmered = shimmerLP.process (shifter.process (shimIn, 2.0f), shimmerLPa) * sh;

        const float h = sum * (2.0f / (float) numLines);           // matrice di Householder
        for (size_t i = 0; i < numLines; ++i)
        {
            const float input = (i & 1u) ? xR : xL;
            const float v = 0.5f * input + gains[i] * (o[i] - h) + ((i & 1u) ? -0.5f : 0.5f) * shimmered;
            lines[i].push (safety (v));
        }

        float lateL = 0.0f, lateR = 0.0f;
        for (size_t i = 0; i < numLines; ++i)
        {
            lateL += outL[i] * o[i];
            lateR += outR[i] * o[i];
        }

        outLeft  = 0.3f * lateL + 0.45f * er * eL;
        outRight = 0.3f * lateR + 0.45f * er * eR;
    }
}
