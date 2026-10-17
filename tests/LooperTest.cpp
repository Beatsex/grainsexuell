#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/Looper.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using grain::dsp::Looper;

namespace
{
constexpr double sr = 48000.0;
constexpr int L = 24000;       // one beat at 120 BPM
constexpr int P = L - 480;     // loop period of an unquantized 24000-frame recording (10 ms seam)

using Schedule = std::function<Looper::Params(size_t pos)>;

std::vector<float> noise(size_t frames, float amp = 0.5f, unsigned seed = 1)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> d(-amp, amp);
    std::vector<float> v(frames);
    for (auto& s : v)
        s = d(rng);
    return v;
}

std::vector<float> run(Looper& l, std::vector<float> x, const Schedule& sched, int block = 500)
{
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
        const int n = static_cast<int>(std::min(x.size() - pos, static_cast<size_t>(block)));
        float* ch[] { x.data() + pos };
        l.process(ch, 1, n, sched(pos));
    }
    return x;
}

Looper::Params params(int mode)
{
    Looper::Params p;
    p.mode = mode;
    return p;
}

// Record for [0, L), then `after`
Schedule recordThen(int after)
{
    return [after](size_t pos) { return params(pos < static_cast<size_t>(L) ? Looper::Record : after); };
}

// noise in [0, L), silence after (the looper alone must keep the output alive)
std::vector<float> phrase(size_t total, unsigned seed = 2)
{
    auto x = noise(total, 0.5f, seed);
    std::fill(x.begin() + L, x.end(), 0.0f);
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
    return std::sqrt(s / static_cast<double>(std::min(to, v.size()) - from));
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

TEST_CASE("looper: Off, nothing recorded and idle controls leave the audio exactly alone")
{
    const auto x = noise(96000);
    for (int mode : { Looper::Off, Looper::Play, Looper::Overdub })
    {
        Looper l;
        l.prepare(sr, 5.0);
        auto p = params(mode);
        p.burst = true;
        p.reverse = true;
        p.speed = 2;
        REQUIRE(run(l, x, [&p](size_t) { return p; }) == x);
    }
    {
        Looper l; // undo with nothing to undo
        l.prepare(sr, 5.0);
        REQUIRE(run(l, x, [](size_t pos) { auto p = params(Looper::Play); p.undo = (pos / 500) % 2 == 1; return p; }) == x);
    }
    {
        Looper l; // after Record the input still passes unchanged while recording
        l.prepare(sr, 5.0);
        auto y = run(l, x, [](size_t) { return params(Looper::Record); });
        REQUIRE(y == x);
        REQUIRE(l.isRecording());
    }
}

TEST_CASE("looper: a recorded phrase loops exactly, with a seam that joins itself")
{
    const auto x = phrase(144000);
    Looper l;
    l.prepare(sr, 5.0);
    const auto y = run(l, x, recordThen(Looper::Play));
    REQUIRE(l.loopLength() == L);
    REQUIRE(l.loopPeriod() == P);

    for (size_t t = 0; t < static_cast<size_t>(L); ++t) // input passes through while recording
        REQUIRE(y[t] == x[t]);

    double worst = 0;
    for (size_t t = L; t < y.size(); ++t)
    {
        const size_t p = (t - L) % P;
        if (p >= 600) // body: the recording itself (after the 10 ms fade-in)
            worst = std::max(worst, static_cast<double>(std::abs(y[t] - x[p])));
        if (t >= L + 600 && t + P < y.size()) // (the loop fades in over its first 10 ms)
            worst = std::max(worst, static_cast<double>(std::abs(y[t] - y[t + P]))); // periodic, seam included
    }
    INFO("worst " << worst);
    REQUIRE(worst < 1e-6);
}

TEST_CASE("looper: Speed and Reverse")
{
    const auto x = phrase(144000);
    const auto render = [&x](int speed, bool reverse) {
        Looper l;
        l.prepare(sr, 5.0);
        return run(l, x, [speed, reverse](size_t pos) {
            auto p = params(pos < static_cast<size_t>(L) ? Looper::Record : Looper::Play);
            p.speed = speed;
            p.reverse = reverse;
            return p;
        });
    };
    const auto body = [](size_t idx) { return idx >= 480 && idx + 1 < static_cast<size_t>(P); };

    const auto twice = render(2, false);
    const auto half = render(0, false);
    const auto back = render(1, true);
    double worst = 0;
    for (size_t m = 600; m < 100000; ++m) // (the loop fades in over its first 10 ms)
    {
        const size_t t = L + m;
        if (const size_t idx = (2 * m) % P; body(idx))
            worst = std::max(worst, static_cast<double>(std::abs(twice[t] - x[idx])));
        if (const size_t i = (m / 2) % P; body(i))
            worst = std::max(worst, static_cast<double>(std::abs(half[t] - (m % 2 == 0 ? x[i] : 0.5f * (x[i] + x[i + 1])))));
        if (const size_t idx = (P - m % P) % P; body(idx))
            worst = std::max(worst, static_cast<double>(std::abs(back[t] - x[idx])));
    }
    INFO("worst " << worst);
    REQUIRE(worst < 1e-6);
}

TEST_CASE("looper: Overdub stacks on the loop, Undo removes it, a second Undo clears the loop")
{
    // 120 BPM, blocks of 20 so every switch lands on a loop boundary: record A, play, overdub B for exactly one pass
    const size_t total = 140000;
    auto x = noise(total, 0.3f, 5);
    const auto b = noise(total, 0.3f, 6);
    for (size_t t = 0; t < total; ++t)
        x[t] = t < static_cast<size_t>(L) ? x[t] : (t >= 47520 && t < 71040) ? b[t] : 0.0f;

    Looper l;
    l.prepare(sr, 5.0);
    const Schedule sched = [](size_t pos) {
        auto p = params(pos < 24000 ? Looper::Record : pos < 47520 ? Looper::Play : pos < 71040 ? Looper::Overdub : Looper::Play);
        p.undo = (pos >= 100000 && pos < 100020) || (pos >= 120000 && pos < 120020);
        return p;
    };
    const auto y = run(l, x, sched, 20);

    double worstBoth = 0, worstA = 0;
    for (size_t t = 75000; t < 99980; ++t) // A + B
    {
        const size_t p = (t - L) % P;
        if (p >= 600 && p < static_cast<size_t>(P) - 600)
            worstBoth = std::max(worstBoth, static_cast<double>(std::abs(y[t] - (x[p] + x[47520 + p]))));
    }
    for (size_t t = 101000; t < 119980; ++t) // after Undo: A alone
    {
        const size_t p = (t - L) % P;
        if (p >= 600 && p < static_cast<size_t>(P) - 600)
            worstA = std::max(worstA, static_cast<double>(std::abs(y[t] - x[p])));
    }
    INFO("A+B " << worstBoth << ", A " << worstA);
    REQUIRE(worstBoth < 1e-5);
    REQUIRE(worstA < 1e-6);
    REQUIRE(! l.hasOverdub());
    REQUIRE(! l.isPlaying());
    REQUIRE(l.loopLength() == 0);
    REQUIRE(rms(y, 121000, total) == 0.0); // second Undo: the loop is gone
}

TEST_CASE("looper: Quantize rounds the loop to whole beats, tempo-locked")
{
    struct Case
    {
        size_t recordFrames;
        int expectedPeriod;
    };
    // 120 BPM: a beat is 24000 frames. 38000 -> 2 beats (records on to the beat), 5000 -> at least 1 beat, 31000 -> 1 beat (cut)
    for (const auto& c : { Case { 38000, 48000 }, Case { 5000, 24000 }, Case { 31000, 24000 } })
    {
        Looper l;
        l.prepare(sr, 5.0);
        run(l, noise(200000), [&c](size_t pos) {
            auto p = params(pos < c.recordFrames ? Looper::Record : Looper::Play);
            p.quantize = true;
            return p;
        });
        INFO("recorded " << c.recordFrames);
        REQUIRE(l.loopPeriod() == c.expectedPeriod);
        REQUIRE(l.isPlaying());
    }
    {
        Looper l; // without Quantize the period is just what was recorded (minus the seam)
        l.prepare(sr, 5.0);
        run(l, noise(100000), [](size_t pos) { return params(pos < 5000 ? Looper::Record : Looper::Play); });
        REQUIRE(l.loopPeriod() == 5000 - 480);
    }
}

TEST_CASE("looper: Burst retriggers the loop every 1/16 note, and releasing rejoins the loop in time")
{
    const auto x = phrase(144000);
    const auto burstAt = [](size_t on, size_t off) -> Schedule {
        return [on, off](size_t pos) {
            auto p = params(pos < static_cast<size_t>(L) ? Looper::Record : Looper::Play);
            p.burst = pos >= on && pos < off;
            return p;
        };
    };
    Looper a, b;
    a.prepare(sr, 5.0);
    b.prepare(sr, 5.0);
    const auto stutter = run(a, x, burstAt(48000, 78000));
    const auto plain = run(b, x, burstAt(1000000, 1000000));

    double periodic = 0, differs = 0, rejoin = 0;
    for (size_t t = 48400; t + 6000 < 78000; ++t) // 1/16 note at 120 BPM = 6000 frames
        periodic = std::max(periodic, static_cast<double>(std::abs(stutter[t] - stutter[t + 6000])));
    for (size_t t = 48000; t < 78000; ++t)
        differs = std::max(differs, static_cast<double>(std::abs(stutter[t] - plain[t])));
    for (size_t t = 78500; t < stutter.size(); ++t)
        rejoin = std::max(rejoin, static_cast<double>(std::abs(stutter[t] - plain[t])));
    INFO("periodic " << periodic << " differs " << differs << " rejoin " << rejoin);
    REQUIRE(periodic < 1e-6);
    REQUIRE(differs > 0.05);
    REQUIRE(rejoin < 1e-6);
}

TEST_CASE("looper: a partial overdub, Undo and stopping the transport do not click")
{
    // A (441.37 Hz) is recorded; a smooth 0.25 s burst of B (300 Hz) is overdubbed on part of the loop; later Undo, then Off
    const size_t total = 200000;
    std::vector<float> x(total, 0.0f);
    for (size_t i = 0; i < static_cast<size_t>(L); ++i)
        x[i] = 0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 441.37 * static_cast<double>(i) / sr));
    for (size_t i = 0; i < 12000; ++i)
        x[48000 + i] = 0.3f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 300.0 * static_cast<double>(i) / sr)
                                                  * std::sin(std::numbers::pi * static_cast<double>(i) / 12000.0));
    Looper l;
    l.prepare(sr, 5.0);
    const auto y = run(l, x, [](size_t pos) {
        auto p = params(pos < 24000 ? Looper::Record : pos < 48000 ? Looper::Play : pos < 60000 ? Looper::Overdub
                        : pos < 150000 ? Looper::Play : Looper::Off);
        p.undo = pos >= 100000 && pos < 100500;
        return p;
    });
    REQUIRE(allFinite(y));
    INFO("max step " << maxStep(y, 24000, total));
    REQUIRE(maxStep(y, 24000, 48000) < 0.1f);    // loop starts over the (just stopped) input
    REQUIRE(maxStep(y, 60500, total) < 0.1f);    // overdub edges on every pass, Undo, Off
    REQUIRE(rms(y, 151000, total) == 0.0);       // Off: silent after the 10 ms fade-out
    REQUIRE(! l.hasOverdub());
}

