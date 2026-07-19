#pragma once
#include <juce_dsp/juce_dsp.h>

namespace vixb
{

enum class FilterType
{
    LP, HP, BP, Notch, Comb, Ladder, Diode, Analog, Formant,
    VixaFilter, ClubFilter, BounceFilter, HarmonicFilter, VocalFilter
};

// Filtr Moog-style ladder (4-pole, z nieliniowością do "analogowego" brzmienia)
class LadderFilter
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { for (auto& s : stage) s = 0.0f; }

    void setParams (float cutoffHz, float resonance01)
    {
        cutoff = juce::jlimit (20.0f, 20000.0f, cutoffHz);
        resonance = juce::jlimit (0.0f, 1.0f, resonance01);
    }

    float process (float input)
    {
        float fc = cutoff / (float) sampleRate;
        float g = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * fc);
        float fb = resonance * 4.2f;

        float x = input - fb * stage[3];
        stage[0] += g * (std::tanh (x) - std::tanh (stage[0]));
        stage[1] += g * (std::tanh (stage[0]) - std::tanh (stage[1]));
        stage[2] += g * (std::tanh (stage[1]) - std::tanh (stage[2]));
        stage[3] += g * (std::tanh (stage[2]) - std::tanh (stage[3]));

        return stage[3];
    }

private:
    double sampleRate = 44100.0;
    float cutoff = 8000.0f, resonance = 0.2f;
    float stage[4] = { 0, 0, 0, 0 };
};

// "VIXA FILTER" - unikalny hybrydowy filtr: LP + lekki formant + subtelna saturacja,
// zaprojektowany żeby bas zostawał "okrągły" ale czytelny na małych głośnikach.
class VixaFilter
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        lp.reset(); lp.prepare (sr);
        formant.reset (sr, 0.0);
    }

    void setParams (float cutoffHz, float resonance01, float character01)
    {
        lp.setParams (cutoffHz, resonance01);
        character = character01; // 0 = neutralny, 1 = mocno "vixiarski" (formant + saturacja)
    }

    float process (float input)
    {
        float filtered = lp.process (input);
        // dodaj lekki formant w okolicy 400-900 Hz żeby bas "śpiewał"
        float formantFreq = 400.0f + 500.0f * character;
        formant.setCoefficients (juce::IIRCoefficients::makePeakFilter (sampleRate, formantFreq, 1.2, 1.0 + character * 2.0));
        float withFormant = formant.processSingleSampleRaw (filtered);
        float mixed = filtered * (1.0f - character * 0.4f) + withFormant * (character * 0.4f);
        return std::tanh (mixed * (1.0f + character * 0.5f));
    }

private:
    double sampleRate = 44100.0;
    LadderFilter lp;
    juce::IIRFilter formant;
    float character = 0.3f;
};

// Prosty filtr wielotrybowy oparty o juce::dsp::StateVariableTPTFilter dla LP/HP/BP/Notch
class StandardFilter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        svf.prepare (spec);
    }

    void setParams (FilterType type, float cutoffHz, float resonance01)
    {
        svf.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, cutoffHz));
        svf.setResonance (juce::jmap (resonance01, 0.0f, 1.0f, 0.5f, 10.0f));

        switch (type)
        {
            case FilterType::HP: svf.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
            case FilterType::BP: svf.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
            default:             svf.setType (juce::dsp::StateVariableTPTFilterType::lowpass); break;
        }
    }

    float process (float input)
    {
        return svf.processSample (0, input);
    }

    void reset() { svf.reset(); }

private:
    juce::dsp::StateVariableTPTFilter<float> svf;
};

} // namespace vixb
