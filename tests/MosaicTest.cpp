#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/Mosaic.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using grain::dsp::Mosaic;

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

// mono run in blocks of `block`; params can change per block through `paramsFor(blockIndex)`
template <typename ParamsFn>
std::vector<float> run(Mosaic& m, std::vector<float> x, int block, ParamsFn paramsFor)
{
    int b = 0;
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block), ++b)
    {
        const int n = static_cast<int>(std::min(x.size() - pos, static_cast<size_t>(block)));
        float* ch[] { x.data() + pos };
        m.process(ch, 1, n, paramsFor(b));
    }
    return x;
}

std::vector<float> run(Mosaic& m, std::vector<float> x, const Mosaic::Params& p, int block = 512)
{
    return run(m, std::move(x), block, [&](int) { return p; });
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

Mosaic::Params wet(int variant, float swarm = 1.0f)
{
    Mosaic::Params p;
    p.swarm = swarm;
    p.tail = 1.0f;
    p.span = 0.5f;
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

TEST_CASE("mosaic: Mix 0, disabled family and silence leave the audio exactly alone")
{
    const auto x = noise(3.0);
    {
        Mosaic m;
        m.prepare(sr);
        auto p = wet(0);
        p.mix = 0.0f;
        REQUIRE(run(m, x, p) == x);
    }
    {
        Mosaic m;
        m.prepare(sr);
        auto p = wet(0);
        p.enabled = false;
        REQUIRE(run(m, x, p) == x);
    }
    {
        Mosaic m; // switching off after playing: the wet fades out, then the signal is bit-exact again
        m.prepare(sr);
        auto y = run(m, noise(4.0), 512, [](int b) { auto p = wet(0); p.enabled = b < 200; return p; });
        const auto in = noise(4.0);
        REQUIRE(std::equal(y.begin() + 3 * 48000, y.end(), in.begin() + 3 * 48000));
    }
    {
        Mosaic m;
        m.prepare(sr);
        const auto y = run(m, std::vector<float>(3 * 48000, 0.0f), wet(0));
        REQUIRE(rms(y, 0, 3) == 0.0);
    }
}

TEST_CASE("mosaic: layers play at 0.5x / 1x / 2x / 4x")
{
    // Span 1 = 0.75 s loops, so grabs are 1.5 s apart (>= 2 loop lengths) and the window 3.2..4.4 s lies inside
    // one grabbed loop (grabs at about 3.0 s and 4.5 s), where every layer is a steady tone.
    const auto x = sine(440.0, 6.0);
    struct Case
    {
        int variant;
        std::vector<double> present, absent;
    };
    // 660 Hz and 330 Hz are not octaves of 440, so they only see leakage and seam sidebands
    for (const auto& c : { Case { 0, { 220, 440, 880, 1760 }, { 330, 660 } },
                           Case { 1, { 220, 440 }, { 880, 1760, 660 } },
                           Case { 2, { 440, 880, 1760 }, { 220, 660 } } })
    {
        Mosaic m;
        m.prepare(sr);
        auto p = wet(c.variant);
        p.span = 1.0f;
        const auto y = run(m, x, p);
        INFO("variant " << c.variant);
        for (double f : { 110.0, 220.0, 330.0, 440.0, 660.0, 880.0, 1320.0, 1760.0, 3520.0 })
            UNSCOPED_INFO("  " << f << " Hz: " << tone(y, f, 3.2, 4.4));
        double weakestPresent = 1e30, strongestAbsent = 0;
        for (double f : c.present)
            weakestPresent = std::min(weakestPresent, tone(y, f, 3.2, 4.4));
        for (double f : c.absent)
            strongestAbsent = std::max(strongestAbsent, tone(y, f, 3.2, 4.4));
        REQUIRE(weakestPresent > 8.0 * strongestAbsent);
    }
}

TEST_CASE("mosaic: no clicks at loop seams, grabs and variant changes")
{
    // bound = sum(gain*speed) <= 3.0 times the input slope, plus margin for the crossfade slopes
    for (double hz : { 100.0, 440.0 })
    {
        const float slope = 0.5f * static_cast<float>(2.0 * std::numbers::pi * hz / sr);
        const float bound = 3.1f * slope * 1.3f + 0.01f;
        for (int variant = 0; variant < 4; ++variant)
        {
            Mosaic m;
            m.prepare(sr);
            auto p = wet(variant);
            p.scatter = 0.5f;
            const auto y = run(m, sine(hz, 6.0), p, 256);
            INFO("hz " << hz << " variant " << variant << " step " << maxStep(y) << " bound " << bound);
            REQUIRE(allFinite(y));
            REQUIRE(maxStep(y, 24000) < bound);
        }

        Mosaic m; // variant switching every second, with immediate regrabs
        m.prepare(sr);
        const auto y = run(m, sine(hz, 6.0), 256, [](int b) { return wet((b * 256 / 48000) % 4); });
        INFO("variant switching, hz " << hz << " step " << maxStep(y));
        REQUIRE(allFinite(y));
        REQUIRE(maxStep(y, 24000) < bound);
    }
}

TEST_CASE("mosaic: Swarm sets the grab rate")
{
    Mosaic slow, fast;
    slow.prepare(sr);
    fast.prepare(sr);
    auto s = wet(0, 0.0f), f = wet(0, 1.0f);
    s.span = f.span = 0.2f; // short loops (49 ms), so Swarm sets the rate, not the 2-loop minimum
    run(slow, noise(5.0), s);
    run(fast, noise(5.0), f);
    REQUIRE(slow.grabCount() >= 1);
    REQUIRE(fast.grabCount() > slow.grabCount() + 10);
}

TEST_CASE("mosaic: Tail 0 dies quickly, Tail 1 sustains the grabbed loop")
{
    // swarm 0 = one grab every 3 s: the second grab at ~3 s catches the sine, which then stops at 3.2 s
    auto in = sine(440.0, 3.2);
    in.resize(static_cast<size_t>(5.5 * sr), 0.0f);
    double level[2];
    for (int t = 0; t < 2; ++t)
    {
        Mosaic m;
        m.prepare(sr);
        auto p = wet(1, 0.0f);
        p.tail = static_cast<float>(t);
        level[t] = rms(run(m, in, p), 4.2, 5.2);
    }
    REQUIRE(level[0] < 0.005);
    REQUIRE(level[1] > 0.1);
}

TEST_CASE("mosaic: wet level stays within 6 dB of the dry level")
{
    const auto x = noise(6.0);
    for (int v = 0; v < 4; ++v)
    {
        Mosaic m;
        m.prepare(sr);
        const auto y = run(m, x, wet(v));
        const double db = 20.0 * std::log10(rms(y, 3.0, 6.0) / rms(x, 3.0, 6.0));
        INFO("variant " << v << ": " << db << " dB");
        REQUIRE(std::abs(db) < 6.0);
    }
}

TEST_CASE("mosaic: output does not depend on the host block size")
{
    const auto x = noise(2.0, 0.5f, 11);
    auto p = wet(2, 0.8f);
    p.scatter = 0.6f;
    p.span = 0.3f;
    Mosaic ref;
    ref.prepare(sr);
    const auto expected = run(ref, x, p, 512);
    for (int block : { 480, 64, 1 })
    {
        Mosaic m;
        m.prepare(sr);
        INFO("block " << block);
        REQUIRE(run(m, x, p, block) == expected);
    }
}

TEST_CASE("mosaic: out-of-range parameters are clamped, not trusted")
{
    for (double rate : { 44100.0, 192000.0 })
    {
        Mosaic m;
        m.prepare(rate);
        Mosaic::Params p;
        p.span = 5.0f;
        p.scatter = 7.0f;
        p.swarm = -3.0f;
        p.tail = std::numeric_limits<float>::quiet_NaN();
        p.mix = 9.0f;
        p.variant = 99;
        std::vector<float> l(static_cast<size_t>(rate)), r(l.size());
        std::mt19937 rng(5);
        std::uniform_real_distribution<float> d(-0.5f, 0.5f);
        for (size_t i = 0; i < l.size(); ++i)
            l[i] = d(rng), r[i] = d(rng);
        bool finite = true;
        for (size_t pos = 0; pos < l.size(); pos += 512)
        {
            const int n = static_cast<int>(std::min<size_t>(512, l.size() - pos));
            float* ch[] { l.data() + pos, r.data() + pos };
            m.process(ch, 2, n, p);
            for (int i = 0; i < n; ++i)
                finite &= std::isfinite(l[pos + static_cast<size_t>(i)]) && std::isfinite(r[pos + static_cast<size_t>(i)]);
        }
        REQUIRE(finite);
        REQUIRE(m.grabCount() >= 1);
    }
}

TEST_CASE("mosaic: the empty first loop is replaced quickly, even at Swarm 0")
{
    Mosaic m;
    m.prepare(sr);
    run(m, sine(440.0, 0.5), wet(0, 0.0f)); // Swarm 0 = a grab every 3 s, but the startup grab follows after 80 ms
    REQUIRE(m.grabCount() >= 2);
}

TEST_CASE("mosaic: deterministic")
{
    const auto x = noise(3.0);
    Mosaic a, b;
    a.prepare(sr);
    b.prepare(sr);
    auto p = wet(3);
    p.scatter = 1.0f;
    REQUIRE(run(a, x, p) == run(b, x, p));
}

TEST_CASE("mosaic: finite at every sample rate and block size, stereo and mono")
{
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int block : { 1, 64, 480, 4096 })
        {
            Mosaic m;
            m.prepare(rate);
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
                auto p = wet(b % 4);
                p.scatter = 0.7f;
                p.span = 0.9f;
                float* stereo[] { l.data() + pos, r.data() + pos };
                float* mono[] { l.data() + pos };
                if (b % 5 == 4)
                    m.process(mono, 1, n, p);
                else
                    m.process(stereo, 2, n, p);
                for (int i = 0; i < n; ++i)
                    finite &= std::isfinite(l[pos + static_cast<size_t>(i)]) && std::isfinite(r[pos + static_cast<size_t>(i)]);
            }
            INFO("rate " << rate << " block " << block);
            REQUIRE(finite);
        }
}

TEST_CASE("processor: other families are dry, Mosaic grabs without allocating")
{
    PluginProcessor p;
    configure(p, sr, 256);
    setParam(p, grain::ids::mix, 1.0f);
    setParam(p, grain::ids::family, 1.0f); // Cloud: not written yet, must be bit-exact dry

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

    setParam(p, grain::ids::family, 0.0f);
    setParam(p, grain::ids::swarm, 1.0f);
    setParam(p, grain::ids::scatter, 0.5f);
    const int grabsBefore = p.mosaicGrabCount();
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
    REQUIRE(p.mosaicGrabCount() > grabsBefore + 5); // grabs (the biggest copy) really ran under the guard
    REQUIRE(allocations == 0);
}
