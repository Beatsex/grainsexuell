#include "PluginProcessor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "state", grain::createLayout()),
      presets(apvts, grain::presets::PresetManager::defaultUserFolder()),
      bendParam(apvts.getRawParameterValue(grain::ids::bend)),
      hazeParam(apvts.getRawParameterValue(grain::ids::haze)),
      hazeModeParam(apvts.getRawParameterValue(grain::ids::hazemode)),
      duskParam(apvts.getRawParameterValue(grain::ids::dusk)),
      autoGainParam(apvts.getRawParameterValue(grain::ids::autogain)),
      holdParam(apvts.getRawParameterValue(grain::ids::hold)),
      loopParam(apvts.getRawParameterValue(grain::ids::loop)),
      loopSpeedParam(apvts.getRawParameterValue(grain::ids::loopspeed)),
      loopReverseParam(apvts.getRawParameterValue(grain::ids::loopreverse)),
      loopQuantParam(apvts.getRawParameterValue(grain::ids::loopquant)),
      loopBurstParam(apvts.getRawParameterValue(grain::ids::loopburst)),
      loopUndoParam(apvts.getRawParameterValue(grain::ids::loopundo)),
      loopPosParam(apvts.getRawParameterValue(grain::ids::looppos)),
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
    hold.prepare(sampleRate);
    looper.prepare(sampleRate); // 60 s stereo base + overdub layer
    mosaic.prepare(sampleRate);
    cloud.prepare(sampleRate);
    shard.prepare(sampleRate);
    tap.prepare(sampleRate);
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

    double bpm = 120.0; // no host tempo (standalone): fixed fallback
    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;

    autoGain.begin(ch, numCh, n, autoGainParam->load() > 0.5f);
    hold.process(ch, numCh, n, holdParam->load() > 0.5f, bpm); // adds the frozen loop to the engine input

    // the looper runs once per block, either here (Pre-FX: the engines process the loop) or at the end (Post-FX)
    const bool loopPre = static_cast<int>(loopPosParam->load() + 0.5f) == 0;
    const grain::dsp::Looper::Params loopParams { static_cast<int>(loopParam->load() + 0.5f),
                                                  static_cast<int>(loopSpeedParam->load() + 0.5f),
                                                  loopReverseParam->load() > 0.5f,
                                                  loopQuantParam->load() > 0.5f,
                                                  loopBurstParam->load() > 0.5f,
                                                  loopUndoParam->load() > 0.5f,
                                                  bpm };
    if (loopPre)
        looper.process(ch, numCh, n, loopParams);
    const float swarm = swarmParam->load(), tail = tailParam->load(), span = spanParam->load();
    const float scatter = scatterParam->load(), mix = mixParam->load();
    const int variant = static_cast<int>(variantParam->load() + 0.5f), family = static_cast<int>(familyParam->load() + 0.5f);
    mosaic.process(ch, numCh, n, { swarm, tail, span, scatter, mix, variant, family == 0 });
    cloud.process(ch, numCh, n, { swarm, tail, span, scatter, mix, variant, family == 1 });
    shard.process(ch, numCh, n, { swarm, tail, span, scatter, mix, variant, family == 2, bpm });
    tap.process(ch, numCh, n, { swarm, tail, span, scatter, mix, variant, family == 3, bpm });
    modDelay.process(ch, numCh, n, bendParam->load());
    reverb.process(ch, numCh, n, hazeParam->load(), static_cast<int>(hazeModeParam->load() + 0.5f));
    lowpass.process(ch, numCh, n, duskParam->load());
    autoGain.end(ch, numCh, n);
    if (! loopPre)
        looper.process(ch, numCh, n, loopParams); // records and plays the finished output
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this); // real GUI is Phase 10
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("stateVersion", 1, nullptr);
    state.setProperty("preset", presets.currentName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // a project file is untrusted input: a real state is a few KB, and entity declarations or deep nesting
    // would hurt the recursive XML parser
    constexpr int maxStateBytes = 1 << 20;
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > maxStateBytes
        || ! grain::presets::PresetManager::isSafeXml({ static_cast<const char*>(data), static_cast<size_t>(sizeInBytes) }))
        return;

    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml(*xml);
            apvts.replaceState(tree);
            presets.setCurrentName(tree.getProperty("preset").toString()); // states from before presets have none: ""
        }
}
