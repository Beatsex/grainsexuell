#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/Cloud.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using grain::dsp::Cloud;

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

std::vector<float> run(Cloud& c, std::vector<float> x, const Cloud::Params& p, int block = 512)
{
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
        const int n = static_cast<int>(std::min(x.size() - pos, static_cast<size_t>(block)));
        float* ch[] { x.data() + pos };
        c.process(ch, 1, n, p);
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

Cloud::Params wet(int variant, float swarm = 0.7f, float span = 0.5f)
{
    Cloud::Params p;
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

TEST_CASE("cloud: Mix 0, disabled family and silence leave the audio exactly alone")
{
    const auto x = noise(3.0);
    {
        Cloud c;
        c.prepare(sr);
        auto p = wet(0);
        p.mix = 0.0f;
        REQUIRE(run(c, x, p) == x);
    }
    {
        Cloud c;
        c.prepare(sr);
        auto p = wet(0);
        p.enabled = false;
        REQUIRE(run(c, x, p) == x);
    }
    {
        Cloud c;
        c.prepare(sr);
        const auto y = run(c, std::vector<float>(3 * 48000, 0.0f), wet(0));
        REQUIRE(rms(y, 0, 3) == 0.0);
    }
}

TEST_CASE("cloud: variants play the right pitches")
{
    const auto x = sine(440.0, 6.0);
    struct Case
    {
        int variant;
        std::vector<double> present, absent;
    };
    for (const auto& c : { Case { 0, { 440 }, { 880, 220, 660 } },
                           Case { 1, { 440, 880 }, { 220, 660, 1320 } },
                           Case { 2, { 220, 440 }, { 880, 660, 1320 } } })
    {
        Cloud cl;
        cl.prepare(sr);
        const auto y = run(cl, x, wet(c.variant));
        INFO("variant " << c.variant);
        for (double f : { 220.0, 440.0, 660.0, 880.0, 1320.0 })
            UNSCOPED_INFO("  " << f << " Hz: " << tone(y, f, 3.0, 6.0));
        double weakestPresent = 1e30, strongestAbsent = 0;
        for (double f : c.present)
            weakestPresent = std::min(weakestPresent, tone(y, f, 3.0, 6.0));
        for (double f : c.absent)
            strongestAbsent = std::max(strongestAbsent, tone(y, f, 3.0, 6.0));
        REQUIRE(weakestPresent > 8.0 * strongestAbsent);
    }
}

TEST_CASE("cloud: Swarm sets the grain density")
{
    Cloud c;
    c.prepare(sr);
    run(c, noise(4.0), wet(0, 0.5f)); // density 3 * (80/3)^0.5 = 15.5 grains/s
    REQUIRE(c.grainsSpawned() >= 46);
    REQUIRE(c.grainsSpawned() <= 78);

    Cloud dense;
    dense.prepare(sr);
    run(dense, noise(4.0), wet(0, 1.0f)); // 80 grains/s
    REQUIRE(dense.grainsSpawned() > 4 * c.grainsSpawned());
}

TEST_CASE("cloud: every grain is windowed, so there are no clicks")
{
    // a missing window would give steps of grain gain * input amplitude (0.4 and more)
    for (double hz : { 100.0, 440.0 })
        for (int variant = 0; variant < 4; ++variant)
            for (float scatter : { 0.0f, 1.0f })
                for (float swarm : { 0.5f, 1.0f })
                {
                    Cloud c;
                    c.prepare(sr);
                    auto p = wet(variant, swarm);
                    p.scatter = scatter;
                    const auto y = run(c, sine(hz, 4.0), p, 256);
                    INFO("hz " << hz << " variant " << variant << " scatter " << scatter << " swarm " << swarm
                               << " step " << maxStep(y));
                    REQUIRE(allFinite(y));
                    REQUIRE(maxStep(y, 24000) < (swarm > 0.9f ? 0.4f : 0.25f));
                }
}

TEST_CASE("cloud: output does not depend on the host block size")
{
    const auto x = noise(2.0, 0.5f, 11);
    for (int variant : { 1, 3 })
    {
        auto p = wet(variant, 0.8f);
        p.scatter = 0.6f;
        p.tail = 0.6f; // feedback on
        Cloud ref;
        ref.prepare(sr);
        const auto expected = run(ref, x, p, 512);
        for (int block : { 480, 64, 1 })
        {
            Cloud c;
            c.prepare(sr);
            INFO("variant " << variant << " block " << block);
            REQUIRE(run(c, x, p, block) == expected);
        }
    }
}

TEST_CASE("cloud: feedback decays, even for a steady tone at the densest setting")
{
    for (bool tonal : { false, true })
        for (float swarm : { 0.7f, 1.0f })
            for (float span : { 0.5f, 1.0f })
            {
                auto x = tonal ? sine(440.0, 1.0) : noise(1.0);
                x.resize(static_cast<size_t>(10.0 * sr), 0.0f);
                auto p = wet(0, swarm, span);
                p.tail = 1.0f;
                Cloud c;
                c.prepare(sr);
                const auto y = run(c, x, p);
                INFO("tonal " << tonal << " swarm " << swarm << " span " << span);
                REQUIRE(allFinite(y));
                REQUIRE(peak(y) < 4.0f);
                REQUIRE(rms(y, 9.0, 10.0) < 0.15 * rms(y, 0.5, 1.0)); // long grains = slow passes, about -3 dB/s
            }
}

TEST_CASE("cloud: wet level stays within 6 dB of the dry level")
{
    const auto x = noise(6.0);
    for (int v = 0; v < 4; ++v)
    {
        Cloud c;
        c.prepare(sr);
        const auto y = run(c, x, wet(v, 0.8f));
        const double db = 20.0 * std::log10(rms(y, 3.0, 6.0) / rms(x, 3.0, 6.0));
        INFO("variant " << v << ": " << db << " dB");
        REQUIRE(std::abs(db) < 6.0);
    }
}

TEST_CASE("cloud: finite at every sample rate and block size, stereo and mono, out-of-range params")
{
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int block : { 1, 64, 480, 4096 })
        {
            Cloud c;
            c.prepare(rate);
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
                auto p = wet(b % 4, 1.0f, 1.0f);
                p.scatter = 1.0f;
                p.tail = 1.0f;
                if (b % 7 == 6) // out of range values must be clamped, not trusted
                {
                    p.span = 5.0f;
                    p.scatter = 7.0f;
                    p.swarm = -3.0f;
                    p.tail = std::numeric_limits<float>::quiet_NaN();
                    p.mix = 9.0f;
                    p.variant = 99;
                }
                float* stereo[] { l.data() + pos, r.data() + pos };
                float* mono[] { l.data() + pos };
                if (b % 5 == 4)
                    c.process(mono, 1, n, p);
                else
                    c.process(stereo, 2, n, p);
                for (int i = 0; i < n; ++i)
                    finite &= std::isfinite(l[pos + static_cast<size_t>(i)]) && std::isfinite(r[pos + static_cast<size_t>(i)]);
            }
            INFO("rate " << rate << " block " << block);
            REQUIRE(finite);
            REQUIRE(c.grainsSpawned() > 0);
        }
}

