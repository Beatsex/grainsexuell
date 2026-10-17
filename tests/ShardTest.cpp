#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/Shard.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <set>
#include <vector>

using grain::dsp::Shard;

namespace
{
constexpr double sr = 48000.0;
constexpr int slice = 12000; // 1/8 at 120 BPM = 0.25 s (swarm step 3 below is 1/16 = 6000; step 1 is 1/8)

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

std::vector<float> run(Shard& s, std::vector<float> x, const Shard::Params& p, int block = 500)
{
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
        const int n = static_cast<int>(std::min(x.size() - pos, static_cast<size_t>(block)));
        float* ch[] { x.data() + pos };
        s.process(ch, 1, n, p);
    }
    return x;
}

double rms(const std::vector<float>& v, double fromSec, double toSec)
{
    const auto a = static_cast<size_t>(fromSec * sr), b = std::min(v.size(), static_cast<size_t>(toSec * sr));
    double s = 0;
    for (size_t i = a; i < b; ++i)
        s += static_cast<double>(v[i]) * v[i];
    return std::sqrt(s / static_cast<double>(b - a));
}

// Hann-windowed Goertzel magnitude at f over [fromSec, toSec)
double tone(const std::vector<float>& v, double f, double fromSec, double toSec)
{
    const auto a = static_cast<size_t>(fromSec * sr), b = std::min(v.size(), static_cast<size_t>(toSec * sr));
    const double w = 2.0 * std::numbers::pi * f / sr, c = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i)
    {
        const double win = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * static_cast<double>(i - a) / static_cast<double>(b - a)));
        const double s0 = win * v[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2);
}

bool allFinite(const std::vector<float>& v)
{
    for (float s : v)
        if (! std::isfinite(s))
            return false;
    return true;
}

float maxStep(const std::vector<float>& v, size_t from = 0)
{
    float j = 0;
    for (size_t i = from + 1; i < v.size(); ++i)
        j = std::max(j, std::abs(v[i] - v[i - 1]));
    return j;
}

float peak(const std::vector<float>& v)
{
    float p = 0;
    for (float s : v)
        p = std::max(p, std::abs(s));
    return p;
}

float swarmFor(int step) { return (static_cast<float>(step) + 0.5f) / 7.0f; } // 0 = 1/4, 1 = 1/8, 2 = 1/8T, 3 = 1/16 ...

