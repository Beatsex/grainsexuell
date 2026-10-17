#pragma once

#include "Lowpass.h"

#include <array>
#include <vector>

namespace grain::dsp
{
// Always-on recorder shared by the effect engines: a stereo ring buffer plus two copies that are low-passed
// while recording. Reading the audio faster than 1x folds content above Nyquist/speed back into the band
// (aliasing), and filtering the output afterwards cannot undo that, so the 2x / 4x reads use the pre-filtered
// copies. ponytail: 12 dB/oct, and bypassed above 19.9 kHz cutoff (fs >= ~88 kHz for the 2x copy).
class Capture
{
public:
    static constexpr int chunk = 512; // write() takes at most this many frames
    using Stereo = std::array<std::vector<float>, 2>;

    std::array<Stereo, 3> ring; // [0] raw, [1] low-passed for 2x reads, [2] low-passed for 4x reads
    int size = 0, mask = 0, writePos = 0;

    void prepare(double sampleRate, float seconds);
    // Records m <= chunk frames (l and r may be the same pointer for a mono source).
    void write(const float* l, const float* r, int m);

private:
    double sampleRate = 48000.0;
    Lowpass lp2, lp4;
    std::array<std::array<float, chunk>, 2> raw2, raw4;
};
} // namespace grain::dsp
