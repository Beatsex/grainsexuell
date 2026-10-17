#pragma once

#include "Parameters.h"
#include "dsp/AutoGain.h"
#include "dsp/Cloud.h"
#include "dsp/Lowpass.h"
#include "dsp/ModDelay.h"
#include "dsp/Mosaic.h"
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
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return grain::dsp::Reverb::maxT60; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float autoGainDb() const { return autoGain.gainDb(); } // tests; GUI meter later (make atomic then)
    int mosaicGrabCount() const { return mosaic.grabCount(); }
    int cloudGrainCount() const { return cloud.grainsSpawned(); }

private:
    std::atomic<float>* bendParam;
    std::atomic<float>* hazeParam;
    std::atomic<float>* hazeModeParam;
    std::atomic<float>* duskParam;
    std::atomic<float>* autoGainParam;
    std::atomic<float>* swarmParam;
    std::atomic<float>* tailParam;
    std::atomic<float>* spanParam;
    std::atomic<float>* scatterParam;
    std::atomic<float>* mixParam;
    std::atomic<float>* familyParam;
    std::atomic<float>* variantParam;

    // auto gain wraps the chain; chain = effect engine (Mosaic, Cloud; other families later) -> pitch modulation -> reverb -> low-pass
    grain::dsp::AutoGain autoGain;
    grain::dsp::Mosaic mosaic;
    grain::dsp::Cloud cloud;
    grain::dsp::ModDelay modDelay;
    grain::dsp::Reverb reverb;
    grain::dsp::Lowpass lowpass;
};