Shard::Params wet(int variant, float swarm = swarmFor(1), float span = 0.0f)
{
    Shard::Params p;
    p.swarm = swarm;
    p.tail = 0.0f;
    p.span = span;
    p.scatter = 0.0f;
    p.mix = 1.0f;
    p.variant = variant;
    return p;
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

TEST_CASE("shard: Mix 0, disabled family and silence leave the audio exactly alone")
{
    const auto x = noise(3.0);
    for (int variant = 0; variant < 4; ++variant)
    {
        {
            Shard s;
            s.prepare(sr);
            auto p = wet(variant);
            p.mix = 0.0f;
            REQUIRE(run(s, x, p) == x);
        }
        {
            Shard s;
            s.prepare(sr);
            auto p = wet(variant);
            p.enabled = false;
            REQUIRE(run(s, x, p) == x);
        }
        {
            Shard s;
            s.prepare(sr);
            const auto y = run(s, std::vector<float>(3 * 48000, 0.0f), wet(variant));
            REQUIRE(rms(y, 0, 3) == 0.0);
        }
    }
}

TEST_CASE("shard: Shuffle with Span 0 and Scatter 0 is a delay of exactly one slice")
{
    const auto x = noise(3.0, 0.5f, 3);
    Shard s;
    s.prepare(sr);
    const auto y = run(s, x, wet(0)); // 1/8 at 120 BPM
    double worst = 0;
    for (size_t t = 2 * slice; t < y.size(); ++t)
        worst = std::max(worst, static_cast<double>(std::abs(y[t] - x[t - slice])));
    INFO("worst deviation " << worst);
    REQUIRE(worst < 1e-4); // the 2 ms fades of neighbouring slices add up to exactly 1
}

TEST_CASE("shard: Swarm sets the slice length on the tempo grid")
{
    // the same noise delayed by one slice: find the delay for each step by correlation
    const auto x = noise(3.0, 0.5f, 3);
    const int expected[] { 24000, 12000, 8000, 6000, 4000, 3000, 2000 }; // 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T at 120 BPM
    for (int step = 0; step < 7; ++step)
    {
        Shard s;
        s.prepare(sr);
        const auto y = run(s, x, wet(0, swarmFor(step)));
        const size_t d = static_cast<size_t>(expected[step]);
        double worst = 0;
        for (size_t t = 3 * d; t < y.size(); ++t)
            worst = std::max(worst, static_cast<double>(std::abs(y[t] - x[t - d])));
        INFO("step " << step << " slice " << d);
        REQUIRE(worst < 1e-4);
    }
}

TEST_CASE("shard: Reverse slices play the audio backwards")
{
    const auto x = noise(4.0, 0.5f, 7);
    Shard s;
    s.prepare(sr);
    auto p = wet(0);
    p.scatter = 1.0f; // 50% of the slices are reversed
    const auto y = run(s, x, p);

    int forward = 0, backward = 0;
    for (int j = 4; j < 14; ++j)
    {
        const int T = j * slice;
        double fwd = 0, bwd = 0;
        for (int a = 300; a < slice - 300; ++a) // away from the fades
        {
            fwd = std::max(fwd, static_cast<double>(std::abs(y[static_cast<size_t>(T + a)] - x[static_cast<size_t>(T + a - slice)])));
            bwd = std::max(bwd, static_cast<double>(std::abs(y[static_cast<size_t>(T + a)] - x[static_cast<size_t>(T - 516 - a)])));
        }
        INFO("slice " << j << " forward error " << fwd << " backward error " << bwd);
        REQUIRE(std::min(fwd, bwd) < 1e-4); // every slice is exactly one of the two
        (fwd < bwd ? forward : backward)++;
    }
    REQUIRE(forward > 0);
    REQUIRE(backward > 0);
}

TEST_CASE("shard: Crush quantises and holds samples")
{
    Shard s;
    s.prepare(sr);
    const auto y = run(s, sine(220.0, 3.0), wet(1, swarmFor(1), 1.0f)); // 3 bits, hold 12
    std::set<long> levels;
    int changes = 0;
    size_t count = 0;
    const auto q = [](float v) { return std::lround(v * 1e4f); }; // (cos(pi/2) at Mix 1 leaks about 4e-8 of the dry signal)
    for (size_t i = 48000; i < y.size(); ++i, ++count)
    {
        levels.insert(q(y[i]));
        changes += q(y[i]) != q(y[i - 1]) ? 1 : 0;
    }
    INFO("distinct levels " << levels.size() << ", changes " << changes);
    REQUIRE(levels.size() <= 8);
    REQUIRE(levels.size() >= 3);
    REQUIRE(static_cast<double>(changes) <= 1.02 * static_cast<double>(count) / 12.0 + 4);
}

TEST_CASE("shard: Arp plays the recent onsets at rising pitches")
{
    // 440 Hz bursts every 0.5 s: the arpeggio steps through 440, 523, 659 and 880 Hz
    std::vector<float> x(static_cast<size_t>(8.0 * sr), 0.0f);
    for (int k = 1; k < 14; ++k)
        for (int i = 0; i < 2400; ++i)
        {
            const size_t at = static_cast<size_t>(k * 24000 + i);
            if (at < x.size())
                x[at] = 0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 440.0 * i / sr) * std::sin(std::numbers::pi * i / 2400.0));
        }
    Shard s;
    s.prepare(sr);
    const auto y = run(s, x, wet(2, swarmFor(1)));
    INFO("440 " << tone(y, 440, 2, 8) << " 523 " << tone(y, 523.25, 2, 8) << " 659 " << tone(y, 659.26, 2, 8) << " 880 "
                << tone(y, 880, 2, 8) << " ref 350 " << tone(y, 350, 2, 8) << " ref 1200 " << tone(y, 1200, 2, 8));
    const double floor = std::max(tone(y, 350, 2, 8), tone(y, 1200, 2, 8));
    for (double f : { 440.0, 523.25, 659.26, 880.0 })
        REQUIRE(tone(y, f, 2, 8) > 3.0 * floor);

    // and without any onset there is nothing to play
    Shard quiet;
    quiet.prepare(sr);
    REQUIRE(rms(run(quiet, sine(440.0, 3.0, 0.0f), wet(2)), 0, 3) == 0.0);
}

