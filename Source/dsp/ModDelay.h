#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace grain::dsp
{
// Vibrato/chorus-style pitch modulation: a short delay swept by a sine LFO. delay = depth * (0.5 + 0.5 lfo),
// so Bend = 0 means delay 0 and the output is bit-identical to the input (zero latency).
// Linear interpolation; right channel LFO is 90 degrees ahead for width.
class ModDelay
{
public:
    static constexpr float maxDepthSeconds = 0.005f;

    void prepare(double sr)
    {
        sampleRate = sr;
        size = 1;
        while (size < static_cast<int>(maxDepthSeconds * sr) + 8)
            size <<= 1;
        mask = size - 1;
        for (auto& b : buf)
            b.assign(static_cast<size_t>(size), 0.0f);
        write = 0;
        phase = 0.0;
        depthSm.reset(sr, 0.05);
        rateSm.reset(sr, 0.05);
        primed = false;
    }

    void process(float* const* ch, int numCh, int n, float bend)
    {
        const float depthTarget = bend * maxDepthSeconds * static_cast<float>(sampleRate); // samples, peak-to-peak
        const float rateTarget = 0.3f + 3.7f * bend;                                        // Hz

        if (! primed)
        {
            depthSm.setCurrentAndTargetValue(depthTarget);
            rateSm.setCurrentAndTargetValue(rateTarget);
            primed = true;
        }
        else
        {
            depthSm.setTargetValue(depthTarget);
            rateSm.setTargetValue(rateTarget);
        }

        constexpr double twoPi = 2.0 * std::numbers::pi;
        for (int i = 0; i < n; ++i)
        {
            const float depth = depthSm.getNextValue();
            phase += twoPi * rateSm.getNextValue() / sampleRate;
            if (phase >= twoPi)
                phase -= twoPi;

            for (int c = 0; c < numCh; ++c)
            {
                auto& b = buf[static_cast<size_t>(c)];
                b[static_cast<size_t>(write)] = ch[c][i];

                const float lfo = 0.5f + 0.5f * static_cast<float>(std::sin(phase + (c == 1 ? 0.5 * std::numbers::pi : 0.0)));
                const float d = depth * lfo;
                const int di = static_cast<int>(d);
                const float frac = d - static_cast<float>(di);
                const float x0 = b[static_cast<size_t>((write - di) & mask)];
                const float x1 = b[static_cast<size_t>((write - di - 1) & mask)];
                ch[c][i] = x0 + frac * (x1 - x0); // d == 0 returns x0 exactly
            }
            write = (write + 1) & mask;
        }
    }

private:
    double sampleRate = 48000.0, phase = 0.0;
    std::array<std::vector<float>, 2> buf;
    int size = 0, mask = 0, write = 0;
    juce::SmoothedValue<float> depthSm, rateSm;
    bool primed = false;
};
} // namespace grain::dsp
