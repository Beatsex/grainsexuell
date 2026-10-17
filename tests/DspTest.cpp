#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/Lowpass.h"
#include "dsp/ModDelay.h"
#include "dsp/Reverb.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

using namespace grain::dsp;

namespace
{
std::vector<float> noise(int n, float amp = 0.5f, unsigned seed = 1)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> d(-amp, amp);
    std::vector<float> v(static_cast<size_t>(n));
    for (auto& s : v)
        s = d(rng);
    return v;
}

std::vector<float> sine(double hz, double sr, int n, float amp = 0.5f)
{
    std::vector<float> v(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        v[static_cast<size_t>(i)] = amp * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / sr));
    return v;
}

double rms(const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < to; ++i)
        s += static_cast<double>(v[i]) * v[i];
    return std::sqrt(s / static_cast<double>(to - from));
}

bool allFinite(const std::vector<float>& v)
{
    for (float s : v)
        if (! std::isfinite(s))
            return false;
    return true;
}

float maxJump(const std::vector<float>& v)
{
    float j = 0;
    for (size_t i = 1; i < v.size(); ++i)
        j = std::max(j, std::abs(v[i] - v[i - 1]));
    return j;
}

// Runs a mono buffer through fn(float*, n) in blocks.
template <typename Fn>
void inBlocks(std::vector<float>& v, int block, Fn fn)
{
    for (size_t pos = 0; pos < v.size(); pos += static_cast<size_t>(block))
        fn(v.data() + pos, static_cast<int>(std::min(v.size() - pos, static_cast<size_t>(block))));
}

void setParam(PluginProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(plain));
}
} // namespace

TEST_CASE("lowpass: -3 dB at cutoff, steep above, bit-exact when open")
{
    constexpr double sr = 48000.0;
    for (auto [hz, expectedDb] : { std::pair { 1000.0, -3.0 }, std::pair { 4000.0, -24.0 } })
    {
        Lowpass lp;
        lp.prepare(sr);
        auto x = sine(hz, sr, 48000);
        const auto in = x;
        inBlocks(x, 512, [&](float* d, int n) { float* ch[] { d }; lp.process(ch, 1, n, 1000.0f); });
        const double db = 20.0 * std::log10(rms(x, 24000, 48000) / rms(in, 24000, 48000));
        INFO(hz);
        REQUIRE(std::abs(db - expectedDb) < 1.5);
    }

    Lowpass open;
    open.prepare(sr);
    auto x = noise(48000);
    const auto in = x;
    inBlocks(x, 333, [&](float* d, int n) { float* ch[] { d }; open.process(ch, 1, n, 20000.0f); });
    REQUIRE(x == in);

    Lowpass sweep;
    sweep.prepare(sr);
    auto y = noise(48000, 1.0f, 2);
    int block = 0;
    inBlocks(y, 256, [&](float* d, int n) { float* ch[] { d }; sweep.process(ch, 1, n, (block++ % 2) ? 200.0f : 18000.0f); });
    REQUIRE(allFinite(y));
}

TEST_CASE("mod delay: bend 0 is bit-exact, bend 1 modulates safely")
{
    constexpr double sr = 48000.0;
    ModDelay off;
    off.prepare(sr);
    auto x = noise(48000);
    const auto in = x;
    inBlocks(x, 300, [&](float* d, int n) { float* ch[] { d }; off.process(ch, 1, n, 0.0f); });
    REQUIRE(x == in);

    ModDelay on;
    on.prepare(sr);
    auto s = sine(440.0, sr, 96000);
    const auto sIn = s;
    inBlocks(s, 512, [&](float* d, int n) { float* ch[] { d }; on.process(ch, 1, n, 1.0f); });
    REQUIRE(allFinite(s));
    REQUIRE(s != sIn);
    float peak = 0;
    for (float v : s)
        peak = std::max(peak, std::abs(v));
    REQUIRE(peak <= 0.5f + 1e-6f);
    REQUIRE(maxJump(s) < 0.1f);
}