TEST_CASE("shard: Stutter repeats a frozen slice with falling level")
{
    // mix 0 for 2 s so the first slice starts at a known frame, then wet
    const auto x = noise(5.0, 0.5f, 9);
    Shard s;
    s.prepare(sr);
    auto y = x;
    auto p = wet(3, swarmFor(1), 1.0f); // Span 1: up to 7 repeats
    for (size_t pos = 0; pos < y.size(); pos += 500)
    {
        p.mix = pos < 96000 ? 0.0f : 1.0f;
        float* ch[] { y.data() + pos };
        s.process(ch, 1, 500, p);
    }
    const size_t T0 = 96000;
    double worst = 0;
    for (size_t t = T0 + 2000; t < T0 + 5 * slice; ++t) // after the 20 ms mix ramp, inside the first group
        if ((t - T0) % slice >= 300) // not in the 5 ms crossfade where the next repeat starts
            worst = std::max(worst, static_cast<double>(std::abs(y[t + slice] - 0.85f * y[t])));
    INFO("worst deviation " << worst);
    REQUIRE(worst < 1e-4);
}

TEST_CASE("shard: slices are faded, so there are no clicks")
{
    for (double hz : { 100.0, 440.0 })
        for (int variant : { 0, 1, 3 })
            for (float scatter : { 0.0f, 1.0f })
            {
                Shard s;
                s.prepare(sr);
                auto p = wet(variant, swarmFor(3), variant == 0 ? 1.0f : 0.0f);
                p.scatter = scatter;
                const auto y = run(s, sine(hz, 4.0), p, 256);
                INFO("hz " << hz << " variant " << variant << " scatter " << scatter << " step " << maxStep(y, 24000));
                REQUIRE(allFinite(y));
                REQUIRE(maxStep(y, 24000) < 0.2f);
            }
}

TEST_CASE("shard: output does not depend on the host block size")
{
    const auto x = noise(2.0, 0.5f, 11);
    for (int variant = 0; variant < 4; ++variant)
    {
        auto p = wet(variant, swarmFor(4), 0.5f);
        p.scatter = 0.6f;
        p.tail = 0.6f;
        Shard ref;
        ref.prepare(sr);
        const auto expected = run(ref, x, p, 512);
        for (int block : { 480, 64, 1 })
        {
            Shard s;
            s.prepare(sr);
            INFO("variant " << variant << " block " << block);
            REQUIRE(run(s, x, p, block) == expected);
        }
    }
}

TEST_CASE("shard: feedback decays")
{
    for (int variant = 0; variant < 4; ++variant)
        for (float swarm : { swarmFor(0), swarmFor(5) })
            for (float span : { 0.0f, 1.0f })
            {
                auto x = noise(1.0);
                x.resize(static_cast<size_t>(14.0 * sr), 0.0f);
                auto p = wet(variant, swarm, span);
                p.tail = 1.0f;
                p.scatter = 0.5f;
                Shard s;
                s.prepare(sr);
                const auto y = run(s, x, p);
                INFO("variant " << variant << " swarm " << swarm << " span " << span);
                REQUIRE(allFinite(y));
                REQUIRE(peak(y) < 4.0f);
                double early = 0;
                for (int sec = 0; sec < 8; ++sec)
                    early = std::max(early, rms(y, sec, sec + 1));
                REQUIRE(rms(y, 13.0, 14.0) <= 0.5 * early);
            }
}

TEST_CASE("shard: wet level stays within 6 dB of the dry level (Shuffle, Stutter)")
{
    const auto x = noise(6.0);
    for (int v : { 0, 3 })
    {
        Shard s;
        s.prepare(sr);
        const auto y = run(s, x, wet(v, swarmFor(3), 0.5f));
        const double db = 20.0 * std::log10(rms(y, 3.0, 6.0) / rms(x, 3.0, 6.0));
        INFO("variant " << v << ": " << db << " dB");
        REQUIRE(std::abs(db) < 6.0);
    }
}

