#include "VixaAnalyzer.h"

namespace vixb
{

void VixaAnalyzer::prepare (double sr, int fftOrder)
{
    sampleRate = sr;
    fftSize = 1 << fftOrder;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>> ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);

    fftBufferMono.resize ((size_t) fftSize * 2, 0.0f);
    fftBufferL.resize ((size_t) fftSize * 2, 0.0f);
    fftBufferR.resize ((size_t) fftSize * 2, 0.0f);
}

float VixaAnalyzer::bandEnergy (const std::vector<float>& mag, float loHz, float hiHz) const
{
    float binHz = (float) (sampleRate / fftSize);
    int loBin = juce::jmax (0, (int) (loHz / binHz));
    int hiBin = juce::jmin ((int) mag.size() - 1, (int) (hiHz / binHz));
    float sum = 0.0f;
    int count = 0;
    for (int i = loBin; i <= hiBin; ++i) { sum += mag[(size_t) i]; ++count; }
    return count > 0 ? sum / (float) count : 0.0f;
}

float VixaAnalyzer::detectFundamental (const std::vector<float>& mag) const
{
    float binHz = (float) (sampleRate / fftSize);
    // szukaj piku w zakresie typowym dla fundamentalnej basu (25-250 Hz)
    int loBin = juce::jmax (1, (int) (25.0f / binHz));
    int hiBin = juce::jmin ((int) mag.size() - 1, (int) (250.0f / binHz));

    int bestBin = loBin;
    float bestVal = 0.0f;
    for (int i = loBin; i <= hiBin; ++i)
    {
        if (mag[(size_t) i] > bestVal) { bestVal = mag[(size_t) i]; bestBin = i; }
    }
    return (float) bestBin * binHz;
}

std::array<float, 8> VixaAnalyzer::extractHarmonics (const std::vector<float>& mag, float fundamentalHz) const
{
    std::array<float, 8> result {};
    if (fundamentalHz <= 0.0f) return result;

    float binHz = (float) (sampleRate / fftSize);
    float h1 = 0.0001f;

    for (int h = 1; h <= 8; ++h)
    {
        float freq = fundamentalHz * (float) h;
        int bin = (int) (freq / binHz);
        if (bin >= 0 && bin < (int) mag.size())
        {
            // szukaj lokalnego maksimum +/-2 biny (drift stroju)
            float peak = 0.0f;
            for (int off = -2; off <= 2; ++off)
            {
                int b = bin + off;
                if (b >= 0 && b < (int) mag.size())
                    peak = juce::jmax (peak, mag[(size_t) b]);
            }
            result[(size_t) (h - 1)] = peak;
            if (h == 1) h1 = juce::jmax (h1, peak);
        }
    }
    for (auto& v : result) v /= h1; // normalizacja względem H1
    return result;
}