TEST_CASE("looper: Burst held across a re-record stays bounded")
{
    // a 3 s loop, Burst on, then a new 0.5 s phrase recorded and played with Burst still on
    const auto x = noise(480000, 0.5f, 12);
    Looper l;
    l.prepare(sr, 10.0);
    const auto y = run(l, x, [](size_t pos) {
        auto p = params(pos < 144000 ? Looper::Record : pos < 200000 ? Looper::Play : pos < 224000 ? Looper::Record : Looper::Play);
        p.burst = pos >= 160000;
        return p;
    });
    REQUIRE(allFinite(y));
    float peak = 0;
    for (float s : y)
        peak = std::max(peak, std::abs(s));
    INFO("peak " << peak);
    REQUIRE(peak < 1.6f); // live input (0.5) + loop (0.5) + a little blend headroom
}

TEST_CASE("looper: a NaN heard by Post-FX is not recorded")
{
    auto x = noise(96000, 0.4f, 13);
    x[5000] = std::numeric_limits<float>::quiet_NaN();
    Looper l;
    l.prepare(sr, 5.0);
    const auto y = run(l, x, recordThen(Looper::Play));
    for (size_t t = L; t < y.size(); ++t)
        REQUIRE(std::isfinite(y[t])); // the loop (which contains frame 5000) is finite
}

