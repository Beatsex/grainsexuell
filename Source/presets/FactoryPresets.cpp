#include "FactoryPresets.h"

namespace grain::presets
{
const std::array<Preset, numFactory>& factory()
{
    // swarm, tail, span, scatter, bend, dusk, haze, mix, family (0 Mosaic, 1 Cloud, 2 Shard, 3 Tap), variant, hazemode
    // (0 Room, 1 Plate, 2 Hall, 3 Swell). First guesses, tuned by ear later.
    static const std::array<Preset, numFactory> presets { {
        // Mosaic
        { "Tide Ladder",     { 0.55f, 0.45f, 0.50f, 0.15f, 0.05f, 14000.0f, 0.20f, 0.50f, 0, 0, 1 } },
        { "Cellar Door",     { 0.40f, 0.55f, 0.60f, 0.10f, 0.00f,  5000.0f, 0.35f, 0.55f, 0, 1, 2 } },
        { "Paper Stars",     { 0.65f, 0.35f, 0.35f, 0.30f, 0.15f, 20000.0f, 0.25f, 0.60f, 0, 2, 1 } },
        { "Falling Stairs",  { 0.50f, 0.50f, 0.50f, 0.35f, 0.10f,  9000.0f, 0.10f, 0.50f, 0, 3, 0 } },
        // Cloud
        { "Soft Mist",       { 0.60f, 0.30f, 0.45f, 0.30f, 0.05f, 12000.0f, 0.00f, 0.50f, 1, 0, 1 } },
        { "Glass Weather",   { 0.75f, 0.40f, 0.35f, 0.50f, 0.10f, 20000.0f, 0.05f, 0.50f, 1, 1, 2 } },
        { "Peat Smoke",      { 0.55f, 0.50f, 0.70f, 0.40f, 0.00f,  3500.0f, 0.45f, 0.55f, 1, 2, 3 } },
        { "Backwash Hymn",   { 0.50f, 0.55f, 0.60f, 0.35f, 0.05f,  8000.0f, 0.10f, 0.45f, 1, 3, 2 } },
        // Shard
        { "Loose Tiles",     { 0.45f, 0.20f, 0.40f, 0.50f, 0.00f, 20000.0f, 0.10f, 0.50f, 2, 0, 0 } },
        { "Bit Rain",        { 0.60f, 0.25f, 0.55f, 0.00f, 0.00f, 16000.0f, 0.10f, 0.45f, 2, 1, 1 } },
        { "Pulse Garden",    { 0.50f, 0.30f, 0.50f, 0.20f, 0.05f, 20000.0f, 0.25f, 0.35f, 2, 2, 1 } },
        { "Skipping Stone",  { 0.70f, 0.35f, 0.45f, 0.30f, 0.00f, 12000.0f, 0.15f, 0.80f, 2, 3, 0 } },
        // Tap
        { "Copper Echo",     { 0.35f, 0.40f, 0.20f, 0.10f, 0.05f, 10000.0f, 0.20f, 0.40f, 3, 0, 1 } },
        { "Ping Lantern",    { 0.50f, 0.45f, 0.10f, 0.25f, 0.00f, 14000.0f, 0.10f, 0.45f, 3, 1, 1 } },
        { "Rewind Attic",    { 0.30f, 0.40f, 0.00f, 0.15f, 0.10f,  7000.0f, 0.35f, 0.50f, 3, 2, 2 } },
        { "Lantern Fall",    { 0.60f, 0.50f, 0.30f, 0.20f, 0.00f,  9000.0f, 0.00f, 0.40f, 3, 3, 2 } },
    } };
    return presets;
}
} // namespace grain::presets
