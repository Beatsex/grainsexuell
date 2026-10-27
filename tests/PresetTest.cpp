#include "PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

#include <cmath>
#include <numbers>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

using grain::presets::PresetManager;

namespace
{
// a unique folder in the temp directory, deleted again (it is our own, created here)
struct TempDir
{
    juce::File dir;
    TempDir()
    {
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("grainsexuell_test", "");
        dir.createDirectory();
    }
    ~TempDir() { dir.deleteRecursively(); }
};

float plain(PluginProcessor& p, const char* id)
{
    auto* prm = p.apvts.getParameter(id);
    return prm->convertFrom0to1(prm->getValue());
}

void setParam(PluginProcessor& p, const char* id, float value)
{
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(value));
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

const char* const transportIds[] { "loop", "loopspeed", "loopreverse", "loopquant", "loopburst", "loopundo", "looppos", "hold", "autogain" };
} // namespace

TEST_CASE("presets: the factory table is valid")
{
    const auto& table = grain::presets::factory();
    REQUIRE(table.size() == grain::presets::numFactory);
    std::set<std::string> names;
    PluginProcessor p;
    for (const auto& preset : table)
    {
        INFO(preset.name);
        REQUIRE(std::string(preset.name).size() > 0);
        REQUIRE(names.insert(preset.name).second);
        REQUIRE(PresetManager::sanitiseName(preset.name) == preset.name); // usable as a user preset name too
        REQUIRE(std::find_if(grain::presets::categories.begin(), grain::presets::categories.end(),
                             [&](const char* c) { return std::string(c) == preset.category; }) != grain::presets::categories.end());

        for (size_t i = 0; i < grain::presets::soundIds.size(); ++i)
        {
            auto* prm = p.apvts.getParameter(grain::presets::soundIds[i]);
            REQUIRE(prm != nullptr);
            const auto& range = prm->getNormalisableRange();
            REQUIRE(preset.values[i] >= range.start);
            REQUIRE(preset.values[i] <= range.end);
        }
    }
    // none of the preset ids is a transport or switch id
    for (auto* sound : grain::presets::soundIds)
        for (auto* transport : transportIds)
            REQUIRE(std::string(sound) != transport);
}

TEST_CASE("presets: applying one sets the sound controls and leaves the transport and switches alone")
{
    PluginProcessor p;
    setParam(p, "loop", 2.0f);
    setParam(p, "loopspeed", 2.0f);
    setParam(p, "loopburst", 1.0f);
    setParam(p, "looppos", 0.0f);
    setParam(p, "hold", 1.0f);
    setParam(p, "autogain", 1.0f);
    float before[std::size(transportIds)];
    for (size_t i = 0; i < std::size(transportIds); ++i)
        before[i] = plain(p, transportIds[i]);

    for (int n = 0; n < grain::presets::numFactory; ++n)
    {
        p.setCurrentProgram(n);
        const auto& preset = grain::presets::factory()[static_cast<size_t>(n)];
        INFO(preset.name);
        for (size_t i = 0; i < grain::presets::soundIds.size(); ++i)
            REQUIRE(plain(p, grain::presets::soundIds[i]) == Catch::Approx(preset.values[i]).margin(0.01));
        for (size_t i = 0; i < std::size(transportIds); ++i)
            REQUIRE(plain(p, transportIds[i]) == before[i]);
        REQUIRE(p.getCurrentProgram() == n);
        REQUIRE(p.presets.currentName() == preset.name);
    }
}

