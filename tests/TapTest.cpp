#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/Tap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using grain::dsp::Tap;

namespace
{
constexpr double sr = 48000.0;
constexpr double quarters[] { 4.0, 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25 };

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

std::vector<float> impulse(double seconds, size_t at = 1000)
{
    std::vector<float> v(static_cast<size_t>(seconds * sr), 0.0f);
    v[at] = 1.0f;
    return v;
}

std::vector<float> run(Tap& t, std::vector<float> x, const Tap::Params& p, int block = 512)
{
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
        const int n = static_cast<int>(std::min(x.size() - pos, static_cast<size_t>(block)));
        float* ch[] { x.data() + pos };
        t.process(ch, 1, n, p);
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

Tap::Params wet(int variant, float swarm = 0.64f, float span = 0.0f)
{
    Tap::Params p;
    p.swarm = swarm; // 0.64 = step 4 = 1/8
    p.tail = 0.0f;
    p.span = span;
    p.scatter = 0.0f;
    p.mix = 1.0f;
    p.variant = variant;
    return p;
}

float swarmFor(int step) { return (static_cast<float>(step) + 0.5f) / 7.0f; }

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

TEST_CASE("tap: Mix 0, disabled family and silence leave the audio exactly alone")
{
    const auto x = noise(3.0);
    {
        Tap t;
        t.prepare(sr);
        auto p = wet(0);
        p.mix = 0.0f;
        REQUIRE(run(t, x, p) == x);
    }
    {
        Tap t;
        t.prepare(sr);
        auto p = wet(0);
        p.enabled = false;
        REQUIRE(run(t, x, p) == x);
    }
    {
        Tap t;
        t.prepare(sr);
        const auto y = run(t, std::vector<float>(3 * 48000, 0.0f), wet(0));
        REQUIRE(rms(y, 0, 3) == 0.0);
    }
}

TEST_CASE("tap: taps land on the tempo grid for every subdivision (Echo, 120 BPM)")
{
    for (int step = 0; step < 7; ++step)
    {
        Tap t;
        t.prepare(sr);
        const auto y = run(t, impulse(3.0), wet(0, swarmFor(step)));
        const double base = std::clamp(0.5 * quarters[step], 0.02, 2.0);

        int expectedTaps = 0;
        for (int k = 0; k < 4; ++k)
        {
            const double sec = base * (k + 1);
            if (sec > 2.0 + 1e-9)
                continue;
            ++expectedTaps;
            const auto pos = 1000 + static_cast<size_t>(std::llround(sec * sr));
            INFO("step " << step << " tap " << k << " at " << pos);
            REQUIRE(y[pos] == Catch::Approx(std::pow(0.6, k)).margin(2e-3));
        }
        int found = 0;
        for (float s : y)
            found += std::abs(s) > 0.01f ? 1 : 0;
        INFO("step " << step);
        REQUIRE(found == expectedTaps);
    }
}

TEST_CASE("tap: the tempo scales the grid, Span widens the spacing")
{
    {
        Tap t;
        t.prepare(sr);
        auto p = wet(0, swarmFor(4));
        p.bpm = 60.0; // 1/8 = 0.5 s
        const auto y = run(t, impulse(3.0), p);
        REQUIRE(y[1000 + 24000] == Catch::Approx(1.0).margin(2e-3));
        REQUIRE(y[1000 + 12000] == Catch::Approx(0.0).margin(1e-6));
    }
    {
        Tap t;
        t.prepare(sr);
        const auto y = run(t, impulse(3.0), wet(0, swarmFor(4), 1.0f)); // Span 1: 4 steps apart = 1 s
        REQUIRE(y[1000 + 48000] == Catch::Approx(1.0).margin(2e-3));
        REQUIRE(y[1000 + 96000] == Catch::Approx(0.6).margin(2e-3));
        REQUIRE(y[1000 + 12000] == Catch::Approx(0.0).margin(1e-6));
    }
}

TEST_CASE("tap: Ping-pong alternates the taps between left and right")
{
    Tap t;
    t.prepare(sr);
    auto l = impulse(3.0), r = impulse(3.0);
    const auto p = wet(1, swarmFor(2)); // 1/4 = 0.5 s
    for (size_t pos = 0; pos < l.size(); pos += 512)
    {
        const int n = static_cast<int>(std::min<size_t>(512, l.size() - pos));
        float* ch[] { l.data() + pos, r.data() + pos };
        t.process(ch, 2, n, p);
    }
    for (int k = 0; k < 4; ++k)
    {
        const size_t pos = 1000 + 24000 * static_cast<size_t>(k + 1);
        const float on = static_cast<float>(std::pow(0.6, k));
        INFO("tap " << k);
        REQUIRE((k % 2 == 0 ? l : r)[pos] == Catch::Approx(on).margin(2e-3));
        REQUIRE((k % 2 == 0 ? r : l)[pos] == Catch::Approx(0.0).margin(1e-6));
    }
}

TEST_CASE("tap: Cascade spaces the taps closer together and makes them darker")
{
    Tap t;
    t.prepare(sr);
    const auto y = run(t, impulse(3.0), wet(3, swarmFor(4))); // base 0.25 s, spacing 1, 1.75, 2.25
    const double at[] { 0.25, 0.4375, 0.5625 };
    float tapPeak[3] {};
    for (int k = 0; k < 3; ++k)
    {
        const auto c = 1000 + static_cast<size_t>(std::llround(at[k] * sr));
        double e = 0;
        for (size_t i = c - 300; i < c + 300; ++i)
        {
            e += static_cast<double>(y[i]) * y[i];
            tapPeak[k] = std::max(tapPeak[k], std::abs(y[i]));
        }
        INFO("tap " << k << " energy " << e);
        REQUIRE(e > 1e-4);
    }
    REQUIRE(tapPeak[0] == Catch::Approx(1.0).margin(2e-3)); // first tap reads the raw copy
    REQUIRE(tapPeak[2] < 0.5f * tapPeak[0]);                // quieter and low-passed
}

TEST_CASE("tap: Reverse plays Hann-windowed backwards snippets")
{
    // Swarm 0 = 1/1 = 2 s: only tap 0 is in range, snippets are 0.4 s long every 0.2 s (checked against a direct computation)
    const auto x = noise(4.0, 0.5f, 5);
    Tap t;
    t.prepare(sr);
    const auto y = run(t, x, wet(2, swarmFor(0)));

    const int G = 19200, hop = 9600, D = 96000;
    double worst = 0;
    for (int f = 2 * 48000 + 5000; f < 3 * 48000; ++f)
    {
        double ref = 0;
        for (int j = 0; j * hop <= f; ++j)
        {
            const int a = f - j * hop;
            if (a >= G)
                continue;
            const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * a / G);
            const int src = j * hop - D - a;
            ref += w * (src >= 0 ? x[static_cast<size_t>(src)] : 0.0f);
        }
        worst = std::max(worst, std::abs(ref - y[static_cast<size_t>(f)]));
    }
    INFO("worst deviation " << worst);
    REQUIRE(worst < 1e-3);
}

TEST_CASE("tap: every variant is click-free, also while the tempo changes")
{
    for (int variant = 0; variant < 4; ++variant)
        for (float scatter : { 0.0f, 1.0f })
        {
            Tap t;
            t.prepare(sr);
            auto x = sine(440.0, 6.0);
            auto p = wet(variant, swarmFor(4));
            p.scatter = scatter;
            for (size_t pos = 0; pos < x.size(); pos += 256)
            {
                p.bpm = pos > 144000 ? 90.0 : 120.0; // tempo change at 3 s
                float* ch[] { x.data() + pos };
                t.process(ch, 1, 256, p);
            }
            INFO("variant " << variant << " scatter " << scatter << " step " << maxStep(x, 24000));
            REQUIRE(allFinite(x));
            REQUIRE(maxStep(x, 24000) < 0.25f);
        }
}

TEST_CASE("tap: output does not depend on the host block size")
{
    const auto x = noise(2.0, 0.5f, 11);
    for (int variant = 0; variant < 4; ++variant)
    {
        auto p = wet(variant, swarmFor(5));
        p.scatter = 0.6f;
        p.tail = 0.6f;
        p.span = 0.5f;
        Tap ref;
        ref.prepare(sr);
        const auto expected = run(ref, x, p, 512);
        for (int block : { 480, 64, 1 })
        {
            Tap t;
            t.prepare(sr);
            INFO("variant " << variant << " block " << block);
            REQUIRE(run(t, x, p, block) == expected);
        }
    }
}

TEST_CASE("tap: feedback decays at the longest, densest settings")
{
    for (int variant = 0; variant < 4; ++variant)
        for (float swarm : { swarmFor(2), swarmFor(6) })
            for (float span : { 0.0f, 1.0f })
            {
                auto x = noise(1.0);
                x.resize(static_cast<size_t>(14.0 * sr), 0.0f);
                auto p = wet(variant, swarm, span);
                p.tail = 1.0f;
                p.scatter = 0.5f;
                Tap t;
                t.prepare(sr);
                const auto y = run(t, x, p);
                INFO("variant " << variant << " swarm " << swarm << " span " << span);
                REQUIRE(allFinite(y));
                REQUIRE(peak(y) < 4.0f);
                // loop gain up to 0.85 per pass of a delay up to 2 s: slow, but the loudest second must have clearly decayed by 13 s
                double early = 0;
                for (int s = 0; s < 8; ++s)
                    early = std::max(early, rms(y, s, s + 1));
                REQUIRE(rms(y, 13.0, 14.0) < 0.5 * early);
            }
}

TEST_CASE("tap: wet level stays within 6 dB of the dry level")
{
    const auto x = noise(6.0);
    for (int v = 0; v < 4; ++v)
    {
        Tap t;
        t.prepare(sr);
        const auto y = run(t, x, wet(v, swarmFor(4)));
        const double db = 20.0 * std::log10(rms(y, 3.0, 6.0) / rms(x, 3.0, 6.0));
        INFO("variant " << v << ": " << db << " dB");
        REQUIRE(std::abs(db) < 6.0);
    }
}

TEST_CASE("tap: finite at every sample rate and block size, stereo and mono, out-of-range params")
{
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int block : { 1, 64, 480, 4096 })
        {
            Tap t;
            t.prepare(rate);
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
                    t.process(mono, 1, n, p);
                else
                    t.process(stereo, 2, n, p);
                for (int i = 0; i < n; ++i)
                    finite &= std::isfinite(l[pos + static_cast<size_t>(i)]) && std::isfinite(r[pos + static_cast<size_t>(i)]);
            }
            INFO("rate " << rate << " block " << block);
            REQUIRE(finite);
        }
}

