#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "dsp/DualEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

namespace
{
using grain::dsp::DualEngine;
using grain::dsp::EngineSet;

constexpr double sr = 48000.0;

struct Stereo
{
    std::vector<float> l, r;
    bool operator==(const Stereo&) const = default;
};

Stereo noise(int n, float amplitude = 0.3f)
{
    Stereo s { std::vector<float>(static_cast<size_t>(n)), std::vector<float>(static_cast<size_t>(n)) };
    juce::Random rng(1234);
    for (int i = 0; i < n; ++i)
    {
        s.l[static_cast<size_t>(i)] = amplitude * (2.0f * rng.nextFloat() - 1.0f);
        s.r[static_cast<size_t>(i)] = amplitude * (2.0f * rng.nextFloat() - 1.0f);
    }
    return s;
}

Stereo sine(int n, float hz = 220.0f)
{
    Stereo s { std::vector<float>(static_cast<size_t>(n)), std::vector<float>(static_cast<size_t>(n)) };
    for (int i = 0; i < n; ++i)
        s.l[static_cast<size_t>(i)] = s.r[static_cast<size_t>(i)] =
            0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * hz * static_cast<float>(i) / static_cast<float>(sr));
    return s;
}

EngineSet::Params slot(int family, int variant, float mix)
{
    EngineSet::Params p;
    p.swarm = 0.6f;
    p.tail = 0.4f;
    p.span = 0.5f;
    p.scatter = 0.3f;
    p.mix = mix;
    p.family = family;
    p.variant = variant;
    return p;
}

template <typename Proc>
void runBlocks(Stereo& s, int block, Proc&& proc)
{
    const int n = static_cast<int>(s.l.size());
    for (int pos = 0; pos < n; pos += block)
    {
        const int m = std::min(block, n - pos);
        float* ch[] { s.l.data() + pos, s.r.data() + pos };
        proc(ch, m, pos);
    }
}

Stereo runDual(Stereo s, int block, const DualEngine::Params& p)
{
    auto dual = std::make_unique<DualEngine>();
    dual->prepare(sr, p);
    runBlocks(s, block, [&](float* const* ch, int m, int) { dual->process(ch, 2, m, p); });
    return s;
}

Stereo runSet(Stereo s, int block, const EngineSet::Params& p)
{
    auto set = std::make_unique<EngineSet>();
    set->prepare(sr);
    runBlocks(s, block, [&](float* const* ch, int m, int) { set->process(ch, 2, m, p); });
    return s;
}

float maxStep(const Stereo& s)
{
    float m = 0.0f;
    for (size_t i = 1; i < s.l.size(); ++i)
        m = std::max({ m, std::abs(s.l[i] - s.l[i - 1]), std::abs(s.r[i] - s.r[i - 1]) });
    return m;
}
} // namespace

TEST_CASE("slot B off: exactly one engine slot")
{
    const auto in = noise(48000);
    DualEngine::Params p;
    p.a = slot(1, 0, 0.6f);
    p.b = slot(3, 1, 0.5f);
    p.bOn = false;
    REQUIRE(runDual(in, 512, p) == runSet(in, 512, p.a));
}

TEST_CASE("Serial is A then B, Parallel is (A + B) / 2, bit-exact")
{
    const auto in = noise(48000);
    for (int fb : { 0, 1, 2, 3 })
    {
        INFO("family B " << fb);
        DualEngine::Params p;
        p.a = slot(1, 2, 0.7f);
        p.b = slot(fb, 1, 0.5f);
        p.bOn = true;

        p.route = 0;
        const auto serial = runDual(in, 512, p);
        REQUIRE(serial == runSet(runSet(in, 512, p.a), 512, p.b));

        p.route = 1;
        const auto parallel = runDual(in, 512, p);
        const auto a = runSet(in, 512, p.a), b = runSet(in, 512, p.b);
        Stereo expected = a;
        for (size_t i = 0; i < expected.l.size(); ++i)
        {
            expected.l[i] = 0.5f * (a.l[i] + b.l[i]);
            expected.r[i] = 0.5f * (a.r[i] + b.r[i]);
        }
        REQUIRE(parallel == expected);
    }
}

TEST_CASE("Parallel with both slots at Mix 0 is the input, bit-exact")
{
    const auto in = noise(24000);
    DualEngine::Params p;
    p.a = slot(0, 0, 0.0f);
    p.b = slot(2, 3, 0.0f);
    p.bOn = true;
    p.route = 1;
    REQUIRE(runDual(in, 512, p) == in);
}

TEST_CASE("both routes give the same result at any block size")
{
    const auto in = noise(30000);
    for (int route : { 0, 1 })
    {
        DualEngine::Params p;
        p.a = slot(2, 0, 0.6f);
        p.b = slot(1, 1, 0.6f);
        p.bOn = true;
        p.route = route;
        const auto ref = runDual(in, 512, p);
        for (int block : { 480, 100, 1, 4096 })
        {
            INFO("route " << route << " block " << block);
            REQUIRE(runDual(in, block, p) == ref);
        }
    }
}

