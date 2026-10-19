#include "Tap.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace grain::dsp
{
namespace
{
constexpr float ringSeconds = 3.0f;
constexpr double maxDelaySeconds = 2.0;
constexpr float maxFeedback = 0.85f;
constexpr float tapDecay = 0.6f; // tap k is quieter by tapDecay^k
constexpr double maxSnippetSeconds = 0.4;

constexpr double quarters[] { 4.0, 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25 }; // 1/1 .. 1/16, in quarter notes
constexpr double spacing[2][4] { { 1.0, 2.0, 3.0, 4.0 }, { 1.0, 1.75, 2.25, 2.5833 } }; // even, cascade (shrinking)
constexpr int cascadeRing[4] { 0, 1, 2, 2 }; // Cascade: later taps read the more strongly low-passed copies
constexpr float jitterPattern[4] { -0.7f, 0.4f, 0.9f, -0.3f };
constexpr float spreadPattern[4] { -0.8f, 0.8f, -0.5f, 0.5f };

// Transparent below 0.9, bounded by 1.0: keeps the feedback path from ever running away.
float softLimit(float x)
{
    const float a = std::abs(x);
    if (a <= 0.9f)
        return x;
    return std::copysign(0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f), x);
}
} // namespace

void Tap::prepare(double sr)
{
    sampleRate = sr;
    cap.prepare(sr, ringSeconds);
    echoPhase = 0.0;
    echoes.store(0, std::memory_order_relaxed);
    for (auto& g : grains)
        g = Grain {};
    for (int i = 0; i <= hannSize; ++i)
        hann[static_cast<size_t>(i)] = 0.5f - 0.5f * static_cast<float>(std::cos(2.0 * std::numbers::pi * i / hannSize));
    for (size_t k = 0; k < numTaps; ++k)
    {
        jitter[k] = jitterPattern[k];
        spread[k] = spreadPattern[k];
        delaySm[k].reset(sr, 0.05);
        gainSm[k].reset(sr, 0.05);
        delaySm[k].setCurrentAndTargetValue(static_cast<float>(minAge));
        gainSm[k].setCurrentAndTargetValue(0.0f);
    }
    mixSm.reset(sr, 0.02);
    dryGain = 1.0f;
    wetGain = 0.0f;
    lastMix = -1.0f;
    gap = 0.0;
    primed = wasActive = false;
}

void Tap::spawnReverse(double startPos, int lengthFrames, float gain, float panL, float panR)
{
    for (auto& g : grains)
        if (! g.active)
        {
            g.active = true;
            g.pos = startPos;
            g.age = 0;
            g.length = lengthFrames;
            g.gain = gain;
            g.panL = panL;
            g.panR = panR;
            return;
        }
    // all voices busy: this snippet is skipped
}