TEST_CASE("shard: finite at every sample rate and block size, stereo and mono, out-of-range params")
{
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int block : { 1, 64, 480, 4096 })
        {
            Shard s;
            s.prepare(rate);
            std::mt19937 rng(3);
            std::uniform_real_distribution<float> d(-0.5f, 0.5f);
            std::vector<float> l(static_cast<size_t>(rate)), r(l.size());
            for (size_t i = 0; i < l.size(); ++i)
                l[i] = d(rng), r[i] = d(rng);

            bool finite = true;
            int b = 0;
            for (size_t pos = 0; pos < l.size(); pos += static_cast<size_t>(block), ++b)
            {
                const int n = static_cast<int>(std::min(l.size() - pos, static_cast<size_t>(block)));
                auto p = wet(b % 4, swarmFor(b % 7), static_cast<float>(b % 3) * 0.5f);
                p.scatter = 1.0f;
                p.tail = 1.0f;
                p.bpm = b % 2 == 0 ? 120.0 : 400.0;
                if (b % 7 == 6) // out of range values must be clamped, not trusted
                {
                    p.span = 5.0f;
                    p.scatter = 7.0f;
                    p.swarm = -3.0f;
                    p.tail = std::numeric_limits<float>::quiet_NaN();
                    p.mix = 9.0f;
                    p.variant = 99;
                    p.bpm = std::numeric_limits<double>::quiet_NaN();
                }
                float* stereo[] { l.data() + pos, r.data() + pos };
                float* mono[] { l.data() + pos };
                if (b % 5 == 4)
                    s.process(mono, 1, n, p);
                else
                    s.process(stereo, 2, n, p);
                for (int i = 0; i < n; ++i)
                    finite &= std::isfinite(l[pos + static_cast<size_t>(i)]) && std::isfinite(r[pos + static_cast<size_t>(i)]);
            }
            INFO("rate " << rate << " block " << block);
            REQUIRE(finite);
            REQUIRE(s.slicesPlayed() > 0);
        }
}

TEST_CASE("shard: deterministic")
{
    const auto x = noise(3.0);
    Shard a, b;
    a.prepare(sr);
    b.prepare(sr);
    auto p = wet(0, swarmFor(3), 1.0f);
    p.scatter = 1.0f;
    p.tail = 0.5f;
    REQUIRE(run(a, x, p) == run(b, x, p));
}

TEST_CASE("processor: Shard runs for family 2 only and plays slices without allocating")
{
    PluginProcessor p;
    configure(p, sr, 256);
    setParam(p, grain::ids::mix, 1.0f);
    setParam(p, grain::ids::family, 3.0f); // Tap: Shard must stay idle

    const auto src = noise(1.0, 0.3f, 4);
    juce::AudioBuffer<float> b(2, 256);
    juce::MidiBuffer midi;
    for (int blk = 0; blk < 40; ++blk)
    {
        for (int c = 0; c < 2; ++c)
            std::copy_n(src.begin() + blk * 256, 256, b.getWritePointer(c));
        p.processBlock(b, midi);
    }
    REQUIRE(p.shardSliceCount() == 0);

    setParam(p, grain::ids::family, 2.0f);
    setParam(p, grain::ids::swarm, 1.0f);
    setParam(p, grain::ids::scatter, 0.5f);
    setParam(p, grain::ids::tail, 0.8f);
    long allocations = 0;
    for (int blk = 0; blk < 300; ++blk)
    {
        if (blk % 40 == 0)
            setParam(p, grain::ids::variant, static_cast<float>((blk / 40) % 4)); // outside the armed region
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                b.setSample(c, i, src[static_cast<size_t>((blk * 256 + i) % src.size())]);
        allocguard::arm();
        p.processBlock(b, midi);
        allocations += allocguard::disarm();
    }
    REQUIRE(p.shardSliceCount() > 10);
    REQUIRE(allocations == 0);
}
