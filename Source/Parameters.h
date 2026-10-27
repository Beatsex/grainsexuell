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

inline constexpr const char* autogain = "autogain"; // toggle: level into the chain, restore out, safety ceiling

inline constexpr const char* hold = "hold";         // toggle: freeze the last beat and loop it into the engine

inline constexpr const char* loop = "loop";               // looper transport: Off, Record, Play, Overdub
inline constexpr const char* loopspeed = "loopspeed";     // Half, Normal, Double
inline constexpr const char* loopreverse = "loopreverse"; // toggle
inline constexpr const char* loopquant = "loopquant";     // toggle: round the loop to whole beats
inline constexpr const char* loopburst = "loopburst";     // toggle: stutter the loop every 1/16 note
inline constexpr const char* loopundo = "loopundo";       // momentary toggle: rising edge undoes the overdubs
inline constexpr const char* looppos = "looppos";         // Pre-FX or Post-FX

inline constexpr const char* ingain = "ingain";   // input trim in dB: first stage, before Auto Gain
inline constexpr const char* outgain = "outgain"; // output trim in dB: last stage

inline constexpr const char* family2 = "family2";   // second engine slot: Off, Mosaic, Cloud, Shard, Tap
inline constexpr const char* variant2 = "variant2"; // A..D for slot B
inline constexpr const char* mix2 = "mix2";         // dry / wet of slot B
inline constexpr const char* route2 = "route2";     // Serial (A -> B) or Parallel ((A + B) / 2)
inline constexpr const char* drywet = "drywet";   // master dry / wet: the clean input against the whole effect

inline constexpr std::array knobs { swarm, tail, span, scatter, bend, dusk, haze, mix };
inline constexpr std::array selectors { family, variant, hazemode };

// every parameter a MIDI CC can drive (all of them); the index in this list is what the MIDI map stores
inline constexpr std::array mappable { swarm, tail, span, scatter, bend, dusk, haze, mix, family, variant, hazemode,
                                       autogain, hold, loop, loopspeed, loopreverse, loopquant, loopburst, loopundo, looppos,
                                       ingain, outgain, family2, variant2, mix2, route2, drywet };
} // namespace grain::ids

namespace grain
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