TEST_CASE("looper: the loop seam is click-free")
{
    std::vector<float> x(144000, 0.0f);
    for (size_t i = 0; i < static_cast<size_t>(L); ++i) // 441.37 Hz does not divide the loop
        x[i] = 0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 441.37 * static_cast<double>(i) / sr));
    Looper l;
    l.prepare(sr, 5.0);
    const auto y = run(l, x, recordThen(Looper::Play));
    REQUIRE(allFinite(y));
    // (the input itself stops dead at L in this test, so measure the loop from there on)
    INFO("max step " << maxStep(y, L, y.size()));
    REQUIRE(maxStep(y, L, y.size()) < 0.1f);
}

TEST_CASE("looper: output does not depend on the host block size")
{
    const auto x = noise(60000, 0.4f, 8);
    const Schedule sched = [](size_t pos) {
        auto p = params(pos < 12000 ? Looper::Record : pos < 24000 ? Looper::Play : pos < 36000 ? Looper::Overdub : Looper::Play);
        p.speed = pos >= 30000 && pos < 42000 ? 2 : pos >= 42000 && pos < 50000 ? 0 : 1;
        p.reverse = pos >= 36000 && pos < 40000;
        p.burst = pos >= 44000 && pos < 48000;
        p.undo = pos >= 52000 && pos < 52500;
        p.quantize = pos == 0;
        return p;
    };
    Looper ref;
    ref.prepare(sr, 5.0);
    const auto expected = run(ref, x, sched, 500);
    for (int block : { 250, 100, 1 })
    {
        Looper l;
        l.prepare(sr, 5.0);
        INFO("block " << block);
        REQUIRE(run(l, x, sched, block) == expected);
    }
}

