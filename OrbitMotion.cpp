#include "OrbitMotion.h"
#include "../Parameters.h"

namespace orbit
{
    namespace
    {
        constexpr double kTwoPiD = 6.283185307179586;
        constexpr float  kSep = 0.25f;        // separazione base delle due sorgenti (preserva l'immagine stereo)
        constexpr float  kMaxItdSec = 0.00045f;

        double wrapPi (double x)
        {
            while (x >  kTwoPiD * 0.5) x -= kTwoPiD;
            while (x < -kTwoPiD * 0.5) x += kTwoPiD;
            return x;
        }
    }

    void OrbitMotion::prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (Smooth* s : { &enable, &depth, &width, &phaseOffset, &center, &mix, &haas })
            s->setTime (0.04f, sr);
        for (Smooth* s : { &dirSign, &triAmt, &orbAmt })
            s->setTime (0.06f, sr);
        offset.setTime (0.08f, sr);
        rate.setTime (0.15f, sr);
        itdSamplesL.setTime (0.004f, sr);
        itdSamplesR.setTime (0.004f, sr);

        const int maxItd = (int) (kMaxItdSec * 2.0f * sr) + 8;
        itdL.setMaxDelay (maxItd);
        itdR.setMaxDelay (maxItd);
        staticL.setMaxDelay (8);
        staticR.setMaxDelay (8);
        backCoeff = lowpassCoeff (3500.0f, sr);
        reset();
    }

    void OrbitMotion::reset()
    {
        itdL.clear(); itdR.clear(); staticL.clear(); staticR.clear();
        backL.clear(); backR.clear();
        for (Smooth* s : { &enable, &depth, &width, &phaseOffset, &center, &mix, &offset, &rate, &haas,
                           &dirSign, &triAmt, &orbAmt })
            s->reset (s->target);
        itdSamplesL.reset (0.0f);
        itdSamplesR.reset (0.0f);
        correction = 0.0;
    }

    void OrbitMotion::setSettings (const OrbitSettings& s, const TransportInfo& t, int blockSize)
    {
        enable.target      = s.enabled ? 1.0f : 0.0f;
        depth.target       = std::clamp (s.depth, 0.0f, 1.0f);
        width.target       = std::clamp (s.width, 0.0f, 1.0f);
        phaseOffset.target = std::clamp (s.phaseDeg, 0.0f, 180.0f) * kPi / 180.0f;
        center.target      = std::clamp (s.center, -1.0f, 1.0f);
        mix.target         = std::clamp (s.mix, 0.0f, 1.0f);
        offset.target      = std::clamp (s.offsetDeg, 0.0f, 360.0f) * kPi / 180.0f;
        rate.target        = std::clamp (s.rateHz, 0.01f, 20.0f);
        haas.target        = std::clamp (s.haasScale, 0.0f, 2.0f);
        direction = s.direction;
        shape     = s.shape;
        triAmt.target = shape == OrbitChoices::Triangle ? 1.0f : 0.0f;
        orbAmt.target = shape == OrbitChoices::Orbital  ? 1.0f : 0.0f;

        useSync = s.sync && t.hasBpm && t.bpm > 1.0;
        correction = 0.0;

        if (useSync)
        {
            const double beats = std::max (0.0625, s.divisionBeats);
            const double cycleSeconds = beats * 60.0 / t.bpm;            // BPM letto dalla DAW, mai fisso
            syncInc = kTwoPiD / (cycleSeconds * sr);
            syncRateHz = (float) (1.0 / cycleSeconds);

            if (t.playing && t.hasPpq && blockSize > 0)
            {
                // Aggancio di fase alla posizione musicale, corretto gradualmente (nessun salto)
                const double cycles = t.ppq / beats;
                const double target = kTwoPiD * (cycles - std::floor (cycles));
                const double err = wrapPi (target - phase);
                const double amount = std::abs (err) > 1.0 ? 0.2 : 0.05;
                correction = err * amount / (double) blockSize;
            }
        }
    }

    void OrbitMotion::processSample (float& l, float& r) noexcept
    {
        const float en = enable.next();
        const float m  = mix.next() * en;
        const float dp = depth.next(), wd = width.next(), po = phaseOffset.next();
        const float c  = center.next(), off = offset.next(), hs = haas.next();
        const float rt = rate.next();

        // ---- avanzamento della fase (sempre attivo, cosi' la riaccensione e' continua) ----
        phase += useSync ? (syncInc + correction) : (kTwoPiD * (double) rt / (double) sr);
        if (phase >= kTwoPiD) { phase -= kTwoPiD; ++cycleIndex; }
        if (phase < 0.0)      { phase += kTwoPiD; --cycleIndex; }

        // percorso statico ritardato quanto il minimo del percorso orbitale (evita filtri a pettine nel mix)
        staticL.push (l); staticR.push (r);
        const float sL = staticL.at (2), sR = staticR.at (2);

        // verso del movimento: il segno e' smussato, quindi cambiare DIRECTION non provoca salti;
        // in ALTERNATE il segno si inverte a ogni giro, proprio quando la posizione passa per lo zero
        float signTarget = 1.0f;
        if (direction == OrbitChoices::RightToLeft) signTarget = -1.0f;
        else if (direction == OrbitChoices::Alternate) signTarget = (cycleIndex & 1) ? -1.0f : 1.0f;
        dirSign.target = signTarget;
        const float sign = dirSign.next();
        const float tri = triAmt.next(), orb = orbAmt.next();

        const double ph  = phase + (double) off;
        const double phR = ph + (double) po;
        auto shapeAt = [tri] (double a)
        {
            const float sv = (float) std::sin (a);
            const float tv = (2.0f / kPi) * std::asin (std::clamp (sv, -1.0f, 1.0f));
            return sv + tri * (tv - sv);                 // SINE <-> TRIANGLE con dissolvenza
        };

        const float travel = dp * wd;
        const float posL = std::clamp (c - kSep + travel * sign * shapeAt (ph),  -1.0f, 1.0f);
        const float posR = std::clamp (c + kSep + travel * sign * shapeAt (phR), -1.0f, 1.0f);

        // pan a potenza costante delle due sorgenti
        const float thL = (posL + 1.0f) * (kPi * 0.25f);
        const float thR = (posR + 1.0f) * (kPi * 0.25f);
        float oL = l * std::cos (thL) + r * std::cos (thR);
        float oR = l * std::sin (thL) + r * std::sin (thR);

        // indizio "dietro la testa" (solo forma ORBITAL)
        const float z = (float) std::cos (ph);
        const float backAmt = orb * std::max (0.0f, -z) * dp;
        const float lpL = backL.process (oL, backCoeff), lpR = backR.process (oR, backCoeff);
        const float backGain = 1.0f - 0.25f * backAmt;
        oL = (oL + backAmt * (lpL - oL)) * backGain;
        oR = (oR + backAmt * (lpR - oR)) * backGain;

        // differenza temporale interaurale: ritarda l'orecchio lontano
        const float avg = 0.5f * (posL + posR);
        const float itd = hs * kMaxItdSec * sr * std::min (1.0f, std::abs (avg)) * dp;
        itdSamplesL.target = avg > 0.0f ? itd : 0.0f;
        itdSamplesR.target = avg < 0.0f ? itd : 0.0f;
        itdL.push (oL); itdR.push (oR);
        oL = itdL.tap (2.0f + itdSamplesL.next());
        oR = itdR.tap (2.0f + itdSamplesR.next());

        l = sL + m * (oL - sL);
        r = sR + m * (oR - sR);

        if (++uiCounter >= 64)
        {
            uiCounter = 0;
            uiPosition.store (en > 0.01f ? avg : c, std::memory_order_relaxed);
            uiPhase.store ((float) (sign >= 0.0f ? ph : -ph), std::memory_order_relaxed);
            uiDepthZ.store (z, std::memory_order_relaxed);
            uiRateHz.store (useSync ? syncRateHz : rt, std::memory_order_relaxed);
        }
    }
}
