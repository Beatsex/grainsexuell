#pragma once

#include "Parameters.h"
#include "midi/MidiMap.h"
#include "presets/PresetManager.h"
#include "dsp/AutoGain.h"
#include "dsp/DualEngine.h"
#include "dsp/Gain.h"
#include "dsp/Hold.h"
#include "dsp/Looper.h"
#include "dsp/Lowpass.h"
#include "dsp/ModDelay.h"
#include "dsp/Reverb.h"

#include <atomic>

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();

    void prepareToPlay(double sampleRate, int maxBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "grainsexuell"; }
    bool acceptsMidi() const override { return true; } // CC -> parameter (learn), see midi/MidiMap.h
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return grain::dsp::Reverb::maxT60; }

    // the host's preset menu lists the factory presets; user presets are saved and loaded through `presets`
    int getNumPrograms() override { return grain::presets::numFactory; }
    int getCurrentProgram() override { return std::max(0, presets.currentFactoryIndex()); }
    void setCurrentProgram(int index) override { presets.applyFactory(index); }
    const juce::String getProgramName(int index) override
    {
        return index >= 0 && index < grain::presets::numFactory ? juce::String(grain::presets::factory()[static_cast<size_t>(index)].name)
                                                                 : juce::String();
    }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    grain::presets::PresetManager presets; // message thread only (file I/O, host notifications)

    grain::midi::MidiMap midiMap; // CC learn / assign from the GUI thread; read on the audio thread
    std::atomic<int> editorWidth { 1040 }; // remembered in the state; the editor keeps a fixed aspect ratio

    // message thread (a timer calls it): applies CC values the audio thread queued and finishes a MIDI learn
    void applyPendingMidi();

    float autoGainDb() const { return autoGainShown.load(std::memory_order_relaxed); }
    int mosaicGrabCount() const { return engines.a.mosaic.grabCount(); }
    int cloudGrainCount() const { return engines.a.cloud.grainsSpawned(); }
    int shardSliceCount() const { return engines.a.shard.slicesPlayed(); }
    // GUI activity: every grab, grain, slice and tap repeat of both slots so far (inactive engines do not count)
    unsigned activityTotal() const
    {
        unsigned total = 0;
        for (const auto* set : { &engines.a, &engines.b })
            total += static_cast<unsigned>(set->mosaic.grabCount()) + static_cast<unsigned>(set->cloud.grainsSpawned())
                     + static_cast<unsigned>(set->shard.slicesPlayed()) + static_cast<unsigned>(set->tap.echoCount());
        return total;
    }

private:
    void loadMidiMap(const juce::ValueTree& map);

    std::array<juce::RangedAudioParameter*, grain::ids::mappable.size()> mappedParams {};
    std::atomic<float> autoGainShown { 0.0f };
    std::array<std::atomic<float>, grain::ids::mappable.size()> pendingCc; // normalised value from the audio thread, < 0 = none
    struct MidiPump : juce::Timer
    {
        explicit MidiPump(PluginProcessor& p) : owner(p) {}
        ~MidiPump() override { stopTimer(); }
        void timerCallback() override { owner.applyPendingMidi(); }
        PluginProcessor& owner;
    };

    std::atomic<float>* ingainParam;
    std::atomic<float>* outgainParam;
    std::atomic<float>* bendParam;
    std::atomic<float>* hazeParam;
    std::atomic<float>* hazeModeParam;
    std::atomic<float>* duskParam;
    std::atomic<float>* autoGainParam;
    std::atomic<float>* holdParam;
    std::atomic<float>* loopParam;
    std::atomic<float>* loopSpeedParam;
    std::atomic<float>* loopReverseParam;
    std::atomic<float>* loopQuantParam;
    std::atomic<float>* loopBurstParam;
    std::atomic<float>* loopUndoParam;
    std::atomic<float>* loopPosParam;
    std::atomic<float>* swarmParam;
    std::atomic<float>* tailParam;
    std::atomic<float>* spanParam;
    std::atomic<float>* scatterParam;
    std::atomic<float>* mixParam;
    std::atomic<float>* familyParam;
    std::atomic<float>* variantParam;
    std::atomic<float>* family2Param;
    std::atomic<float>* variant2Param;
    std::atomic<float>* mix2Param;
    std::atomic<float>* route2Param;

    grain::dsp::DualEngine::Params engineParams(double bpm) const;

    // auto gain wraps the chain; chain = effect engine (Mosaic, Cloud; other families later) -> pitch modulation -> reverb -> low-pass
    grain::dsp::Gain inGain, outGain; // first and last stage
    grain::dsp::AutoGain autoGain;
    grain::dsp::Hold hold;
    grain::dsp::Looper looper;
    grain::dsp::DualEngine engines; // slot A (Family) and the optional slot B (Family B), serial or parallel
    grain::dsp::ModDelay modDelay;
    grain::dsp::Reverb reverb;
    grain::dsp::Lowpass lowpass;
    MidiPump midiPump { *this }; // last member: stops first
};
