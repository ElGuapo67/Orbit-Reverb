// Test offline di ORBIT REVERB: fa suonare il processore senza DAW e MISURA il comportamento.
// Uscita: una riga PASS/FAIL per test, codice di ritorno != 0 se qualcosa fallisce.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"
#include <random>
#include <chrono>

namespace
{
    int failures = 0, passes = 0;

    void check (bool ok, const juce::String& name, const juce::String& detail = {})
    {
        (ok ? passes : failures)++;
        std::printf ("[%s] %s%s\n", ok ? "PASS" : "FAIL", name.toRawUTF8(),
                     detail.isEmpty() ? "" : ("  (" + detail + ")").toRawUTF8());
    }

    struct MockPlayHead : juce::AudioPlayHead
    {
        double bpm = 120.0, ppq = 0.0; bool playing = true, enabled = false;
        juce::Optional<PositionInfo> getPosition() const override
        {
            if (! enabled) return {};
            PositionInfo i;
            i.setBpm (bpm); i.setPpqPosition (ppq); i.setIsPlaying (playing);
            return i;
        }
    };

    struct Harness
    {
        std::unique_ptr<OrbitReverbProcessor> p = std::make_unique<OrbitReverbProcessor>();
        MockPlayHead head;
        double sr = 48000.0;
        int block = 512;

        void prepare (double sampleRate, int blockSize = 512, int inCh = 2, int outCh = 2)
        {
            sr = sampleRate; block = blockSize;
            p->setPlayConfigDetails (inCh, outCh, sr, block);
            p->setPlayHead (&head);
            p->prepareToPlay (sr, block);
        }

        void set (const char* id, float value)
        {
            auto* prm = p->apvts.getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        }

        float get (const char* id) { return p->apvts.getRawParameterValue (id)->load(); }

