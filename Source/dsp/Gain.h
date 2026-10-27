#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>

namespace grain::dsp
{
// Smoothed trim gain in dB (+/-24). At exactly 0 dB with no ramp running it does nothing, so the audio stays bit-exact.
// The ramp is per sample, so the result does not depend on the block size.
class Gain
{
public:
    static constexpr float maxDb = 24.0f;

    void prepare(double sampleRate)
    {
        gain.reset(sampleRate, 0.02);
        gain.setCurrentAndTargetValue(1.0f);
    }

    void process(float* const* ch, int numCh, int n, float db)
    {
        const float clean = std::isfinite(db) ? std::clamp(db, -maxDb, maxDb) : 0.0f;
        // under half a 0.1 dB step is 0 dB: snapping to the grid lands 3.6e-7 dB off 0 on some compilers (macOS)
        gain.setTargetValue(std::abs(clean) < 0.05f ? 1.0f : std::pow(10.0f, clean / 20.0f));
        if (! gain.isSmoothing() && gain.getTargetValue() == 1.0f)
            return;
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.getNextValue();
            for (int c = 0; c < numCh; ++c)
                ch[c][i] *= g;
        }
    }

private:
    juce::SmoothedValue<float> gain { 1.0f };
};
} // namespace grain::dsp