TEST_CASE("presets: every factory preset renders finite, bounded and audibly wet")
{
    const double sr = 48000.0;
    for (int n = 0; n < grain::presets::numFactory; ++n)
    {
        PluginProcessor p;
        configure(p, sr, 512);
        p.setCurrentProgram(n);
        const float mix = plain(p, "mix");

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        std::vector<float> in, out;
        for (int blk = 0; blk < 300; ++blk) // 3.2 s of plucks every 0.5 s
            for (int i = 0; i < 512; ++i)
            {
                const double t = static_cast<double>(blk * 512 + i) / sr, tt = std::fmod(t, 0.5);
                const float v = 0.3f * static_cast<float>(std::exp(-tt / 0.2) * std::min(1.0, tt / 0.005) * std::min(1.0, (0.5 - tt) / 0.005)
                                                          * std::sin(2.0 * std::numbers::pi * 330.0 * tt));
                in.push_back(v);
            }
        for (int blk = 0; blk < 300; ++blk)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    buf.setSample(c, i, in[static_cast<size_t>(blk * 512 + i)]);
            p.processBlock(buf, midi);
            for (int i = 0; i < 512; ++i)
                out.push_back(buf.getSample(0, i));
        }

        float peak = 0, step = 0;
        double wetEnergy = 0;
        const float dry = std::cos(mix * 1.57079633f);
        for (size_t i = 1; i < out.size(); ++i)
        {
            REQUIRE(std::isfinite(out[i]));
            peak = std::max(peak, std::abs(out[i]));
            step = std::max(step, std::abs(out[i] - out[i - 1]));
            if (i > 48000)
                wetEnergy += std::pow(static_cast<double>(out[i] - dry * in[i]), 2.0);
        }
        const double wetRms = std::sqrt(wetEnergy / static_cast<double>(out.size() - 48000));
        INFO(grain::presets::factory()[static_cast<size_t>(n)].name << ": peak " << peak << ", step " << step << ", wet rms " << wetRms);
        REQUIRE(peak < 4.0f);
        REQUIRE(step < 1.5f);
        REQUIRE(wetRms > 0.003);
    }
}

TEST_CASE("presets: names are sanitised, so a name can never leave the folder")
{
    for (auto* bad : { "", "   ", "../x", "..", "a/b", "a\\b", "C:evil", "star*", "q?", "a|b", "<x>", ".hidden", "trailing.", "CON", "con.txt",
                       "Nul", "COM1", "lpt9.x", "tab\tname" })
    {
        INFO("[" << bad << "]");
        REQUIRE(PresetManager::sanitiseName(bad).isEmpty());
    }
    REQUIRE(PresetManager::sanitiseName("CONIN$").isEmpty());
    REQUIRE(PresetManager::sanitiseName("conout$.txt").isEmpty());
    REQUIRE(PresetManager::sanitiseName(juce::String(juce::CharPointer_UTF8("COM\xc2\xb9"))).isEmpty());  // COM + superscript 1
    REQUIRE(PresetManager::sanitiseName(juce::String(juce::CharPointer_UTF8("lpt\xc2\xb3.x"))).isEmpty()); // LPT + superscript 3
    REQUIRE(PresetManager::sanitiseName(juce::String::repeatedString("x", 65)).isEmpty());
    REQUIRE(PresetManager::sanitiseName(juce::String::repeatedString("x", 64)).length() == 64);
    REQUIRE(PresetManager::sanitiseName("  Fog Machine 2-b_c  ") == "Fog Machine 2-b_c");
    REQUIRE(PresetManager::sanitiseName("COM10") == "COM10"); // only COM1..9 are device names
}

TEST_CASE("presets: user presets save, load, list and delete")
{
    TempDir tmp;
    PluginProcessor p;
    PresetManager manager(p.apvts, tmp.dir.getChildFile("Presets")); // not created yet

    REQUIRE(manager.userPresetNames().isEmpty());
    setParam(p, "swarm", 0.9f);
    setParam(p, "family", 2.0f);
    setParam(p, "variant", 1.0f);
    setParam(p, "dusk", 1200.0f);
    REQUIRE(manager.saveUser("  My Patch "));
    REQUIRE(manager.currentName() == "My Patch");
    REQUIRE(manager.saveUser("Another"));
    REQUIRE(manager.userPresetNames() == juce::StringArray { "Another", "My Patch" });

    setParam(p, "swarm", 0.1f);
    setParam(p, "family", 0.0f);
    setParam(p, "variant", 3.0f);
    setParam(p, "dusk", 20000.0f);
    setParam(p, "hold", 1.0f); // a preset must not change this
    REQUIRE(manager.applyUser("My Patch"));
    REQUIRE(plain(p, "swarm") == Catch::Approx(0.9f).margin(0.01));
    REQUIRE(plain(p, "family") == 2.0f);
    REQUIRE(plain(p, "variant") == 1.0f);
    REQUIRE(plain(p, "dusk") == Catch::Approx(1200.0f).margin(5.0));
    REQUIRE(plain(p, "hold") == 1.0f);
    REQUIRE(manager.currentName() == "My Patch");
    REQUIRE(manager.currentFactoryIndex() == -1);

    REQUIRE(! manager.applyUser("missing"));
    REQUIRE(manager.deleteUser("My Patch"));
    REQUIRE(! manager.deleteUser("My Patch"));
    REQUIRE(manager.userPresetNames() == juce::StringArray { "Another" });

    // bad names do nothing: nothing is written next to or above the folder
    const auto before = tmp.dir.findChildFiles(juce::File::findFilesAndDirectories, true).size();
    for (auto* bad : { "../escape", "a/b", "", "CON", "x:y" })
    {
        REQUIRE(! manager.saveUser(bad));
        REQUIRE(! manager.applyUser(bad));
        REQUIRE(! manager.deleteUser(bad));
    }
    REQUIRE(tmp.dir.findChildFiles(juce::File::findFilesAndDirectories, true).size() == before);
    REQUIRE(! tmp.dir.getChildFile("escape.xml").exists());

    // a folder that cannot be created (a file is in the way) makes Save fail instead of throwing
    const auto blocker = tmp.dir.getChildFile("blocker");
    blocker.replaceWithText("x");
    PresetManager blocked(p.apvts, blocker.getChildFile("Presets"));
    REQUIRE(! blocked.saveUser("Nope"));
}