TEST_CASE("switching B on and off and changing the route does not click")
{
    // A dry (Mix 0), B a Tap echo of a sine: every change is a crossfade between smooth signals
    const auto in = sine(96000);
    DualEngine::Params p;
    p.a = slot(0, 0, 0.0f);
    p.b = slot(3, 0, 0.5f);
    p.bOn = false;
    p.route = 0;

    auto dual = std::make_unique<DualEngine>();
    dual->prepare(sr, p);
    Stereo out = in;
    runBlocks(out, 256, [&](float* const* ch, int m, int pos) {
        const int t = pos / 12000; // a change every 0.25 s
        p.bOn = t % 4 != 0;
        p.route = (t / 2) % 2;
        dual->process(ch, 2, m, p);
    });
    for (float x : out.l)
        REQUIRE(std::isfinite(x));
    REQUIRE(maxStep(out) < 3.0f * maxStep(in)); // a hard switch jumps by up to the full amplitude (~0.5)
}

TEST_CASE("slot B does not allocate on the audio thread, in either route or while switching")
{
    auto in = noise(48000);
    DualEngine::Params p;
    p.a = slot(1, 0, 0.5f);
    p.b = slot(3, 2, 0.5f);
    p.bOn = true;
    auto dual = std::make_unique<DualEngine>();
    dual->prepare(sr, p);

    allocguard::arm();
    runBlocks(in, 300, [&](float* const* ch, int m, int pos) {
        p.route = (pos / 6000) % 2;
        p.bOn = (pos / 9000) % 3 != 0;
        p.b.family = (pos / 15000) % 4;
        dual->process(ch, 2, m, p);
    });
    REQUIRE(allocguard::disarm() == 0);
}

TEST_CASE("both slots at maximum Tail stay finite and do not grow")
{
    DualEngine::Params p;
    p.a = slot(1, 0, 1.0f);
    p.b = slot(3, 0, 1.0f);
    p.a.tail = p.b.tail = 1.0f;
    p.bOn = true;
    auto dual = std::make_unique<DualEngine>();
    dual->prepare(sr, p);

    Stereo s = noise(48000 * 9, 0.5f);
    for (size_t i = 48000; i < s.l.size(); ++i)
        s.l[i] = s.r[i] = 0.0f; // 1 s of noise, then 8 s of silence
    runBlocks(s, 512, [&](float* const* ch, int m, int) { dual->process(ch, 2, m, p); });

    const auto rms = [&s](size_t from, size_t to) {
        double sum = 0.0;
        for (size_t i = from; i < to; ++i)
            sum += static_cast<double>(s.l[i]) * s.l[i];
        return std::sqrt(sum / static_cast<double>(to - from));
    };
    for (float x : s.l)
        REQUIRE(std::isfinite(x));
    std::vector<double> perSecond;
    for (size_t sec = 0; sec < 9; ++sec)
        perSecond.push_back(rms(48000 * sec, 48000 * (sec + 1)));
    CAPTURE(perSecond);
    // the wet signal starts empty (Mix 1), peaks in second 1, then every second is quieter than the one before
    for (size_t sec = 2; sec < perSecond.size(); ++sec)
        REQUIRE(perSecond[sec] < perSecond[sec - 1]);
}

TEST_CASE("slot B: off by default, off after a factory preset, kept by user presets, off for old states")
{
    PluginProcessor p;
    REQUIRE(p.apvts.getRawParameterValue(grain::ids::family2)->load() == 0.0f);

    const auto set = [&p](const char* id, float plain) {
        auto* prm = p.apvts.getParameter(id);
        prm->setValueNotifyingHost(prm->convertTo0to1(plain));
    };
    set(grain::ids::family2, 2.0f); // Cloud
    set(grain::ids::route2, 1.0f);
    p.presets.applyFactory(0);
    REQUIRE(p.apvts.getRawParameterValue(grain::ids::family2)->load() == 0.0f);
    REQUIRE(p.apvts.getRawParameterValue(grain::ids::route2)->load() == 0.0f);

    // user preset round trip keeps B
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("grain_routing_test");
    folder.deleteRecursively();
    grain::presets::PresetManager manager(p.apvts, folder);
    set(grain::ids::family2, 4.0f); // Tap
    set(grain::ids::variant2, 2.0f);
    set(grain::ids::route2, 1.0f);
    REQUIRE(manager.saveUser("Two Engines"));
    set(grain::ids::family2, 0.0f);
    set(grain::ids::route2, 0.0f);
    REQUIRE(manager.applyUser("Two Engines"));
    REQUIRE(p.apvts.getRawParameterValue(grain::ids::family2)->load() == 4.0f);
    REQUIRE(p.apvts.getRawParameterValue(grain::ids::variant2)->load() == 2.0f);
    REQUIRE(p.apvts.getRawParameterValue(grain::ids::route2)->load() == 1.0f);

    // a user file from before slot B (no family2 entry) turns B off
    folder.getChildFile("Old.xml").replaceWithText(R"(<GrainPreset version="1" name="Old"><P id="swarm" value="0.3"/></GrainPreset>)");
    REQUIRE(manager.applyUser("Old"));
    REQUIRE(p.apvts.getRawParameterValue(grain::ids::family2)->load() == 0.0f);
    folder.deleteRecursively();

    // a project state from before slot B loads with B off
    PluginProcessor fresh, b;
    juce::MemoryBlock block;
    fresh.getStateInformation(block);
    auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(xml != nullptr);
    for (auto* id : grain::presets::slotBIds)
        for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
            if (child->getStringAttribute("id") == id)
            {
                xml->removeChildElement(child, true);
                break;
            }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);
    b.apvts.getParameter(grain::ids::family2)->setValueNotifyingHost(1.0f);
    b.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    REQUIRE(b.apvts.getRawParameterValue(grain::ids::family2)->load() == 0.0f);
}
