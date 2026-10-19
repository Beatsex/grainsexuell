#include "PluginProcessor.h"
#include "dsp/Tap.h"
#include "gui/ActivityTrace.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

TEST_CASE("ActivityTrace: empty, per-second sum, bars, counter restarts")
{
    grain::gui::ActivityTrace t;
    REQUIRE(t.perSecond() == 0);
    for (int i = 0; i < grain::gui::ActivityTrace::numBars; ++i)
        REQUIRE(t.bar(i) == 0.0f);
    REQUIRE(t.bar(-1) == 0.0f);
    REQUIRE(t.bar(1000) == 0.0f);

    t.push(500); // the first total only sets the baseline
    REQUIRE(t.perSecond() == 0);

    unsigned total = 500;
    for (int i = 0; i < 30; ++i) // 2 per tick for 2 s
        t.push(total += 2);
    REQUIRE(t.perSecond() == 2 * grain::gui::ActivityTrace::ticksPerSecond);
    REQUIRE(t.bar(grain::gui::ActivityTrace::numBars - 1) == 1.0f); // newest at the peak
    REQUIRE(t.bar(0) == 0.0f);                                      // oldest still empty

    t.push(total += 8); // a burst: the newest bar is the peak, the others scale down
    REQUIRE(t.bar(grain::gui::ActivityTrace::numBars - 1) == 1.0f);
    REQUIRE(t.bar(grain::gui::ActivityTrace::numBars - 2) == 0.25f);

    t.push(3); // the host re-prepared: counters restart, no giant jump
    REQUIRE(t.bar(grain::gui::ActivityTrace::numBars - 1) == 0.0f);
    t.push(5);
    REQUIRE(t.bar(grain::gui::ActivityTrace::numBars - 1) > 0.0f);
}

TEST_CASE("Tap counts one repeat per audible tap per step")
{
    constexpr double sr = 48000.0;
    grain::dsp::Tap tap;
    tap.prepare(sr);
    grain::dsp::Tap::Params p;
    p.swarm = 0.5f; // 1/4 triplet: 1/3 s at 120 BPM
    p.span = 0.0f;  // taps at 1, 2, 3, 4 steps: all within 2 s, all audible
    p.mix = 0.5f;
    p.bpm = 120.0;

    std::vector<float> l(512), r(512);
    for (int block = 0; block < 4 * 48000 / 512; ++block) // 4 s
    {
        for (int i = 0; i < 512; ++i)
            l[static_cast<size_t>(i)] = r[static_cast<size_t>(i)] = 0.1f * std::sin(0.01f * static_cast<float>(block * 512 + i));
        float* ch[] { l.data(), r.data() };
        tap.process(ch, 2, 512, p);
    }
    // 4 taps x 3 steps/s x 4 s = 48 (the last partial step may be missing)
    REQUIRE(tap.echoCount() >= 46);
    REQUIRE(tap.echoCount() <= 48);

    grain::dsp::Tap off;
    off.prepare(sr);
    p.enabled = false; // not heard: no activity
    float* ch[] { l.data(), r.data() };
    for (int block = 0; block < 100; ++block)
        off.process(ch, 2, 512, p);
    REQUIRE(off.echoCount() == 0);
}

TEST_CASE("the processor's activity total grows while an engine plays and stands still at Mix 0")
{
    PluginProcessor p;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(p.setBusesLayout(layout));
    p.setRateAndBufferSizeDetails(48000.0, 512);
    p.prepareToPlay(48000.0, 512);
    p.apvts.getParameter(grain::ids::family)->setValueNotifyingHost(1.0f / 3.0f); // Cloud

    const auto play = [&p](int blocks) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i)
                buffer.setSample(0, i, 0.2f * std::sin(0.03f * static_cast<float>(i))), buffer.setSample(1, i, buffer.getSample(0, i));
            p.processBlock(buffer, midi);
        }
    };
    play(94); // 1 s
    const unsigned afterOne = p.activityTotal();
    REQUIRE(afterOne > 0);

    p.apvts.getParameter(grain::ids::mix)->setValueNotifyingHost(0.0f);
    play(20); // let the mix fade out
    const unsigned settled = p.activityTotal();
    play(94);
    REQUIRE(p.activityTotal() == settled);
}