void Tap::process(float* const* ch, int numCh, int n, const Params& requested)
{
    if (cap.size == 0)
        return; // process before prepare: leave the audio alone

    const auto unit = [](float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; };
    Params p = requested;
    p.swarm = unit(requested.swarm);
    p.tail = unit(requested.tail);
    p.span = unit(requested.span);
    p.scatter = unit(requested.scatter);
    p.mix = unit(requested.mix);
    p.variant = std::clamp(requested.variant, 0, 3);
    const double bpm = std::isfinite(requested.bpm) && requested.bpm > 0.0 ? std::clamp(requested.bpm, 20.0, 400.0) : 120.0;

    const float mixTarget = p.enabled ? p.mix : 0.0f;
    if (! primed)
    {
        mixSm.setCurrentAndTargetValue(mixTarget);
        primed = true;
    }
    else
        mixSm.setTargetValue(mixTarget);

    const bool reverse = p.variant == 2, ping = p.variant == 1, cascade = p.variant == 3;
    const int step = std::min(6, static_cast<int>(p.swarm * 7.0f));
    const double baseSec = std::clamp(60.0 / bpm * quarters[step], 0.02, maxDelaySeconds);
    const double stepSec = baseSec * (1 + static_cast<int>(std::lround(p.span * 3.0f)));
    const double minDelay = minAge, maxDelay = maxDelaySeconds * sampleRate;

    float sumGain = 0.0f;
    for (size_t k = 0; k < numTaps; ++k)
    {
        const double sec = stepSec * spacing[cascade ? 1 : 0][k] * (1.0 + 0.15 * p.scatter * jitter[k]);
        const float g = sec <= maxDelaySeconds ? std::pow(tapDecay, static_cast<float>(k)) : 0.0f; // later taps drop out
        delaySm[k].setTargetValue(static_cast<float>(std::clamp(sec * sampleRate, minDelay, maxDelay)));
        gainSm[k].setTargetValue(g);
        sumGain += g;
    }
    const float feedback = p.tail * maxFeedback / std::max(1.0f, sumGain);
    const int snippet = std::max(64, static_cast<int>(std::min(stepSec, maxSnippetSeconds) * sampleRate));
    const int mask = cap.mask;

    for (int pos = 0, m = 0; pos < n; pos += m)
    {
        m = std::min(chunk, n - pos);
        const bool wetActive = mixSm.isSmoothing() || mixSm.getTargetValue() > 0.0f;
        if (! wetActive)
        {
            if (wasActive)
                for (auto& g : grains)
                    g.active = false;
            wasActive = false;
            cap.write(ch[0] + pos, ch[numCh > 1 ? 1 : 0] + pos, m); // Mix 0: audio untouched, still recording
            continue;
        }
        if (! wasActive)
        {
            gap = 1.0; // the first snippet starts on the first frame, then one every half snippet
            for (size_t k = 0; k < numTaps; ++k) // no glide from a stale position
            {
                delaySm[k].setCurrentAndTargetValue(delaySm[k].getTargetValue());
                gainSm[k].setCurrentAndTargetValue(gainSm[k].getTargetValue());
            }
        }
        wasActive = true;

        // ponytail: activity = one repeat per audible tap per tap step; exact per-tap onsets if the GUI ever needs them
        echoPhase += m;
        const double stepFrames = stepSec * sampleRate;
        while (echoPhase >= stepFrames)
        {
            echoPhase -= stepFrames;
            int audible = 0;
            for (size_t k = 0; k < numTaps; ++k)
                audible += gainSm[k].getTargetValue() > 0.0f ? 1 : 0;
            echoes.fetch_add(audible, std::memory_order_relaxed);
        }

        for (int f = 0; f < m; ++f)
        {
            const double now = static_cast<double>(cap.writePos) + f;
            float wetL = 0.0f, wetR = 0.0f;

            for (size_t k = 0; k < numTaps; ++k)
            {
                const double d = delaySm[k].getNextValue();
                const float g = gainSm[k].getNextValue();

                float panL = 1.0f, panR = 1.0f;
                if (numCh > 1)
                {
                    if (ping)
                        panL = (k % 2 == 0) ? 1.0f : 0.0f, panR = 1.0f - panL;
                    else
                    {
                        const float pan = p.scatter * spread[k];
                        panL = std::min(1.0f, 1.0f - pan);
                        panR = std::min(1.0f, 1.0f + pan);
                    }
                }

                if (reverse)
                    continue;
                const auto& src = cap.ring[static_cast<size_t>(cascade ? cascadeRing[k] : 0)];
                const double rp = now - d;
                const auto i0 = static_cast<std::int64_t>(std::floor(rp));
                const float frac = static_cast<float>(rp - static_cast<double>(i0));
                const auto a = static_cast<size_t>(i0 & mask), b = static_cast<size_t>((i0 + 1) & mask);
                const float l = src[0][a] + frac * (src[0][b] - src[0][a]);
                const float r = src[1][a] + frac * (src[1][b] - src[1][a]);
                const float x = ping ? 0.5f * (l + r) : l;
                wetL += g * panL * x;
                wetR += g * panR * (ping ? x : r);
            }

            if (reverse)
            {
                gap -= 1.0;
                while (gap <= 0.0)
                {
                    for (size_t k = 0; k < numTaps; ++k)
                    {
                        const float pan = numCh > 1 ? p.scatter * spread[k] : 0.0f;
                        spawnReverse(now - delaySm[k].getCurrentValue(), snippet, gainSm[k].getCurrentValue(),
                                     std::min(1.0f, 1.0f - pan), std::min(1.0f, 1.0f + pan));
                    }
                    gap += 0.5 * snippet;
                }
            }

            for (auto& g : grains)
            {
                if (! g.active)
                    continue;
                const float x = static_cast<float>(g.age) * (static_cast<float>(hannSize) / static_cast<float>(g.length));
                const int wi = static_cast<int>(x);
                const float w = hann[static_cast<size_t>(wi)] + (x - static_cast<float>(wi)) * (hann[static_cast<size_t>(wi) + 1] - hann[static_cast<size_t>(wi)]);

                const auto& src = cap.ring[0];
                const auto i0 = static_cast<std::int64_t>(std::floor(g.pos));
                const float frac = static_cast<float>(g.pos - static_cast<double>(i0));
                const auto a = static_cast<size_t>(i0 & mask), b = static_cast<size_t>((i0 + 1) & mask);
                const float gw = g.gain * w;
                wetL += gw * g.panL * (src[0][a] + frac * (src[0][b] - src[0][a]));
                wetR += gw * g.panR * (src[1][a] + frac * (src[1][b] - src[1][a]));

                g.pos -= 1.0;
                if (++g.age >= g.length)
                    g.active = false;
            }

            const float dryL = ch[0][pos + f];
            const float dryR = numCh > 1 ? ch[1][pos + f] : dryL;
            rec[0][static_cast<size_t>(f)] = dryL + softLimit(feedback * wetL);
            rec[1][static_cast<size_t>(f)] = dryR + softLimit(feedback * (numCh > 1 ? wetR : wetL));

            const float mix = mixSm.getNextValue();
            if (mix != lastMix)
            {
                dryGain = std::cos(mix * 1.57079633f);
                wetGain = std::sin(mix * 1.57079633f);
                lastMix = mix;
            }
            ch[0][pos + f] = dryL * dryGain + wetL * wetGain;
            if (numCh > 1)
                ch[1][pos + f] = dryR * dryGain + wetR * wetGain;
        }

        // record after rendering: everything above only read audio older than this chunk
        cap.write(rec[0].data(), rec[1].data(), m);
    }
}
} // namespace grain::dsp
