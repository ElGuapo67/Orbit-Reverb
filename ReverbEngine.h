#pragma once
#include "DspUtils.h"
#include "PitchShifter.h"
#include <array>

namespace orbit
{
    // Valori "efficaci" calcolati dal processore a partire da parametri + MODE (una volta per blocco)
    struct ReverbSettings
    {
        float size       = 0.5f;   // 0..1
        float decaySec   = 2.5f;   // RT60 in secondi
        float preDelayMs = 20.0f;
        float diffusion  = 0.75f;  // 0..1
        float damping    = 0.4f;   // 0..1
        float modulation = 0.25f;  // 0..1 (gia' scalata dal modo)
        float erLevel    = 0.3f;   // livello early reflections 0..1
        float shimmer    = 0.0f;   // quantita' di trasposizione +12 nel feedback 0..1
        bool  freeze     = false;
    };

    /*  Architettura
        input -> pre-delay -> [early reflections (multi-tap)]
                           -> diffusione (4 allpass per canale)
                           -> FDN a 8 linee (matrice di Householder, ortogonale = senza perdita)
                                ogni linea: ritardo modulato (Hermite) + damping (LP a un polo) + guadagno RT60
                                opzionale: shimmer (+12 st) reiniettato nel feedback
        uscita: tap decorrelati L/R delle linee + early reflections
        FREEZE: guadagno di loop = 1, input escluso, damping escluso (matrice ortogonale -> energia costante),
                con limitatore morbido di sicurezza dentro il loop.                                          */
    class ReverbEngine
    {
    public:
        static constexpr int numLines = 8;

        void prepare (double sampleRate);
        void reset();
        void setSettings (const ReverbSettings& s);   // chiamata una volta per blocco (thread audio, no allocazioni)

        // feed = guadagno dell'energia in ingresso (inviluppo ATTACK)
        void processSample (float inL, float inR, float feed, float& outL, float& outR) noexcept;

    private:
        float sr = 48000.0f;

        DelayLine inputL, inputR;                  // pre-delay + sorgente early reflections
        std::array<Allpass, 4> diffL, diffR;
        std::array<DelayLine, numLines> lines;
        std::array<OnePoleLP, numLines> damp;
        std::array<float, numLines> lfoPhase {}, lfoInc {}, gains {};
        PitchShifter shifter;
        OnePoleHP shimmerHP;
        OnePoleLP shimmerLP;

        Smooth sizeScale, preDelaySamples, modDepthSamples, freezeAmt, shimmerAmt, erLevel, diffusionG;
        float dampCoeff = 1.0f, targetDecay = 2.5f;
        float shimmerHPa = 0.0f, shimmerLPa = 1.0f;
        int   blockCounter = 0;

        static constexpr std::array<float, numLines> baseMs { 31.7f, 37.9f, 41.3f, 47.1f, 53.9f, 61.1f, 67.3f, 73.7f };
        static constexpr std::array<float, numLines> lfoHz  { 0.13f, 0.17f, 0.21f, 0.27f, 0.31f, 0.37f, 0.41f, 0.47f };
        static constexpr std::array<float, numLines> outL   { 1.f, 1.f, -1.f, 1.f, -1.f, -1.f, 1.f, -1.f };
        static constexpr std::array<float, numLines> outR   { 1.f, -1.f, 1.f, 1.f, -1.f, 1.f, -1.f, -1.f };
        static constexpr std::array<float, 8> erMsL  { 7.1f, 11.3f, 17.9f, 23.5f, 31.7f, 37.3f, 43.9f, 52.1f };
        static constexpr std::array<float, 8> erMsR  { 8.3f, 13.1f, 16.1f, 25.9f, 29.3f, 39.7f, 47.3f, 55.7f };
        static constexpr std::array<float, 8> erGain { 0.80f, -0.68f, 0.60f, -0.52f, 0.44f, -0.37f, 0.31f, -0.26f };

        static constexpr float maxSizeScale = 2.0f, maxPreDelayMs = 500.0f, maxModMs = 6.0f;

        float lineLength (int i) const noexcept { return baseMs[(size_t) i] * 0.001f * sr * sizeScale.current; }
        void  updateGains() noexcept;
    };
}
