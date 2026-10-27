#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace grain::dsp
{
// Master dry/wet: an equal-power blend of the input (after In gain) with the finished effect (after Dusk and
// Auto Gain). At 100 % with no ramp running it does nothing, so the wet path stays bit-exact. Per-sample ramp.
class DryWet
{
public:
    void prepare(double sampleRate, int maxBlock)
    {
        dry.setSize(2, std::max(maxBlock, 4096)); // never resized on the audio thread
        wet.reset(sampleRate, 0.02);
        wet.setCurrentAndTargetValue(1.0f);
    }

    // call before the effect chain
    void capture(const float* const* ch, int numCh, int n)
    {
        // ponytail: a block bigger than prepared skips the blend (100 % wet); copy in chunks if a host ever does that
        captured = n <= dry.getNumSamples();
        if (captured)
            for (int c = 0; c < numCh; ++c)
                dry.copyFrom(c, 0, ch[c], n);
    }

    // call after the effect chain
    void blend(float* const* ch, int numCh, int n, float amount)
    {
        wet.setTargetValue(std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 1.0f);
        if (! wet.isSmoothing() && wet.getTargetValue() == 1.0f)
            return;
        if (! captured)
        {
            wet.skip(n);
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const float m = wet.getNextValue() * (std::numbers::pi_v<float> / 2.0f);
            const float w = std::sin(m), d = std::cos(m);
            for (int c = 0; c < numCh; ++c)
                ch[c][i] = w * ch[c][i] + d * dry.getSample(c, i);
        }
    }

private:
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> wet { 1.0f };
    bool captured = false;
};
} // namespace grain::dsp