TEST_CASE("reverb: every mode decays, stays finite, and haze 0 is bit-exact")
{
    constexpr double sr = 48000.0;

    for (int mode = 0; mode < Reverb::numModes; ++mode)
    {
        const float t60 = Reverb::t60[static_cast<size_t>(mode)];
        INFO("mode " << mode);

        Reverb r;
        r.prepare(sr);
        std::vector<float> imp(static_cast<size_t>((1.5 * t60 + 1.0) * sr), 0.0f);
        imp[0] = 1.0f;
        inBlocks(imp, 512, [&](float* d, int n) { float* ch[] { d }; r.process(ch, 1, n, 1.0f, mode); });
        REQUIRE(allFinite(imp));

        const double early = rms(imp, static_cast<size_t>(0.02 * sr), static_cast<size_t>(0.2 * sr));
        const auto lateFrom = static_cast<size_t>(1.5 * t60 * sr);
        const double late = rms(imp, lateFrom, lateFrom + static_cast<size_t>(0.05 * sr));
        REQUIRE(early > 1e-6);
        REQUIRE(late < early * 1e-3); // -60 dB

        Reverb loud;
        loud.prepare(sr);
        auto n = noise(48000, 1.0f, 7);
        inBlocks(n, 480, [&](float* d, int k) { float* ch[] { d }; loud.process(ch, 1, k, 1.0f, mode); });
        REQUIRE(allFinite(n));
        float peak = 0;
        for (float v : n)
            peak = std::max(peak, std::abs(v));
        REQUIRE(peak < 4.0f);

        Reverb dry;
        dry.prepare(sr);
        auto x = noise(48000, 0.5f, 3);
        const auto in = x;
        inBlocks(x, 1000, [&](float* d, int k) { float* ch[] { d }; dry.process(ch, 1, k, 0.0f, mode); });
        REQUIRE(x == in);
    }
}

TEST_CASE("reverb: switching mode while ringing stays finite and click-free")
{
    constexpr double sr = 48000.0;
    Reverb r;
    r.prepare(sr);
    auto x = sine(440.0, sr, 96000);
    int block = 0;
    inBlocks(x, 4800, [&](float* d, int n) { float* ch[] { d }; r.process(ch, 1, n, 0.5f, block++ % Reverb::numModes); });
    REQUIRE(allFinite(x));
    REQUIRE(maxJump(x) < 0.25f);
}

TEST_CASE("full chain runs at every sample rate and block size")
{
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int block : { 1, 64, 480, 4096 })
        {
            PluginProcessor p;
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::stereo());
            layout.outputBuses.add(juce::AudioChannelSet::stereo());
            REQUIRE(p.setBusesLayout(layout));
            p.setRateAndBufferSizeDetails(sr, block);
            p.prepareToPlay(sr, block);
            setParam(p, grain::ids::bend, 1.0f);
            setParam(p, grain::ids::haze, 0.7f);
            setParam(p, grain::ids::dusk, 1500.0f);
            setParam(p, grain::ids::hazemode, 2.0f);

            const auto src = noise(static_cast<int>(sr * 0.25), 0.5f, 5);
            juce::AudioBuffer<float> b(2, block);
            juce::MidiBuffer midi;
            bool finite = true;
            for (size_t pos = 0; pos < src.size(); pos += static_cast<size_t>(block))
            {
                const int n = static_cast<int>(std::min(src.size() - pos, static_cast<size_t>(block)));
                juce::AudioBuffer<float> view(b.getArrayOfWritePointers(), 2, n);
                for (int c = 0; c < 2; ++c)
                    std::copy_n(src.begin() + static_cast<long>(pos), n, view.getWritePointer(c));
                p.processBlock(view, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                        finite &= std::isfinite(view.getSample(c, i));
            }
            INFO("sr " << sr << " block " << block);
            REQUIRE(finite);
        }
}

TEST_CASE("processBlock with every stage engaged does not allocate")
{
    PluginProcessor p;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(p.setBusesLayout(layout));
    p.setRateAndBufferSizeDetails(48000.0, 256);
    p.prepareToPlay(48000.0, 256);
    setParam(p, grain::ids::bend, 0.6f);
    setParam(p, grain::ids::haze, 0.6f);
    setParam(p, grain::ids::dusk, 2000.0f);

    const auto src = noise(256, 0.5f, 9);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    long allocations = 0;
    for (int i = 0; i < 100; ++i)
    {
        if (i % 10 == 0) // mode switch + sweep, set outside the armed region
        {
            setParam(p, grain::ids::hazemode, static_cast<float>((i / 10) % 4));
            setParam(p, grain::ids::dusk, i % 20 == 0 ? 500.0f : 15000.0f);
        }
        for (int c = 0; c < 2; ++c)
            std::copy(src.begin(), src.end(), buffer.getWritePointer(c));
        allocguard::arm();
        p.processBlock(buffer, midi);
        allocations += allocguard::disarm();
    }
    REQUIRE(allocations == 0);
}
