#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/AutoGain.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using grain::dsp::AutoGain;

namespace
{
constexpr double sr = 48000.0;

// Sine whose RMS is rmsDb dBFS (peak = RMS * sqrt 2).
std::vector<float> sineRms(double rmsDb, double seconds, double hz = 440.0)
{
    const double amp = std::pow(10.0, rmsDb / 20.0) * std::sqrt(2.0);
    std::vector<float> v(static_cast<size_t>(seconds * sr));
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = static_cast<float>(amp * std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / sr));
    return v;
}

double rmsDb(const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < to; ++i)
        s += static_cast<double>(v[i]) * v[i];
    return 10.0 * std::log10(s / static_cast<double>(to - from) + 1e-20);
}

// Runs begin() then end() (nothing between) in mono blocks; returns the output.
std::vector<float> run(AutoGain& ag, std::vector<float> x, int block, bool enabled)
{
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
        const int n = static_cast<int>(std::min(x.size() - pos, static_cast<size_t>(block)));
        float* ch[] { x.data() + pos };
        ag.begin(ch, 1, n, enabled);
        ag.end(ch, 1, n);
    }
    return x;
}

// Runs only begin() (what the chain would see).
std::vector<float> runBegin(AutoGain& ag, std::vector<float> x, int block, bool enabled)
{
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
        const int n = static_cast<int>(std::min(x.size() - pos, static_cast<size_t>(block)));
        float* ch[] { x.data() + pos };
        ag.begin(ch, 1, n, enabled);
    }
    return x;
}

void setParam(PluginProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(plain));
}

void configure(PluginProcessor& p, int block)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(p.setBusesLayout(layout));
    p.setRateAndBufferSizeDetails(sr, block);
    p.prepareToPlay(sr, block);
}
} // namespace

TEST_CASE("auto gain off: bit-exact, gain stays 0 dB")
{
    AutoGain ag;
    ag.prepare(sr);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> d(-0.9f, 0.9f);
    std::vector<float> x(48000);
    for (auto& s : x)
        s = d(rng);
    REQUIRE(run(ag, x, 480, false) == x);
    REQUIRE(ag.gainDb() == 0.0f);
}

TEST_CASE("auto gain: quiet source is raised, hot source lowered, clamped to +/-12 dB")
{
    {
        AutoGain ag;
        ag.prepare(sr);
        runBegin(ag, sineRms(-36.0, 10.0), 512, true);
        REQUIRE(ag.gainDb() >= 10.4f); // desired +12, stops inside the 1.5 dB dead zone
        REQUIRE(ag.gainDb() <= 12.0f);
    }
    {
        AutoGain ag;
        ag.prepare(sr);
        runBegin(ag, sineRms(-6.0, 6.0), 512, true);
        REQUIRE(ag.gainDb() <= -10.4f); // desired -12
        REQUIRE(ag.gainDb() >= -12.0f);
    }
    {
        AutoGain ag; // mid level: the chain sees about -18 dBFS RMS
        ag.prepare(sr);
        const auto seen = runBegin(ag, sineRms(-24.0, 10.0), 512, true);
        REQUIRE(std::abs(rmsDb(seen, seen.size() - 24000, seen.size()) - AutoGain::targetDb) < 2.0);
    }
}

TEST_CASE("auto gain: holds on silence, does not hunt inside the dead zone")
{
    AutoGain ag;
    ag.prepare(sr);
    runBegin(ag, sineRms(-24.0, 10.0), 512, true);
    const float settled = ag.gainDb();
    REQUIRE(settled > 3.0f);

    runBegin(ag, std::vector<float>(5 * 48000, 0.0f), 512, true);
    // silence: gain holds. It moves for ~40 ms until the fast detector sees the signal stopped (was +6 dB
    // when only the slow detector was used, because its tail reads as "input got quieter").
    REQUIRE(std::abs(ag.gainDb() - settled) < 0.2f);
    const float afterStop = ag.gainDb();
    runBegin(ag, std::vector<float>(5 * 48000, 0.0f), 512, true);
    REQUIRE(ag.gainDb() == afterStop); // and then does not move at all

    // steady level near the target: gain stays put
    AutoGain steady;
    steady.prepare(sr);
    runBegin(steady, sineRms(-17.0, 10.0), 512, true);
    const float g = steady.gainDb();
    runBegin(steady, sineRms(-17.0, 5.0), 512, true);
    REQUIRE(std::abs(steady.gainDb() - g) < 0.01f);
}

TEST_CASE("auto gain: begin+end is transparent and block size does not matter")
{
    const auto x = sineRms(-30.0, 8.0);
    float final64 = 0, final4096 = 0;
    for (int block : { 64, 4096 })
    {
        AutoGain ag;
        ag.prepare(sr);
        const auto y = run(ag, x, block, true);
        float maxDiff = 0;
        for (size_t i = 0; i < x.size(); ++i)
            maxDiff = std::max(maxDiff, std::abs(y[i] - x[i]));
        INFO("block " << block);
        REQUIRE(maxDiff < 1e-6f);
        (block == 64 ? final64 : final4096) = ag.gainDb();
    }
    REQUIRE(std::abs(final64 - final4096) < 0.5f);
}

