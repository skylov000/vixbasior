#pragma once
#include <juce_dsp/juce_dsp.h>

namespace vixb
{

class SimpleEnvelope
{
public:
    void prepare (double sr) { env.setSampleRate (sr); }

    void setParams (float attackMs, float decayMs, float sustain01, float releaseMs)
    {
        juce::ADSR::Parameters p;
        p.attack  = juce::jmax (0.001f, attackMs / 1000.0f);
        p.decay   = juce::jmax (0.001f, decayMs / 1000.0f);
        p.sustain = juce::jlimit (0.0f, 1.0f, sustain01);
        p.release = juce::jmax (0.001f, releaseMs / 1000.0f);
        env.setParameters (p);
    }

    void noteOn()  { env.noteOn(); }
    void noteOff() { env.noteOff(); }
    bool isActive() const { return env.isActive(); }
    float nextSample() { return env.getNextSample(); }

private:
    juce::ADSR env;
};

enum class LfoShape { Sine, Triangle, Square, SawUp, SawDown, Random, Chaos };

class LFO
{
public:
    void prepare (double sr) { sampleRate = sr; }
    void setRateHz (float hz) { rateHz = hz; }
    void setShape (LfoShape s) { shape = s; }

    float nextSample()
    {
        float out = 0.0f;
        switch (shape)
        {
            case LfoShape::Sine:     out = std::sin (juce::MathConstants<float>::twoPi * phase); break;
            case LfoShape::Triangle: out = 4.0f * std::abs (phase - std::floor (phase + 0.75f) + 0.25f) - 1.0f; break;
            case LfoShape::Square:   out = phase < 0.5f ? 1.0f : -1.0f; break;
            case LfoShape::SawUp:    out = 2.0f * phase - 1.0f; break;
            case LfoShape::SawDown:  out = 1.0f - 2.0f * phase; break;
            case LfoShape::Random:
                if (phase < lastPhase) heldValue = random.nextFloat() * 2.0f - 1.0f;
                out = heldValue;
                break;
            case LfoShape::Chaos:
            {
                // logistic map - proste chaotyczne zachowanie do "Vixa Chaos" modulacji
                chaosX = chaosR * chaosX * (1.0f - chaosX);
                out = chaosX * 2.0f - 1.0f;
                break;
            }
        }
        lastPhase = phase;
        phase += (float) (rateHz / sampleRate);
        if (phase >= 1.0f) phase -= 1.0f;
        return out;
    }

private:
    double sampleRate = 44100.0;
    float rateHz = 2.0f;
    float phase = 0.0f, lastPhase = 0.0f;
    float heldValue = 0.0f;
    float chaosX = 0.42f, chaosR = 3.9f;
    LfoShape shape = LfoShape::Sine;
    juce::Random random;
};

// Prosty wpis w macierzy modulacji: źródło -> cel, z ilością
struct ModConnection
{
    juce::String source;      // np. "LFO1", "ENV1", "Velocity", "MacroVixa"
    juce::String destination; // np. "OscA.Pitch", "Filter.Cutoff"
    float amount = 0.0f;      // -1..1
};

} // namespace vixb
