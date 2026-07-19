#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace vixb
{

//==============================================================================
VixVoice::VixVoice() {}

void VixVoice::prepare (double sr)
{
    sampleRate = sr;
    oscA.prepare (sr); oscB.prepare (sr); oscC.prepare (sr); oscSub.prepare (sr);
    filter.prepare (sr);
    ampEnv.prepare (sr);
    filterEnv.prepare (sr);
}

void VixVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    juce::ignoreUnused (velocity);
    baseFrequency = (float) juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    oscA.reset(); oscB.reset(); oscC.reset(); oscSub.reset();

    oscA.setFrequency (baseFrequency);
    oscB.setFrequency (baseFrequency);
    oscC.setFrequency (baseFrequency);
    oscSub.setFrequency (baseFrequency * 0.5f);

    ampEnv.setParams (params.ampAttackMs, params.ampDecayMs, params.ampSustain, params.ampReleaseMs);
    filterEnv.setParams (params.filtAttackMs, params.filtDecayMs, params.filtSustain, params.filtReleaseMs);
    ampEnv.noteOn();
    filterEnv.noteOn();
}

void VixVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        filterEnv.noteOff();
    }
    else
    {
        clearCurrentNote();
    }
}

void VixVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! ampEnv.isActive() && ! isKeyDown())
    {
        // pozwól ogonowi wybrzmieć jeśli release trwa, inaczej wyjdź
    }

    oscA.setMode (params.oscAMode);
    oscB.setMode (params.oscBMode);
    oscB.setDetuneCents (params.oscBDetuneCents);
    oscC.setMode (params.oscCMode);
    oscC.setDetuneCents (params.oscCDetuneCents);

    saturator.setDrive (params.saturationDrive + params.vixaTargets.saturationAmt);

    for (int i = 0; i < numSamples; ++i)
    {
        float a = oscA.renderSample() * params.oscAMix;
        float b = oscB.renderSample() * params.oscBMix;
        float c = oscC.renderSample() * params.oscCMix;
        float sub = oscSub.renderSample() * (params.subMix + params.vixaTargets.subLevel);
        float noise = params.noiseMix > 0.0001f ? ((juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * params.noiseMix) : 0.0f;

        float mixed = a + b + c + sub + noise;

        float fEnv = filterEnv.nextSample();
        float cutoff = params.filterCutoffHz * params.vixaTargets.filterCutoffMul
                       * (1.0f + params.filterEnvAmount * fEnv);
        cutoff = juce::jlimit (20.0f, 18000.0f, cutoff);

        filter.setParams (cutoff, params.filterResonance,
                           juce::jlimit (0.0f, 1.0f, params.filterCharacter + params.vixaTargets.filterCharacter));
        float filtered = filter.process (mixed);

        filtered = saturator.process (filtered);

        float aEnv = ampEnv.nextSample();
        float out = filtered * aEnv * (0.6f + params.vixaTargets.transientPunch * 0.4f);

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
        }

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample (ch, startSample + i, out);
    }
}

//==============================================================================
VixBasiorAudioProcessor::VixBasiorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new VixVoice());
    synth.addSound (new VixSound());
}

VixBasiorAudioProcessor::~VixBasiorAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout VixBasiorAudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P> (juce::ParameterID { "oscAMix", 1 }, "Osc A Mix", Range (0.0f, 1.0f), 0.8f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "oscBMix", 1 }, "Osc B Mix", Range (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "oscCMix", 1 }, "Osc C Mix", Range (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "subMix", 1 }, "Sub Mix", Range (0.0f, 1.0f), 0.9f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "noiseMix", 1 }, "Noise Mix", Range (0.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "oscBDetune", 1 }, "Osc B Detune", Range (-50.0f, 50.0f), 7.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "oscCDetune", 1 }, "Osc C Detune", Range (-50.0f, 50.0f), -7.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "filterCutoff", 1 }, "Filter Cutoff", Range (20.0f, 18000.0f, 0.0f, 0.3f), 1200.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "filterResonance", 1 }, "Filter Resonance", Range (0.0f, 1.0f), 0.25f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "filterEnvAmount", 1 }, "Filter Env Amount", Range (-1.0f, 1.0f), 0.4f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "ampAttack", 1 }, "Amp Attack", Range (0.0f, 2000.0f, 0.0f, 0.3f), 3.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "ampDecay", 1 }, "Amp Decay", Range (0.0f, 4000.0f, 0.0f, 0.3f), 120.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "ampSustain", 1 }, "Amp Sustain", Range (0.0f, 1.0f), 0.85f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "ampRelease", 1 }, "Amp Release", Range (0.0f, 4000.0f, 0.0f, 0.3f), 200.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "saturationDrive", 1 }, "Saturation", Range (0.0f, 1.0f), 0.2f));

    // VIXA ENGINE
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "vixaStyle", 1 }, "Vixa Style",
        juce::StringArray { "Punch", "Bounce", "Dance", "Soft", "Club", "Wide", "Majki", "Wojtuli", "Pumpsound", "Xsound", "Custom" }, 2));
    params.push_back (std::make_unique<P> (juce::ParameterID { "vixaAmount", 1 }, "Vixa Amount", Range (0.0f, 1.0f), 0.5f));

    // Efekty globalne
    params.push_back (std::make_unique<P> (juce::ParameterID { "chorusMix", 1 }, "Chorus Mix", Range (0.0f, 1.0f), 0.2f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "stereoWidth", 1 }, "Stereo Width", Range (0.0f, 2.0f), 1.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "ottDepth", 1 }, "OTT Depth", Range (0.0f, 1.0f), 0.0f));

    return { params.begin(), params.end() };
}

void VixBasiorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<VixVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);

    analyzer.prepare (sampleRate);
    chorus.prepare (sampleRate);
    widener.prepare (sampleRate);
    ott.prepare (sampleRate);
}

void VixBasiorAudioProcessor::updateVoiceParamsFromAPVTS()
{
    VixVoice::VoiceParams p;
    p.oscAMix = apvts.getRawParameterValue ("oscAMix")->load();
    p.oscBMix = apvts.getRawParameterValue ("oscBMix")->load();
    p.oscCMix = apvts.getRawParameterValue ("oscCMix")->load();
    p.subMix  = apvts.getRawParameterValue ("subMix")->load();
    p.noiseMix = apvts.getRawParameterValue ("noiseMix")->load();
    p.oscBDetuneCents = apvts.getRawParameterValue ("oscBDetune")->load();
    p.oscCDetuneCents = apvts.getRawParameterValue ("oscCDetune")->load();
    p.filterCutoffHz = apvts.getRawParameterValue ("filterCutoff")->load();
    p.filterResonance = apvts.getRawParameterValue ("filterResonance")->load();
    p.filterEnvAmount = apvts.getRawParameterValue ("filterEnvAmount")->load();
    p.ampAttackMs = apvts.getRawParameterValue ("ampAttack")->load();
    p.ampDecayMs  = apvts.getRawParameterValue ("ampDecay")->load();
    p.ampSustain  = apvts.getRawParameterValue ("ampSustain")->load();
    p.ampReleaseMs= apvts.getRawParameterValue ("ampRelease")->load();
    p.saturationDrive = apvts.getRawParameterValue ("saturationDrive")->load();

    int styleIdx = (int) apvts.getRawParameterValue ("vixaStyle")->load();
    vixaEngine.setStyle (static_cast<VixaStyle> (styleIdx));
    vixaEngine.setAmount (apvts.getRawParameterValue ("vixaAmount")->load());
    p.vixaTargets = vixaEngine.computeTargets();

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<VixVoice*> (synth.getVoice (i)))
            v->params = p;
}

void VixBasiorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    updateVoiceParamsFromAPVTS();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    float chorusMix = apvts.getRawParameterValue ("chorusMix")->load();
    float width = apvts.getRawParameterValue ("stereoWidth")->load();
    float ottDepth = apvts.getRawParameterValue ("ottDepth")->load();

    chorus.setParams (0.6f, 4.0f, chorusMix);
    widener.setWidth (width);
    ott.setDepth (ottDepth);

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float l = left[i], r = right[i];
        if (ottDepth > 0.001f) { l = ott.process (l); r = ott.process (r); }
        chorus.process (l, r);
        widener.process (l, r);
        left[i] = l; right[i] = r;
    }

    // Analiza ostatniego bufora - do wyświetlenia w GUI (Bass Score / Vixa Analyzer)
    auto result = analyzer.analyze (left, right, buffer.getNumSamples());
    {
        juce::ScopedLock sl (analysisLock);
        latestAnalysis = std::move (result);
    }
}

AnalysisResult VixBasiorAudioProcessor::getLatestAnalysis()
{
    juce::ScopedLock sl (analysisLock);
    return latestAnalysis;
}

juce::AudioProcessorEditor* VixBasiorAudioProcessor::createEditor()
{
    return new VixBasiorAudioProcessorEditor (*this);
}

void VixBasiorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void VixBasiorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace vixb

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vixb::VixBasiorAudioProcessor();
}
