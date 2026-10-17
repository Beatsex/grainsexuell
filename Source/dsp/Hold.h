#pragma once

#include <array>
#include <vector>

namespace grain::dsp
{
// Hold: freeze sampler. A private ring records the (leveled) input all the time. Switching Hold on stops the
// recording and loops the last beat straight out of the ring (nothing is copied), adding it to the live input
// before the effect engine. Switching it off fades the loop out and resumes recording. Off = bit-exact.
//
//  - Loop: one beat at the given tempo, folded into 150 ms .. 2 s, fixed when Hold goes on
//  - Seam: the loop's last 10 ms are replaced by an equal-power crossfade with the audio just before the loop
//    start, so the loop (period = length - seam) joins itself without a click
//  - Fades: 20 ms in, 50 ms out
class Hold
{
public:
    void prepare(double sampleRate);
    void process(float* const* ch, int numCh, int n, bool hold, double bpm);

    int loopPeriod() const { return period; } // frames; tests

private:
    double sampleRate = 48000.0;
    std::array<std::vector<float>, 2> ring;
    int size = 0, mask = 0, writePos = 0;
    bool held = false;
    float gain = 0.0f, fadeIn = 0.01f, fadeOut = 0.01f; // gain and its per-frame steps
    int start = 0, period = 1, seam = 1, loopPos = 0;
};
} // namespace grain::dsp
