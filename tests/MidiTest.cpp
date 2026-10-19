#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
using grain::midi::MidiMap;

int indexOfId(const char* id)
{
    for (size_t i = 0; i < grain::ids::mappable.size(); ++i)
        if (juce::String(grain::ids::mappable[i]) == id)
            return static_cast<int>(i);
    FAIL("unknown id");
    return -1;
}

void prepare(PluginProcessor& p)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(p.setBusesLayout(layout));
    p.setRateAndBufferSizeDetails(48000.0, 256);
    p.prepareToPlay(48000.0, 256);
}

void sendCc(PluginProcessor& p, int cc, int value)
{
    juce::AudioBuffer<float> buffer(2, 64);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, cc, value), 10);
    p.processBlock(buffer, midi);
    p.applyPendingMidi(); // the message-thread half (a 100 Hz timer in a real host)
}

float normalised(PluginProcessor& p, const char* id) { return p.apvts.getParameter(id)->getValue(); }
} // namespace

TEST_CASE("every parameter is mappable and ids are unique")
{
    PluginProcessor p;
    for (size_t i = 0; i < grain::ids::mappable.size(); ++i)
    {
        REQUIRE(p.apvts.getParameter(grain::ids::mappable[i]) != nullptr);
        for (size_t j = i + 1; j < grain::ids::mappable.size(); ++j)
            REQUIRE(juce::String(grain::ids::mappable[i]) != grain::ids::mappable[j]);
    }
    REQUIRE(grain::ids::mappable.size() == 22);
    for (size_t i = 0; i < grain::ids::knobs.size(); ++i) // the editor relies on knobs being the first entries
        REQUIRE(juce::String(grain::ids::knobs[i]) == grain::ids::mappable[i]);
}

TEST_CASE("MidiMap: assign, one CC per parameter, one parameter per CC, bad input ignored")
{
    MidiMap map;
    REQUIRE_FALSE(map.onCc(10, 64).has_value());

    map.assign(10, 3);
    auto hit = map.onCc(10, 127);
    REQUIRE(hit.has_value());
    REQUIRE(hit->param == 3);
    REQUIRE(hit->normalised == 1.0f);
    REQUIRE(map.onCc(10, 0)->normalised == 0.0f);
    REQUIRE(map.onCc(10, 500)->normalised == 1.0f); // clamped
    REQUIRE(map.onCc(10, -5)->normalised == 0.0f);

    map.assign(11, 3); // the parameter moves to the new CC
    REQUIRE_FALSE(map.onCc(10, 64).has_value());
    REQUIRE(map.ccFor(3) == 11);

    map.assign(11, 4); // the CC moves to the new parameter
    REQUIRE(map.ccFor(3) == -1);
    REQUIRE(map.ccFor(4) == 11);

    map.assign(200, 5); // out of range: ignored
    map.assign(-1, 5);
    REQUIRE(map.ccFor(5) == -1);
    REQUIRE_FALSE(map.onCc(200, 1).has_value());
    REQUIRE_FALSE(map.onCc(-1, 1).has_value());

    map.clear(4);
    REQUIRE_FALSE(map.onCc(11, 1).has_value());
}

TEST_CASE("MidiMap: learn takes the next CC once, then stops")
{
    MidiMap map;
    map.startLearn(7);
    REQUIRE(map.learning() == 7);
    REQUIRE_FALSE(map.onCc(21, 100).has_value()); // consumed by learning (handed to the message thread)
    REQUIRE(map.learning() == -1);
    REQUIRE(map.ccFor(7) == -1);                  // not assigned until the message thread polls
    REQUIRE(map.pollLearn());
    REQUIRE_FALSE(map.pollLearn());
    REQUIRE(map.ccFor(7) == 21);
    REQUIRE(map.onCc(21, 127)->param == 7);

    map.startLearn(2);
    map.cancelLearn();
    REQUIRE_FALSE(map.onCc(22, 1).has_value());
    REQUIRE_FALSE(map.pollLearn()); // a cancelled learn never assigns
    REQUIRE(map.ccFor(2) == -1);
}