TEST_CASE("presets: corrupt, hostile or out-of-range files are handled safely")
{
    TempDir tmp;
    PluginProcessor p;
    PresetManager manager(p.apvts, tmp.dir);
    setParam(p, "loop", 2.0f);
    setParam(p, "hold", 1.0f);

    const auto write = [&tmp](const char* name, const juce::String& text) { tmp.dir.getChildFile(juce::String(name) + ".xml").replaceWithText(text); };
    write("wrongroot", "<Other version=\"1\"/>");
    write("noversion", "<GrainPreset/>");
    write("garbage", "this is not xml at all <<<");
    write("empty", "");
    write("big", "<GrainPreset version=\"1\"><!--" + juce::String::repeatedString("x", 70000) + "--></GrainPreset>");
    for (auto* bad : { "wrongroot", "noversion", "garbage", "empty", "big" })
    {
        INFO(bad);
        REQUIRE(! manager.applyUser(bad));
    }

    // entity expansion and deep nesting (the parser is recursive) are refused before parsing
    write("entity", "<!DOCTYPE GrainPreset [<!ENTITY a \"aaaa\"><!ENTITY b \"&a;&a;&a;&a;\">]><GrainPreset version=\"1\"><P id=\"swarm\" value=\"&b;\"/></GrainPreset>");
    write("lowercase", "<!doctype x><GrainPreset version=\"1\"/>");
    write("deep", "<GrainPreset version=\"1\">" + juce::String::repeatedString("<a>", 5000) + juce::String::repeatedString("</a>", 5000) + "</GrainPreset>");
    for (auto* bad : { "entity", "lowercase", "deep" })
    {
        INFO(bad);
        REQUIRE(! manager.applyUser(bad));
    }
    REQUIRE(PresetManager::isSafeXml("<GrainPreset version=\"1\"><P id=\"x\" value=\"1\"/><P id=\"y\" value=\"2\"/></GrainPreset>"));
    std::string nested32, nested33, tricky;
    for (int i = 0; i < 33; ++i)
        (i < 32 ? nested32 : nested33) += "<a>";
    nested33 = nested32 + nested33;
    for (int i = 0; i < 40; ++i)
        tricky += "<a x=\"/>\">"; // a '/>' inside a quoted attribute value must not hide the nesting
    REQUIRE(PresetManager::isSafeXml(nested32));
    REQUIRE(! PresetManager::isSafeXml(nested33));
    REQUIRE(! PresetManager::isSafeXml(tricky));

    write("wild","<GrainPreset version=\"1\">"
                  "<P id=\"swarm\" value=\"5\"/><P id=\"tail\" value=\"-3\"/><P id=\"dusk\" value=\"nan\"/>"
                  "<P id=\"family\" value=\"99\"/><P id=\"loop\" value=\"3\"/><P id=\"hold\" value=\"0\"/>"
                  "<P id=\"bogus\" value=\"1\"/><P id=\"mix\"/><P value=\"1\"/></GrainPreset>");
    REQUIRE(manager.applyUser("wild"));
    REQUIRE(plain(p, "swarm") == 1.0f);
    REQUIRE(plain(p, "tail") == 0.0f);
    REQUIRE(plain(p, "family") == 3.0f);
    REQUIRE(plain(p, "dusk") >= 200.0f);
    REQUIRE(plain(p, "dusk") <= 20000.0f);
    REQUIRE(plain(p, "loop") == 2.0f); // transport ids in a file are ignored
    REQUIRE(plain(p, "hold") == 1.0f);
}