TEST_CASE("cloud: deterministic")
{
    const auto x = noise(3.0);
    Cloud a, b;
    a.prepare(sr);
    b.prepare(sr);
    auto p = wet(3);
    p.scatter = 1.0f;
    p.tail = 0.5f;
    REQUIRE(run(a, x, p) == run(b, x, p));
}

TEST_CASE("processor: Cloud runs for family 1 only and spawns grains without allocating")
{
    PluginProcessor p;
    configure(p, sr, 256);
    setParam(p, grain::ids::mix, 1.0f);
    setParam(p, grain::ids::family, 2.0f); // Shard: not written yet, must be bit-exact dry

    const auto src = noise(1.0, 0.3f, 4);
    juce::AudioBuffer<float> b(2, 256);
    juce::MidiBuffer midi;
    bool exact = true;
    for (int blk = 0; blk < 40; ++blk)
    {
        for (int c = 0; c < 2; ++c)
            std::copy_n(src.begin() + blk * 256, 256, b.getWritePointer(c));
        p.processBlock(b, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                exact &= b.getSample(c, i) == src[static_cast<size_t>(blk * 256 + i)];
    }
    REQUIRE(exact);
    REQUIRE(p.cloudGrainCount() == 0);

    setParam(p, grain::ids::family, 1.0f);
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
    REQUIRE(p.cloudGrainCount() > 50); // grains really spawned under the guard
    REQUIRE(allocations == 0);
}

TEST_CASE("cloud: dense clouds are not louder than the input (grains must not add up coherently)")
{
    const auto x = noise(6.0);
    for (float span : { 0.5f, 1.0f })
    {
        Cloud c;
        c.prepare(sr);
        const auto y = run(c, x, wet(0, 1.0f, span)); // scatter 0: the case that used to be +13 dB
        const double db = 20.0 * std::log10(rms(y, 3.0, 6.0) / rms(x, 3.0, 6.0));
        INFO("span " << span << ": " << db << " dB");
        REQUIRE(std::abs(db) < 4.0);
    }
}
