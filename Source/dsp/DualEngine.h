#pragma once

#include "EngineSet.h"

#include <array>

namespace grain::dsp
{
// Two engine slots. B is optional and runs either
//   Serial:   input -> A -> B
//   Parallel: A and B both hear the input, output = (A + B) / 2  (Mix 0 on both = the input, bit-exact)
// B fades in and out over 20 ms (switching B on/off, or changing the route, which fades B out, switches, fades back in).
// While B is off it keeps recording (inaudibly, Mix 0), so its memory has no gap when it comes back; its source
// crossfades over 20 ms after a route change.
// With B off and settled, A's output is untouched: bit-identical to a single slot. B works on a preallocated side buffer, in chunks,
// so nothing allocates on the audio thread and the result does not depend on the host block size (route changes take
// effect at block starts).
class DualEngine
{
public:
    struct Params
    {
        EngineSet::Params a, b;
        bool bOn = false;
        int route = 0; // 0 Serial, 1 Parallel
    };

    void prepare(double sampleRate, const Params& initial);
    void process(float* const* ch, int numCh, int n, const Params& p);

    EngineSet a, b;

private:
    static constexpr int sideFrames = 2048;

    std::array<std::array<float, sideFrames>, 2> side {};
    juce::SmoothedValue<float> fade { 0.0f }; // 0 = B silent, 1 = B fully in
    juce::SmoothedValue<float> source { 0.0f }; // what B records: 0 = A output (serial), 1 = input (parallel)
    int activeRoute = 0;
    EngineSet::Params lastB; // B keeps its last settings while it fades out
};
} // namespace grain::dsp
