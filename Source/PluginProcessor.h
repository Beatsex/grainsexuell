#pragma once

#include "Parameters.h"
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

private:
    std::atomic<float>* bendParam;
    std::atomic<float>* hazeParam;
    std::atomic<float>* hazeModeParam;
    std::atomic<float>* duskParam;

    // signal path after the (not yet written) effect engine: pitch modulation -> reverb -> low-pass
    grain::dsp::ModDelay modDelay;
    grain::dsp::Reverb reverb;
    grain::dsp::Lowpass lowpass;
};