TEST_CASE("a CC moves the mapped parameter through processBlock")
{
    PluginProcessor p;
    prepare(p);
    p.midiMap.assign(21, indexOfId(grain::ids::mix));
    p.midiMap.assign(22, indexOfId(grain::ids::family));
    p.midiMap.assign(23, indexOfId(grain::ids::hold));

    sendCc(p, 21, 127);
    REQUIRE_THAT(normalised(p, grain::ids::mix), Catch::Matchers::WithinAbs(1.0, 1e-6));
    sendCc(p, 21, 0);
    REQUIRE_THAT(normalised(p, grain::ids::mix), Catch::Matchers::WithinAbs(0.0, 1e-6));

    sendCc(p, 22, 127); // last choice
    REQUIRE(static_cast<int>(p.apvts.getRawParameterValue(grain::ids::family)->load() + 0.5f) == 3);
    sendCc(p, 23, 127);
    REQUIRE(p.apvts.getRawParameterValue(grain::ids::hold)->load() > 0.5f);

    const float before = normalised(p, grain::ids::swarm);
    sendCc(p, 99, 127); // unmapped
    REQUIRE(normalised(p, grain::ids::swarm) == before);

    // learn through the audio path
    p.midiMap.startLearn(indexOfId(grain::ids::swarm));
    sendCc(p, 30, 5); // captured here, assigned by applyPendingMidi inside sendCc
    REQUIRE(p.midiMap.ccFor(indexOfId(grain::ids::swarm)) == 30);
    sendCc(p, 30, 127);
    REQUIRE_THAT(normalised(p, grain::ids::swarm), Catch::Matchers::WithinAbs(1.0, 1e-6));
}

TEST_CASE("non-CC MIDI and empty buffers leave the audio and parameters alone")
{
    PluginProcessor p;
    prepare(p);
    p.apvts.getParameter(grain::ids::mix)->setValueNotifyingHost(0.0f);
    juce::AudioBuffer<float> buffer(2, 64);
    for (int i = 0; i < 64; ++i)
        buffer.setSample(0, i, 0.1f * static_cast<float>(i % 7));
    buffer.copyFrom(1, 0, buffer, 0, 0, 64);
    juce::AudioBuffer<float> expected;
    expected.makeCopyOf(buffer);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 3);
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 9000), 4);
    midi.addEvent(juce::MidiMessage::programChange(1, 5), 5);
    p.processBlock(buffer, midi);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            REQUIRE(buffer.getSample(ch, i) == expected.getSample(ch, i));
}

TEST_CASE("the MIDI map and the editor size survive a state round trip; bad entries are ignored")
{
    PluginProcessor a;
    a.midiMap.assign(21, indexOfId(grain::ids::swarm));
    a.midiMap.assign(64, indexOfId(grain::ids::hold));
    a.editorWidth = 900;
    juce::MemoryBlock state;
    a.getStateInformation(state);

    PluginProcessor b;
    b.midiMap.assign(5, indexOfId(grain::ids::mix)); // replaced by the loaded map
    b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(b.midiMap.ccFor(indexOfId(grain::ids::swarm)) == 21);
    REQUIRE(b.midiMap.ccFor(indexOfId(grain::ids::hold)) == 64);
    REQUIRE(b.midiMap.ccFor(indexOfId(grain::ids::mix)) == -1);
    REQUIRE(b.editorWidth.load() == 900);

    // hand-made state with garbage entries and a silly width
    juce::XmlElement xml("state");
    auto* map = xml.createNewChildElement("MidiMap");
    for (auto [id, cc] : std::initializer_list<std::pair<const char*, int>> { { "evil", 3 }, { "mix", 999 }, { "tail", -4 }, { "span", 7 } })
    {
        auto* e = map->createNewChildElement("CC");
        e->setAttribute("id", id);
        e->setAttribute("cc", cc);
    }
    xml.setAttribute("editorWidth", 99999);
    juce::MemoryBlock bad;
    juce::AudioProcessor::copyXmlToBinary(xml, bad);
    PluginProcessor c;
    c.setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
    REQUIRE(c.midiMap.ccFor(indexOfId(grain::ids::mix)) == -1);
    REQUIRE(c.midiMap.ccFor(indexOfId(grain::ids::tail)) == -1);
    REQUIRE(c.midiMap.ccFor(indexOfId(grain::ids::span)) == 7);
    REQUIRE(c.editorWidth.load() <= 2400);
}

