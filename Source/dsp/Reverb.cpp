#include "Reverb.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace grain::dsp
{
namespace
{
// Delay lengths in samples at 48 kHz (scaled per mode and sample rate, then moved to the next prime).
constexpr std::array<int, Reverb::numLines> baseLen { 1327, 1559, 1877, 2113, 2371, 2647, 2927, 3203 };
constexpr std::array<float, Reverb::numModes> lenScale { 0.4f, 0.7f, 1.3f, 1.8f };
constexpr std::array<float, Reverb::numModes> damping { 0.25f, 0.12f, 0.4f, 0.5f };
constexpr std::array<float, Reverb::numModes> modSamples48k { 0.4f, 1.5f, 2.5f, 5.0f };
constexpr std::array<int, 4> allpassLen { 142, 107, 163, 121 };

constexpr std::array<float, Reverb::numLines> inL { 1, 1, 1, 1, -1, -1, -1, -1 };
constexpr std::array<float, Reverb::numLines> inR { 1, -1, 1, -1, 1, -1, 1, -1 };
constexpr std::array<float, Reverb::numLines> outL { 1, 1, -1, -1, 1, 1, -1, -1 };
constexpr std::array<float, Reverb::numLines> outR { 1, -1, -1, 1, 1, -1, -1, 1 };

int nextPrime(int n)
{
    for (;; ++n)
    {
        bool prime = n > 1;
        for (int d = 2; d * d <= n && prime; ++d)
            prime = n % d != 0;
        if (prime)
            return n;
    }
}

int powerOfTwoAtLeast(int n)
{
    int p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

void hadamard8(float* v)
{
    for (int h = 1; h < 8; h <<= 1)
        for (int i = 0; i < 8; i += 2 * h)
            for (int j = i; j < i + h; ++j)
            {
                const float a = v[j], b = v[j + h];
                v[j] = a + b;
                v[j + h] = a - b;
            }
    for (int i = 0; i < 8; ++i)
        v[i] *= 0.35355339f; // 1/sqrt(8): orthogonal, so only the line gains lose energy
}
} // namespace

void Reverb::prepare(double sr)
{
    sampleRate = sr;
    const double k = sr / 48000.0;
    int maxLen = 0;

    for (int m = 0; m < numModes; ++m)
    {
        auto& md = modes[static_cast<size_t>(m)];
        double sumG2 = 0.0;
        for (int i = 0; i < numLines; ++i)
        {
            const auto si = static_cast<size_t>(i);
            md.len[si] = nextPrime(static_cast<int>(std::lround(baseLen[si] * lenScale[static_cast<size_t>(m)] * k)));
            md.gain[si] = static_cast<float>(std::pow(10.0, -3.0 * md.len[si] / (t60[static_cast<size_t>(m)] * sr)));
            sumG2 += static_cast<double>(md.gain[si]) * md.gain[si];
            maxLen = std::max(maxLen, md.len[si]);
        }
        md.damp = damping[static_cast<size_t>(m)];
        md.modDepth = static_cast<float>(modSamples48k[static_cast<size_t>(m)] * k);
        // steady-state wet power == dry power for white noise (equal impulse energy), see plan
        md.outGain = 0.5f * static_cast<float>(std::sqrt(1.0 - sumG2 / numLines));
    }

    const int maxMod = static_cast<int>(std::ceil(modSamples48k[numModes - 1] * k)) + 2;
    const int size = powerOfTwoAtLeast(maxLen + maxMod + 4);
    mask = size - 1;
    for (auto& l : lines)
        l.assign(static_cast<size_t>(size), 0.0f);

    for (size_t a = 0; a < allpass.size(); ++a)
    {
        allpass[a].len = nextPrime(static_cast<int>(std::lround(allpassLen[a] * k)));
        const int s = powerOfTwoAtLeast(allpass[a].len + 1);
        allpass[a].buf.assign(static_cast<size_t>(s), 0.0f);
        allpass[a].mask = s - 1;
        allpass[a].pos = 0;
    }

    for (int i = 0; i < numLines; ++i)
    {
        const auto si = static_cast<size_t>(i);
        lfoPhase[si] = 0.7f * static_cast<float>(i);
        lfoInc[si] = static_cast<float>(2.0 * std::numbers::pi * (0.07 + 0.043 * i) / sr);
    }

    lp.fill(0.0f);
    writePos = 0;
    currentMode = pendingMode = -1;
    lastHaze = -1.0f;
    duck.reset(sr, 0.02);
    duck.setCurrentAndTargetValue(1.0f);
    hazeSm.reset(sr, 0.02);
}

void Reverb::applyMode(int mode)
{
    active = modes[static_cast<size_t>(mode)];
    for (auto& l : lines)
        std::fill(l.begin(), l.end(), 0.0f);
    for (auto& a : allpass)
        std::fill(a.buf.begin(), a.buf.end(), 0.0f);
    lp.fill(0.0f);
    currentMode = mode;
}

void Reverb::process(float* const* ch, int numCh, int n, float haze, int mode)
{
    mode = std::clamp(mode, 0, static_cast<int>(numModes) - 1);

    if (currentMode < 0)
    {
        applyMode(mode);
        hazeSm.setCurrentAndTargetValue(haze);
    }
    pendingMode = mode;
    hazeSm.setTargetValue(haze);
    if (pendingMode != currentMode)
        duck.setTargetValue(0.0f);

    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr float halfPi = 0.5f * std::numbers::pi_v<float>;

    for (int i = 0; i < n; ++i)
    {
        float duckGain = duck.getNextValue();
        if (pendingMode != currentMode && duckGain == 0.0f)
        {
            applyMode(pendingMode);
            duck.setTargetValue(1.0f);
        }

        const float h = hazeSm.getNextValue();
        if (h != lastHaze)
        {
            dryGain = std::cos(h * halfPi);
            wetGain = std::sin(h * halfPi);
            lastHaze = h;
        }

        const float dryL = ch[0][i];
        const float dryR = numCh > 1 ? ch[1][i] : dryL;

        float dl = allpass[1].process(allpass[0].process(dryL));
        float dr = allpass[3].process(allpass[2].process(dryR));

        std::array<float, numLines> raw, fb;
        for (size_t k = 0; k < numLines; ++k)
        {
            lfoPhase[k] += lfoInc[k];
            if (lfoPhase[k] >= twoPi)
                lfoPhase[k] -= twoPi;

            const float delay = static_cast<float>(active.len[k]) + active.modDepth * std::sin(lfoPhase[k]);
            const int di = static_cast<int>(delay);
            const float frac = delay - static_cast<float>(di);
            const float x0 = lines[k][static_cast<size_t>((writePos - di) & mask)];
            const float x1 = lines[k][static_cast<size_t>((writePos - di - 1) & mask)];
            raw[k] = x0 + frac * (x1 - x0);

            lp[k] = raw[k] + active.damp * (lp[k] - raw[k]);
            fb[k] = lp[k] * active.gain[k];
        }

        float wetL = 0.0f, wetR = 0.0f;
        for (size_t k = 0; k < numLines; ++k)
        {
            wetL += outL[k] * raw[k];
            wetR += outR[k] * raw[k];
        }
        wetL *= active.outGain * duckGain;
        wetR *= active.outGain * duckGain;

        hadamard8(fb.data());
        for (size_t k = 0; k < numLines; ++k)
            lines[k][static_cast<size_t>(writePos)] = fb[k] + 0.5f * (inL[k] * dl + inR[k] * dr);
        writePos = (writePos + 1) & mask;

        ch[0][i] = dryL * dryGain + wetL * wetGain;
        if (numCh > 1)
            ch[1][i] = dryR * dryGain + wetR * wetGain;
    }
}
} // namespace grain::dsp
