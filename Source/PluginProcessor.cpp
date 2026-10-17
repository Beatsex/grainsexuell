#include "PluginProcessor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "state", grain::createLayout()),
      bendParam(apvts.getRawParameterValue(grain::ids::bend)),
      hazeParam(apvts.getRawParameterValue(grain::ids::haze)),
      hazeModeParam(apvts.getRawParameterValue(grain::ids::hazemode)),
      duskParam(apvts.getRawParameterValue(grain::ids::dusk)),
      autoGainParam(apvts.getRawParameterValue(grain::ids::autogain)),
      swarmParam(apvts.getRawParameterValue(grain::ids::swarm)),
      tailParam(apvts.getRawParameterValue(grain::ids::tail)),
      spanParam(apvts.getRawParameterValue(grain::ids::span)),
      scatterParam(apvts.getRawParameterValue(grain::ids::scatter)),
      mixParam(apvts.getRawParameterValue(grain::ids::mix)),
      familyParam(apvts.getRawParameterValue(grain::ids::family)),
      variantParam(apvts.getRawParameterValue(grain::ids::variant))
{
}

void PluginProcessor::prepareToPlay(double sampleRate, int)
{
    autoGain.prepare(sampleRate);
    mosaic.prepare(sampleRate);
    cloud.prepare(sampleRate);
    modDelay.prepare(sampleRate);
    reverb.prepare(sampleRate);
    lowpass.prepare(sampleRate);
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    // mono or stereo in -> stereo out, or mono -> mono
    return (out == juce::AudioChannelSet::stereo() && (in == juce::AudioChannelSet::mono() || in == out))
           || (out == juce::AudioChannelSet::mono() && in == out);
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Mono in on a stereo bus: JUCE leaves channel 1 undefined, so fan out. (Effect engine: Phase 3+.)
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() == 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());

    const int numCh = std::min(buffer.getNumChannels(), 2);
    if (numCh == 0)
        return;

    auto* const* ch = buffer.getArrayOfWritePointers();
    const int n = buffer.getNumSamples();

    // One NaN/Inf (or a finite sample so large its square overflows) would otherwise live on forever in the
    // filter, reverb and level-detector state. +/-1e4 is +80 dBFS: never real audio.
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i)
            ch[c][i] = std::isfinite(ch[c][i]) ? std::clamp(ch[c][i], -1.0e4f, 1.0e4f) : 0.0f;

    autoGain.begin(ch, numCh, n, autoGainParam->load() > 0.5f);
    const float swarm = swarmParam->load(), tail = tailParam->load(), span = spanParam->load();
    const float scatter = scatterParam->load(), mix = mixParam->load();
    const int variant = static_cast<int>(variantParam->load() + 0.5f), family = static_cast<int>(familyParam->load() + 0.5f);
    mosaic.process(ch, numCh, n, { swarm, tail, span, scatter, mix, variant, family == 0 });
    cloud.process(ch, numCh, n, { swarm, tail, span, scatter, mix, variant, family == 1 });
    modDelay.process(ch, numCh, n, bendParam->load());
    reverb.process(ch, numCh, n, hazeParam->load(), static_cast<int>(hazeModeParam->load() + 0.5f));
    lowpass.process(ch, numCh, n, duskParam->load());
    autoGain.end(ch, numCh, n);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this); // real GUI is Phase 10
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
