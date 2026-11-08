#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace orbit
{
    inline constexpr float kPi    = 3.14159265358979f;
    inline constexpr float kTwoPi = 6.28318530717959f;

    // Coefficiente di uno smoother a un polo per una costante di tempo in secondi
    inline float onePoleCoeff (float seconds, float sampleRate)
    {
        if (seconds <= 0.0f) return 1.0f;
        return 1.0f - std::exp (-1.0f / (seconds * sampleRate));
    }

    // Coefficiente di un passa-basso a un polo per una frequenza di taglio
    inline float lowpassCoeff (float hz, float sampleRate)
    {
        hz = std::min (hz, 0.49f * sampleRate);
        return 1.0f - std::exp (-kTwoPi * hz / sampleRate);
    }

    // Limitatore morbido di sicurezza: trasparente sotto |limit|, impedisce qualunque crescita illimitata
    inline float safety (float x, float limit = 4.0f)
    {
        if (x > limit || x < -limit)
            return limit * std::tanh (x / limit);
        return x;
    }

    //==========================================================================
    // Linea di ritardo circolare (dimensione potenza di 2), lettura intera o frazionaria (Hermite cubica).
    // Convenzione: le letture avvengono PRIMA del push; tap(d) restituisce x[n - d] con d >= 1.
    class DelayLine
    {
    public:
        void setMaxDelay (int maxSamples)
        {
            int s = 1;
            while (s < maxSamples + 8) s <<= 1;
            buffer.assign ((size_t) s, 0.0f);
            mask = s - 1;
            writeIndex = 0;
        }

        void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); writeIndex = 0; }
        int  capacity() const { return mask - 4; }

        inline void push (float x) noexcept
        {
            buffer[(size_t) writeIndex] = x;
            writeIndex = (writeIndex + 1) & mask;
        }

        inline float at (int samplesAgo) const noexcept   // samplesAgo >= 1
        {
            return buffer[(size_t) ((writeIndex - samplesAgo) & mask)];
        }

        inline float tap (float delay) const noexcept      // delay >= 2 (frazionario)
        {
            delay = std::clamp (delay, 2.0f, (float) capacity());
            const int   i = (int) delay;
            const float f = delay - (float) i;
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

    private:
        std::vector<float> buffer;
        int mask = 0, writeIndex = 0;
    };

    //==========================================================================
    // Allpass di Schroeder a lunghezza fissa (diffusione)
    class Allpass
    {
    public:
        void prepare (int lengthSamples) { length = std::max (2, lengthSamples); line.setMaxDelay (length + 2); }
        void clear() { line.clear(); }

        inline float process (float x, float g) noexcept
        {
            const float z = line.at (length);
            const float v = x - g * z;
            line.push (v);
            return z + g * v;
        }

    private:
        DelayLine line;
        int length = 2;
    };

    //==========================================================================
    struct OnePoleLP
    {
        float y = 0.0f;
        inline float process (float x, float a) noexcept { y += a * (x - y); return y; }
        void clear() { y = 0.0f; }
    };

    struct OnePoleHP
    {
        float lp = 0.0f;
        inline float process (float x, float a) noexcept { lp += a * (x - lp); return x - lp; }
        void clear() { lp = 0.0f; }
    };

    // Smoother lineare-esponenziale minimale senza allocazioni
    struct Smooth
    {
        float current = 0.0f, target = 0.0f, coeff = 1.0f;
        void setTime (float seconds, float sr) { coeff = onePoleCoeff (seconds, sr); }
        void reset (float v) { current = target = v; }
        inline float next() noexcept { current += coeff * (target - current); return current; }
    };
}
