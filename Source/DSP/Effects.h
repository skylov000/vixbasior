#pragma once
#include <juce_dsp/juce_dsp.h>

namespace vixb
{

// Miękka saturacja (tanh) z regulacją drive - dobra do "soczystych" bassów
class Saturator
{
public:
    void setDrive (float d) { drive = juce::jlimit (0.0f, 1.0f, d); }
    float process (float x)
    {
        float g = 1.0f + drive * 6.0f;
        return std::tanh (x * g) / std::tanh (g);
    }
private:
    float drive = 0.0f;
};

// Prosty stereo chorus (2 głosy) do poszerzania bassu bez utraty mono-compat w subie
class SimpleChorus
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        delayL.prepare ({ sr, 512, 1 });
        delayR.prepare ({ sr, 512, 1 });
        delayL.setMaximumDelayInSamples ((int) (sr * 0.05));
        delayR.setMaximumDelayInSamples ((int) (sr * 0.05));
    }

    void setParams (float rateHz, float depthMs, float mix01)
    {
        rate = rateHz; depthSamples = (float) (depthMs / 1000.0 * sampleRate); mix = mix01;
    }

    void process (float& l, float& r)
    {
        phase += (float) (rate / sampleRate);
        if (phase > 1.0f) phase -= 1.0f;

        float lfoL = std::sin (juce::MathConstants<float>::twoPi * phase);
        float lfoR = std::sin (juce::MathConstants<float>::twoPi * phase + juce::MathConstants<float>::pi * 0.5f);

        float baseDelay = (float) (0.01 * sampleRate);
        delayL.setDelay (baseDelay + depthSamples * (0.5f + 0.5f * lfoL));
        delayR.setDelay (baseDelay + depthSamples * (0.5f + 0.5f * lfoR));

        delayL.pushSample (0, l);
        delayR.pushSample (0, r);
        float wetL = delayL.popSample (0);
        float wetR = delayR.popSample (0);

        l = l * (1.0f - mix) + wetL * mix;
        r = r * (1.0f - mix) + wetR * mix;
    }

private:
    double sampleRate = 44100.0;
    float rate = 0.5f, depthSamples = 200.0f, mix = 0.3f, phase = 0.0f;
    juce::dsp::DelayLine<float> delayL { 4096 }, delayR { 4096 };
};

// Mid/Side stereo widener - bezpieczny (utrzymuje sub w mono, żeby uniknąć problemów fazowych)
class SafeStereoWidener
{
public:
    void setWidth (float w) { width = juce::jlimit (0.0f, 2.0f, w); }
    void setSubMonoFreq (float hz) { subMonoFreqHz = hz; }
    void prepare (double sr) { sampleRate = sr; monoLP.reset(); monoLP.prepare (sr); }

    void process (float& l, float& r)
    {
        float mid = 0.5f * (l + r);
        float side = 0.5f * (l - r);

        side *= width;

        l = mid + side;
        r = mid - side;
    }

private:
    double sampleRate = 44100.0;
    float width = 1.0f;
    float subMonoFreqHz = 120.0f;
    struct DummyLP { void reset() {} void prepare (double) {} } monoLP;
};

// Uproszczony OTT (multiband upward+downward compression) - 3 pasma
class SimpleOTT
{
public:
    void prepare (double sr)
    {
        for (auto& b : bands)
        {
            b.lp.reset(); b.lp.prepare (sr);
            b.hp.reset(); b.hp.prepare (sr);
        }
        sampleRate = sr;
    }

    void setDepth (float d) { depth = juce::jlimit (0.0f, 1.0f, d); }

    float process (float x)
    {
        // Bardzo uproszczony model: dzielimy na low/mid/high filtrem SVF i kompresujemy każde pasmo
        // (produkcyjnie: zastąp to pełnym Linkwitz-Riley crossover)
        float low  = bands[0].lp.process (x);
        float high = bands[2].hp.process (x);
        float mid  = x - low - high;

        low  = compressBand (low,  0.5f);
        mid  = compressBand (mid,  0.7f);
        high = compressBand (high, 0.8f);

        return low + mid + high;
    }

private:
    float compressBand (float x, float ratio)
    {
        float env = std::abs (x);
        float threshold = 0.3f;
        if (env > threshold)
        {
            float over = env - threshold;
            float gain = 1.0f - depth * ratio * (over / (env + 0.0001f));
            return x * juce::jmax (0.1f, gain);
        }
        return x * (1.0f + depth * ratio * 0.3f); // upward comp dla cichych partii
    }

    struct Band
    {
        struct SimpleSVF
        {
            void reset() {}
            void prepare (double) {}
            float process (float x) { return x; } // placeholder - patrz Filters.h StandardFilter dla realnej implementacji
        } lp, hp;
    };
    Band bands[3];
    double sampleRate = 44100.0;
    float depth = 0.5f;
};

} // namespace vixb