TEST_CASE("auto gain: ceiling never exceeds 1.0 and is transparent below -1 dBFS")
{
    AutoGain loud;
    loud.prepare(sr);
    const auto x = sineRms(-6.0, 4.0); // amplitude 0.71, scaled below to overs
    std::vector<float> hot(x.size());
    for (size_t i = 0; i < x.size(); ++i)
        hot[i] = 4.0f * x[i];
    float peak = 0;
    for (float v : run(loud, hot, 480, true))
        peak = std::max(peak, std::abs(v));
    REQUIRE(peak <= 1.0f);
    REQUIRE(peak > 0.9f);

    AutoGain quiet;
    quiet.prepare(sr);
    std::vector<float> half(48000);
    for (size_t i = 0; i < half.size(); ++i)
        half[i] = 0.5f * static_cast<float>(std::sin(0.05 * static_cast<double>(i)));
    const auto y = run(quiet, half, 480, true);
    float maxDiff = 0;
    for (size_t i = 0; i < half.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(y[i] - half[i]));
    REQUIRE(maxDiff < 1e-6f);
}

TEST_CASE("processor: NaN/Inf input is dropped and does not poison the state")
{
    for (bool autoGain : { false, true })
    {
        PluginProcessor p;
        configure(p, 256);
        setParam(p, grain::ids::bend, 0.5f);
        setParam(p, grain::ids::haze, 0.5f);
        setParam(p, grain::ids::dusk, 3000.0f);
        setParam(p, grain::ids::autogain, autoGain ? 1.0f : 0.0f);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> b(2, 256);
        bool finite = true;
        for (int block = 0; block < 40; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    b.setSample(c, i, 0.3f * std::sin(0.07f * static_cast<float>(block * 256 + i)));
            if (block == 3)
            {
                b.setSample(0, 10, std::numeric_limits<float>::quiet_NaN());
                b.setSample(1, 20, std::numeric_limits<float>::infinity());
            }
            p.processBlock(b, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    finite &= std::isfinite(b.getSample(c, i));
        }
        INFO("autogain " << autoGain);
        REQUIRE(finite);
    }
}

TEST_CASE("processor: a huge finite sample does not kill the auto gain level detector")
{
    PluginProcessor p;
    configure(p, 256);
    setParam(p, grain::ids::autogain, 1.0f);

    const auto quiet = sineRms(-36.0, 20.0);
    juce::AudioBuffer<float> b(2, 256);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos + 256 <= quiet.size(); pos += 256)
    {
        for (int c = 0; c < 2; ++c)
            std::copy_n(quiet.begin() + static_cast<long>(pos), 256, b.getWritePointer(c));
        if (pos == 256 * 3)
        {
            b.setSample(0, 5, 1.0e30f); // squares to +inf in float
            b.setSample(1, 6, -1.0e30f);
        }
        p.processBlock(b, midi);
    }
    REQUIRE(p.autoGainDb() >= 10.4f); // quiet source: detector recovered and raised the gain to its +12 dB clamp
}

TEST_CASE("auto gain: end() maps NaN to 0 and never exceeds 1.0")
{
    AutoGain ag;
    ag.prepare(sr);
    std::vector<float> x(480, 0.1f);
    float* ch[] { x.data() };
    ag.begin(ch, 1, 480, true);
    x[10] = std::numeric_limits<float>::quiet_NaN(); // as if the chain produced it
    x[20] = std::numeric_limits<float>::infinity();
    ag.end(ch, 1, 480);
    REQUIRE(x[10] == 0.0f);
    REQUIRE(x[20] == 1.0f);
}

TEST_CASE("processBlock with auto gain and every stage engaged does not allocate")
{
    PluginProcessor p;
    configure(p, 256);
    setParam(p, grain::ids::bend, 0.6f);
    setParam(p, grain::ids::haze, 0.6f);
    setParam(p, grain::ids::dusk, 2000.0f);
    setParam(p, grain::ids::autogain, 1.0f);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    long allocations = 0;
    for (int i = 0; i < 100; ++i)
    {
        if (i % 25 == 0)
            setParam(p, grain::ids::autogain, (i / 25) % 2 == 0 ? 1.0f : 0.0f); // toggle outside the armed region
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < 256; ++s)
                buffer.setSample(c, s, 0.9f * std::sin(0.03f * static_cast<float>(i * 256 + s))); // hot: ceiling path runs
        allocguard::arm();
        p.processBlock(buffer, midi);
        allocations += allocguard::disarm();
    }
    REQUIRE(allocations == 0);
}