TEST_CASE("presets: program list, state round trip with the preset name, and states from before presets")
{
    PluginProcessor a, b, old;
    REQUIRE(a.getNumPrograms() == grain::presets::numFactory);
    REQUIRE(a.getProgramName(4) == "Soft Mist");
    REQUIRE(a.getProgramName(-1).isEmpty());
    REQUIRE(a.getProgramName(grain::presets::numFactory).isEmpty());
    std::set<std::string> names;
    for (int i = 0; i < grain::presets::numFactory; ++i)
        REQUIRE(names.insert(a.getProgramName(i).toStdString()).second);

    a.setCurrentProgram(4);
    REQUIRE(a.getCurrentProgram() == 4);
    juce::MemoryBlock block;
    a.getStateInformation(block);
    b.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(b.presets.currentName() == "Soft Mist");
    REQUIRE(b.getCurrentProgram() == 4);
    REQUIRE(plain(b, "swarm") == Catch::Approx(plain(a, "swarm")));

    // a state without the preset name and version (saved before Phase 9) still loads
    auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(xml != nullptr);
    xml->removeAttribute("preset");
    xml->removeAttribute("stateVersion");
    juce::MemoryBlock legacy;
    juce::AudioProcessor::copyXmlToBinary(*xml, legacy);
    old.setStateInformation(legacy.getData(), static_cast<int>(legacy.getSize()));
    REQUIRE(old.presets.currentName().isEmpty());
    REQUIRE(old.getCurrentProgram() == 0);
    REQUIRE(plain(old, "swarm") == Catch::Approx(plain(a, "swarm")));

    // hostile project state: entity declarations and oversized blobs are refused, nothing changes
    setParam(old, "swarm", 0.123f);
    const std::string hostile = "<!DOCTYPE x [<!ENTITY e \"1\">]><state/>";
    old.setStateInformation(hostile.data(), static_cast<int>(hostile.size()));
    const std::vector<char> huge(static_cast<size_t>(2) << 20, 'x');
    old.setStateInformation(huge.data(), static_cast<int>(huge.size()));
    old.setStateInformation(nullptr, 0);
    REQUIRE(plain(old, "swarm") == Catch::Approx(0.123f).margin(0.001));
}

TEST_CASE("presets: applying presets from another thread while audio runs is safe")
{
    PluginProcessor p;
    configure(p, 48000.0, 256);
    std::thread other([&p] {
        for (int i = 0; i < 600; ++i)
            p.setCurrentProgram(i % grain::presets::numFactory);
    });

    std::mt19937 rng(5);
    std::uniform_real_distribution<float> d(-0.3f, 0.3f);
    juce::AudioBuffer<float> buf(2, 256);
    juce::MidiBuffer midi;
    bool finite = true;
    for (int blk = 0; blk < 600; ++blk)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                buf.setSample(c, i, d(rng));
        p.processBlock(buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                finite &= std::isfinite(buf.getSample(c, i));
    }
    other.join();
    REQUIRE(finite);
}

TEST_CASE("Init puts every sound control and slot B back to its default and leaves the rest alone")
{
    PluginProcessor p;
    const auto set = [&p](const char* id, float normalised) { p.apvts.getParameter(id)->setValueNotifyingHost(normalised); };
    p.presets.applyFactory(5);
    set(grain::ids::family2, 0.5f);
    set(grain::ids::mix2, 0.9f);
    set(grain::ids::hold, 1.0f);    // a switch: not part of a preset
    set(grain::ids::ingain, 0.75f); // the trims: not part of a preset

    p.presets.applyInit();
    for (auto* id : grain::presets::soundIds)
    {
        INFO(id);
        auto* prm = p.apvts.getParameter(id);
        REQUIRE(prm->getValue() == Catch::Approx(prm->getDefaultValue()));
    }
    for (auto* id : grain::presets::slotBIds)
    {
        INFO(id);
        auto* prm = p.apvts.getParameter(id);
        REQUIRE(prm->getValue() == Catch::Approx(prm->getDefaultValue()));
    }
    REQUIRE(p.apvts.getParameter(grain::ids::hold)->getValue() == 1.0f);
    REQUIRE(p.apvts.getParameter(grain::ids::ingain)->getValue() == Catch::Approx(0.75f));
    REQUIRE(p.presets.currentName() == "Init");
    REQUIRE(p.presets.currentFactoryIndex() == -1);
}
