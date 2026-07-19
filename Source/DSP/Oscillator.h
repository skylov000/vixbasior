#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace vixb
{

enum class OscMode
{
    Sine, Saw, Square, Triangle, PWM, FM, AM, RM, Wavetable, Noise
};

// PolyBLEP - antyaliasing dla fal analogowych bez tabel
inline float polyBlep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    else if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Prosty silnik wavetable - tablica N harmonicznych renderowanych do bufora
class WavetableSet
{
public:
    // Generuje "bank" wavetable z zestawu harmonicznych (do rozbudowy: wczytywanie .wav)
    void generate (int numTables = 32, int tableSize = 2048)
    {
        tables.clear();
        tables.resize ((size_t) numTables);
        for (int t = 0; t < numTables; ++t)
        {
            auto& table = tables[(size_t) t];
            table.resize ((size_t) tableSize);
            int maxHarmonic = juce::jmax (1, 64 - t * 2); // im dalej w bank, tym mniej harmonicznych (anty-alias)

            for (int i = 0; i < tableSize; ++i)
            {
                double phase = (double) i / (double) tableSize;
                double sample = 0.0;
                for (int h = 1; h <= maxHarmonic; ++h)
                    sample += std::sin (2.0 * juce::MathConstants<double>::pi * phase * h) / h; // saw-like
                table[(size_t) i] = (float) sample;
            }
            // normalizacja
            float maxVal = 0.0001f;
            for (auto v : table) maxVal = juce::jmax (maxVal, std::abs (v));
            for (auto& v : table) v /= maxVal;
        }
    }

    float sample (int tableIndex, float phase01) const
    {
        if (tables.empty()) return 0.0f;
        tableIndex = juce::jlimit (0, (int) tables.size() - 1, tableIndex);
        const auto& table = tables[(size_t) tableIndex];
        float pos = phase01 * (float) table.size();
        int i0 = (int) pos % (int) table.size();
        int i1 = (i0 + 1) % (int) table.size();
        float frac = pos - std::floor (pos);
        return table[(size_t) i0] + frac * (table[(size_t) i1] - table[(size_t) i0]);
    }

    int numTables() const { return (int) tables.size(); }

private:
    std::vector<std::vector<float>> tables;
};

class Oscillator
{
public:
    void prepare (double sr) { sampleRate = sr; }

    void setFrequency (float freqHz) { frequency = freqHz; }
    void setMode (OscMode m) { mode = m; }
    void setPulseWidth (float pw) { pulseWidth = juce::jlimit (0.02f, 0.98f, pw); }
    void setDetuneCents (float cents) { detuneCents = cents; }
    void setWavetable (WavetableSet* wt, float position01)
    {
        wavetable = wt;
        wtPosition = juce::jlimit (0.0f, 1.0f, position01);
    }
    void setFmAmount (float amt) { fmAmount = amt; }

    void reset() { phase = 0.0f; }

    // renderuje jedną próbkę; modInput używane dla FM/AM/RM od innego oscylatora
    float renderSample (float modInput = 0.0f)
    {
        float detuneMul = std::pow (2.0f, detuneCents / 1200.0f);
        float freq = frequency * detuneMul;

        if (mode == OscMode::FM)
            freq *= (1.0f + fmAmount * modInput);

        float dt = (float) (freq / sampleRate);
        float out = 0.0f;

        switch (mode)
        {
            case OscMode::Sine:
                out = std::sin (juce::MathConstants<float>::twoPi * phase);
                break;

            case OscMode::Saw:
            {
                out = 2.0f * phase - 1.0f;
                out -= polyBlep (phase, dt);
                break;
            }

            case OscMode::Square:
            case OscMode::PWM:
            {
                float pw = (mode == OscMode::PWM) ? pulseWidth : 0.5f;
                out = phase < pw ? 1.0f : -1.0f;
                out += polyBlep (phase, dt);
                float t2 = phase - pw; if (t2 < 0.0f) t2 += 1.0f;
                out -= polyBlep (t2, dt);
                break;
            }

            case OscMode::Triangle:
            {
                float sq = phase < 0.5f ? 1.0f : -1.0f;
                sq += polyBlep (phase, dt);
                float t2 = phase - 0.5f; if (t2 < 0.0f) t2 += 1.0f;
                sq -= polyBlep (t2, dt);
                // integracja square -> triangle (leaky)
                triIntegrator = triIntegrator * 0.999f + sq * dt * 4.0f;
                out = triIntegrator;
                break;
            }

            case OscMode::AM:
                out = std::sin (juce::MathConstants<float>::twoPi * phase) * (0.5f + 0.5f * modInput);
                break;

            case OscMode::RM:
                out = std::sin (juce::MathConstants<float>::twoPi * phase) * modInput;
                break;

            case OscMode::FM:
                out = std::sin (juce::MathConstants<float>::twoPi * phase);
                break;

            case OscMode::Wavetable:
                if (wavetable != nullptr && wavetable->numTables() > 0)
                {
                    int idx = (int) (wtPosition * (float) (wavetable->numTables() - 1));
                    out = wavetable->sample (idx, phase);
                }
                break;

            case OscMode::Noise:
                out = random.nextFloat() * 2.0f - 1.0f;
                break;
        }

        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        if (phase < 0.0f) phase += 1.0f;

        return out;
    }

private:
    double sampleRate = 44100.0;
    float frequency = 110.0f;
    float phase = 0.0f;
    float pulseWidth = 0.5f;
    float detuneCents = 0.0f;
    float fmAmount = 0.0f;
    float triIntegrator = 0.0f;
    OscMode mode = OscMode::Saw;
    WavetableSet* wavetable = nullptr;
    float wtPosition = 0.0f;
    juce::Random random;
};

} // namespace vixb
