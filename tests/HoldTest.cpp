#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/Hold.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using grain::dsp::Hold;

namespace
{
constexpr double sr = 48000.0;

std::vector<float> sine(double hz, double seconds, float amp = 0.5f)
{
    std::vector<float> v(static_cast<size_t>(seconds * sr));
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = amp * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / sr));
    return v;
}

std::vector<float> noise(double seconds, float amp = 0.5f, unsigned seed = 1)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> d(-amp, amp);
    std::vector<float> v(static_cast<size_t>(seconds * sr));
    for (auto& s : v)
        s = d(rng);
    return v;
}

// Runs in blocks; holdAt(blockStartFrame) says whether Hold is on for that block.
std::vector<float> run(Hold& h, std::vector<float> x, const std::function<bool(size_t)>& holdAt, double bpm = 120.0, int block = 500)
{
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
        const int n = static_cast<int>(std::min(x.size() - pos, static_cast<size_t>(block)));
        float* ch[] { x.data() + pos };
        h.process(ch, 1, n, holdAt(pos), bpm);
    }
    return x;
}

bool allFinite(const std::vector<float>& v)
{
    for (float s : v)
        if (! std::isfinite(s))
            return false;
    return true;
}

float maxStep(const std::vector<float>& v, size_t from, size_t to)
{
    float j = 0;
    for (size_t i = from + 1; i < std::min(to, v.size()); ++i)
        j = std::max(j, std::abs(v[i] - v[i - 1]));
    return j;
}

double rms(const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < std::min(to, v.size()); ++i)
        s += static_cast<double>(v[i]) * v[i];
    return std::sqrt(s / static_cast<double>(to - from));
}

void setParam(PluginProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(plain));
}

void configure(PluginProcessor& p, double rate, int block)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(p.setBusesLayout(layout));
    p.setRateAndBufferSizeDetails(rate, block);
    p.prepareToPlay(rate, block);
}
} // namespace

TEST_CASE("hold: off leaves the audio exactly alone, also after a hold cycle")
{
    const auto x = noise(4.0);
    {
        Hold h;
        h.prepare(sr);
        REQUIRE(run(h, x, [](size_t) { return false; }) == x);
    }
    {
        Hold h;
        h.prepare(sr);
        const auto y = run(h, x, [](size_t pos) { return pos >= 48000 && pos < 96000; }); // on from 1 s to 2 s
        for (size_t i = 0; i < 48000; ++i)
            REQUIRE(y[i] == x[i]);
        for (size_t i = 96000 + 3000; i < x.size(); ++i) // fade-out (50 ms) is over
            REQUIRE(y[i] == x[i]);
    }
}

TEST_CASE("hold: the frozen loop repeats exactly and contains the last beat")
{
    // noise for 1 s, then silence; Hold from 1 s on. 120 BPM: one beat = 24000 = the repeat period (tempo-locked),
    // the loop is read from 24480 frames before the press (beat + 480 frames of seam)
    auto x = noise(4.0, 0.5f, 3);
    std::fill(x.begin() + 48000, x.end(), 0.0f);
    Hold h;
    h.prepare(sr);
    const auto y = run(h, x, [](size_t pos) { return pos >= 48000; });
    REQUIRE(h.loopPeriod() == 24000);

    double worst = 0;
    for (size_t t = 48000 + 2000; t < y.size() - 24000; ++t)
        worst = std::max(worst, static_cast<double>(std::abs(y[t] - y[t + 24000])));
    INFO("periodicity error " << worst);
    REQUIRE(worst < 1e-6);

    // body of the loop = the input 24480 frames before the press
    worst = 0;
    for (size_t p = 1000; p < 24000; ++p)
        worst = std::max(worst, static_cast<double>(std::abs(y[48000 + p] - x[48000 - 24480 + p])));
    INFO("content error " << worst);
    REQUIRE(worst < 1e-6);
}

TEST_CASE("hold: the loop is one beat of the tempo, folded into 150 ms .. 2 s")
{
    struct Case
    {
        double bpm;
        int length;
    };
    // 20 BPM is 3 s, folded to 1.5 s; 600 BPM is clamped to 400 BPM = 0.15 s
    for (const auto& c : { Case { 60.0, 48000 }, Case { 120.0, 24000 }, Case { 240.0, 12000 }, Case { 400.0, 7200 },
                           Case { 20.0, 72000 }, Case { 600.0, 7200 } })
    {
        const int expectedLength = c.length;
        auto x = noise(6.0, 0.5f, 4);
        std::fill(x.begin() + 3 * 48000, x.end(), 0.0f);
        Hold h;
        h.prepare(sr);
        run(h, x, [](size_t pos) { return pos >= 3 * 48000; }, c.bpm);
        INFO("bpm " << c.bpm);
        REQUIRE(h.loopPeriod() == expectedLength); // exactly one beat
    }
}

TEST_CASE("hold: the seam and the fades are click-free")
{
    // 441.37 Hz does not divide the loop length, so the seam joins two different phases
    auto x = sine(441.37, 5.0);
    std::fill(x.begin() + 2 * 48000, x.end(), 0.0f);
    Hold h;
    h.prepare(sr);
    const auto y = run(h, x, [](size_t pos) { return pos >= 2 * 48000 && pos < 4 * 48000; });
    REQUIRE(allFinite(y));
    INFO("max step " << maxStep(y, 2 * 48000, y.size())); // (the input itself is cut to zero at 2 s)
    REQUIRE(maxStep(y, 2 * 48000, y.size()) < 0.1f);
    REQUIRE(maxStep(y, 0, 2 * 48000) < 0.1f);

    // after release the loop is gone within 50 ms
    REQUIRE(rms(y, 4 * 48000 + 3000, y.size()) == 0.0);
}

