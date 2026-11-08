# ORBIT REVERB

Riverbero creativo VST3 (C++ / JUCE 8.0.8) con funzione **ORBIT**: il riverbero ruota intorno all'ascoltatore mentre il suono originale resta fermo al centro.

![Interfaccia](docs/screenshot.png)

---

## Stato delle verifiche (cosa è stato provato davvero)

| Verifica | Risultato | Dove |
|---|---|---|
| Compilazione VST3 + Standalone + test | Riuscita | Linux x64, GCC 13, JUCE 8.0.8 |
| Suite di test offline (26 test con misure) | **26 PASS, 0 FAIL** | `docs/TEST_RESULTS.txt` |
| pluginval, severità 10 (massima), editor incluso | **SUCCESS** | `docs/PLUGINVAL_RESULTS.txt` |
| Validatore ufficiale Steinberg (dentro pluginval) | **Non eseguito** (eseguibile non disponibile) | — |
| Compilazione e test su **Windows** | **Non eseguiti da me** | vedi sotto: workflow pronto |
| Prova dentro **FL Studio** | **Non eseguita da me** | — |

Il codice è multipiattaforma e non usa nulla di specifico per Linux. La build Windows va però fatta sul tuo PC o con GitHub Actions: segui le istruzioni qui sotto.

Durante la validazione pluginval ha trovato un bug reale: dopo il ripristino di un progetto, un pulsante (Freeze) poteva restare disallineato con la DAW. È stato corretto e coperto da un test di regressione. Il test è stato verificato: fallisce senza la correzione e passa con la correzione.

---

## Compilare su Windows (per FL Studio)

### Requisiti
1. **Visual Studio 2022 Community** (gratis): durante l'installazione spunta **"Sviluppo di applicazioni desktop con C++"**.
2. **CMake** 3.22 o più recente: <https://cmake.org/download/> (scegli "Add CMake to the PATH").
3. **Git**: <https://git-scm.com/download/win>

JUCE viene scaricato automaticamente da CMake (versione 8.0.8) al primo avvio.

### Comandi
Apri il **Prompt dei comandi** nella cartella del progetto ed esegui:

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
build\OrbitTests_artefacts\Release\OrbitTests.exe
```

La terza riga esegue i test: alla fine deve comparire `26 PASS, 0 FAIL`.

Il plugin compilato si trova in:

```
build\OrbitReverb_artefacts\Release\VST3\ORBIT REVERB.vst3
```

(È una cartella che finisce in `.vst3`: va copiata per intero.)

### In alternativa: compilazione automatica con GitHub (senza installare nulla)
1. Crea un repository su GitHub e carica questi file, compresa la cartella `.github`.
2. Vai su **Actions**: il workflow *Build ORBIT REVERB (Windows VST3)* parte da solo. Puoi anche avviarlo con "Run workflow".
3. A fine build scarica l'artefatto **ORBIT-REVERB-VST3-Windows**: contiene il `.vst3` per Windows. Il workflow esegue anche i test.

> Il workflow è pronto ma non l'ho eseguito io: se fallisce, il log di Actions indica il punto esatto.

### Installare in FL Studio
1. Copia la cartella **`ORBIT REVERB.vst3`** in `C:\Program Files\Common Files\VST3\`
2. In FL Studio apri **Options → Manage plugins**.
3. Premi **Find installed plugins / Start scan** (spunta "Verify plugins" se c'è).
4. Cerca **ORBIT REVERB** nella lista e attiva la stellina (preferito).
5. Nel **Mixer** seleziona una traccia, apri uno slot vuoto e scegli **ORBIT REVERB**.

I 12 preset di fabbrica compaiono sia nel menu PRESET del plugin sia nel menu preset di FL Studio.

---

## Architettura DSP

```
ingresso ──┬──────────────────────────────────────────────── DRY (mai elaborato) ─┐
           │                                                                       │
           └─ ATTACK (inviluppo energia) ─ PRE-DELAY ─┬─ early reflections        │
                                                      └─ diffusione (4 allpass/can.)│
                                                          └─ FDN 8 linee           │
                                                             (Householder, damping, │
                                                              modulazione, RT60,    │
                                                              shimmer +12 nel loop) │
              LOW CUT / HIGH CUT ─ WIDTH (M/S) ─ ORBIT MOTION ─ RELEASE/GATED/DUCKING ─ WET ─┴─ OUTPUT
