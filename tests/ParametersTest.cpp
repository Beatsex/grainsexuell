#include "PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

TEST_CASE("parameter layout: 8 knobs + 3 selectors, unique ids, sane defaults")
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

    REQUIRE(p.getParameters().size() == 11);
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
