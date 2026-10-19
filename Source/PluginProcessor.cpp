#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "state", grain::createLayout()),
      presets(apvts, grain::presets::PresetManager::defaultUserFolder()),
      ingainParam(apvts.getRawParameterValue(grain::ids::ingain)),
      outgainParam(apvts.getRawParameterValue(grain::ids::outgain)),
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
      variantParam(apvts.getRawParameterValue(grain::ids::variant)),
      family2Param(apvts.getRawParameterValue(grain::ids::family2)),
      variant2Param(apvts.getRawParameterValue(grain::ids::variant2)),
      mix2Param(apvts.getRawParameterValue(grain::ids::mix2)),
      route2Param(apvts.getRawParameterValue(grain::ids::route2))
{
    for (size_t i = 0; i < mappedParams.size(); ++i)
    {
        mappedParams[i] = apvts.getParameter(grain::ids::mappable[i]);
        pendingCc[i].store(-1.0f);
    }
    // CCs are applied on the message thread (hosts expect parameter edits there); no message loop (tests): no pump
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        midiPump.startTimerHz(100);
}

void PluginProcessor::applyPendingMidi()
{
    midiMap.pollLearn();
    for (size_t i = 0; i < mappedParams.size(); ++i)
        if (const float v = pendingCc[i].exchange(-1.0f, std::memory_order_acq_rel); v >= 0.0f && mappedParams[i] != nullptr)
            mappedParams[i]->setValueNotifyingHost(v);
}

void PluginProcessor::prepareToPlay(double sampleRate, int)
{
    inGain.prepare(sampleRate);
    outGain.prepare(sampleRate);
    autoGain.prepare(sampleRate);
    hold.prepare(sampleRate);
    looper.prepare(sampleRate); // 60 s stereo base + overdub layer
    engines.prepare(sampleRate, engineParams(120.0));
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

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // CC -> parameter: only queue the value here; applyPendingMidi() (message thread, 100 Hz timer) applies it
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (! message.isController())
            continue;
        if (const auto hit = midiMap.onCc(message.getControllerNumber(), message.getControllerValue()))
            if (hit->param >= 0 && static_cast<size_t>(hit->param) < pendingCc.size())
                pendingCc[static_cast<size_t>(hit->param)].store(hit->normalised, std::memory_order_release);
    }

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

    inGain.process(ch, numCh, n, ingainParam->load()); // first stage: also drives Auto Gain's measurement

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
    engines.process(ch, numCh, n, engineParams(bpm));
    modDelay.process(ch, numCh, n, bendParam->load());
    reverb.process(ch, numCh, n, hazeParam->load(), static_cast<int>(hazeModeParam->load() + 0.5f));
    lowpass.process(ch, numCh, n, duskParam->load());
    autoGain.end(ch, numCh, n);
    if (! loopPre)
        looper.process(ch, numCh, n, loopParams); // records and plays the finished output
    outGain.process(ch, numCh, n, outgainParam->load()); // last stage (the looper has recorded before it)
    autoGainShown.store(autoGain.gainDb(), std::memory_order_relaxed);
}

grain::dsp::DualEngine::Params PluginProcessor::engineParams(double bpm) const
{
    const auto index = [](const std::atomic<float>* prm) { return static_cast<int>(prm->load() + 0.5f); };
    grain::dsp::DualEngine::Params p;
    p.a = { swarmParam->load(), tailParam->load(), spanParam->load(), scatterParam->load(), mixParam->load(),
            index(familyParam), index(variantParam), bpm };
    p.b = p.a; // B shares the macros; it has its own family, variant and mix
    const int familyB = index(family2Param) - 1; // -1 = Off
    p.bOn = familyB >= 0;
    p.b.family = std::max(0, familyB);
    p.b.variant = index(variant2Param);
    p.b.mix = mix2Param->load();
    p.route = index(route2Param);
    return p;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("stateVersion", 1, nullptr);
    state.setProperty("preset", presets.currentName(), nullptr);
    state.setProperty("editorWidth", editorWidth.load(), nullptr);

    juce::ValueTree map("MidiMap");
    for (size_t i = 0; i < grain::ids::mappable.size(); ++i)
        if (const int cc = midiMap.ccFor(static_cast<int>(i)); cc >= 0)
            map.appendChild(juce::ValueTree("CC", { { "id", grain::ids::mappable[i] }, { "cc", cc } }), nullptr);
    state.appendChild(map, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::loadMidiMap(const juce::ValueTree& map)
{
    midiMap.clearAll();
    for (const auto& e : map)
    {
        const auto id = e.getProperty("id").toString();
        const int cc = e.getProperty("cc", -1);
        for (size_t i = 0; i < grain::ids::mappable.size(); ++i)
            if (id == grain::ids::mappable[i] && cc >= 0 && cc < grain::midi::MidiMap::numCc)
                midiMap.assign(cc, static_cast<int>(i));
    }
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
            auto tree = juce::ValueTree::fromXml(*xml);
            loadMidiMap(tree.getChildWithName("MidiMap"));
            if (auto map = tree.getChildWithName("MidiMap"); map.isValid())
                tree.removeChild(map, nullptr); // not a parameter node
            editorWidth = std::clamp(static_cast<int>(tree.getProperty("editorWidth", 1040)), 480, 2400);
            apvts.replaceState(tree);
            presets.setCurrentName(tree.getProperty("preset").toString()); // states from before presets have none: ""
        }
}