        // Elabora il buffer a blocchi (in place) facendo avanzare il trasporto
        void run (juce::AudioBuffer<float>& buf, std::function<void (int)> perBlock = nullptr)
        {
            juce::MidiBuffer midi;
            const int chans = buf.getNumChannels();
            for (int start = 0, b = 0; start < buf.getNumSamples(); start += block, ++b)
            {
                if (perBlock) perBlock (b);
                const int n = std::min (block, buf.getNumSamples() - start);
                juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), chans, start, n);
                p->processBlock (view, midi);
                head.ppq += (double) n / sr * head.bpm / 60.0;
            }
        }
    };

    juce::AudioBuffer<float> makeBuffer (double sr, double seconds, int chans = 2)
    {
        juce::AudioBuffer<float> b (chans, (int) (sr * seconds));
        b.clear();
        return b;
    }

    void addNoise (juce::AudioBuffer<float>& b, int start, int end, float amp, unsigned seed = 1)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = start; i < std::min (end, b.getNumSamples()); ++i)
                b.setSample (ch, i, amp * d (rng));
    }

    bool allFinite (const juce::AudioBuffer<float>& b, float& peak)
    {
        peak = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float v = b.getSample (ch, i);
                if (! std::isfinite (v)) return false;
                peak = std::max (peak, std::abs (v));
            }
        return true;
    }

    float rms (const juce::AudioBuffer<float>& b, int ch, int start, int end)
    {
        double s = 0.0; int n = 0;
        for (int i = std::max (0, start); i < std::min (end, b.getNumSamples()); ++i, ++n)
            s += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return n > 0 ? (float) std::sqrt (s / n) : 0.0f;
    }

    float rmsStereo (const juce::AudioBuffer<float>& b, int start, int end)
    {
        const float l = rms (b, 0, start, end), r = rms (b, b.getNumChannels() > 1 ? 1 : 0, start, end);
        return std::sqrt (0.5f * (l * l + r * r));
    }

    float toDb (float x) { return 20.0f * std::log10 (std::max (x, 1e-9f)); }

    // RT60 con integrazione di Schroeder (fit tra -5 e -25 dB)
    float measureRT60 (const juce::AudioBuffer<float>& b, double sr)
    {
        const int n = b.getNumSamples();
        std::vector<double> edc ((size_t) n);
        double acc = 0.0;
        for (int i = n - 1; i >= 0; --i)
        {
            const double v = (double) b.getSample (0, i) + b.getSample (1, i);
            acc += v * v;
            edc[(size_t) i] = acc;
        }
        const double total = edc[0];
        int i5 = -1, i25 = -1;
        for (int i = 0; i < n; ++i)
        {
            const double db = 10.0 * std::log10 (edc[(size_t) i] / total + 1e-30);
            if (i5 < 0 && db <= -5.0) i5 = i;
            if (i25 < 0 && db <= -25.0) { i25 = i; break; }
        }
        if (i5 < 0 || i25 < 0) return -1.0f;
        return (float) (60.0 / (20.0 / ((double) (i25 - i5) / sr)));
    }

    // Bilanciamento L/R per finestre: -1 tutto a sinistra, +1 tutto a destra
    std::vector<float> balanceSeries (const juce::AudioBuffer<float>& b, int start, double sr, double winSec)
    {
        std::vector<float> out;
        const int w = (int) (winSec * sr);
        for (int i = start; i + w <= b.getNumSamples(); i += w)
        {
            const float l = rms (b, 0, i, i + w), r = rms (b, 1, i, i + w);
            out.push_back ((r - l) / (r + l + 1e-9f));
        }
        return out;
    }

    float seriesRange (const std::vector<float>& v)
    {
        auto [mn, mx] = std::minmax_element (v.begin(), v.end());
        return *mx - *mn;
    }

    // periodo dominante (in finestre) tramite autocorrelazione: prende il PRIMO picco locale vicino al massimo
    // (altrimenti i multipli del periodo, quasi identici, possono vincere per rumore)
    float dominantPeriod (std::vector<float> v, int minLag, int maxLag)
    {
        float mean = 0.0f;
        for (auto x : v) mean += x;
        mean /= (float) v.size();
        for (auto& x : v) x -= mean;
        std::vector<float> ac;
        for (int lag = 0; lag <= maxLag && lag < (int) v.size() / 2; ++lag)
        {
            float s = 0.0f;
            for (size_t i = 0; i + (size_t) lag < v.size(); ++i) s += v[i] * v[i + (size_t) lag];
            ac.push_back (s / (float) (v.size() - (size_t) lag));
        }
        float best = -1e9f;
        for (int lag = minLag; lag < (int) ac.size(); ++lag) best = std::max (best, ac[(size_t) lag]);
        for (int lag = std::max (1, minLag); lag + 1 < (int) ac.size(); ++lag)
            if (ac[(size_t) lag] >= 0.85f * best && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1])
                return (float) lag;
        return -1.0f;
    }

    // conferma indipendente: periodo medio dai passaggi per lo zero (verso l'alto) della serie
    float zeroCrossPeriod (std::vector<float> v)
    {
        float mean = 0.0f; for (auto x : v) mean += x; mean /= (float) v.size();
        std::vector<int> ups;
        for (size_t i = 1; i < v.size(); ++i) if (v[i - 1] - mean < 0.0f && v[i] - mean >= 0.0f) ups.push_back ((int) i);
        if (ups.size() < 3) return -1.0f;
        return (float) (ups.back() - ups.front()) / (float) (ups.size() - 1);
    }

    float toneEnergy (const juce::AudioBuffer<float>& b, int start, int len, double sr, double freq)
    {
        // Goertzel su canale sinistro+destro, finestra di Hann
        double s1 = 0, s2 = 0;
        const double k = 2.0 * std::cos (2.0 * juce::MathConstants<double>::pi * freq / sr);
        for (int i = 0; i < len; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (len - 1));
            const double x = w * (b.getSample (0, start + i) + b.getSample (1, start + i));
            const double s0 = x + k * s1 - s2;
            s2 = s1; s1 = s0;
        }
        return (float) (s1 * s1 + s2 * s2 - k * s1 * s2);
    }

    void wetOnly (Harness& h) { h.set (ParamID::dry, 0.0f); h.set (ParamID::wet, 100.0f); }
}

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Modalita' snapshot: salva un'immagine dell'interfaccia (OrbitTests --snapshot file.png [orbit])
    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
    {
        Harness h; h.prepare (48000.0);
        h.p->setCurrentProgram (6);                                     // Psychedelic Orbit
        auto buf = makeBuffer (48000.0, 1.37); addNoise (buf, 0, buf.getNumSamples(), 0.3f);
        h.run (buf);                                                    // fa muovere l'orbita
        std::unique_ptr<juce::AudioProcessorEditor> ed (h.p->createEditor());
        ed->setVisible (true);
        for (int i = 0; i < 40; ++i)                                    // lascia girare i timer della GUI
        {
            h.run (buf);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot: %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        return 0;
    }
    std::printf ("=== ORBIT REVERB - test offline ===\n");

    // 1. Dry indipendente dal wet e da ORBIT
    {
        Harness h; h.prepare (48000.0);
        h.set (ParamID::wet, 0.0f); h.set (ParamID::orbitOn, 1.0f); h.set (ParamID::orbitDepth, 100.0f);
        auto in = makeBuffer (48000.0, 2.0); addNoise (in, 0, in.getNumSamples(), 0.5f);
        auto buf = in; h.run (buf);
        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 4800; i < buf.getNumSamples(); ++i)
                maxDiff = std::max (maxDiff, std::abs (buf.getSample (ch, i) - in.getSample (ch, i)));
        check (maxDiff < 1e-5f, "Dry intatto con Wet 0 e ORBIT attivo", "diff max " + juce::String (maxDiff, 8));
    }

    // 2. Risposta all'impulso a tutte le frequenze di campionamento + RT60
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        Harness h; h.prepare (sr); wetOnly (h);
        h.set (ParamID::decay, 2.0f); h.set (ParamID::preDelay, 0.0f); h.set (ParamID::lowCut, 20.0f);
        h.set (ParamID::highCut, 20000.0f); h.set (ParamID::damping, 0.0f);
        h.p->reset();
        auto buf = makeBuffer (sr, 6.0);
        buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f);
        h.run (buf);
        float peak; const bool fin = allFinite (buf, peak);
        const float rt = measureRT60 (buf, sr);
        check (fin && peak < 2.0f && rt > 1.5f && rt < 2.7f,
               "Impulso @ " + juce::String (sr / 1000.0, 1) + " kHz: stabile e RT60 corretto",
               "RT60 misurato " + juce::String (rt, 2) + " s (atteso ~2.0), picco " + juce::String (peak, 3));
    }

    // 3. DECAY controlla davvero la durata
    {
        float last = 0.0f; bool mono = true; juce::String det;
        for (float d : { 0.4f, 1.0f, 3.0f, 8.0f })
        {
            Harness h; h.prepare (48000.0); wetOnly (h);
            h.set (ParamID::decay, d); h.set (ParamID::preDelay, 0.0f); h.set (ParamID::damping, 0.0f);
            h.set (ParamID::lowCut, 20.0f); h.set (ParamID::highCut, 20000.0f);
            h.p->reset();
            auto buf = makeBuffer (48000.0, std::max (3.0, d * 2.5));
            buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f);
            h.run (buf);
            const float rt = measureRT60 (buf, 48000.0);
            det << juce::String (d, 1) << "s->" << juce::String (rt, 2) << "s ";
            mono = mono && rt > last && std::abs (rt - d) / d < 0.35f;
            last = rt;
        }
        check (mono, "DECAY: RT60 segue il parametro", det.trim());
    }

    // 4. FREEZE: coda congelata, stabile per 30 s, nessuna crescita
    {
        Harness h; h.prepare (48000.0); wetOnly (h);
        h.set (ParamID::size, 80.0f); h.set (ParamID::decay, 3.0f);
        auto buf = makeBuffer (48000.0, 32.0);
        addNoise (buf, 0, 48000, 0.3f);
        addNoise (buf, 48000 * 10, 48000 * 12, 0.9f, 7);          // audio nuovo durante il freeze: non deve entrare
        h.run (buf, [&] (int b) { if (b == (int) (1.2 * 48000 / 512)) h.set (ParamID::freeze, 1.0f); });
        float peak; const bool fin = allFinite (buf, peak);
        const float first = rmsStereo (buf, 48000 * 2, 48000 * 3);
        float mn = 1e9f, mx = 0.0f;
        for (int s = 3; s < 31; ++s) { const float r = rmsStereo (buf, 48000 * s, 48000 * (s + 1)); mn = std::min (mn, r); mx = std::max (mx, r); }
        const bool notGrowing = toDb (mx) - toDb (first) < 1.5f;
        const bool sustained  = toDb (mn) - toDb (first) > -6.0f;
        check (fin && notGrowing && sustained, "FREEZE: coda infinita stabile per 30 s e chiusa al nuovo audio",
               "livello 2-3s " + juce::String (toDb (first), 1) + " dB, min " + juce::String (toDb (mn), 1)
               + " dB, max " + juce::String (toDb (mx), 1) + " dB");
    }

    // 5. Stabilita' estrema in tutti i modi
    {
        bool ok = true; juce::String det;
        for (int m = 0; m < 7; ++m)
        {
            Harness h; h.prepare (48000.0); wetOnly (h);
            h.set (ParamID::mode, (float) m); h.set (ParamID::size, 100.0f); h.set (ParamID::decay, 60.0f);
            h.set (ParamID::diffusion, 100.0f); h.set (ParamID::modulation, 100.0f); h.set (ParamID::damping, 0.0f);
            h.set (ParamID::orbitOn, 1.0f); h.set (ParamID::orbitDepth, 100.0f); h.set (ParamID::orbitRate, 10.0f);
            auto buf = makeBuffer (48000.0, 20.0);
            for (int s = 0; s < 20; s += 4) addNoise (buf, 48000 * s, 48000 * s + 24000, 0.9f, (unsigned) s + 3);
            h.run (buf);
            float peak; const bool fin = allFinite (buf, peak);
            ok = ok && fin && peak < 6.0f;
            det << Modes::names[m] << " " << juce::String (peak, 2) << "  ";
        }
        check (ok, "Stabilita' con parametri estremi in tutti i 7 modi (20 s)", "picchi: " + det.trim());
    }

    // 6. Automazione casuale di tutti i parametri durante la riproduzione
    {
        Harness h; h.prepare (48000.0, 256);
        std::mt19937 rng (42); std::uniform_real_distribution<float> u (0.0f, 1.0f);
        auto params = h.p->getParameters();
        auto buf = makeBuffer (48000.0, 15.0); addNoise (buf, 0, buf.getNumSamples(), 0.4f);
        h.run (buf, [&] (int)
        {
            for (int k = 0; k < 4; ++k)
            {
                auto* prm = params[(int) (u (rng) * (float) params.size()) % params.size()];
                if (prm != h.p->getBypassParameter()) prm->setValueNotifyingHost (u (rng));
            }
        });
        float peak; const bool fin = allFinite (buf, peak);
        check (fin && peak < 8.0f, "Automazione casuale continua di tutti i parametri (15 s)", "picco " + juce::String (peak, 2));
    }

    // 7. ORBIT: il riverbero si muove davvero tra L e R, al tempo giusto
    float orbitRangeOn = 0.0f;
    {
        auto measure = [] (bool on, float rateHz, float& range, float& periodSec)
        {
            Harness h; h.prepare (48000.0); wetOnly (h);
            h.set (ParamID::orbitOn, on ? 1.0f : 0.0f); h.set (ParamID::orbitRate, rateHz);
            h.set (ParamID::orbitDepth, 100.0f); h.set (ParamID::orbitWidth, 100.0f); h.set (ParamID::decay, 1.5f);
            auto buf = makeBuffer (48000.0, 12.0); addNoise (buf, 0, buf.getNumSamples(), 0.3f);
            h.run (buf);
            auto bal = balanceSeries (buf, 48000 * 2, 48000.0, 0.025);
            range = seriesRange (bal);
            periodSec = dominantPeriod (bal, 8, 200) * 0.025f;
        };
        float rOff, pOff, rOn, pOn;
        measure (false, 1.0f, rOff, pOff);
        measure (true, 1.0f, rOn, pOn);
        orbitRangeOn = rOn;
        check (rOn > 0.8f && rOff < 0.25f, "ORBIT sposta il riverbero tra sinistra e destra",
               "escursione bilanciamento: OFF " + juce::String (rOff, 2) + ", ON " + juce::String (rOn, 2));
        check (std::abs (pOn - 1.0f) < 0.08f, "ORBIT RATE: periodo del movimento", "1.0 Hz -> periodo misurato " + juce::String (pOn, 3) + " s");
    }

    // 8. ORBIT senza salti: variazione massima di posizione tra campioni (click detector sul pan)
    {
        Harness h; h.prepare (48000.0); wetOnly (h);
        h.set (ParamID::orbitOn, 1.0f); h.set (ParamID::orbitDepth, 100.0f); h.set (ParamID::orbitWidth, 100.0f);
        // sinusoide costante in ingresso -> il wet e' quasi stazionario: un salto di pan sarebbe un gradino
        auto buf = makeBuffer (48000.0, 8.0);
        for (int i = 0; i < buf.getNumSamples(); ++i) { const float s = 0.3f * std::sin (0.0261799f * i); buf.setSample (0, i, s); buf.setSample (1, i, s); }
        std::mt19937 rng (5); std::uniform_real_distribution<float> u (0.0f, 1.0f);
        float maxStep = 0.0f, last = 0.0f;
        h.run (buf, [&] (int b)
        {
            if (b % 20 == 0) // automazione brusca di RATE / DEPTH / DIRECTION / SHAPE / OFFSET
            {
                h.set (ParamID::orbitRate, 0.05f + 6.0f * u (rng)); h.set (ParamID::orbitDepth, 100.0f * u (rng));
                h.set (ParamID::orbitDirection, (float) (int) (3.0f * u (rng))); h.set (ParamID::orbitShape, (float) (int) (3.0f * u (rng)));
                h.set (ParamID::orbitOffset, 360.0f * u (rng));
            }
            const float pos = h.p->getOrbitMotion().uiPosition.load();
            if (b > 40) maxStep = std::max (maxStep, std::abs (pos - last));
            last = pos;
        });
        // a 512 campioni per blocco una rotazione veloce (6 Hz, escursione 2) puo' spostarsi ~0.4 per blocco
        check (maxStep < 0.5f, "ORBIT: nessun salto di posizione con automazione brusca", "passo max per blocco " + juce::String (maxStep, 3));
    }

    // 9. SYNC al BPM della DAW, cambio di BPM durante la riproduzione
    {
        Harness h; h.prepare (48000.0); wetOnly (h);
        h.head.enabled = true; h.head.bpm = 120.0; h.head.ppq = 0.0;
        h.set (ParamID::orbitOn, 1.0f); h.set (ParamID::orbitSync, 1.0f); h.set (ParamID::orbitDivision, 4.0f); // 1/4
        h.set (ParamID::orbitDepth, 100.0f); h.set (ParamID::orbitWidth, 100.0f); h.set (ParamID::decay, 1.0f);
        auto buf = makeBuffer (48000.0, 16.0); addNoise (buf, 0, buf.getNumSamples(), 0.3f);
        const int switchBlock = (int) (8.0 * 48000 / 512);
        float maxPhaseErr = 0.0f;
        h.run (buf, [&] (int b)
        {
            if (b == switchBlock) h.head.bpm = 90.0;
            if ((b > 150 && b < switchBlock) || b > switchBlock + 150)
            {
                const double beats = 1.0;
                const double expected = 2.0 * juce::MathConstants<double>::pi * (h.head.ppq / beats - std::floor (h.head.ppq / beats));
                double err = std::fmod ((double) h.p->getOrbitMotion().uiPhase.load() - expected, 2.0 * juce::MathConstants<double>::pi);
                if (err > juce::MathConstants<double>::pi) err -= 2.0 * juce::MathConstants<double>::pi;
                if (err < -juce::MathConstants<double>::pi) err += 2.0 * juce::MathConstants<double>::pi;
                maxPhaseErr = std::max (maxPhaseErr, (float) std::abs (err));
            }
        });
        auto bal1 = balanceSeries (buf, 48000 * 2, 48000.0, 0.01);
        bal1.resize (std::min<size_t> (bal1.size(), 500));
        juce::AudioBuffer<float> second (buf.getArrayOfWritePointers(), 2, 48000 * 11, 48000 * 5);
        auto bal2 = balanceSeries (second, 0, 48000.0, 0.01);
        const float p1 = dominantPeriod (bal1, 20, 150) * 0.01f, p2 = dominantPeriod (bal2, 20, 150) * 0.01f;
        const float z1 = zeroCrossPeriod (bal1) * 0.01f, z2 = zeroCrossPeriod (bal2) * 0.01f;
        std::printf ("[INFO] SYNC, verifica con passaggi per lo zero: 120 BPM -> %.3f s, 90 BPM -> %.3f s\n", z1, z2);
        check (std::abs (p1 - 0.5f) < 0.03f && std::abs (p2 - 0.6667f) < 0.04f,
               "SYNC: giro da 1/4 a tempo con il BPM (anche dopo un cambio di BPM)",
               "120 BPM -> " + juce::String (p1, 3) + " s (atteso 0.500), 90 BPM -> " + juce::String (p2, 3) + " s (atteso 0.667)");
        check (maxPhaseErr < 0.35f, "SYNC: fase agganciata alla posizione musicale (PPQ)", "errore max " + juce::String (maxPhaseErr, 3) + " rad");
    }

    // 10. SHIMMER produce davvero l'ottava sopra (pitch shifting, non EQ)
    {
        auto ratio = [] (int mode)
        {
            Harness h; h.prepare (48000.0); wetOnly (h);
            h.set (ParamID::mode, (float) mode); h.set (ParamID::decay, 6.0f); h.set (ParamID::highCut, 20000.0f);
            h.set (ParamID::lowCut, 20.0f); h.set (ParamID::damping, 0.0f);
            auto buf = makeBuffer (48000.0, 4.0);
            for (int i = 0; i < 24000; ++i) { const float s = 0.4f * std::sin (2.0f * 3.14159265f * 440.0f * i / 48000.0f); buf.setSample (0, i, s); buf.setSample (1, i, s); }
            h.run (buf);
            const float e440 = toneEnergy (buf, 48000 * 2, 48000, 48000.0, 440.0);
            const float e880 = toneEnergy (buf, 48000 * 2, 48000, 48000.0, 880.0);
            return toDb (std::sqrt (e880 / (e440 + 1e-20f)));
        };
        const float clean = ratio (Modes::Clean), shimmer = ratio (Modes::Shimmer);
        check (shimmer - clean > 15.0f, "SHIMMER: compare l'ottava sopra (880 Hz da un La 440)",
               "880/440 nella coda: CLEAN " + juce::String (clean, 1) + " dB, SHIMMER " + juce::String (shimmer, 1) + " dB");
    }

    // 11. DUCKING abbassa il wet mentre il dry suona
    {
        auto wetLevel = [] (float duck, float& during, float& after)
        {
            Harness h; h.prepare (48000.0); wetOnly (h);
            h.set (ParamID::ducking, duck); h.set (ParamID::decay, 3.0f);
            auto buf = makeBuffer (48000.0, 4.0); addNoise (buf, 0, 48000 * 2, 0.3f);
            h.run (buf);
            during = rmsStereo (buf, 48000, 48000 * 2);
            after  = rmsStereo (buf, 48000 * 2 + 24000, 48000 * 3);
        };
        float d0, a0, d1, a1;
        wetLevel (0.0f, d0, a0); wetLevel (100.0f, d1, a1);
        check (toDb (d0) - toDb (d1) > 6.0f && toDb (a0) - toDb (a1) < 3.0f,
               "DUCKING: wet ridotto durante il suono, coda libera dopo",
               "durante: -" + juce::String (toDb (d0) - toDb (d1), 1) + " dB, dopo: -" + juce::String (toDb (a0) - toDb (a1), 1) + " dB");
    }

    // 12. ATTACK: il riverbero cresce lentamente (swell)
    {
        auto early = [] (float att)
        {
            Harness h; h.prepare (48000.0); wetOnly (h);
            h.set (ParamID::attack, att); h.set (ParamID::preDelay, 0.0f);
            auto buf = makeBuffer (48000.0, 2.0); addNoise (buf, 4800, 48000 * 2, 0.3f);
            h.run (buf);
            return std::make_pair (rmsStereo (buf, 4800, 4800 + 9600), rmsStereo (buf, 48000 + 24000, 48000 * 2));
        };
        const auto fast = early (0.0f), slow = early (1500.0f);
        check (toDb (fast.first) - toDb (slow.first) > 10.0f && std::abs (toDb (fast.second) - toDb (slow.second)) < 3.0f,
               "ATTACK: energia del riverbero che cresce (swell) e poi raggiunge il livello pieno",
               "primi 200 ms: -" + juce::String (toDb (fast.first) - toDb (slow.first), 1) + " dB; a regime diff "
               + juce::String (toDb (fast.second) - toDb (slow.second), 1) + " dB");
    }

    // 13. RELEASE e 14. GATED
    {
        auto tail = [] (int mode, float rel)
        {
            Harness h; h.prepare (48000.0); wetOnly (h);
            h.set (ParamID::mode, (float) mode); h.set (ParamID::release, rel); h.set (ParamID::decay, 4.0f);
            auto buf = makeBuffer (48000.0, 3.0); addNoise (buf, 0, 48000, 0.3f);
            h.run (buf);
            return rmsStereo (buf, 48000 + 38400, 48000 + 48000);   // 0.8-1.0 s dopo la fine
        };
        const float natural = tail (Modes::Clean, kReleaseInfinite);
        const float released = tail (Modes::Clean, 300.0f);
        const float gated = tail (Modes::Gated, 300.0f);
        check (toDb (natural) - toDb (released) > 20.0f, "RELEASE: dissolvenza della coda dopo la fine del suono",
               "a 0.8-1.0 s: -" + juce::String (toDb (natural) - toDb (released), 1) + " dB rispetto a Natural");
        check (toDb (natural) - toDb (gated) > 40.0f, "GATED: taglio netto dopo il tempo di gate",
               "a 0.8-1.0 s: -" + juce::String (toDb (natural) - toDb (gated), 1) + " dB rispetto a CLEAN");
    }

    // 15. Salvataggio / ripristino dello stato
    {
        Harness a; a.prepare (48000.0);
        a.set (ParamID::size, 77.0f); a.set (ParamID::mode, 4.0f); a.set (ParamID::orbitOn, 1.0f);
        a.set (ParamID::orbitDivision, 6.0f); a.set (ParamID::decay, 12.5f); a.set (ParamID::orbitCenter, -40.0f);
        juce::MemoryBlock state; a.p->getStateInformation (state);
        Harness b; b.prepare (48000.0);
        b.p->setStateInformation (state.getData(), (int) state.getSize());
        bool same = true;
        for (auto* prm : a.p->getParameters())
        {
            auto* ra = dynamic_cast<juce::RangedAudioParameter*> (prm);
            auto* rb = b.p->apvts.getParameter (ra->getParameterID());
            same = same && std::abs (ra->getValue() - rb->getValue()) < 1e-6f;
        }
        check (same, "Stato: salvataggio e ripristino di tutti i parametri");
    }

    // 15b. Ripristino con valori intermedi sui parametri a scatti (scenario di pluginval)
    {
        Harness h; h.prepare (48000.0);
        std::mt19937 rng (99); std::uniform_real_distribution<float> u (0.0f, 1.0f);
        auto params = h.p->getParameters();
        // come fa il wrapper VST3 quando l'host cambia un parametro
        auto hostSet = [] (juce::AudioProcessorParameter* prm, float v) { prm->setValue (v); prm->sendValueChangedMessageToListeners (v); };
        for (auto* prm : params) hostSet (prm, u (rng));
        juce::MemoryBlock state; h.p->getStateInformation (state);
        std::vector<float> saved;
        for (auto* prm : params) saved.push_back (prm->getValue());
        for (auto* prm : params) hostSet (prm, u (rng));           // valori casuali, anche intermedi per i bool
        h.p->setStateInformation (state.getData(), (int) state.getSize());
        float worst = 0.0f; juce::String which;
        for (int i = 0; i < params.size(); ++i)
        {
            // confronto sul valore "effettivo": per i bool e le scelte conta lo scatto, non il numero grezzo
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (params[i]);
            const float a = ranged->convertFrom0to1 (saved[(size_t) i]);
            const float b = ranged->convertFrom0to1 (params[i]->getValue());
            const float diffNorm = std::abs (params[i]->getValue() - ranged->convertTo0to1 (a));
            if (std::abs (a - b) > 1e-4f * std::max (1.0f, std::abs (a)) || diffNorm > 0.1f)
            { worst = std::max (worst, diffNorm); which << ranged->getName (32) << " "; }
        }
        check (which.isEmpty(), "Stato: ripristino corretto anche partendo da valori intermedi (bool e scelte)",
               which.isEmpty() ? juce::String ("tutti i parametri riallineati") : "disallineati: " + which);
    }

    // 16. Preset di fabbrica
    {
        Harness h; h.prepare (48000.0);
        bool ok = h.p->getNumPrograms() == 12; juce::String det;
        for (int i = 0; i < h.p->getNumPrograms(); ++i)
        {
            h.p->setCurrentProgram (i);
            auto buf = makeBuffer (48000.0, 3.0); addNoise (buf, 0, 24000, 0.5f, (unsigned) i);
            h.run (buf);
            float peak; ok = ok && allFinite (buf, peak) && peak < 4.0f;
            det << h.p->getProgramName (i) << ", ";
        }
        h.p->setCurrentProgram (11);
        ok = ok && std::abs (h.get (ParamID::attack) - 1200.0f) < 1.0f && h.get (ParamID::mode) == 1.0f;
        check (ok, "12 preset di fabbrica caricabili e stabili", det.dropLastCharacters (2));
    }

    // 17. Mono in -> stereo out, 18. cambio di sample rate, 19. bypass
    {
        Harness h; h.prepare (48000.0, 512, 1, 2);
        auto buf = makeBuffer (48000.0, 2.0, 2); addNoise (buf, 0, 24000, 0.5f);
        buf.clear (1, 0, buf.getNumSamples());
        h.run (buf);
        float peak; const bool ok = allFinite (buf, peak) && rms (buf, 1, 30000, 60000) > 1e-4f;
        check (ok, "Ingresso mono -> uscita stereo", "picco " + juce::String (peak, 3));
    }
    {
        Harness h; bool ok = true;
        for (double sr : { 44100.0, 96000.0, 48000.0, 88200.0 })
        {
            h.prepare (sr);
            auto buf = makeBuffer (sr, 2.0); addNoise (buf, 0, (int) sr / 4, 0.5f);
            h.run (buf);
            float peak; ok = ok && allFinite (buf, peak) && peak < 4.0f;
        }
        check (ok, "Cambio di sample rate sulla stessa istanza (44.1 > 96 > 48 > 88.2 kHz)");
    }
    {
        Harness h; h.prepare (48000.0);
        h.set (ParamID::bypass, 1.0f);
        auto in = makeBuffer (48000.0, 1.0); addNoise (in, 0, in.getNumSamples(), 0.5f);
        auto buf = in; h.run (buf);
        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 4800; i < buf.getNumSamples(); ++i)
                maxDiff = std::max (maxDiff, std::abs (buf.getSample (ch, i) - in.getSample (ch, i)));
        check (maxDiff < 1e-5f, "Bypass: uscita identica all'ingresso", "diff max " + juce::String (maxDiff, 8));
    }

    // 20. Compatibilita' mono con ORBIT attivo
    {
        Harness h; h.prepare (48000.0); wetOnly (h);
        h.set (ParamID::orbitOn, 1.0f); h.set (ParamID::orbitDepth, 100.0f); h.set (ParamID::orbitWidth, 100.0f);
        h.set (ParamID::orbitShape, 2.0f); h.set (ParamID::mode, (float) Modes::Orbital);
        auto buf = makeBuffer (48000.0, 10.0); addNoise (buf, 0, buf.getNumSamples(), 0.3f);
        auto ref = buf;
        h.run (buf);
        Harness r; r.prepare (48000.0); wetOnly (r); r.set (ParamID::mode, (float) Modes::Orbital);
        r.set (ParamID::orbitMix, 0.0f);
        r.run (ref);
        auto monoRms = [] (const juce::AudioBuffer<float>& b)
        {
            double s = 0; for (int i = 96000; i < b.getNumSamples(); ++i) { const double m = 0.5 * (b.getSample (0, i) + b.getSample (1, i)); s += m * m; }
            return (float) std::sqrt (s / (b.getNumSamples() - 96000));
        };
        const float diff = toDb (monoRms (buf)) - toDb (monoRms (ref));
        check (std::abs (diff) < 3.0f, "Compatibilita' mono: livello della somma L+R con ORBIT al massimo",
               "differenza rispetto al riverbero fermo " + juce::String (diff, 2) + " dB");
    }

    // Prestazioni
    {
        Harness h; h.prepare (48000.0);
        h.set (ParamID::mode, (float) Modes::Shimmer); h.set (ParamID::orbitOn, 1.0f); h.set (ParamID::orbitShape, 2.0f);
        auto buf = makeBuffer (48000.0, 20.0); addNoise (buf, 0, buf.getNumSamples(), 0.3f);
        const auto t0 = std::chrono::steady_clock::now();
        h.run (buf);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("[INFO] CPU: 20 s di audio stereo a 48 kHz (SHIMMER + ORBIT) elaborati in %.2f s -> %.1f%% di un core\n",
                     secs, 100.0 * secs / 20.0);
    }

    std::printf ("=== %d PASS, %d FAIL ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
