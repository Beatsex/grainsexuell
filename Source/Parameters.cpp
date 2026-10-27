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
    add(ids::mix, "Mix A", unit, 0.5f);

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::family, 1 }, "Family", juce::StringArray { "Mosaic", "Cloud", "Shard", "Tap" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::variant, 1 }, "Variant", juce::StringArray { "A", "B", "C", "D" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::hazemode, 1 }, "Haze Mode", juce::StringArray { "Room", "Plate", "Hall", "Swell" }, 1));

    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::autogain, 1 }, "Auto Gain", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::hold, 1 }, "Hold", false));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::loop, 1 }, "Loop", juce::StringArray { "Off", "Record", "Play", "Overdub" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::loopspeed, 1 }, "Loop Speed", juce::StringArray { "Half", "Normal", "Double" }, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::loopreverse, 1 }, "Loop Reverse", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::loopquant, 1 }, "Loop Quantize", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::loopburst, 1 }, "Loop Burst", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::loopundo, 1 }, "Loop Undo", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::looppos, 1 }, "Loop Position", juce::StringArray { "Pre-FX", "Post-FX" }, 1));

    const juce::NormalisableRange<float> trim { -24.0f, 24.0f, 0.1f };
    add(ids::ingain, "Input Gain", trim, 0.0f, "dB");
    add(ids::outgain, "Output Gain", trim, 0.0f, "dB");

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::family2, 1 }, "Family B", juce::StringArray { "Off", "Mosaic", "Cloud", "Shard", "Tap" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::variant2, 1 }, "Variant B", juce::StringArray { "A", "B", "C", "D" }, 0));
    add(ids::mix2, "Mix B", unit, 0.5f);
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ids::route2, 1 }, "Routing B", juce::StringArray { "Serial", "Parallel" }, 0));
    add(ids::drywet, "Dry/Wet", unit, 1.0f); // 100 %: the effect as before this control existed

    return layout;
}
} // namespace grain
