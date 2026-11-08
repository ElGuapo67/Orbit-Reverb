#pragma once
#include "DspUtils.h"

namespace orbit
{
    // Pitch shifter a due grani sovrapposti (delay modulato con finestre di Hann complementari).
    // Usato nel loop di feedback dello SHIMMER per trasporre la coda di un'ottava sopra.
    // Latenza: nessuna sul percorso del plugin (lavora solo dentro la coda wet).
    class PitchShifter
    {
    public:
        void prepare (float sampleRate)
        {
            window = 0.06f * sampleRate;                 // grani da 60 ms
            line.setMaxDelay ((int) (window * 2.0f) + 16);
            phase = 0.0f;
        }

        void clear() { line.clear(); phase = 0.0f; }

        // ratio = 2.0 -> +12 semitoni
        inline float process (float x, float ratio) noexcept
        {
            line.push (x);
            phase -= (ratio - 1.0f) / window;            // il ritardo si accorcia -> il pitch sale
            if (phase < 0.0f) phase += 1.0f;
            if (phase >= 1.0f) phase -= 1.0f;

            float p2 = phase + 0.5f;
            if (p2 >= 1.0f) p2 -= 1.0f;

            const float d1 = 3.0f + phase * window;
            const float d2 = 3.0f + p2 * window;
            const float w1 = std::sin (kPi * phase);
            const float w2 = std::sin (kPi * p2);
            // finestre sin^2 complementari: somma di potenza costante
            return line.tap (d1) * w1 * w1 + line.tap (d2) * w2 * w2;
        }

    private:
        DelayLine line;
        float window = 2880.0f, phase = 0.0f;
    };
}
