#include "PluginProcessor.h"
#include "dsp/Gain.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace
{
constexpr double sr = 48000.0;

std::vector<float> sine(int n)
{
    std::vector<float> x(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        x[static_cast<size_t>(i)] = 0.4f * std::sin(0.05f * static_cast<float>(i));
    return x;
}

// runs the gain over `x` in blocks of `block`, target `db` the whole time
std::vector<float> run(std::vector<float> x, int block, float db)
{
    grain::dsp::Gain g;
    g.prepare(sr);
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
        const int m = static_cast<int>(std::min(static_cast<size_t>(block), x.size() - pos));
        float* ch[] { x.data() + pos, x.data() + pos };
        // a mono buffer shared by both channels would be scaled twice, so process one channel
        g.process(ch, 1, m, db);
    }
    return x;
}

void prepare(PluginProcessor& p)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(p.setBusesLayout(layout));
    p.setRateAndBufferSizeDetails(sr, 256);
    p.prepareToPlay(sr, 256);
    p.apvts.getParameter(grain::ids::mix)->setValueNotifyingHost(0.0f); // bit-exact dry through the engines
}

void setDb(PluginProcessor& p, const char* id, float db)
{
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(db));
}

float lastBlockPeak(PluginProcessor& p, float amplitude, int blocks = 40)
{
    juce::AudioBuffer<float> buffer(2, 256);
    float peak = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 256; ++i)
            for (int c = 0; c < 2; ++c)
                buffer.setSample(c, i, (i % 2 == 0 ? 1.0f : -1.0f) * amplitude);
        juce::MidiBuffer midi;
        p.processBlock(buffer, midi);
        peak = buffer.getMagnitude(0, 256);
    }
    return peak;
}
} // namespace

TEST_CASE("Gain: 0 dB is bit-exact, +/-6 dB scales, NaN and huge values are clamped")
{
    const auto in = sine(4800);
    REQUIRE(run(in, 256, 0.0f) == in);

    const auto up = run(in, 256, 6.0206f);
    const auto down = run(in, 256, -6.0206f);
    for (size_t i = 3000; i < in.size(); ++i) // after the 20 ms ramp
    {
        REQUIRE(up[i] == Catch::Approx(2.0f * in[i]).margin(1e-5));
        REQUIRE(down[i] == Catch::Approx(0.5f * in[i]).margin(1e-5));
    }

    const auto nan = run(in, 256, std::nanf(""));
    REQUIRE(nan == in); // treated as 0 dB
    const auto huge = run(in, 256, 1000.0f); // clamped to +24 dB = x15.85
    REQUIRE(huge[4000] == Catch::Approx(15.8489f * in[4000]).margin(1e-3));
}

TEST_CASE("Gain: the ramp does not depend on the block size and has no jump")
{
    const auto in = sine(4800);
    const auto a = run(in, 512, 12.0f);
    for (int block : { 480, 100, 1 })
        REQUIRE(run(in, block, 12.0f) == a);

    // the biggest step of the gained sine stays near the plain sine's biggest step times the final gain
    float maxStep = 0.0f;
    for (size_t i = 1; i < a.size(); ++i)
        maxStep = std::max(maxStep, std::abs(a[i] - a[i - 1]));
    REQUIRE(maxStep < 0.4f * 0.05f * 3.99f * 1.1f); // 0.4 * w * gain(+12 dB = 3.98)
}

TEST_CASE("Input and Output Gain move the level, in either order of use, without touching other settings")
{
    const auto lin = [](float db) { return std::pow(10.0f, db / 20.0f); }; // parameter steps are 0.1 dB
    PluginProcessor p;
    prepare(p);
    REQUIRE(lastBlockPeak(p, 0.2f) == Catch::Approx(0.2f).margin(1e-7f)); // 0 dB: untouched

    setDb(p, grain::ids::ingain, 6.0f);
    REQUIRE(lastBlockPeak(p, 0.2f) == Catch::Approx(0.2f * lin(6.0f)).margin(1e-4f));
    setDb(p, grain::ids::outgain, -6.0f);
    REQUIRE(lastBlockPeak(p, 0.2f) == Catch::Approx(0.2f).margin(1e-4f)); // +6 in, -6 out

    setDb(p, grain::ids::ingain, 0.0f);
    REQUIRE(lastBlockPeak(p, 0.2f) == Catch::Approx(0.2f * lin(-6.0f)).margin(1e-4f)); // only -6 out
    setDb(p, grain::ids::outgain, 0.0f);
    REQUIRE(lastBlockPeak(p, 0.2f) == Catch::Approx(0.2f).margin(1e-7f));
}

