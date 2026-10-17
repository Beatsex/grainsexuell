#pragma once

#include "Capture.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace grain::dsp
{
// Cloud: granular engine. A scheduler spawns Hann-windowed grains that read the recent past (up to ~2 s back)
// at the variant's pitch and direction; many overlapping grains make a cloud.
//
//  - Swarm:   density, 3 .. 80 grains per second (exponential)
//  - Span:    grain length, 10 .. 400 ms (log)
//  - Scatter: spray: start position up to 1.2 s (plus a 20 ms base) into the past, spacing jitter, +/-30 cents pitch jitter, stereo spread
//  - Tail:    feedback: the wet signal is mixed back into the recording at up to Tail * 0.85, scaled down at high
//             density (coherent worst case) and soft-limited, so the loop always decays
//  - Mix:     dry*cos + wet*sin; Mix 0 (or Family != Cloud) leaves the audio bit-exact
//
// Causality: a grain never reads closer than minAge frames (> one 512 chunk) to the live write position, so
// every read only touches audio recorded before the current chunk. That keeps the result independent of the host
// block size, and the feedback (recorded after the chunk) well defined.
class Cloud
{
public:
    struct Params
    {
        float swarm = 0.5f, tail = 0.5f, span = 0.5f, scatter = 0.0f, mix = 0.5f;
        int variant = 0; // 0 Mist, 1 Shimmer, 2 Murk, 3 Backwash
        bool enabled = true;
    };

    void prepare(double sampleRate);
    void process(float* const* ch, int numCh, int n, const Params& p);
    int grainsSpawned() const { return spawned.load(std::memory_order_relaxed); }

private:
    static constexpr int maxGrains = 64;
    static constexpr int chunk = Capture::chunk;
    static constexpr int minAge = chunk + 4;
    static constexpr int hannSize = 1024;

    struct Grain
    {
        bool active = false;
        bool lowpassed = false; // reads the 2x pre-filtered copy
        double pos = 0.0;       // ring index (fractional)
        float step = 1.0f;      // signed frames per output frame
        float gain = 0.0f, panL = 1.0f, panR = 1.0f;
        int age = 0, length = 1;
    };

    void spawn(const Params& p, int framesIntoChunk, float density, int lengthFrames, int numCh);
    float rand01();

    double sampleRate = 48000.0;
    Capture cap;
    std::array<Grain, maxGrains> grains;
    std::array<float, hannSize + 1> hann {};
    std::array<std::array<float, chunk>, 2> rec;

    juce::SmoothedValue<float> mixSm;
    float dryGain = 1.0f, wetGain = 0.0f, lastMix = -1.0f;
    double gap = 0.0; // frames until the next grain
    std::atomic<int> spawned { 0 };
    bool primed = false, wasActive = false;
    std::uint32_t rng = 1;
};
} // namespace grain::dsp
