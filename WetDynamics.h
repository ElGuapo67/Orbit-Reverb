#pragma once
#include "DspUtils.h"

namespace orbit
{
    /*  Inviluppi applicati alla componente riverberata (il dry non viene mai toccato).

        Un rilevatore segue il livello del segnale in ingresso e decide se il suono e' "attivo"
        (gate con isteresi a circa -49/-54 dBFS e hold di 40 ms).

        ATTACK  -> guadagno sull'energia che ENTRA nel riverbero. Quando il suono inizia, il guadagno
                   sale da 0 a 1 in circa "Attack" ms (curva esponenziale, ~95 % al tempo indicato);
                   quando il suono si ferma torna a 0 in 25 ms, cosi' ogni nuova frase "cresce" di nuovo.
                   Attack = 0 -> nessun effetto. Valori lunghi = riverbero che si gonfia (swell).
        RELEASE -> dissolvenza AGGIUNTIVA dell'uscita wet dopo la fine del suono, in circa "Release" ms.
                   Al massimo ("Natural") e' disattivato: la coda segue solo il DECAY.
                   Quando il suono riparte il wet torna a 1 in 5 ms (senza click).
        GATED   -> (modo GATED) il wet resta pieno per "Release" ms dopo la fine del suono (lunghezza
                   del gate, 400 ms se Release e' su Natural) e poi viene chiuso con un taglio netto di ~12 ms.
        DUCKING -> il wet viene abbassato mentre il segnale originale suona:
                   gain = 1 / (1 + 10 * ducking * envelope), envelope con attacco 5 ms e rilascio 250 ms.     */
    class WetDynamics
    {
    public:
        void prepare (float sampleRate)
        {
            sr = sampleRate;
            detAtt   = onePoleCoeff (0.001f, sr);
            detRel   = onePoleCoeff (0.060f, sr);
            duckAtt  = onePoleCoeff (0.005f, sr);
            duckRel  = onePoleCoeff (0.250f, sr);
            feedClose = onePoleCoeff (0.025f, sr);
            fastOpen  = onePoleCoeff (0.005f, sr);
            gateCut   = onePoleCoeff (0.004f, sr);
            holdSamples = (int) (0.04f * sr);
            reset();
        }

        void reset()
        {
            env = duckEnv = 0.0f;
            feed = 1.0f; rel = 1.0f;
            open = false; holdCounter = 0; gateCounter = 0;
        }

        void setParams (float attackMs, float releaseMs, bool gatedMode, float ducking01)
        {
            attackOn  = attackMs > 0.5f;
            attCoeff  = onePoleCoeff (attackMs * 0.001f / 3.0f, sr);
            natural   = releaseMs >= kNaturalThreshold;
            relCoeff  = onePoleCoeff (releaseMs * 0.001f / 3.0f, sr);
            gated     = gatedMode;
            gateLen   = (int) ((natural ? 400.0f : std::min (releaseMs, 3000.0f)) * 0.001f * sr);
            duck      = std::clamp (ducking01, 0.0f, 1.0f);
        }

        // detector = |ingresso| (max dei canali). Restituisce i due guadagni.
        inline void process (float detector, float& feedGain, float& wetGain) noexcept
        {
            env += (detector > env ? detAtt : detRel) * (detector - env);
            duckEnv += (detector > duckEnv ? duckAtt : duckRel) * (detector - duckEnv);

            if (env > 0.0035f)        { open = true; holdCounter = holdSamples; }
            else if (env < 0.0020f)   { if (holdCounter > 0) --holdCounter; else open = false; }

            // ATTACK (energia in ingresso)
            if (! attackOn)  feed = 1.0f;
            else             feed += (open ? attCoeff : feedClose) * ((open ? 1.0f : 0.0f) - feed);

            // RELEASE / GATED (uscita wet)
            if (gated)
            {
                if (open) gateCounter = gateLen;
                else if (gateCounter > 0) --gateCounter;
                const bool hold = open || gateCounter > 0;
                rel += (hold ? fastOpen : gateCut) * ((hold ? 1.0f : 0.0f) - rel);
            }
            else if (natural)
            {
                rel += fastOpen * (1.0f - rel);
            }
            else
            {
                rel += (open ? fastOpen : relCoeff) * ((open ? 1.0f : 0.0f) - rel);
            }

            const float duckGain = 1.0f / (1.0f + 10.0f * duck * duckEnv);
            feedGain = feed;
            wetGain  = rel * duckGain;
        }

        static constexpr float kNaturalThreshold = 9999.0f;

    private:
        float sr = 48000.0f;
        float env = 0.0f, duckEnv = 0.0f, feed = 1.0f, rel = 1.0f;
        float detAtt = 0, detRel = 0, duckAtt = 0, duckRel = 0, feedClose = 0, fastOpen = 0, gateCut = 0;
        float attCoeff = 1.0f, relCoeff = 1.0f, duck = 0.0f;
        bool  attackOn = false, natural = true, gated = false, open = false;
        int   holdSamples = 0, holdCounter = 0, gateLen = 0, gateCounter = 0;
    };
}
