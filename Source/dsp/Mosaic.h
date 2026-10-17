#pragma once

#include "Lowpass.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>
#include <vector>

namespace grain::dsp
{
// Mosaic: layered micro-loops. The input is always recorded into a ring; every so often the most recent Span
// of audio is grabbed as a loop and played back as four layers at 0.5x / 1x / 2x / 4x (variant picks which
// layers play and in which direction). A new grab crossfades over the old one. Latency 0.
//
//  - Swarm: grab rate (3 s .. 0.12 s, never faster than 2 x the loop length) and fade-in of the fast layers
//  - Tail:  how long a grabbed loop keeps sounding (decay time constant 0.15 s .. no decay)
//  - Span:  loop length 25 ms .. 750 ms
//  - Scatter: random start offset, grab-time jitter and per-layer stereo spread
//  - Mix:   dry*cos + wet*sin; Mix 0 (or Family != Mosaic) leaves the audio bit-exact
//
// Aliasing: the 2x and 4x layers read from copies of the input that were low-passed while recording, because
// filtering after the speed-up cannot undo aliasing. ponytail: 12 dB/oct pre-filter, oversampling if it bothers.
class Mosaic
{
public:
    struct Params
    {
        float swarm = 0.5f, tail = 0.5f, span = 0.5f, scatter = 0.0f, mix = 0.5f;
        int variant = 0; // 0 Stack, 1 Low, 2 High, 3 Tumble
        bool enabled = true;
    };

    static constexpr int numLayers = 4;

    void prepare(double sampleRate);
    void process(float* const* ch, int numCh, int n, const Params& p);
    int grabCount() const { return grabs; }

private:
    static constexpr int chunk = 512;
    static constexpr std::array<float, numLayers> speeds { 0.5f, 1.0f, 2.0f, 4.0f };
    static constexpr std::array<int, numLayers> sourceOf { 0, 0, 1, 2 }; // raw, low-passed for 2x, low-passed for 4x

    struct LayerCfg
    {
        float gain, dir;
    };
    // [variant][layer]; gains keep the wet RMS close to the dry RMS
    static constexpr std::array<std::array<LayerCfg, numLayers>, 4> tables { {
        { { { 0.55f, 1 }, { 0.60f, 1 }, { 0.40f, 1 }, { 0.30f, 1 } } },   // A Stack
        { { { 0.70f, 1 }, { 0.70f, 1 }, { 0.00f, 1 }, { 0.00f, 1 } } },   // B Low
        { { { 0.00f, 1 }, { 0.60f, 1 }, { 0.50f, 1 }, { 0.35f, 1 } } },   // C High
        { { { 0.55f, -1 }, { 0.60f, 1 }, { 0.40f, -1 }, { 0.30f, 1 } } }, // D Tumble
    } };

    using Stereo = std::array<std::vector<float>, 2>;

    struct Slot
    {
        std::array<Stereo, 3> src; // [pre-roll X][loop L][post-roll X] for raw / lp2 / lp4
        int length = 0, pre = 0;
        std::array<LayerCfg, numLayers> cfg {};
        std::array<float, numLayers> phase {}, pan {};
        float fade = 0.0f, fadeTarget = 0.0f, env = 1.0f;
    };

    void grab(const Params& p);
    float rand01();
    static float read(const float* b, int X, int L, int i0, float frac, bool reverse);

    double sampleRate = 48000.0;
    int ringSize = 0, ringMask = 0, writePos = 0;
    std::array<Stereo, 3> ring;
    std::array<Slot, 2> slots;
    int cur = 0;

    Lowpass lp2, lp4;
    std::array<std::array<float, chunk>, 2> raw2, raw4;

    juce::SmoothedValue<float> mixSm, swarmSm;
    float dryGain = 1.0f, wetGain = 0.0f, lastMix = -1.0f;
    int countdown = 0, grabs = 0, lastVariant = 0;
    bool primed = false, wasActive = false, forceGrab = false;
    std::uint32_t rng = 1;
};
} // namespace grain::dsp
