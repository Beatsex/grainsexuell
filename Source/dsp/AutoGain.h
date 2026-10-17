#pragma once

#include <algorithm>
#include <cmath>

namespace grain::dsp
{
// Auto gain staging. begin() runs before the effect chain, end() after it:
//  - begin(): measures the input (stereo-linked RMS, ~300 ms), slews a gain toward "-18 dBFS RMS" (max +/-12 dB)
//    and applies it to the block, so the chain always sees a similar level (holds on silence and note ends);
//  - end(): applies the exact inverse of the same per-sample ramp, so the user's level is unchanged, then a
//    safety ceiling that never exceeds 1.0.
// Gain moves at a constant dB/s (no hunting), holds on silence, and ignores small errors (dead zone).
// Off and gain at 0 dB: both calls leave the audio bit-exact.
class AutoGain
{
public:
    static constexpr float targetDb = -18.0f;      // 0 VU convention: ~18 dB peak headroom
    static constexpr float maxDb = 12.0f;          // correction range either way
    static constexpr float deadZoneDb = 1.5f;      // no move for smaller errors
    static constexpr float holdBelowDb = -60.0f;   // below this input level the gain holds
    static constexpr float upDbPerSec = 3.0f;      // slow when raising
    static constexpr float downDbPerSec = 10.0f;   // faster when lowering
    static constexpr float integrationSec = 0.3f;  // VU-like level integration
    static constexpr float fastSec = 0.03f;        // fast detector: notices that the signal just stopped
    static constexpr float decayHoldDb = 6.0f;     // fast this far below slow = signal ended, hold the gain
    static constexpr float ceilingKnee = 0.891f;   // -1 dBFS: transparent below, smooth saturation above

    void prepare(double sr)
    {
        sampleRate = sr;
        coeff = 1.0f - std::exp(-1.0f / (integrationSec * static_cast<float>(sr)));
        fastCoeff = 1.0f - std::exp(-1.0f / (fastSec * static_cast<float>(sr)));
        power = fastPower = 0.0f;
        gainDb_ = 0.0f;
        g0 = g1 = 1.0f;
        ceilingOn = false;
    }

    float gainDb() const { return gainDb_; }
    float levelDb() const { return 10.0f * std::log10(power + 1e-12f); }
    float fastLevelDb() const { return 10.0f * std::log10(fastPower + 1e-12f); }

    // begin() and end() must get the same channels, numCh >= 1 and n (as processBlock does).
    void begin(float* const* ch, int numCh, int n, bool enabled)
    {
        if (numCh < 1)
            return;

        // detector runs always (warm when switched on) and never touches the audio
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                const float x = ch[c][i];
                if (std::isfinite(x))
                    m += x * x;
            }
            m /= static_cast<float>(numCh);
            power += coeff * (m - power);
            fastPower += fastCoeff * (m - fastPower);
        }

        // Gain computer, once per block. The slow detector still rings for ~1 s after a note ends and would
        // read as "input got quieter", so the fast one holds the gain while the signal is silent or just stopped.
        const float lvl = levelDb(), fast = fastLevelDb();
        const float desired = enabled ? std::clamp(targetDb - lvl, -maxDb, maxDb) : 0.0f;
        const bool holding = fast < holdBelowDb || fast < lvl - decayHoldDb;
        const bool move = enabled ? (! holding && std::abs(desired - gainDb_) > deadZoneDb) : gainDb_ != 0.0f;
        if (move)
        {
            const float seconds = static_cast<float>(n) / static_cast<float>(sampleRate);
            const float delta = desired - gainDb_;
            gainDb_ += delta > 0.0f ? std::min(delta, upDbPerSec * seconds) : std::max(delta, -downDbPerSec * seconds);
        }

        g0 = g1;
        g1 = std::pow(10.0f, gainDb_ / 20.0f);
        ceilingOn = enabled || gainDb_ != 0.0f;

        if (g0 == 1.0f && g1 == 1.0f)
            return;
        for (int i = 0; i < n; ++i)
        {
            const float g = ramp(i, n);
            for (int c = 0; c < numCh; ++c)
                ch[c][i] *= g;
        }
    }

    void end(float* const* ch, int numCh, int n)
    {
        if (numCh < 1)
            return;
        const bool scaled = g0 != 1.0f || g1 != 1.0f;
        if (! scaled && ! ceilingOn)
            return;

        for (int i = 0; i < n; ++i)
        {
            const float inv = scaled ? 1.0f / ramp(i, n) : 1.0f;
            for (int c = 0; c < numCh; ++c)
            {
                float x = ch[c][i] * inv;
                if (ceilingOn && ! (std::abs(x) <= ceilingKnee)) // also true for NaN
                    x = std::isnan(x) ? 0.0f : soft(x);
                ch[c][i] = x;
            }
        }
    }

private:
    float ramp(int i, int n) const { return g0 + (g1 - g0) * (static_cast<float>(i + 1) / static_cast<float>(n)); }

    // ponytail: zero-latency saturator, adds harmonics on overs and is not oversampled; upgrade path is an
    // oversampled look-ahead limiter (which must report its latency to the host).
    static float soft(float x)
    {
        const float t = ceilingKnee, a = std::abs(x);
        return std::copysign(t + (1.0f - t) * std::tanh((a - t) / (1.0f - t)), x);
    }

    double sampleRate = 48000.0;
    float coeff = 0.0f, fastCoeff = 0.0f, power = 0.0f, fastPower = 0.0f, gainDb_ = 0.0f, g0 = 1.0f, g1 = 1.0f;
    bool ceilingOn = false;
};
} // namespace grain::dsp
