#include "FactoryPresets.h"

namespace grain::presets
{
const std::array<Preset, numFactory>& factory()
{
    // swarm, tail, span, scatter, bend, dusk, haze, mix, family (0 Mosaic, 1 Cloud, 2 Shard, 3 Tap), variant, hazemode
    // (0 Room, 1 Plate, 2 Hall, 3 Swell). First guesses, tuned by ear later.
    // Only append: the index is the host's program number, so saved projects keep their preset.
    static const std::array<Preset, numFactory> presets { {
        // Mosaic
        { "Tide Ladder",      "Drift",    { 0.55f, 0.45f, 0.50f, 0.15f, 0.05f, 14000.0f, 0.20f, 0.50f, 0, 0, 1 } },
        { "Cellar Door",      "Ambient",  { 0.40f, 0.55f, 0.60f, 0.10f, 0.00f,  5000.0f, 0.35f, 0.55f, 0, 1, 2 } },
        { "Paper Stars",      "Shimmer",  { 0.65f, 0.35f, 0.35f, 0.30f, 0.15f, 20000.0f, 0.25f, 0.60f, 0, 2, 1 } },
        { "Falling Stairs",   "Drift",    { 0.50f, 0.50f, 0.50f, 0.35f, 0.10f,  9000.0f, 0.10f, 0.50f, 0, 3, 0 } },
        // Cloud
        { "Soft Mist",        "Ambient",  { 0.60f, 0.30f, 0.45f, 0.30f, 0.05f, 12000.0f, 0.00f, 0.50f, 1, 0, 1 } },
        { "Glass Weather",    "Shimmer",  { 0.75f, 0.40f, 0.35f, 0.50f, 0.10f, 20000.0f, 0.05f, 0.50f, 1, 1, 2 } },
        { "Peat Smoke",       "Ambient",  { 0.55f, 0.50f, 0.70f, 0.40f, 0.00f,  3500.0f, 0.45f, 0.55f, 1, 2, 3 } },
        { "Backwash Hymn",    "Ambient",  { 0.50f, 0.55f, 0.60f, 0.35f, 0.05f,  8000.0f, 0.10f, 0.45f, 1, 3, 2 } },
        // Shard
        { "Loose Tiles",      "Rhythmic", { 0.45f, 0.20f, 0.40f, 0.50f, 0.00f, 20000.0f, 0.10f, 0.50f, 2, 0, 0 } },
        { "Bit Rain",         "Rhythmic", { 0.60f, 0.25f, 0.55f, 0.00f, 0.00f, 16000.0f, 0.10f, 0.45f, 2, 1, 1 } },
        { "Pulse Garden",     "Rhythmic", { 0.50f, 0.30f, 0.50f, 0.20f, 0.05f, 20000.0f, 0.25f, 0.35f, 2, 2, 1 } },
        { "Skipping Stone",   "Rhythmic", { 0.70f, 0.35f, 0.45f, 0.30f, 0.00f, 12000.0f, 0.15f, 0.80f, 2, 3, 0 } },
        // Tap
        { "Copper Echo",      "Echo",     { 0.35f, 0.40f, 0.20f, 0.10f, 0.05f, 10000.0f, 0.20f, 0.40f, 3, 0, 1 } },
        { "Ping Lantern",     "Echo",     { 0.50f, 0.45f, 0.10f, 0.25f, 0.00f, 14000.0f, 0.10f, 0.45f, 3, 1, 1 } },
        { "Rewind Attic",     "Echo",     { 0.30f, 0.40f, 0.00f, 0.15f, 0.10f,  7000.0f, 0.35f, 0.50f, 3, 2, 2 } },
        { "Lantern Fall",     "Echo",     { 0.60f, 0.50f, 0.30f, 0.20f, 0.00f,  9000.0f, 0.00f, 0.40f, 3, 3, 2 } },
        // added in 0.3.0 (shipped as user files with 0.2.0)
        { "Backwards Letter", "Echo",     { 0.40f, 0.40f, 0.20f, 0.20f, 0.05f, 12000.0f, 0.30f, 0.50f, 3, 2, 2 } },
        { "Basement Choir",   "Ambient",  { 0.35f, 0.65f, 0.75f, 0.15f, 0.05f,  3000.0f, 0.45f, 0.55f, 0, 1, 2 } },
        { "Breath on Glass",  "Shimmer",  { 0.30f, 0.30f, 0.55f, 0.25f, 0.00f, 14000.0f, 0.20f, 0.45f, 1, 0, 1 } },
        { "Broken Metronome", "Rhythmic", { 0.85f, 0.15f, 0.20f, 0.25f, 0.00f, 20000.0f, 0.00f, 0.50f, 2, 0, 0 } },
        { "Canyon Call",      "Echo",     { 0.20f, 0.50f, 0.60f, 0.10f, 0.00f,  9000.0f, 0.20f, 0.40f, 3, 0, 2 } },
        { "Card Trick",       "Rhythmic", { 0.55f, 0.25f, 0.60f, 0.40f, 0.00f, 20000.0f, 0.05f, 0.55f, 2, 0, 0 } },
        { "Chime Field",      "Shimmer",  { 0.70f, 0.30f, 0.25f, 0.25f, 0.05f, 20000.0f, 0.30f, 0.50f, 0, 2, 1 } },
        { "Dub Siren Room",   "Lo-Fi",    { 0.40f, 0.55f, 0.30f, 0.20f, 0.10f,  3500.0f, 0.25f, 0.45f, 3, 0, 1 } },
        { "Dust Choir",       "Drift",    { 0.80f, 0.40f, 0.40f, 0.45f, 0.05f, 10000.0f, 0.30f, 0.50f, 1, 0, 2 } },
        { "Engine Room",      "Lo-Fi",    { 0.40f, 0.55f, 0.80f, 0.20f, 0.10f,  4000.0f, 0.20f, 0.50f, 1, 2, 0 } },
        { "Glass Elevator",   "Shimmer",  { 0.60f, 0.40f, 0.30f, 0.20f, 0.00f, 18000.0f, 0.15f, 0.55f, 0, 0, 1 } },
        { "Halo Static",      "Shimmer",  { 0.85f, 0.30f, 0.20f, 0.60f, 0.05f, 18000.0f, 0.15f, 0.45f, 1, 1, 1 } },
        { "Ice Lanterns",     "Shimmer",  { 0.65f, 0.45f, 0.30f, 0.40f, 0.10f, 20000.0f, 0.35f, 0.50f, 1, 1, 3 } },
        { "Lo-Fi Lullaby",    "Lo-Fi",    { 0.30f, 0.35f, 0.55f, 0.20f, 0.15f,  4500.0f, 0.35f, 0.45f, 2, 1, 2 } },
        { "Low Tide Bells",   "Ambient",  { 0.40f, 0.55f, 0.50f, 0.25f, 0.00f,  8000.0f, 0.55f, 0.60f, 0, 1, 2 } },
        { "Mirror Hallway",   "Echo",     { 0.55f, 0.50f, 0.40f, 0.35f, 0.10f,  8000.0f, 0.45f, 0.45f, 3, 2, 3 } },
        { "Morning Static",   "Rhythmic", { 0.55f, 0.35f, 0.45f, 0.60f, 0.20f, 15000.0f, 0.10f, 0.45f, 0, 0, 0 } },
        { "Morse Garden",     "Rhythmic", { 0.80f, 0.30f, 0.50f, 0.35f, 0.05f, 12000.0f, 0.20f, 0.50f, 2, 3, 1 } },
        { "Music Box Robot",  "Shimmer",  { 0.65f, 0.25f, 0.40f, 0.15f, 0.00f, 16000.0f, 0.20f, 0.40f, 2, 2, 1 } },
        { "Neon Ladder",      "Rhythmic", { 0.45f, 0.35f, 0.60f, 0.30f, 0.05f, 20000.0f, 0.30f, 0.40f, 2, 2, 2 } },
        { "Old Radio Tower",  "Lo-Fi",    { 0.35f, 0.50f, 0.35f, 0.25f, 0.15f,  3000.0f, 0.15f, 0.45f, 3, 3, 0 } },
        { "Pixel Hail",       "Rhythmic", { 0.75f, 0.30f, 0.70f, 0.20f, 0.00f, 20000.0f, 0.05f, 0.45f, 2, 1, 1 } },
        { "Pollen Drift",     "Drift",    { 0.55f, 0.35f, 0.50f, 0.50f, 0.25f, 16000.0f, 0.25f, 0.45f, 1, 0, 0 } },
        { "Rewound Rain",     "Drift",    { 0.70f, 0.40f, 0.35f, 0.55f, 0.00f, 12000.0f, 0.20f, 0.45f, 1, 3, 1 } },
        { "Rust Cassette",    "Lo-Fi",    { 0.45f, 0.20f, 0.35f, 0.10f, 0.10f,  7000.0f, 0.10f, 0.50f, 2, 1, 0 } },
        { "Satellite Drift",  "Ambient",  { 0.30f, 0.55f, 0.45f, 0.30f, 0.20f, 10000.0f, 0.50f, 0.45f, 3, 1, 3 } },
        { "Shattered Tape",   "Rhythmic", { 0.65f, 0.30f, 0.45f, 0.80f, 0.05f, 14000.0f, 0.15f, 0.50f, 2, 0, 1 } },
        { "Slapback Diner",   "Echo",     { 0.90f, 0.10f, 0.00f, 0.05f, 0.00f, 14000.0f, 0.05f, 0.35f, 3, 0, 0 } },
        { "Slow Carousel",    "Ambient",  { 0.30f, 0.60f, 0.70f, 0.10f, 0.10f, 11000.0f, 0.30f, 0.50f, 0, 0, 2 } },
        { "Spiral Staircase", "Drift",    { 0.65f, 0.45f, 0.40f, 0.45f, 0.15f, 13000.0f, 0.15f, 0.50f, 0, 3, 1 } },
        { "Stereo Fireflies", "Shimmer",  { 0.80f, 0.40f, 0.25f, 0.45f, 0.05f, 20000.0f, 0.25f, 0.40f, 3, 1, 1 } },
        { "Stuck Record",     "Rhythmic", { 0.50f, 0.40f, 0.30f, 0.10f, 0.00f, 18000.0f, 0.10f, 0.55f, 2, 3, 0 } },
        { "Sugar Ladder",     "Shimmer",  { 0.80f, 0.25f, 0.15f, 0.40f, 0.10f, 20000.0f, 0.20f, 0.45f, 0, 2, 0 } },
        { "Tennis Court",     "Echo",     { 0.60f, 0.35f, 0.15f, 0.20f, 0.00f, 16000.0f, 0.05f, 0.40f, 3, 1, 0 } },
        { "Tidal Prayer",     "Ambient",  { 0.45f, 0.60f, 0.70f, 0.30f, 0.05f,  9000.0f, 0.40f, 0.50f, 1, 3, 3 } },
        { "Undertow",         "Ambient",  { 0.50f, 0.50f, 0.55f, 0.30f, 0.00f,  6000.0f, 0.20f, 0.50f, 0, 1, 3 } },
        { "Velvet Smoke",     "Lo-Fi",    { 0.50f, 0.50f, 0.60f, 0.25f, 0.15f,  5000.0f, 0.30f, 0.50f, 1, 2, 1 } },
        { "Waterfall Steps",  "Echo",     { 0.45f, 0.45f, 0.50f, 0.15f, 0.00f, 11000.0f, 0.20f, 0.45f, 3, 3, 2 } },
        { "Wool Fog",         "Ambient",  { 0.60f, 0.45f, 0.65f, 0.35f, 0.00f,  2500.0f, 0.40f, 0.55f, 1, 2, 2 } },
        { "Wrong Way Home",   "Ambient",  { 0.45f, 0.55f, 0.60f, 0.20f, 0.05f, 10000.0f, 0.25f, 0.55f, 0, 3, 2 } },
    } };
    return presets;
}
} // namespace grain::presets
