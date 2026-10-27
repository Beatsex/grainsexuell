#pragma once

#include <array>

namespace grain::presets
{
// A preset sets only the sound controls, never the transport or the switches (Loop, Hold, Undo, Burst, Reverse,
// Speed, Quantize, Loop Position, Auto Gain): loading a preset must not start a recording or drop a held loop.
// Values are plain parameter values (Dusk in Hz, selectors as indices).
inline constexpr std::array<const char*, 11> soundIds { "swarm", "tail", "span", "scatter", "bend", "dusk",
                                                        "haze", "mix", "family", "variant", "hazemode" };

// Slot B (the second engine) belongs to a preset too, but the factory table does not list it: a factory preset, or a
// user file saved before slot B existed, turns B off (its parameter defaults).
inline constexpr std::array<const char*, 4> slotBIds { "family2", "variant2", "mix2", "route2" };

// the preset menu's submenus, in this order
inline constexpr std::array<const char*, 6> categories { "Ambient", "Shimmer", "Rhythmic", "Echo", "Lo-Fi", "Drift" };

struct Preset
{
    const char* name;
    const char* category; // one of `categories`
    std::array<float, 11> values; // in soundIds order
};

inline constexpr int numFactory = 56;
const std::array<Preset, numFactory>& factory();
} // namespace grain::presets