AnalysisResult VixaAnalyzer::analyze (const float* left, const float* right, int numSamples)
{
    AnalysisResult result;
    if (fft == nullptr || numSamples <= 0) return result;

    int n = juce::jmin (numSamples, fftSize);

    std::fill (fftBufferL.begin(), fftBufferL.end(), 0.0f);
    std::fill (fftBufferR.begin(), fftBufferR.end(), 0.0f);
    std::fill (fftBufferMono.begin(), fftBufferMono.end(), 0.0f);

    double sumL = 0.0, sumR = 0.0, sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
    double rmsAcc = 0.0;

    for (int i = 0; i < n; ++i)
    {
        float l = left[i];
        float r = (right != nullptr) ? right[i] : l;
        fftBufferL[(size_t) i] = l;
        fftBufferR[(size_t) i] = r;
        fftBufferMono[(size_t) i] = 0.5f * (l + r);

        sumL += l; sumR += r;
        sumLR += (double) l * r;
        sumL2 += (double) l * l;
        sumR2 += (double) r * r;
        rmsAcc += 0.5 * (l * l + r * r);
    }

    result.rms = (float) std::sqrt (rmsAcc / juce::jmax (1, n));
    result.lufsApprox = 20.0f * std::log10 (juce::jmax (0.0000001f, result.rms)) - 0.691f; // przybliżenie LUFS-I

    // korelacja fazowa
    double denom = std::sqrt (sumL2 * sumR2);
    result.phaseCorrelation = denom > 0.0000001 ? (float) (sumLR / denom) : 1.0f;

    // szerokość stereo: stosunek energii side/mid
    double mid = 0.0, side = 0.0;
    for (int i = 0; i < n; ++i)
    {
        float m = 0.5f * (fftBufferL[(size_t) i] + fftBufferR[(size_t) i]);
        float s = 0.5f * (fftBufferL[(size_t) i] - fftBufferR[(size_t) i]);
        mid += m * m; side += s * s;
    }
    result.stereoWidth01 = (float) juce::jlimit (0.0, 1.0, side / juce::jmax (0.0001, mid + side));

    // transient strength: różnica między pierwszymi 5ms a resztą bufora (uproszczone)
    int attackSamples = juce::jmin (n, (int) (0.005 * sampleRate));
    float attackRms = 0.0f, sustainRms = 0.0f;
    for (int i = 0; i < attackSamples; ++i) attackRms += std::abs (fftBufferMono[(size_t) i]);
    for (int i = attackSamples; i < n; ++i) sustainRms += std::abs (fftBufferMono[(size_t) i]);
    attackRms /= juce::jmax (1, attackSamples);
    sustainRms /= juce::jmax (1, n - attackSamples);
    result.transientStrength = juce::jlimit (0.0f, 1.0f, attackRms / juce::jmax (0.0001f, attackRms + sustainRms));

    // FFT na sygnale mono
    window->multiplyWithWindowingTable (fftBufferMono.data(), (size_t) fftSize);
    fft->performFrequencyOnlyForwardTransform (fftBufferMono.data());

    std::vector<float> magnitudes ((size_t) fftSize / 2);
    float maxMag = 0.0001f;
    for (size_t i = 0; i < magnitudes.size(); ++i)
    {
        magnitudes[i] = fftBufferMono[i];
        maxMag = juce::jmax (maxMag, magnitudes[i]);
    }
    for (auto& m : magnitudes) m /= maxMag; // normalizacja 0..1

    result.spectrumMagnitudes = magnitudes;

    result.subEnergy    = bandEnergy (magnitudes, 20.0f, 60.0f);
    result.bassEnergy   = bandEnergy (magnitudes, 60.0f, 150.0f);
    result.lowMidEnergy = bandEnergy (magnitudes, 150.0f, 400.0f);
    result.midEnergy    = bandEnergy (magnitudes, 400.0f, 1000.0f);
    result.highEnergy   = bandEnergy (magnitudes, 1000.0f, 5000.0f);

    result.fundamentalHz = detectFundamental (magnitudes);
    result.harmonicLevels = extractHarmonics (magnitudes, result.fundamentalHz);

    result.bassScore0to100 = computeBassScore (result, result.bassScoreNotes);
    result.vixaQuality = computeStyleMatches (result);
    result.deviceAudibility = computeDeviceAudibility (result);
    result.vixaHelperTips = computeHelperTips (result);

    return result;
}

int VixaAnalyzer::computeBassScore (const AnalysisResult& r, juce::StringArray& notes) const
{
    float score = 0.0f;

    // 1) Obecność subu, ale nie za dużo (idealnie sub nie powinien dominować nad bassEnergy)
    float subRatio = r.subEnergy / juce::jmax (0.0001f, r.subEnergy + r.bassEnergy);
    float subScore = 1.0f - std::abs (subRatio - 0.4f) * 2.0f; // optimum ok. 40% energii nisko w subie
    subScore = juce::jlimit (0.0f, 1.0f, subScore);
    score += subScore * 25.0f;
    if (subRatio > 0.65f) notes.add ("Za duzo SUB - zmniejsz SUB, dodaj wiecej srodka.");
    else if (subRatio < 0.15f) notes.add ("Za malo SUB - dodaj SUB OSC lub zwieksz jego poziom.");

    // 2) Harmoniczne 2/3/4 obecne w rozsądnej proporcji (typowe dla "vixiarskiego" brzmienia)
    float h2 = r.harmonicLevels[1], h3 = r.harmonicLevels[2], h4 = r.harmonicLevels[3];
    float harmonicScore = juce::jlimit (0.0f, 1.0f, (h2 + h3 + h4) / 2.4f);
    score += harmonicScore * 25.0f;
    if (harmonicScore < 0.35f) notes.add ("Za malo harmonicznych - sprobuj zwiekszyc Harmoniczne (Harmonic Drive) lub dodaj saturacje.");
    else notes.add ("Harmoniczne 2/3/4 w dobrej proporcji.");

    // 3) Obecność energii w środku (400-1000Hz) - kluczowe dla słyszalności na telefonie
    float midScore = juce::jlimit (0.0f, 1.0f, r.midEnergy / 0.5f);
    score += midScore * 20.0f;
    if (midScore < 0.3f) notes.add ("Za malo srodka (400-1000Hz) - bass moze zanikac na telefonie.");

    // 4) Stereo width - szeroko, ale bez utraty korelacji fazowej
    float stereoScore = juce::jlimit (0.0f, 1.0f, r.stereoWidth01 * 1.3f);
    if (r.phaseCorrelation < 0.2f) stereoScore *= 0.4f; // kara za problemy fazowe
    score += stereoScore * 15.0f;
    if (r.phaseCorrelation < 0.2f) notes.add ("Mozliwy problem fazowy - sprawdz korelacje w mono.");
    else notes.add (juce::String ("Stereo: ") + juce::String ((int) (r.stereoWidth01 * 100)) + "%.");

    // 5) Dynamika / transjent
    float transientScore = juce::jlimit (0.0f, 1.0f, r.transientStrength * 2.0f);
    score += transientScore * 15.0f;
    if (transientScore < 0.3f) notes.add ("Slaby atak - dodaj Transient Designer lub Punch (VIXA ENGINE).");

    return juce::jlimit (0, 100, (int) std::round (score));
}

