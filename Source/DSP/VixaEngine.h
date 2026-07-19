#pragma once
#include <juce_core/juce_core.h>

namespace vixb
{

enum class VixaStyle
{
    Punch, Bounce, Dance, Soft, Club, Wide,
    Majki, Wojtuli, Pumpsound, Xsound, Custom
};

// Docelowe wartości parametrów, do których VixaEngine "ciągnie" brzmienie
// gdy pokrętło Amount rośnie od 0 do 1. To jest serce unikalnej funkcji pluginu:
// jedno pokrętło zmienia jednocześnie harmoniczne / filtr / stereo / saturację / transjenty.
struct VixaTargets
{
    float harmonicDrive   = 0.0f; // ile "Harmonic Generator" dokłada 2/3 harmonicznej
    float filterCharacter = 0.0f; // parametr "character" VixaFilter (formant + sat)
    float stereoWidth     = 0.0f; // 0..2, 1 = mono, >1 = szerzej
    float saturationAmt   = 0.0f;
    float transientPunch  = 0.0f; // wzmocnienie ataku (transient designer)
    float subLevel        = 0.0f; // ile dokładamy sub oscylatora
    float filterCutoffMul = 1.0f; // mnożnik cutoff (np. Bounce = przyciemniony, Club = jasny)
};

class VixaEngine
{
public:
    void setStyle (VixaStyle s) { style = s; }
    void setAmount (float amt) { amount = juce::jlimit (0.0f, 1.0f, amt); }

    VixaTargets computeTargets() const
    {
        VixaTargets base = presetFor (style);
        VixaTargets out;
        out.harmonicDrive   = base.harmonicDrive   * amount;
        out.filterCharacter = base.filterCharacter * amount;
        out.stereoWidth     = 1.0f + (base.stereoWidth - 1.0f) * amount;
        out.saturationAmt   = base.saturationAmt   * amount;
        out.transientPunch  = base.transientPunch  * amount;
        out.subLevel        = base.subLevel        * amount;
        out.filterCutoffMul = 1.0f + (base.filterCutoffMul - 1.0f) * amount;
        return out;
    }

private:
    static VixaTargets presetFor (VixaStyle s)
    {
        // Wartości dobrane "ręcznie" pod charakterystykę stylów wymienionych przez usera.
        // To jest punkt startowy do dalszego strojenia słuchem (sound design).
        switch (s)
        {
            case VixaStyle::Punch:     return { 0.6f, 0.3f, 1.1f, 0.35f, 0.9f, 0.4f, 1.05f };
            case VixaStyle::Bounce:    return { 0.5f, 0.4f, 1.15f, 0.3f, 0.8f, 0.5f, 0.85f };
            case VixaStyle::Dance:     return { 0.55f, 0.35f, 1.3f, 0.25f, 0.5f, 0.4f, 1.0f };
            case VixaStyle::Soft:      return { 0.25f, 0.15f, 1.2f, 0.15f, 0.2f, 0.6f, 0.8f };
            case VixaStyle::Club:      return { 0.7f, 0.5f, 1.05f, 0.4f, 0.6f, 0.7f, 1.15f };
            case VixaStyle::Wide:      return { 0.4f, 0.3f, 1.6f, 0.2f, 0.3f, 0.3f, 1.0f };
            case VixaStyle::Majki:     return { 0.65f, 0.55f, 1.25f, 0.35f, 0.7f, 0.55f, 1.05f };
            case VixaStyle::Wojtuli:   return { 0.6f, 0.45f, 1.2f, 0.3f, 0.6f, 0.5f, 0.95f };
            case VixaStyle::Pumpsound: return { 0.5f, 0.4f, 1.15f, 0.3f, 0.85f, 0.45f, 1.1f };
            case VixaStyle::Xsound:    return { 0.55f, 0.4f, 1.2f, 0.28f, 0.55f, 0.5f, 1.0f };
            case VixaStyle::Custom:    return { 0.5f, 0.35f, 1.15f, 0.25f, 0.5f, 0.45f, 1.0f };
            default:                   return {};
        }
    }

    VixaStyle style = VixaStyle::Dance;
    float amount = 0.0f;
};

} // namespace vixb
