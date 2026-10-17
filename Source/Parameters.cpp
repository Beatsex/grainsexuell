#include "Parameters.h"

namespace grain
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&layout](const char* id, const char* name, juce::NormalisableRange<float> range, float def,
                         const char* label = "")
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 }, name, range, def, juce::AudioParameterFloatAttributes().withLabel(label)));
    };

    const juce::NormalisableRange<float> unit { 0.0f, 1.0f };

    add(ids::swarm, "Swarm", unit, 0.5f);
    add(ids::tail, "Tail", unit, 0.5f);
    add(ids::span, "Span", unit, 0.5f);
    add(ids::scatter, "Scatter", unit, 0.0f);
    add(ids::bend, "Bend", unit, 0.0f);

    juce::NormalisableRange<float> dusk { 200.0f, 20000.0f };
    dusk.setSkewForCentre(2000.0f);
    add(ids::dusk, "Dusk", dusk, 20000.0f, "Hz");

    add(ids::haze, "Haze", unit, 0.0f);
    add(ids::mix, "Mix", unit, 0.5f);

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::family, 1 }, "Family", juce::StringArray { "Mosaic", "Cloud", "Shard", "Tap" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::variant, 1 }, "Variant", juce::StringArray { "A", "B", "C", "D" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::hazemode, 1 }, "Haze Mode", juce::StringArray { "Room", "Plate", "Hall", "Swell" }, 1));

    return layout;
}
} // namespace grain