TEST_CASE("looper: a full buffer carries on as Play")
{
    Looper l;
    l.prepare(sr, 2.0); // 96000 frames
    const auto y = run(l, noise(240000), [](size_t) { return params(Looper::Record); });
    REQUIRE(allFinite(y));
    REQUIRE(l.loopLength() == 96000);
    REQUIRE(l.isPlaying());
    REQUIRE(! l.isRecording());
}

TEST_CASE("looper: finite at every sample rate, stereo and mono, with random controls and odd tempos")
{
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Looper l;
        l.prepare(rate, 1.0);
        std::mt19937 rng(9);
        std::uniform_real_distribution<float> d(-0.5f, 0.5f);
        std::uniform_int_distribution<int> pick(0, 3);
        std::vector<float> a(static_cast<size_t>(rate * 4)), b(a.size());
        for (size_t i = 0; i < a.size(); ++i)
            a[i] = d(rng), b[i] = d(rng);

        bool finite = true;
        int blk = 0;
        for (size_t pos = 0; pos < a.size(); pos += 480, ++blk)
        {
            const int n = static_cast<int>(std::min<size_t>(480, a.size() - pos));
            Looper::Params p;
            p.mode = pick(rng);
            p.speed = pick(rng) % 3;
            p.reverse = pick(rng) == 0;
            p.quantize = pick(rng) == 0;
            p.burst = pick(rng) == 0;
            p.undo = pick(rng) == 0;
            p.bpm = blk % 4 == 0 ? std::numeric_limits<double>::quiet_NaN() : blk % 4 == 1 ? -3.0 : blk % 4 == 2 ? 1e9 : 90.0;
            if (blk % 11 == 10)
                p.mode = 99, p.speed = -4; // out of range is clamped
            float* stereo[] { a.data() + pos, b.data() + pos };
            float* mono[] { a.data() + pos };
            if (blk % 3 == 2)
                l.process(mono, 1, n, p);
            else
                l.process(stereo, 2, n, p);
            for (int i = 0; i < n; ++i)
                finite &= std::isfinite(a[pos + static_cast<size_t>(i)]) && std::isfinite(b[pos + static_cast<size_t>(i)]);
        }
        INFO("rate " << rate);
        REQUIRE(finite);
    }
}

