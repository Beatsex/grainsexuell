#pragma once

#include "Capture.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace grain::dsp
{
// Shard: glitch engine. The recent past is cut into slices on the tempo grid and replayed rearranged, crushed or
// pitched. Every slice is a voice with 5 ms fades; voice length = slice + one fade, so consecutive voices
// crossfade to unity.
//
//  - Swarm:   slice length in 7 steps, 1/4 .. 1/32 incl. triplets (slow .. fast), folded into 30 ms .. 750 ms
//  - Span:    A: how far back slices come from (1..8 slices)   B: crush depth (12 .. 3 bits, 1x .. 12x sample hold)
//             C: unused                                          D: repeats of a frozen slice (1..8, less near the ring limit)
//  - Scatter: chance of a reversed slice (up to 50%) and stereo spread
//  - Tail:    feedback of the wet signal into the recording, at most 0.85, soft-limited
//  - Mix:     dry*cos + wet*sin; Mix 0 (or Family != Shard) leaves the audio bit-exact
//
// Variants: A Shuffle (random slice among the last Span+1), B Crush (newest slice repeated, bitcrushed),
// C Arp (recent onsets of the input, cycled with pitch ratios 1, 1.19, 1.5, 2), D Stutter (a frozen slice repeated, -1.4 dB per repeat).
//
// Causality as in Cloud and Tap: no voice reads closer than minAge frames (> one 512 chunk) to the live position.
class Shard
{
public:
    struct Params
    {
        float swarm = 0.5f, tail = 0.5f, span = 0.5f, scatter = 0.0f, mix = 0.5f;
        int variant = 0; // 0 Shuffle, 1 Crush, 2 Arp, 3 Stutter
        bool enabled = true;
        double bpm = 120.0;
    };

    void prepare(double sampleRate);
    void process(float* const* ch, int numCh, int n, const Params& p);
    int slicesPlayed() const { return played.load(std::memory_order_relaxed); }

private:
    static constexpr int maxVoices = 8;
    static constexpr int numOnsets = 4;
    static constexpr int chunk = Capture::chunk;
    static constexpr int minAge = chunk + 4;

    struct Voice
    {
        bool active = false;
        bool lowpassed = false; // reads the 2x pre-filtered copy
        double pos = 0.0;       // absolute frame (ring index = floor(pos) & mask), fractional
        float step = 1.0f;
        float gain = 1.0f, panL = 1.0f, panR = 1.0f;
        int age = 0, length = 1;
    };

    void spawn(const Params& p, double now, double sliceFrames, int maxBack, int numCh);
    void startVoice(double pos, float step, bool lowpassed, float gain, const Params& p, int numCh, int sliceFrames);
    void detectOnset(float dryL, float dryR);
    float rand01();

    double sampleRate = 48000.0;
    Capture cap;
    std::array<Voice, maxVoices> voices;
    std::array<std::array<float, chunk>, 2> rec;
    int fade = 240; // 5 ms

    // onset detector (runs on the dry input, one sample at a time)
    float fast = 0.0f, slow = 0.0f, fastCoef = 0.0f, slowCoef = 0.0f;
    int refractory = 0, refractoryFrames = 0;
    std::array<std::int64_t, numOnsets> onsets {};
    int numFound = 0, onsetHead = 0;

    std::int64_t absFrame = 0; // frames processed; ring index = absFrame & mask
    juce::SmoothedValue<float> mixSm;
    float dryGain = 1.0f, wetGain = 0.0f, lastMix = -1.0f;
    double gap = 0.0; // frames until the next slice
    int stepCount = 0;                   // slices spawned since activation (arp position)
    int repeatsLeft = 0;                 // Stutter
    double frozenPos = 0.0;              // Stutter: start of the frozen slice
    float stutterLevel = 1.0f;
    int holdCounter = 0;                 // Crush: sample-and-hold
    float heldL = 0.0f, heldR = 0.0f;
    std::atomic<int> played { 0 };
    bool primed = false, wasActive = false;
    std::uint32_t rng = 1;
};
} // namespace grain::dsp
