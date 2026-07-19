#include "PluginEditor.h"

namespace vixb
{

VixBasiorAudioProcessorEditor::VixBasiorAudioProcessorEditor (VixBasiorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      visualizer (p), scorePanel (p)
{
    setupSlider (oscAMixSlider, "oscAMix", "OSC A");
    setupSlider (oscBMixSlider, "oscBMix", "OSC B");
    setupSlider (subMixSlider, "subMix", "SUB");
    setupSlider (filterCutoffSlider, "filterCutoff", "CUTOFF");
    setupSlider (filterResonanceSlider, "filterResonance", "RESO");
    setupSlider (vixaAmountSlider, "vixaAmount", "VIXA AMOUNT");
    setupSlider (chorusSlider, "chorusMix", "CHORUS");
    setupSlider (widthSlider, "stereoWidth", "WIDTH");
    setupSlider (ottSlider, "ottDepth", "OTT");

    vixaStyleBox.addItemList ({ "Punch", "Bounce", "Dance", "Soft", "Club", "Wide",
                                 "Majki", "Wojtuli", "Pumpsound", "Xsound", "Custom" }, 1);
    addAndMakeVisible (vixaStyleBox);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "vixaStyle", vixaStyleBox);

    addAndMakeVisible (visualizer);
    addAndMakeVisible (scorePanel);

    setResizable (true, true);
    setResizeLimits (900, 560, 1800, 1120);
    setSize (1200, 720);
}

void VixBasiorAudioProcessorEditor::setupSlider (juce::Slider& s, const juce::String& paramID, const juce::String& labelText)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff7c4dff));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a2a35));
    s.setColour (juce::Slider::thumbColourId, juce::Colour (0xff00e5c8));
    addAndMakeVisible (s);

    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
    label->attachToComponent (&s, false);
    addAndMakeVisible (*label);
    labels.push_back (std::move (label));

    attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, paramID, s));
}

void VixBasiorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c24));

    auto bounds = getLocalBounds();
    g.setColour (juce::Colour (0xff0f0f13));
    g.fillRect (bounds.removeFromTop (46));

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText ("VIXBASIOR", 16, 0, 300, 46, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff7c4dff));
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("Vixiarski Bass Synthesizer", 190, 0, 300, 46, juce::Justification::centredLeft);
}

void VixBasiorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (46); // header

    auto rightPanel = bounds.removeFromRight (340).reduced (12);
    scorePanel.setBounds (rightPanel);

    auto topArea = bounds.removeFromTop (bounds.getHeight() * 2 / 5).reduced (12);
    visualizer.setBounds (topArea);

    auto knobArea = bounds.reduced (12);
    int knobW = 100, knobH = 100;
    int gap = 10;
    auto row1 = knobArea.removeFromTop (knobH + 24);
    juce::Slider* row1Sliders[] = { &oscAMixSlider, &oscBMixSlider, &subMixSlider,
                                     &filterCutoffSlider, &filterResonanceSlider };
    int x = row1.getX();
    for (auto* s : row1Sliders)
    {
        s->setBounds (x, row1.getY() + 20, knobW, knobH);
        x += knobW + gap;
    }

    knobArea.removeFromTop (10);
    auto row2 = knobArea.removeFromTop (knobH + 24);
    vixaStyleBox.setBounds (row2.getX(), row2.getY() + 40, 160, 26);
    x = row2.getX() + 170;
    juce::Slider* row2Sliders[] = { &vixaAmountSlider, &chorusSlider, &widthSlider, &ottSlider };
    for (auto* s : row2Sliders)
    {
        s->setBounds (x, row2.getY() + 20, knobW, knobH);
        x += knobW + gap;
    }
}

} // namespace vixb
