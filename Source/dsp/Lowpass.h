#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace grain::dsp
{
// 12 dB/oct RBJ biquad (Butterworth Q). At or above bypassHz the filter is crossfaded out (10 ms) so the
// fully open setting is bit-exact. Cutoff is smoothed in the log domain.
class Lowpass
{
public:
    static constexpr float bypassHz = 19900.0f;

    void prepare(double sr)
    {
        sampleRate = sr;
        cutoffLog2.reset(sr, 0.02);
        active.reset(sr, 0.01);
        z1 = z2 = { 0.0, 0.0 };
        primed = false;
    }

    // ch: up to 2 channels, processed in place
    void process(float* const* ch, int numCh, int n, float hz)
    {
        const float target = std::log2(std::clamp(hz, 20.0f, 0.45f * static_cast<float>(sampleRate)));
        const float activeTarget = hz >= bypassHz ? 0.0f : 1.0f;

        if (! primed)
        {
            cutoffLog2.setCurrentAndTargetValue(target);
            active.setCurrentAndTargetValue(activeTarget);
            setCoeffs(std::exp2(target));
            primed = true;
        }
        else
        {
            cutoffLog2.setTargetValue(target);
            active.setTargetValue(activeTarget);
        }

        for (int i = 0; i < n; ++i)
        {
            if (cutoffLog2.isSmoothing())
                setCoeffs(std::exp2(cutoffLog2.getNextValue()));
            const float a = active.getNextValue();

            for (int c = 0; c < numCh; ++c)
            {
                const double x = ch[c][i];
                const double y = b0 * x + z1[c];
                z1[c] = b1 * x - a1 * y + z2[c];
                z2[c] = b2 * x - a2 * y;
                ch[c][i] = static_cast<float>(x + a * (y - x)); // a == 0 returns x exactly
            }
        }
    }

private:
    void setCoeffs(float hz)
    {
        const double w0 = 2.0 * std::numbers::pi * hz / sampleRate;
        const double cosw = std::cos(w0), alpha = std::sin(w0) / (2.0 * 0.70710678);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - cosw) * 0.5 / a0;
        b1 = (1.0 - cosw) / a0;
        b2 = b0;
        a1 = -2.0 * cosw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    double sampleRate = 48000.0;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<double, 2> z1 {}, z2 {};
    juce::SmoothedValue<float> cutoffLog2, active;
    bool primed = false;
};
} // namespace grain::dsp
