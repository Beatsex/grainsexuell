#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

namespace grain::dsp
{
// 8-line feedback delay network (Hadamard mixing, per-line damping, slow delay modulation, input allpasses).
// Out = dry * cos(haze*pi/2) + wet * sin(haze*pi/2): Haze 0 is bit-exact dry, Haze 1 fully wet.
// Mode change ducks the wet signal (20 ms), swaps delay lengths/decay and clears the lines, then unducks.
class Reverb
{
public:
    enum Mode { Room, Plate, Hall, Swell, numModes };
    static constexpr int numLines = 8;
    static constexpr std::array<float, numModes> t60 { 0.5f, 2.0f, 4.5f, 8.0f }; // seconds
    static constexpr float maxT60 = 8.0f;

    void prepare(double sampleRate);
    void process(float* const* ch, int numCh, int n, float haze, int mode);

private:
    struct ModeData
    {
        std::array<int, numLines> len {};
        std::array<float, numLines> gain {};
        float damp = 0, modDepth = 0, outGain = 0;
    };

    struct Allpass
    {
        std::vector<float> buf;
        int mask = 0, pos = 0, len = 1;
        float process(float x)
        {
            constexpr float g = 0.6f;
            const float d = buf[static_cast<size_t>((pos - len) & mask)];
            const float w = x + g * d;
            buf[static_cast<size_t>(pos)] = w;
            pos = (pos + 1) & mask;
            return d - g * w;
        }
    };

    void applyMode(int mode);

    double sampleRate = 48000.0;
    std::array<ModeData, numModes> modes;
    ModeData active;
    std::array<std::vector<float>, numLines> lines;
    std::array<Allpass, 4> allpass; // 0,1 = left, 2,3 = right
    std::array<float, numLines> lp {}, lfoPhase {}, lfoInc {};
    int mask = 0, writePos = 0;
    int currentMode = -1, pendingMode = -1;
    float lastHaze = -1.0f, dryGain = 1.0f, wetGain = 0.0f;
    juce::SmoothedValue<float> duck, hazeSm;
};
} // namespace grain::dsp
