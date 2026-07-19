#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace vixb
{

//==============================================================================
// Panel wizualizacji: FFT + harmoniczne, odświeżany timerem z ostatniej analizy
//==============================================================================
class VixaVisualizerPanel : public juce::Component, private juce::Timer
{
public:
    explicit VixaVisualizerPanel (VixBasiorAudioProcessor& p) : processor (p)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff141419));
        g.fillRoundedRectangle (bounds, 8.0f);

        auto result = processor.getLatestAnalysis();
        if (result.spectrumMagnitudes.empty()) return;

        // --- Spektrum (log frequency) ---
        auto plotArea = bounds.reduced (10.0f);
        g.setColour (juce::Colour (0xff2a2a35));
        for (int i = 1; i < 5; ++i)
        {
            float x = plotArea.getX() + plotArea.getWidth() * (float) i / 5.0f;
            g.drawVerticalLine ((int) x, plotArea.getY(), plotArea.getBottom());
        }

        juce::Path path;
        int numBins = (int) result.spectrumMagnitudes.size();
        int usableBins = juce::jmin (numBins, 800); // ~20kHz zakres praktyczny
        for (int i = 0; i < usableBins; ++i)
        {
            float t = (float) i / (float) usableBins;
            float logT = std::log10 (1.0f + t * 9.0f); // log-ish scaling
            float x = plotArea.getX() + logT * plotArea.getWidth();
            float mag = juce::jlimit (0.0f, 1.0f, result.spectrumMagnitudes[(size_t) i]);
            float y = plotArea.getBottom() - mag * plotArea.getHeight();
            if (i == 0) path.startNewSubPath (x, plotArea.getBottom());
            path.lineTo (x, y);
        }
        path.lineTo (plotArea.getRight(), plotArea.getBottom());
        path.closeSubPath();

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7c4dff), plotArea.getX(), 0,
                                                   juce::Colour (0xff00e5c8), plotArea.getRight(), 0, false));
        g.fillPath (path);
    }

private:
    void timerCallback() override { repaint(); }
    VixBasiorAudioProcessor& processor;
};

//==============================================================================
// Panel BASS SCORE / VIXA QUALITY / VIXA HELPER
//==============================================================================
class VixaScorePanel : public juce::Component, private juce::Timer
{
public:
    explicit VixaScorePanel (VixBasiorAudioProcessor& p) : processor (p)
    {
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff141419));
        g.fillRoundedRectangle (bounds, 8.0f);

        auto result = processor.getLatestAnalysis();
        auto area = bounds.reduced (14.0f);

        // BASS SCORE
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        g.drawText ("BASS SCORE", area.removeFromTop (28.0f), juce::Justification::left);

        auto scoreColour = result.bassScore0to100 > 85 ? juce::Colour (0xff00e5c8)
                          : result.bassScore0to100 > 60 ? juce::Colour (0xfff5c542)
                                                          : juce::Colour (0xffff5566);
        g.setColour (scoreColour);
        g.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold)));
        g.drawText (juce::String (result.bassScore0to100) + "%", area.removeFromTop (50.0f), juce::Justification::left);

        area.removeFromTop (6.0f);
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        for (auto& note : result.bassScoreNotes)
        {
            g.drawText (juce::String::fromUTF8 ("- ") + note, area.removeFromTop (18.0f), juce::Justification::left);
        }

        area.removeFromTop (10.0f);
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        g.setColour (juce::Colours::white);
        g.drawText ("VIXA QUALITY", area.removeFromTop (22.0f), juce::Justification::left);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        for (auto& sm : result.vixaQuality)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawText (juce::String (sm.percent) + "%  " + sm.name, area.removeFromTop (17.0f), juce::Justification::left);
        }

        area.removeFromTop (10.0f);
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        g.drawText ("VIXA HELPER", area.removeFromTop (22.0f), juce::Justification::left);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.setColour (juce::Colour (0xffffcf5c));
        for (auto& tip : result.vixaHelperTips)
            g.drawText (juce::String::fromUTF8 ("* ") + tip, area.removeFromTop (17.0f), juce::Justification::left);
    }

private:
    void timerCallback() override { repaint(); }
    VixBasiorAudioProcessor& processor;
};

//==============================================================================
class VixBasiorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VixBasiorAudioProcessorEditor (VixBasiorAudioProcessor&);
    ~VixBasiorAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VixBasiorAudioProcessor& audioProcessor;

    VixaVisualizerPanel visualizer;
    VixaScorePanel scorePanel;

    juce::Slider oscAMixSlider, oscBMixSlider, subMixSlider;
    juce::Slider filterCutoffSlider, filterResonanceSlider;
    juce::Slider vixaAmountSlider;
    juce::ComboBox vixaStyleBox;
    juce::Slider chorusSlider, widthSlider, ottSlider;

    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment;

    void setupSlider (juce::Slider& s, const juce::String& paramID, const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VixBasiorAudioProcessorEditor)
};

} // namespace vixb
