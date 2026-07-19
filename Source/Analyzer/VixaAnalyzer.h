#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace vixb
{

// Wynik pojedynczej analizy bloku audio - to jest to, co pokazuje UI (Bass Score, Vixa Quality, podpowiedzi)
struct AnalysisResult
{
    // Spektrum uśrednione (magnitude, znormalizowane 0..1) do rysowania FFT w UI
    std::vector<float> spectrumMagnitudes;

    // Energia w pasmach istotnych dla oceny bassu (Hz)
    float subEnergy   = 0.0f; // 20-60 Hz
    float bassEnergy  = 0.0f; // 60-150 Hz
    float lowMidEnergy= 0.0f; // 150-400 Hz
    float midEnergy   = 0.0f; // 400-1000 Hz - kluczowe dla słyszalności na telefonie
    float highEnergy  = 0.0f; // 1000-5000 Hz - "gryzące" harmoniczne

    // Harmoniczne względem wykrytej częstotliwości podstawowej
    float fundamentalHz = 0.0f;
    std::array<float, 8> harmonicLevels {}; // H1..H8, znormalizowane względem H1

    float stereoWidth01   = 0.0f; // 0 = mono, 1 = bardzo szeroko
    float phaseCorrelation= 1.0f; // -1..1, <0 = ryzyko wyciszenia w mono
    float rms             = 0.0f;
    float lufsApprox       = -23.0f;
    float transientStrength= 0.0f; // 0..1, atak / sustain

    // Wyniki końcowe pokazywane userowi
    int bassScore0to100 = 0;
    juce::StringArray bassScoreNotes;

    struct StyleMatch { juce::String name; int percent; };
    std::vector<StyleMatch> vixaQuality; // np. {"Majki Style", 96}

    struct DeviceCheck { juce::String device; bool ok; juce::String note; };
    std::vector<DeviceCheck> deviceAudibility; // laptop / telefon / klub / słuchawki

    juce::StringArray vixaHelperTips; // "Dodaj 12% Chorus." itd.
};

// Silnik analizy - działa na blokach audio (np. wywoływany z bufora audio w processBlock,
// lub na zamrożonym buforze po naciśnięciu "Analyze")
class VixaAnalyzer
{
public:
    void prepare (double sampleRate, int fftOrder = 12); // fftOrder=12 -> FFT 4096

    // Analizuje stereo blok próbek (interleaved L/R niekoniecznie - tu dwa oddzielne wskaźniki)
    AnalysisResult analyze (const float* left, const float* right, int numSamples);

private:
    double sampleRate = 44100.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    int fftSize = 4096;

    std::vector<float> fftBufferMono;
    std::vector<float> fftBufferL, fftBufferR;

    float bandEnergy (const std::vector<float>& magnitudes, float loHz, float hiHz) const;
    float detectFundamental (const std::vector<float>& magnitudes) const;
    std::array<float, 8> extractHarmonics (const std::vector<float>& magnitudes, float fundamentalHz) const;

    int  computeBassScore (const AnalysisResult& r, juce::StringArray& notes) const;
    std::vector<AnalysisResult::StyleMatch> computeStyleMatches (const AnalysisResult& r) const;
    std::vector<AnalysisResult::DeviceCheck> computeDeviceAudibility (const AnalysisResult& r) const;
    juce::StringArray computeHelperTips (const AnalysisResult& r) const;
};

} // namespace vixb
