#pragma once
#include "DspUtils.h"
#include <atomic>

namespace orbit
{
    struct OrbitSettings
    {
        bool  enabled   = false;
        float rateHz    = 0.25f;
        float depth     = 0.7f;   // 0..1
        float width     = 0.8f;   // 0..1  escursione L-R
        float phaseDeg  = 0.0f;   // 0..180 sfasamento tra le due sorgenti (L e R del wet)
        int   direction = 0;      // 0 L>R, 1 R>L, 2 alternata
        int   shape     = 0;      // 0 sine, 1 triangle, 2 orbital (con indizi davanti/dietro)
        bool  sync      = false;
        double divisionBeats = 4.0;
        float center    = 0.0f;   // -1..1
        float mix       = 1.0f;   // 0..1
        float offsetDeg = 0.0f;   // 0..360
        float haasScale = 1.0f;   // differenze temporali tra i canali (enfatizzate nel modo ORBITAL)
    };

    struct TransportInfo
    {
        double bpm = 120.0;
        double ppq = 0.0;
        bool   hasBpm = false, hasPpq = false, playing = false;
    };

    /*  ORBIT MOTION - movimento stereo continuo applicato SOLO al segnale wet.

        - Il wet stereo viene trattato come DUE sorgenti (sinistra e destra del riverbero) che
          vengono posizionate con un pan a potenza costante: la larghezza stereo del riverbero
          e' preservata (nessuna somma mono, nessuno scambio brusco dei canali).
        - Posizione: centro + profondita' * larghezza * forma(fase), con PHASE che sfasa le due sorgenti
          (0 = si muovono insieme, 180 = si incrociano).
        - Differenza temporale interaurale (fino a ~0.45 ms) sul canale piu' lontano + lieve attenuazione:
          rende il movimento percepibile come rotazione, con impatto mono contenuto.
        - Forma ORBITAL: aggiunge un indizio "dietro la testa" (filtro passa-basso + -2.5 dB
          quando la sorgente passa dietro) per un movimento tridimensionale.
        - SYNC: la fase e' agganciata alla posizione musicale della DAW (PPQ) tramite un anello di
          correzione dolce: niente salti quando cambia il BPM o si sposta la riproduzione.
        - Tutti i parametri sono smussati: nessun click durante l'automazione.                         */
    class OrbitMotion
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void setSettings (const OrbitSettings& s, const TransportInfo& t, int blockSize);
        void processSample (float& l, float& r) noexcept;

        // Valori pubblicati per la GUI (solo lettura dal message thread)
        std::atomic<float> uiPosition { 0.0f }, uiPhase { 0.0f }, uiRateHz { 0.25f }, uiDepthZ { 1.0f };

    private:
        float sr = 48000.0f;
        double phase = 0.0;        // radianti, sempre in avanzamento (anche con ORBIT spento -> riaccensione fluida)
        double baseInc = 0.0;      // incremento per campione
        double correction = 0.0;   // correzione PLL distribuita sul blocco
        int    cycleIndex = 0;     // per la direzione alternata

        Smooth enable, depth, width, phaseOffset, center, mix, offset, rate, haas, dirSign, triAmt, orbAmt;
        int direction = 0, shape = 0, uiCounter = 0;

        bool   useSync = false;
        double syncInc = 0.0;
        float  syncRateHz = 0.25f;

        DelayLine itdL, itdR, staticL, staticR;
        Smooth itdSamplesL, itdSamplesR;
        OnePoleLP backL, backR;
        float backCoeff = 1.0f;

    };
}
