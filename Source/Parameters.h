#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace grain::ids
{
inline constexpr const char* swarm = "swarm";     // macro 1: how busy
inline constexpr const char* tail = "tail";       // macro 2: repeats / feedback
inline constexpr const char* span = "span";       // loop / grain / tap length
inline constexpr const char* scatter = "scatter"; // randomness, stereo spread
inline constexpr const char* bend = "bend";       // pitch modulation amount
inline constexpr const char* dusk = "dusk";       // low-pass cutoff
inline constexpr const char* haze = "haze";       // reverb amount
inline constexpr const char* mix = "mix";         // dry / wet

inline constexpr const char* family = "family";   // Mosaic, Cloud, Shard, Tap
inline constexpr const char* variant = "variant"; // A..D
inline constexpr const char* hazemode = "hazemode"; // Room, Plate, Hall, Swell

inline constexpr std::array knobs { swarm, tail, span, scatter, bend, dusk, haze, mix };
inline constexpr std::array selectors { family, variant, hazemode };
} // namespace grain::ids

namespace grain
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
