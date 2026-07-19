# VixBasior — Vixiarski Bass Synthesizer (VST3/JUCE)

## Co jest w tym pakiecie

Działający **szkielet projektu JUCE/C++** z realną implementacją kluczowych elementów:

```
VixBasior/
├── CMakeLists.txt
└── Source/
    ├── PluginProcessor.h/.cpp     # silnik audio, głos syntezatora, APVTS (parametry)
    ├── PluginEditor.h/.cpp        # GUI: wizualizer FFT, panel BASS SCORE, gałki
    ├── DSP/
    │   ├── Oscillator.h           # Sine/Saw/Square/Tri/PWM/FM/AM/RM/Wavetable/Noise (PolyBLEP)
    │   ├── Filters.h              # Ladder, VixaFilter (unikalny), StandardFilter (LP/HP/BP)
    │   ├── Modulation.h           # ADSR, LFO (w tym "Chaos" - logistic map)
    │   ├── VixaEngine.h           # UNIKALNE MAKRO: jedno pokrętło = zestaw stylów vixiarskich
    │   └── Effects.h              # Saturator, Chorus, Stereo Widener, uproszczony OTT
    └── Analyzer/
        ├── VixaAnalyzer.h/.cpp    # AI/VIXA ANALYZER: FFT, harmoniczne, stereo, scoring, tipy
```

To **kompiluje się** jako plugin VST3 + Standalone po podłączeniu JUCE (patrz niżej). Nie jest to
gotowy komercyjny produkt (brak 500 presetów, brak pełnego oversamplingu x2-x16, brak GPU-UI) —
to solidny fundament architektoniczny, który zaimplementowałem zgodnie z Twoją specyfikacją,
gotowy do rozbudowy.

---

## Jak zbudować

1. Zainstaluj Visual Studio 2022 (Desktop development with C++) oraz CMake ≥ 3.22.
2. Sklonuj JUCE obok tego folderu:
   ```
   git clone https://github.com/juce-framework/JUCE.git
   ```
   (struktura: `VixBasior/JUCE/...`)
3. W folderze `VixBasior`:
   ```
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
4. Plik VST3 pojawi się w `build/VixBasior_artefacts/Release/VST3/VixBasior.vst3` — skopiuj go do
   `C:\Program Files\Common Files\VST3\`.

---

## Architektura silnika dźwiękowego

**Sygnał głosu (na próbkę):**

```
OSC A ─┐
OSC B ─┼─ mix ─► VixaFilter (LP + formant + saturacja) ─► Saturator ─► × AmpEnv ─► out
OSC C ─┤              ▲
SUB   ─┤          FilterEnv × filterEnvAmount
NOISE ─┘
```

**Sygnał master (post-synth):**

```
Synth voices (sum) ─► SimpleOTT (opcjonalnie) ─► SimpleChorus ─► SafeStereoWidener ─► out
                                                                        │
                                                                        ▼
                                                              VixaAnalyzer.analyze()
                                                                        │
                                                                        ▼
                                                         GUI: BASS SCORE / VIXA QUALITY / HELPER