std::vector<AnalysisResult::StyleMatch> VixaAnalyzer::computeStyleMatches (const AnalysisResult& r) const
{
    // Heurystyczne dopasowanie do stylów na podstawie proporcji energii i harmonicznych.
    // To NIE jest model ML - to zestaw reguł DSP odzwierciedlających charakterystykę stylów.
    std::vector<AnalysisResult::StyleMatch> out;

    auto clampPct = [] (float v) { return juce::jlimit (0, 100, (int) std::round (v * 100.0f)); };

    float h2345 = r.harmonicLevels[1] + r.harmonicLevels[2] + r.harmonicLevels[3] + r.harmonicLevels[4];
    float punch = r.transientStrength;
    float wide  = r.stereoWidth01;
    float sub   = r.subEnergy;

    out.push_back ({ "Majki Style",     clampPct (0.4f * h2345 / 3.0f + 0.3f * punch + 0.3f * wide) });
    out.push_back ({ "U.Wojtuli Style", clampPct (0.5f * h2345 / 3.0f + 0.25f * punch + 0.25f * (1.0f - sub)) });
    out.push_back ({ "Pumpsound Style", clampPct (0.35f * punch + 0.35f * h2345 / 3.0f + 0.3f * sub) });
    out.push_back ({ "Xsound Style",    clampPct (0.4f * wide + 0.3f * h2345 / 3.0f + 0.3f * punch) });

    std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.percent > b.percent; });
    return out;
}

std::vector<AnalysisResult::DeviceCheck> VixaAnalyzer::computeDeviceAudibility (const AnalysisResult& r) const
{
    std::vector<AnalysisResult::DeviceCheck> out;

    bool phoneOk = r.midEnergy > 0.25f && r.subEnergy < 0.7f;
    out.push_back ({ "Telefon", phoneOk, phoneOk ? "Bass bedzie slyszalny." : "Za malo srodka / za duzo subu - moze zanikac." });

    bool laptopOk = r.lowMidEnergy > 0.2f;
    out.push_back ({ "Laptop", laptopOk, laptopOk ? "OK." : "Dodaj energii w 150-400Hz." });

    bool clubOk = r.subEnergy > 0.2f && r.phaseCorrelation > 0.1f;
    out.push_back ({ "Glosniki klubowe", clubOk, clubOk ? "Solidny SUB, bezpieczna faza." : "Sprawdz SUB i korelacje fazowa." });

    bool headphonesOk = r.stereoWidth01 > 0.1f;
    out.push_back ({ "Sluchawki", headphonesOk, headphonesOk ? "Dobra przestrzennosc." : "Bass brzmi wasko - dodaj stereo." });

    return out;
}

juce::StringArray VixaAnalyzer::computeHelperTips (const AnalysisResult& r) const
{
    juce::StringArray tips;

    float subRatio = r.subEnergy / juce::jmax (0.0001f, r.subEnergy + r.bassEnergy);
    if (subRatio > 0.6f)
        tips.add ("Zmniejsz SUB o ok. " + juce::String ((int) ((subRatio - 0.4f) * 100)) + "%.");

    float h2345 = r.harmonicLevels[1] + r.harmonicLevels[2] + r.harmonicLevels[3];
    if (h2345 < 1.0f)
        tips.add ("Dodaj 7-12% saturacji, aby wzbogacic harmoniczne 2/3/4.");

    if (r.midEnergy < 0.25f)
        tips.add ("Brakuje harmonicznych pomiedzy 400-1000Hz - bass bedzie zanikal na telefonach.");

    if (r.stereoWidth01 < 0.15f)
        tips.add ("Dodaj 10-15% Chorus lub Stereo Widener, aby poszerzyc bass.");

    if (r.phaseCorrelation < 0.2f)
        tips.add ("Zmniejsz stereo o ok. 9% - wykryto ryzyko problemu fazowego w mono.");

    if (r.transientStrength < 0.25f)
        tips.add ("Zwieksz Punch w VIXA ENGINE lub dodaj Transient Designer dla mocniejszego ataku.");

    if (tips.isEmpty())
        tips.add ("Bass brzmi solidnie - mozesz eksperymentowac z automatyzacja filtra dla ruchu.");

    return tips;
}

} // namespace vixb