TEST_CASE("legacy states without a MIDI map load; duplicate CCs keep one owner; non-numeric cc is ignored")
{
    PluginProcessor p;
    p.midiMap.assign(9, indexOfId(grain::ids::mix));
    juce::XmlElement legacy("state"); // saved before Phase 10: no MidiMap, no editorWidth
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(legacy, block);
    p.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(p.midiMap.ccFor(indexOfId(grain::ids::mix)) == -1); // an empty map replaces the old one
    REQUIRE(p.editorWidth.load() >= 480);

    juce::XmlElement xml("state");
    auto* map = xml.createNewChildElement("MidiMap");
    for (auto [id, cc] : std::initializer_list<std::pair<const char*, const char*>> { { "mix", "5" }, { "tail", "5" }, { "span", "abc" } })
    {
        auto* e = map->createNewChildElement("CC");
        e->setAttribute("id", id);
        e->setAttribute("cc", cc);
    }
    xml.setAttribute("editorWidth", 10); // below the minimum
    juce::MemoryBlock dup;
    juce::AudioProcessor::copyXmlToBinary(xml, dup);
    p.setStateInformation(dup.getData(), static_cast<int>(dup.getSize()));
    const int owners = (p.midiMap.ccFor(indexOfId(grain::ids::mix)) == 5 ? 1 : 0) + (p.midiMap.ccFor(indexOfId(grain::ids::tail)) == 5 ? 1 : 0);
    REQUIRE(owners == 1);
    REQUIRE(p.midiMap.ccFor(indexOfId(grain::ids::span)) <= 0); // "abc" reads as 0 at worst, never out of range
    REQUIRE(p.editorWidth.load() == 480);
}

TEST_CASE("an oversized state is refused without touching the map")
{
    PluginProcessor p;
    p.midiMap.assign(9, indexOfId(grain::ids::mix));
    std::vector<char> big(2 * 1024 * 1024, 'a');
    p.setStateInformation(big.data(), static_cast<int>(big.size()));
    REQUIRE(p.midiMap.ccFor(indexOfId(grain::ids::mix)) == 9);
}

TEST_CASE("the editor builds, keeps its aspect ratio, remembers its size and follows a family change")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PluginProcessor p;
    prepare(p);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        REQUIRE(editor != nullptr);
        REQUIRE(std::abs(editor->getHeight() - editor->getWidth() * PluginEditor::designHeight / PluginEditor::designWidth) <= 1); // integer pixels

        editor->setSize(1000, 1000 * PluginEditor::designHeight / PluginEditor::designWidth);
        REQUIRE(p.editorWidth.load() == 1000);

        // optional: GRAIN_SNAPSHOT_DIR=<dir> writes PNGs of the editor for a visual check
        if (const auto dir = juce::SystemStats::getEnvironmentVariable("GRAIN_SNAPSHOT_DIR", {}); dir.isNotEmpty())
            for (int w : { 506, 760, 1400 })
            {
                editor->setSize(w, w * PluginEditor::designHeight / PluginEditor::designWidth);
                juce::PNGImageFormat png;
                juce::FileOutputStream out(juce::File(dir).getChildFile("editor_" + juce::String(w) + ".png"));
                if (out.openedOk())
                {
                    out.setPosition(0);
                    out.truncate();
                    png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), out);
                }
            }
        editor->setSize(1000, 1000 * PluginEditor::designHeight / PluginEditor::designWidth);

        for (int f = 0; f < 4; ++f)
        {
            p.apvts.getParameter(grain::ids::family)->setValueNotifyingHost(static_cast<float>(f) / 3.0f);
            REQUIRE(static_cast<int>(p.apvts.getRawParameterValue(grain::ids::family)->load() + 0.5f) == f);
        }
    }
    // a second editor opens at the remembered size
    std::unique_ptr<juce::AudioProcessorEditor> again(p.createEditor());
    REQUIRE(again->getWidth() == 1000);
}