```

### VIXA ENGINE (unikalna funkcja)

`VixaEngine::computeTargets()` zwraca `VixaTargets` — zestaw docelowych wartości (harmoniczne,
charakter filtra, stereo, saturacja, transjenty, sub, cutoff), interpolowanych parametrem
`vixaAmount` (0–1) w zależności od wybranego stylu (`Majki`, `Wojtuli`, `Pumpsound`, `Xsound`,
`Bounce`, `Club`, `Dance`, `Soft`, `Punch`, `Wide`, `Custom`). To jest **jedno pokrętło**
zmieniające jednocześnie wiele parametrów DSP — realizuje dokładnie to, o co prosiłeś.
Wartości w `presetFor()` to punkt startowy — do dostrojenia słuchem względem referencyjnych
utworów każdego artysty/stylu.

### VIXA ANALYZER / AI ANALYZER (najważniejsza funkcja)

`VixaAnalyzer::analyze()` liczy w czasie rzeczywistym (na buforze audio, FFT 4096):

- energie pasmowe: sub (20-60Hz), bass (60-150Hz), low-mid (150-400Hz), mid (400-1000Hz, kluczowe
  dla słyszalności na telefonie), high (1-5kHz)
- częstotliwość podstawową i poziomy harmonicznych H1-H8 (peak-picking wokół oczekiwanych binów)
- szerokość stereo (stosunek energii side/mid) i korelację fazową (ryzyko wyciszenia w mono)
- siłę transjentu (stosunek RMS ataku do sustainu)

Na tej podstawie liczy:

- **BASS SCORE 0-100%** — ważona suma pięciu składowych (sub, harmoniczne, mid, stereo, transjent)
  z listą tekstowych uwag (`bassScoreNotes`)
- **VIXA QUALITY** — dopasowanie procentowe do stylów (Majki/Wojtuli/Pumpsound/Xsound) na bazie
  reguł DSP (nie jest to model ML — patrz uwaga niżej)
- **Device Audibility** — heurystyczna ocena słyszalności na telefonie/laptopie/klubie/słuchawkach
- **VIXA HELPER** — konkretne podpowiedzi tekstowe ("Zmniejsz SUB o X%", "Dodaj Y% saturacji" itd.)

**Ważna uwaga o "AI":** zaimplementowałem to jako przejrzysty, deterministyczny system reguł DSP
oparty na realnej analizie FFT — nie jako sieć neuronową. To podejście jest szybsze (działa co
blok audio w czasie rzeczywistym, zero latencji ML), przewidywalne i łatwe do dalszego strojenia.
Jeśli faktycznie zależy Ci na modelu ML (np. wytrenowanym na referencyjnych utworach danego
gatunku), to osobny, znacznie większy projekt (zbieranie datasetu, trening, eksport do C++/ONNX)
— mogę zaprojektować taki pipeline w kolejnym kroku, jeśli chcesz.

---

## Co zostało celowo uproszczone (do rozbudowy)

| Funkcja z specyfikacji         | Stan w tym pakiecie                              | Następny krok |
|---------------------------------|---------------------------------------------------|---------------|
| 300+ wavetables na oscylator    | `WavetableSet` generuje banki proceduralnie        | Wczytywanie plików .wav jako wavetable |
| Granular / Harmonic Engine osc  | Nie zaimplementowane                               | Dodać `OscMode::Granular` z ziarnami z bufora |
| Pełny bank filtrów (Comb/Diode/Formant/Vocal/Club/Bounce/Harmonic) | Zaimplementowano Ladder + VixaFilter + Standard (LP/HP/BP) | Dodać pozostałe jako osobne klasy w `Filters.h` |
| OTT/Multiband | Uproszczony 3-pasmowy, bez prawdziwego Linkwitz-Riley crossover | Zaimplementować pełny crossover 4-pasmowy |
| Oversampling x2/x4/x8/x16 | Nie zaimplementowane | `juce::dsp::Oversampling<float>` w processBlock |
| GPU rendering UI | Standard JUCE Graphics (CPU) | OpenGLContext + custom shader dla wizualizera |
| 500 presetów | Brak | System `.vixpreset` (XML z ValueTree APVTS) + biblioteka |
| Drag&drop modulacji, MIDI Learn | Brak w GUI (macierz modulacji istnieje jako `ModConnection`) | Dodać `ModMatrixComponent` z drag&drop |
| Undo/Redo | Brak | `juce::UndoManager` podpięty do APVTS |

---

## Plan implementacji krok po kroku (rekomendowana kolejność)

1. **Fundament (ZROBIONE w tym pakiecie):** struktura JUCE, APVTS, jeden głos syntezatora,
   filtr, obwiednie, VixaEngine, Analyzer, podstawowe GUI.
2. **Dopracowanie brzmienia:** dodać wavetable loader (import .wav), Comb/Formant/Diode filters,
   pełny 4-pasmowy crossover do OTT, oversampling x2/x4 na saturacji (anty-aliasing).
3. **Macierz modulacji:** UI z drag&drop (LFO/ENV/Random/Chaos/Macro/Velocity → dowolny parametr),
   MIDI Learn przez `juce::MidiKeyboardState` + prawy-klik na knobie.
4. **Preset manager:** zapis/odczyt `.vixpreset` (ValueTree→XML), kategorie, wyszukiwarka,
   500 presetów do zaprojektowania przez sound designera (to praca kreatywna, nie kodowa).
5. **UI polish:** skalowanie 50-200% (przez `AffineTransform` na top-level Component), dark/light
   theme (ColourScheme), fullscreen, GPU rendering wizualizera (OpenGL).
6. **Undo/Redo + drag&drop.**
7. **Testy/optymalizacja:** profilowanie CPU, SIMD w Oscillator/Filter (juce::dsp::SIMDRegister),
   wielowątkowy render głosów dla dużej polifonii.
8. **(Opcjonalnie) prawdziwy model ML** dla Analyzera, jeśli reguły DSP okażą się niewystarczające.

---

Chcesz, żebym w kolejnym kroku dopracował konkretny moduł głębiej (np. pełny bank filtrów,
system presetów, albo macierz modulacji z drag&drop w GUI)? Powiedz który — zrobię to od razu.
