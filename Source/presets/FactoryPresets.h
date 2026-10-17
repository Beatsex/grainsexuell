#pragma once

#include <array>

namespace grain::presets
{
// A preset sets only the sound controls, never the transport or the switches (Loop, Hold, Undo, Burst, Reverse,
// Speed, Quantize, Loop Position, Auto Gain): loading a preset must not start a recording or drop a held loop.
// Values are plain parameter values (Dusk in Hz, selectors as indices).
inline constexpr std::array<const char*, 11> soundIds { "swarm", "tail", "span", "scatter", "bend", "dusk",
                                                        "haze", "mix", "family", "variant", "hazemode" };

struct Preset
{
    const char* name;
    std::array<float, 11> values; // in soundIds order
};

inline constexpr int numFactory = 16;
const std::array<Preset, numFactory>& factory();
} // namespace grain::presets
