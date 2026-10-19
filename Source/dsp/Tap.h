#pragma once

#include "Capture.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace grain::dsp
{
// Tap: tempo-synced multitap delay. Four taps read the recent past at multiples of a tempo subdivision.
//
//  - Swarm:   subdivision of the beat: 1/1, 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16 (slow .. fast); base time clamped to 20 ms .. 2 s
//  - Span:    spacing: tap k sits at (k+1) * (1..4) base steps; taps later than 2 s drop out
//  - Scatter: timing jitter (static per tap, up to +/-15%) and stereo spread
//  - Tail:    feedback of the wet signal into the delay line, at most 0.85, scaled by the tap gains and soft-limited
//  - Mix:     dry*cos + wet*sin; Mix 0 (or Family != Tap) leaves the audio bit-exact
//
// Variants: A Echo (even taps), B Ping-pong (taps alternate hard left / right, fed from the mid signal),
// C Reverse (each tap plays Hann-windowed backwards snippets), D Cascade (spacing shrinks, taps get darker).
//
// Causality as in Cloud: no read comes closer than minAge frames (> one 512 chunk) to the write position, so the
// result does not depend on the host block size. Delay times glide over 50 ms when tempo or settings change.
class Tap
{
public:
    struct Params
    {
        float swarm = 0.5f, tail = 0.5f, span = 0.5f, scatter = 0.0f, mix = 0.5f;
        int variant = 0; // 0 Echo, 1 Ping-pong, 2 Reverse, 3 Cascade
        bool enabled = true;
        double bpm = 120.0; // host tempo, or the fallback
    };

    void prepare(double sampleRate);
    void process(float* const* ch, int numCh, int n, const Params& p);
    int echoCount() const { return echoes.load(std::memory_order_relaxed); } // repeats heard so far (GUI activity)

private:
    static constexpr int numTaps = 4;
    static constexpr int maxGrains = 16;
    static constexpr int chunk = Capture::chunk;
    static constexpr int minAge = chunk + 4;
    static constexpr int hannSize = 1024;

    struct Grain // one backwards snippet (variant C)
    {
        bool active = false;
        double pos = 0.0; // ring index, moves backwards
        float gain = 0.0f, panL = 1.0f, panR = 1.0f;
        int age = 0, length = 1;
    };

    void spawnReverse(double startPos, int lengthFrames, float gain, float panL, float panR);

    double sampleRate = 48000.0;
    Capture cap;
    std::array<Grain, maxGrains> grains;
    std::array<float, hannSize + 1> hann {};
    std::array<std::array<float, chunk>, 2> rec;
    std::array<float, numTaps> jitter {}, spread {}; // fixed per tap, -1 .. 1

    std::array<juce::SmoothedValue<float>, numTaps> delaySm, gainSm; // delay in frames
    juce::SmoothedValue<float> mixSm;
    float dryGain = 1.0f, wetGain = 0.0f, lastMix = -1.0f;
    double gap = 0.0; // frames until the next reverse snippet
    bool primed = false, wasActive = false;
    double echoPhase = 0.0; // frames into the current tap step
    std::atomic<int> echoes { 0 };
};
} // namespace grain::dsp
