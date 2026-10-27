#include "PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

TEST_CASE("parameter layout: 8 knobs + 3 selectors + auto gain and hold toggles, unique ids, sane defaults")
{
    PluginProcessor p;
    std::set<std::string> seen;

    for (auto* id : grain::ids::knobs)
    {
        auto* param = dynamic_cast<juce::AudioParameterFloat*>(p.apvts.getParameter(id));
        INFO(id);
        REQUIRE(param != nullptr);
        REQUIRE(seen.insert(id).second);

        const auto& range = param->getNormalisableRange();
        REQUIRE(param->get() >= range.start);
        REQUIRE(param->get() <= range.end);

        // normalised round trip
        for (float v : { range.start, range.snapToLegalValue(0.5f * (range.start + range.end)), range.end })
            REQUIRE(range.convertFrom0to1(range.convertTo0to1(v)) == Catch::Approx(v).margin(1e-3));
    }

    for (auto* id : grain::ids::selectors)
    {
        auto* param = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter(id));
        INFO(id);
        REQUIRE(param != nullptr);
        REQUIRE(seen.insert(id).second);
        REQUIRE(param->choices.size() == 4);
        REQUIRE(param->getIndex() == (std::string(id) == grain::ids::hazemode ? 1 : 0)); // Plate by default
    }

    auto* autoGain = dynamic_cast<juce::AudioParameterBool*>(p.apvts.getParameter(grain::ids::autogain));
    REQUIRE(autoGain != nullptr);
    REQUIRE(! autoGain->get()); // off by default keeps the output bit-exact

    auto* hold = dynamic_cast<juce::AudioParameterBool*>(p.apvts.getParameter(grain::ids::hold));
    REQUIRE(hold != nullptr);
    REQUIRE(! hold->get()); // off by default keeps the output bit-exact

    // looper: transport Off, normal speed, switches off, Post-FX
    const auto choice = [&p](const char* id) { return dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter(id)); };
    const auto toggle = [&p](const char* id) { return dynamic_cast<juce::AudioParameterBool*>(p.apvts.getParameter(id)); };
    REQUIRE(choice(grain::ids::loop) != nullptr);
    REQUIRE(choice(grain::ids::loop)->getIndex() == 0);
    REQUIRE(choice(grain::ids::loopspeed)->getIndex() == 1);
    REQUIRE(choice(grain::ids::looppos)->getIndex() == 1);
    for (auto* id : { grain::ids::loopreverse, grain::ids::loopquant, grain::ids::loopburst, grain::ids::loopundo })
    {
        INFO(id);
        REQUIRE(toggle(id) != nullptr);
        REQUIRE(! toggle(id)->get());
    }

    for (auto* id : { grain::ids::ingain, grain::ids::outgain })
    {
        auto* trim = dynamic_cast<juce::AudioParameterFloat*>(p.apvts.getParameter(id));
        INFO(id);
        REQUIRE(trim != nullptr);
        REQUIRE(trim->get() == Catch::Approx(0.0f).margin(1e-5)); // the 0.1 dB grid can miss 0 by 3.6e-7 (macOS); Gain treats it as 0 dB
        REQUIRE(trim->getNormalisableRange().start == -24.0f);
        REQUIRE(trim->getNormalisableRange().end == 24.0f);
    }

    REQUIRE(p.getParameters().size() == 27);
}

TEST_CASE("loading a state saved before the hold parameter existed leaves Hold off")
{
    PluginProcessor a, b;
    b.apvts.getParameter(grain::ids::hold)->setValueNotifyingHost(1.0f); // Hold is on in the live instance

    juce::MemoryBlock block;
    a.getStateInformation(block);
    auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(xml != nullptr);
    for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        if (child->getStringAttribute("id") == grain::ids::hold)
        {
            xml->removeChildElement(child, true); // an old state has no entry for it
            break;
        }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml, old);
    b.setStateInformation(old.getData(), static_cast<int>(old.getSize()));

    REQUIRE(b.apvts.getParameter(grain::ids::hold)->getValue() == Catch::Approx(0.0f));
}

TEST_CASE("state round trip keeps parameter values")
{
    PluginProcessor a, b;
    a.apvts.getParameter(grain::ids::swarm)->setValueNotifyingHost(0.8f);
    a.apvts.getParameter(grain::ids::family)->setValueNotifyingHost(1.0f);

    juce::MemoryBlock block;
    a.getStateInformation(block);
    b.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    REQUIRE(b.apvts.getParameter(grain::ids::swarm)->getValue() == Catch::Approx(0.8f));
    REQUIRE(b.apvts.getParameter(grain::ids::family)->getValue() == Catch::Approx(1.0f));
}