TEST_CASE("processor: Pre-FX loops go through the engine, Post-FX loops are played raw")
{
    // Tap (1/8 = 12000 frames) at Mix 1. An impulse near the end of the loop echoes 24000 frames later, i.e. past the
    // loop end: Pre-FX replays that echo in the next pass (the engine sees the loop again), Post-FX recorded only the
    // echoes that happened during the recording.
    const auto render = [](bool pre) {
        PluginProcessor p;
        configure(p, sr, 500);
        setParam(p, grain::ids::family, 3.0f);
        setParam(p, grain::ids::mix, 1.0f);
        setParam(p, grain::ids::swarm, 0.64f);
        setParam(p, grain::ids::span, 0.0f);
        setParam(p, grain::ids::tail, 0.0f);
        setParam(p, grain::ids::scatter, 0.0f);
        setParam(p, grain::ids::looppos, pre ? 0.0f : 1.0f);

        juce::AudioBuffer<float> buf(2, 500);
        juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 600; ++blk)
        {
            const int pos = blk * 500;
            if (pos == 0)
                setParam(p, grain::ids::loop, 1.0f); // Record, 72000 frames
            if (pos == 72000)
                setParam(p, grain::ids::loop, 2.0f); // Play
            buf.clear();
            if (57600 >= pos && 57600 < pos + 500)
                for (int c = 0; c < 2; ++c)
                    buf.setSample(c, 57600 - pos, 0.8f);
            p.processBlock(buf, midi);
            for (int i = 0; i < 500; ++i)
                out.push_back(buf.getSample(0, i));
        }
        return out;
    };
    const auto peakAround = [](const std::vector<float>& v, size_t centre) {
        float m = 0;
        for (size_t i = centre - 3; i <= centre + 3; ++i)
            m = std::max(m, std::abs(v[i]));
        return m;
    };
    const size_t window = 72000 + 2 * 71520 + 10080; // pass 2, position 10080
    const auto pre = render(true), post = render(false);
    INFO("pre " << peakAround(pre, window) << " post " << peakAround(post, window));
    REQUIRE(peakAround(pre, window) > 0.4f);   // 0.8 * 0.6: the wrapped echo
    REQUIRE(peakAround(post, window) < 1e-3f); // not recorded, not replayed
}

TEST_CASE("processor: the looper transport runs without allocating")
{
    PluginProcessor g;
    configure(g, sr, 256);
    const auto src = noise(48000, 0.3f, 4);
    juce::AudioBuffer<float> b(2, 256);
    juce::MidiBuffer midi;
    long allocations = 0;
    for (int blk = 0; blk < 600; ++blk)
    {
        if (blk % 40 == 0) // outside the armed region: walk through every state and switch
        {
            const int step = (blk / 40) % 6;
            setParam(g, grain::ids::loop, static_cast<float>(step == 0 ? 1 : step == 1 ? 2 : step == 2 ? 3 : step == 3 ? 2 : step == 4 ? 3 : 0));
            setParam(g, grain::ids::loopspeed, static_cast<float>(step % 3));
            setParam(g, grain::ids::loopreverse, step % 2 == 1 ? 1.0f : 0.0f);
            setParam(g, grain::ids::loopburst, step == 3 ? 1.0f : 0.0f);
            setParam(g, grain::ids::loopundo, step == 4 ? 1.0f : 0.0f);
            setParam(g, grain::ids::looppos, step < 3 ? 1.0f : 0.0f);
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                b.setSample(c, i, src[static_cast<size_t>((blk * 256 + i) % src.size())]);
        allocguard::arm();
        g.processBlock(b, midi);
        allocations += allocguard::disarm();
    }
    REQUIRE(allocations == 0);
}