TEST_CASE("tap: deterministic")
{
    const auto x = noise(3.0);
    Tap a, b;
    a.prepare(sr);
    b.prepare(sr);
    auto p = wet(2);
    p.scatter = 1.0f;
    p.tail = 0.5f;
    REQUIRE(run(a, x, p) == run(b, x, p));
}

namespace
{
struct FixedTempo : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm(60.0);
        return info;
    }
};
} // namespace

TEST_CASE("processor: Tap follows the host tempo and does not allocate")
{
    const auto src = noise(1.0, 0.3f, 4);
    juce::AudioBuffer<float> b(2, 256);
    juce::MidiBuffer midi;

    // tempo: host says 60 BPM, so 1/8 is 0.5 s (the standalone fallback of 120 BPM would give 0.25 s)
    PluginProcessor q;
    configure(q, sr, 256);
    FixedTempo head;
    q.setPlayHead(&head);
    setParam(q, grain::ids::mix, 1.0f);
    setParam(q, grain::ids::family, 3.0f);
    setParam(q, grain::ids::variant, 0.0f);
    setParam(q, grain::ids::swarm, swarmFor(4));
    setParam(q, grain::ids::span, 0.0f);
    setParam(q, grain::ids::tail, 0.0f);
    setParam(q, grain::ids::scatter, 0.0f);
    std::vector<float> out;
    for (int blk = 0; blk < 120; ++blk) // 0.64 s
    {
        for (int c = 0; c < 2; ++c)
        {
            b.clear(c, 0, 256);
            if (blk == 0)
                b.setSample(c, 100, 1.0f);
        }
        q.processBlock(b, midi);
        for (int i = 0; i < 256; ++i)
            out.push_back(b.getSample(0, i));
    }
    REQUIRE(out[100 + 24000] == Catch::Approx(1.0).margin(5e-3));
    REQUIRE(out[100 + 12000] == Catch::Approx(0.0).margin(1e-6));

    // allocation guard with Tap engaged, all variants
    PluginProcessor g;
    configure(g, sr, 256);
    setParam(g, grain::ids::mix, 1.0f);
    setParam(g, grain::ids::family, 3.0f);
    setParam(g, grain::ids::scatter, 0.5f);
    setParam(g, grain::ids::tail, 0.8f);
    long allocations = 0;
    for (int blk = 0; blk < 300; ++blk)
    {
        if (blk % 40 == 0)
        {
            setParam(g, grain::ids::variant, static_cast<float>((blk / 40) % 4)); // outside the armed region
            setParam(g, grain::ids::swarm, swarmFor((blk / 40) % 7));
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