TEST_CASE("Input Gain acts before Auto Gain: Auto Gain gives the boosted level back")
{
    PluginProcessor p;
    prepare(p);
    p.apvts.getParameter(grain::ids::autogain)->setValueNotifyingHost(1.0f);
    setDb(p, grain::ids::ingain, 12.0f);
    const float peak = lastBlockPeak(p, 0.05f, 400); // quiet input: Auto Gain neither boosts nor cuts beyond its dead zone
    REQUIRE(peak > 0.05f * 3.0f); // about +12 dB (x3.98), not undone
}

TEST_CASE("states saved before the gains existed load them at 0 dB")
{
    PluginProcessor a, b;
    setDb(b, grain::ids::ingain, 10.0f);
    setDb(b, grain::ids::outgain, -10.0f);

    juce::MemoryBlock block;
    a.getStateInformation(block);
    auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(xml != nullptr);
    for (auto* id : { grain::ids::ingain, grain::ids::outgain })
        for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
            if (child->getStringAttribute("id") == id)
            {
                xml->removeChildElement(child, true);
                break;
            }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);
    b.setStateInformation(old.getData(), static_cast<int>(old.getSize()));

    REQUIRE(b.apvts.getRawParameterValue(grain::ids::ingain)->load() == Catch::Approx(0.0f).margin(1e-5));
    REQUIRE(b.apvts.getRawParameterValue(grain::ids::outgain)->load() == Catch::Approx(0.0f).margin(1e-5));
}

TEST_CASE("master Dry/Wet: 0 % is the input after In gain, 100 % is the effect untouched")
{
    const auto render = [](float drywet) {
        PluginProcessor p;
        prepare(p);
        p.presets.applyFactory(5); // a wet, reverby preset: the effect is clearly not the input
        setDb(p, grain::ids::ingain, -6.0f);
        auto* prm = p.apvts.getParameter(grain::ids::drywet);
        prm->setValueNotifyingHost(prm->convertTo0to1(drywet));
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        std::vector<float> in, out;
        const auto x = sine(256 * 200);
        for (int b = 0; b < 200; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample(c, i, x[static_cast<size_t>(b * 256 + i)]);
            p.processBlock(buffer, midi);
            for (int i = 0; i < 256; ++i)
            {
                in.push_back(x[static_cast<size_t>(b * 256 + i)]);
                out.push_back(buffer.getSample(0, i));
            }
        }
        return std::pair { in, out };
    };

    const auto [in, dry] = render(0.0f);
    const float trim = std::pow(10.0f, -6.0f / 20.0f);
    for (size_t i = 256 * 20; i < dry.size(); ++i) // after the 20 ms ramps
        REQUIRE(dry[i] == Catch::Approx(in[i] * trim).margin(1e-5));

    // 100 % (the default) equals a processor that never touched the control
    PluginProcessor plain;
    REQUIRE(plain.apvts.getRawParameterValue(grain::ids::drywet)->load() == 1.0f);
    const auto [in2, wet] = render(1.0f);
    double diff = 0;
    for (size_t i = 256 * 20; i < wet.size(); ++i)
        diff += std::abs(wet[i] - dry[i]);
    REQUIRE(diff > 1.0); // the effect really is different from the dry signal

    const auto [in3, half] = render(0.5f);
    for (size_t i = 256 * 20; i < half.size(); ++i) // equal power: both sides at cos(45 deg)
        REQUIRE(half[i] == Catch::Approx(0.70710678f * (wet[i] + dry[i])).margin(1e-4));
}

TEST_CASE("gain: a trim a hair off 0 dB (the 0.1 dB grid on macOS) stays bit-exact")
{
    const auto x = sine(4096);
    REQUIRE(run(x, 256, 3.58e-7f) == x);
    REQUIRE(run(x, 256, -3.58e-7f) == x);
}