TEST_CASE("hold: switching off and on again during the fade-out does not click")
{
    auto x = sine(441.37, 4.0);
    std::fill(x.begin() + 2 * 48000, x.end(), 0.0f);
    Hold h;
    h.prepare(sr);
    // on at 2 s, off for one 500-frame block (10 ms, inside the 50 ms fade-out), on again
    const auto y = run(h, x, [](size_t pos) { return pos >= 2 * 48000 && ! (pos >= 3 * 48000 && pos < 3 * 48000 + 500); });
    REQUIRE(allFinite(y));
    INFO("max step " << maxStep(y, 2 * 48000, y.size()));
    REQUIRE(maxStep(y, 2 * 48000, y.size()) < 0.1f);
}

TEST_CASE("hold: output does not depend on the host block size")
{
    const auto x = noise(4.0, 0.5f, 6);
    const auto holdAt = [](size_t pos) { return pos >= 48000 && pos < 144000; }; // toggles at multiples of every block size below
    Hold ref;
    ref.prepare(sr);
    const auto expected = run(ref, x, holdAt, 120.0, 500);
    for (int block : { 250, 100, 1 })
    {
        Hold h;
        h.prepare(sr);
        INFO("block " << block);
        REQUIRE(run(h, x, holdAt, 120.0, block) == expected);
    }
}

TEST_CASE("hold: finite at every sample rate, stereo and mono, with odd tempos")
{
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Hold h;
        h.prepare(rate);
        std::mt19937 rng(3);
        std::uniform_real_distribution<float> d(-0.5f, 0.5f);
        std::vector<float> l(static_cast<size_t>(rate * 3)), r(l.size());
        for (size_t i = 0; i < l.size(); ++i)
            l[i] = d(rng), r[i] = d(rng);

        bool finite = true;
        int b = 0;
        for (size_t pos = 0; pos < l.size(); pos += 480, ++b)
        {
            const int n = static_cast<int>(std::min<size_t>(480, l.size() - pos));
            float* stereo[] { l.data() + pos, r.data() + pos };
            float* mono[] { l.data() + pos };
            const double bpm = b % 5 == 0 ? std::numeric_limits<double>::quiet_NaN() : b % 5 == 1 ? 0.0 : b % 5 == 2 ? -50.0 : 1e9;
            const bool on = (b / 40) % 2 == 1;
            if (b % 3 == 2)
                h.process(mono, 1, n, on, bpm);
            else
                h.process(stereo, 2, n, on, bpm);
            for (int i = 0; i < n; ++i)
                finite &= std::isfinite(l[pos + static_cast<size_t>(i)]) && std::isfinite(r[pos + static_cast<size_t>(i)]);
        }
        INFO("rate " << rate);
        REQUIRE(finite);
    }
}

TEST_CASE("processor: Hold feeds the loop into the engine, and does not allocate")
{
    // Tap at Mix 1: after the input stops, only the held loop can keep the output alive
    const auto src = noise(1.0, 0.3f, 4);
    for (bool holdOn : { false, true })
    {
        PluginProcessor p;
        configure(p, sr, 256);
        setParam(p, grain::ids::family, 3.0f);
        setParam(p, grain::ids::mix, 1.0f);
        setParam(p, grain::ids::swarm, 0.64f); // 1/8: echoes of the input are over 1 s after it stops
        setParam(p, grain::ids::span, 0.0f);
        setParam(p, grain::ids::tail, 0.0f);

        juce::AudioBuffer<float> b(2, 256);
        juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 562; ++blk) // about 3 s
        {
            if (blk == 187) // about 1 s: the input noise ends here
                setParam(p, grain::ids::hold, holdOn ? 1.0f : 0.0f);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                {
                    const size_t at = static_cast<size_t>(blk * 256 + i);
                    b.setSample(c, i, at < src.size() ? src[at] : 0.0f);
                }
            p.processBlock(b, midi);
            for (int i = 0; i < 256; ++i)
                out.push_back(b.getSample(0, i));
        }
        double late = 0;
        for (size_t i = 100000; i < out.size(); ++i)
            late += static_cast<double>(out[i]) * out[i];
        late = std::sqrt(late / static_cast<double>(out.size() - 100000));
        INFO("hold " << holdOn << " late rms " << late);
        if (holdOn)
            REQUIRE(late > 0.02);
        else
            REQUIRE(late < 1e-6);
    }

    PluginProcessor g;
    configure(g, sr, 256);
    setParam(g, grain::ids::mix, 0.7f);
    setParam(g, grain::ids::family, 1.0f);
    juce::AudioBuffer<float> b(2, 256);
    juce::MidiBuffer midi;
    long allocations = 0;
    for (int blk = 0; blk < 300; ++blk)
    {
        if (blk % 50 == 0)
            setParam(g, grain::ids::hold, (blk / 50) % 2 == 0 ? 0.0f : 1.0f); // outside the armed region
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                b.setSample(c, i, src[static_cast<size_t>((blk * 256 + i) % src.size())]);
        allocguard::arm();
        g.processBlock(b, midi);
        allocations += allocguard::disarm();
    }
    REQUIRE(allocations == 0);
}
