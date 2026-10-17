#include "AllocGuard.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
constexpr int blockSize = 256;

void configure(PluginProcessor& p, const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(in);
    layout.outputBuses.add(out);
    REQUIRE(p.setBusesLayout(layout));
    p.setRateAndBufferSizeDetails(48000.0, blockSize);
    p.prepareToPlay(48000.0, blockSize);
}

float testSample(int ch, int i)
{
    return i == 0 ? 1.0f : 0.5f * std::sin(0.05f * static_cast<float>(i) + static_cast<float>(ch));
}
} // namespace

TEST_CASE("stereo in -> stereo out is bit-identical, zero latency")
{
    PluginProcessor p;
    configure(p, juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo());
    REQUIRE(p.getLatencySamples() == 0);

    juce::AudioBuffer<float> buffer(2, blockSize), expected;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample(ch, i, testSample(ch, i));
    expected.makeCopyOf(buffer);

    juce::MidiBuffer midi;
    p.processBlock(buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            REQUIRE(buffer.getSample(ch, i) == expected.getSample(ch, i));
}

TEST_CASE("mono in -> stereo out puts the signal on both channels")
{
    PluginProcessor p;
    configure(p, juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo());

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    for (int i = 0; i < blockSize; ++i)
        buffer.setSample(0, i, testSample(0, i));
    buffer.setSample(1, 3, 123.0f); // stale garbage JUCE may leave in channel 1

    juce::MidiBuffer midi;
    p.processBlock(buffer, midi);

    for (int i = 0; i < blockSize; ++i)
    {
        REQUIRE(buffer.getSample(0, i) == testSample(0, i));
        REQUIRE(buffer.getSample(1, i) == testSample(0, i));
    }
}

TEST_CASE("silence in = silence out")
{
    PluginProcessor p;
    configure(p, juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo());

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    juce::MidiBuffer midi;
    p.processBlock(buffer, midi);

    REQUIRE(buffer.getMagnitude(0, blockSize) == 0.0f);
}

TEST_CASE("processBlock does not allocate")
{
    PluginProcessor p;
    configure(p, juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo());

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    juce::MidiBuffer midi;

    allocguard::arm();
    for (int i = 0; i < 100; ++i)
        p.processBlock(buffer, midi);
    REQUIRE(allocguard::disarm() == 0);
}