```

- **FDN a 8 linee** con matrice di Householder (ortogonale), interpolazione cubica di Hermite, guadagno per linea calcolato dall'RT60: `g = 10^(-3·L/(fs·RT60))`.
- **Latenza riportata alla DAW: 0 campioni.** Il dry non è ritardato. I ritardi interni (pre-delay, linee, 2 campioni nel percorso ORBIT) riguardano solo la coda.
- **Sicurezza:** `ScopedNoDenormals`, parametri smussati, limitatore morbido nel loop (interviene solo sopra ±4), margine di stabilità con lo shimmer.
- **Sample rate:** tutte le lunghezze sono calcolate dal sample rate al `prepareToPlay`. Testato a 44.1, 48, 88.2 e 96 kHz e con cambio di sample rate sulla stessa istanza.
- **Canali:** stereo→stereo, mono→stereo, mono→mono.
- **Thread audio:** nessuna allocazione e nessun lock nel `processBlock`. La GUI legge solo valori atomici pubblicati dal DSP.

## Comportamento dei controlli

| Controllo | Comportamento |
|---|---|
| **SIZE** | Scala le linee FDN (circa 10–150 ms) e le early reflections. |
| **DECAY** | RT60 reale da 0,1 a 60 s. Misurato: 0,4→0,36 s, 1→0,95 s, 3→2,76 s, 8→7,44 s. |
| **PRE-DELAY** | 0–500 ms, smussato. |
| **ATTACK** | Inviluppo sull'energia che **entra** nel riverbero. Quando un suono inizia, sale da 0 a 1 in circa *Attack* ms (curva esponenziale, ~95 % al tempo indicato). A suono fermo torna a 0 in 25 ms, così ogni frase cresce di nuovo. 0 = disattivato. |
| **RELEASE** | Dissolvenza **aggiuntiva** del wet dopo la fine del suono, in circa *Release* ms. Al massimo (**Natural**) è disattivata. Quando il suono riparte il wet torna pieno in 5 ms, senza click. |
| **DIFFUSION** | Coefficiente degli allpass in ingresso (0–0,78). |
| **DAMPING** | Passa-basso dentro il loop (da 18 kHz a circa 1,4 kHz): la coda si scurisce mentre decade. |
| **LOW / HIGH CUT** | Filtri a 12 dB/ottava (SVF TPT) solo sul wet. |
| **WIDTH** | Ampiezza M/S del wet, 0–200 %. |
| **MODULATION** | Modulazione delle linee (fino a 6 ms): coda più viva e meno metallica. |
| **DRY / WET / OUTPUT** | Guadagni smussati. Il dry è indipendente da tutto il resto. |
| **DUCKING** | `gain = 1 / (1 + 10·ducking·inviluppo)`, attacco 5 ms e rilascio 250 ms. Misurato: −11,6 dB durante il suono, coda quasi intatta dopo. |
| **FREEZE** | Guadagno del loop = 1, ingresso escluso, damping escluso. Misurato: livello stabile per 30 s (variazione ~1 dB), nessuna crescita, nuovo audio escluso. |

## ORBIT MOTION

Applicato **solo al wet**. Il dry resta identico, verificato: differenza 0,0 con ORBIT attivo.

- Il wet stereo viene trattato come **due sorgenti** (lato L e lato R del riverbero), posizionate con un pan a potenza costante: la larghezza stereo resta, senza somme mono né scambi bruschi dei canali.
- **Posizione** = CENTER ± separazione + DEPTH × WIDTH × forma(fase). **PHASE** sfasa le due sorgenti: 0° si muovono insieme, 180° si incrociano.
- **Differenza temporale tra le orecchie:** fino a ~0,45 ms sull'orecchio lontano (×1,6 nel modo ORBITAL).
- **SHAPE:** SINE morbido, TRIANGLE a velocità costante, ORBITAL con indizio "dietro la testa" (passa-basso a 3,5 kHz e −2,5 dB quando la sorgente passa dietro).
- **DIRECTION:** L>R, R>L, ALTERNATE (inverte il verso a ogni giro). Il verso è smussato, quindi cambiarlo non provoca salti.
- **SYNC:** il periodo è calcolato dal BPM della DAW (mai valori fissi) e la fase è agganciata alla posizione musicale (PPQ) con una correzione graduale. Misurato: 1/4 → 0,500 s a 120 BPM e 0,666 s a 90 BPM dopo un cambio di BPM in riproduzione.
- **ORBIT MIX** dosa la parte in movimento. **MOTION OFFSET** sposta la fase iniziale.
- **Compatibilità mono** con ORBIT al massimo: −1,0 dB rispetto al riverbero fermo (misurato).

## MODE

| Modo | Cosa cambia nel DSP |
|---|---|
| CLEAN | early reflections 0,35, modulazione ×0,6 |
| LUSH | modulazione ×1,8 + 0,15, diffusione +0,2, decay ×1,25, width ×1,2, ER bassi |
| DARK | damping minimo 55 %, High Cut massimo 4,5 kHz |
| GATED | gate sul wet: pieno per *Release* ms (400 ms se Natural), poi taglio di ~12 ms. Misurato: −140 dB a 0,8 s |
| SHIMMER | pitch shifter granulare **+12 semitoni** nel feedback. Misurato: 880 Hz da un La 440 a ~0 dB (CLEAN: −57 dB) |
| INFINITE | decay ×6 (40–180 s), damping dimezzato |
| ORBITAL | ORBIT sempre attivo, profondità ×1,35, differenza temporale ×1,6 |

## Preset

Vocal Space · Wide Trap Vocal · Drill Atmosphere · Dark Hall · Infinite Pad · Dreamy R&B · Psychedelic Orbit · Fast Stereo Bounce · Cinematic Huge Hall · Short Plate · Shimmer Cloud · Reverse-Style Swell

- Nota su **Reverse-Style Swell**: non inverte l'audio. È un effetto *equivalente*, ottenuto con un ATTACK lungo (1,2 s) che fa crescere l'energia del riverbero dopo ogni attacco, un pre-delay di 120 ms e un RELEASE corto. Il nome lo dichiara come "style".
- I **preset utente** si salvano con **SAVE** in `%APPDATA%\OrbitReverb\Presets\` (file `.orbitpreset`) e compaiono nel menu sotto USER.

## Struttura dei file

```
CMakeLists.txt                  build (VST3 + Standalone + test)
Source/Parameters.*             30 parametri automatizzabili
Source/Presets.*                12 preset di fabbrica
Source/PluginProcessor.*        processBlock, MODE, stato, preset utente
Source/PluginEditor.*           interfaccia e collegamenti ai parametri
Source/dsp/DspUtils.h           linee di ritardo, allpass, filtri, smoothing
Source/dsp/ReverbEngine.*       motore FDN, freeze, shimmer
Source/dsp/PitchShifter.h       pitch shifter granulare (+12)
Source/dsp/WetDynamics.h        Attack, Release, Gated, Ducking
Source/dsp/OrbitMotion.*        ORBIT, sync al BPM
Source/gui/*                    look and feel, knob, pulsante ORBIT, display
tests/OrbitTests.cpp            26 test con misure + snapshot della GUI
.github/workflows/              build Windows automatica
docs/                           screenshot e risultati dei test
```

## Limiti noti

- Finestra a dimensione fissa (1120×660), non ridimensionabile.
- Lo shimmer usa un pitch shifter granulare: su materiale molto percussivo si possono sentire leggere granulosità, tipiche di questa tecnica.
- Testato fino a 96 kHz. 176,4 e 192 kHz dovrebbero funzionare (le dimensioni dipendono dal sample rate) ma non sono stati testati.
- Nessuna verifica eseguita su Windows né dentro FL Studio da parte mia.
