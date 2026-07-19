#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Oscillator.h"
#include "DSP/Filters.h"
#include "DSP/Modulation.h"
#include "DSP/VixaEngine.h"
#include "DSP/Effects.h"
#include "Analyzer/VixaAnalyzer.h"

namespace vixb
{

//==============================================================================
// Pojedynczy głos syntezatora: OSC A/B/C + SUB + NOISE, filtr, obwiednie
//==============================================================================
class VixVoice : public juce::SynthesiserVoice
{
public:
    VixVoice();

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    void prepare (double sampleRate);

    // Parametry sterowane z procesora (aktualizowane co blok)
    struct VoiceParams
    {
        OscMode oscAMode = OscMode::Saw, oscBMode = OscMode::Saw, oscCMode = OscMode::Square;
        float oscAMix = 0.8f, oscBMix = 0.5f, oscCMix = 0.0f, subMix = 0.9f, noiseMix = 0.0f;
        float oscBDetuneCents = 7.0f, oscCDetuneCents = -7.0f;
        float filterCutoffHz = 1200.0f, filterResonance = 0.25f, filterCharacter = 0.3f;
        float ampAttackMs = 3.0f, ampDecayMs = 120.0f, ampSustain = 0.85f, ampReleaseMs = 200.0f;
        float filtAttackMs = 5.0f, filtDecayMs = 250.0f, filtSustain = 0.6f, filtReleaseMs = 200.0f;
        float filterEnvAmount = 0.4f; // -1..1
        VixaTargets vixaTargets;
        float saturationDrive = 0.2f;
    };
    VoiceParams params;

private:
    double sampleRate = 44100.0;
    float baseFrequency = 110.0f;

    Oscillator oscA, oscB, oscC, oscSub;
    VixaFilter filter;
    SimpleEnvelope ampEnv, filterEnv;
    Saturator saturator;
};

//==============================================================================
struct VixSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
class VixBasiorAudioProcessor : public juce::AudioProcessor
{
public:
    VixBasiorAudioProcessor();
    ~VixBasiorAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VixBasior"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Ostatni wynik analizy - odczytywany przez GUI (thread-safe przez std::atomic w wewnętrznej strukturze
    // lub prostą blokadę - tutaj: kopiowany pod mutexem raz na blok)
    AnalysisResult getLatestAnalysis();

private:
    juce::Synthesiser synth;
    VixaAnalyzer analyzer;
    VixaEngine vixaEngine;

    SimpleChorus chorus;
    SafeStereoWidener widener;
    SimpleOTT ott;

    juce::CriticalSection analysisLock;
    AnalysisResult latestAnalysis;

    void updateVoiceParamsFromAPVTS();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VixBasiorAudioProcessor)
};

} // namespace vixb
