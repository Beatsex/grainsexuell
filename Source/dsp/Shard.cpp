#include "Shard.h"

#include <algorithm>
#include <cmath>

namespace grain::dsp
{
namespace
{
constexpr float ringSeconds = 3.0f;
constexpr double reachSeconds = 2.4; // farthest a slice may start in the past (the ring is longer)
constexpr float maxFeedback = 0.85f;
constexpr double quarters[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0 }; // 1/4 .. 1/32T, in quarter notes
constexpr double arpRatios[] { 1.0, 1.189207, 1.498307, 2.0 };                           // 0, +3, +7, +12 semitones
constexpr float stutterDecay = 0.85f;

// Transparent below 0.9, bounded by 1.0: keeps the feedback path from ever running away.
float softLimit(float x)
{
    const float a = std::abs(x);
    if (a <= 0.9f)
        return x;
    return std::copysign(0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f), x);
}

float crush(float x, float levels)
{
    return std::round(std::clamp(x, -1.0f, 1.0f) * levels) / levels;
}
} // namespace

void Shard::prepare(double sr)
{
    sampleRate = sr;
    cap.prepare(sr, ringSeconds);
    for (auto& v : voices)
        v = Voice {};
    fade = std::max(16, static_cast<int>(std::lround(0.005 * sr)));
    fastCoef = 1.0f - std::exp(-1.0f / static_cast<float>(0.002 * sr));
    slowCoef = 1.0f - std::exp(-1.0f / static_cast<float>(0.05 * sr));
    refractoryFrames = static_cast<int>(0.06 * sr);
    fast = slow = 0.0f;
    refractory = 0;
    onsets.fill(0);
    numFound = onsetHead = 0;
    absFrame = 0;
    mixSm.reset(sr, 0.02);
    dryGain = 1.0f;
    wetGain = 0.0f;
    lastMix = -1.0f;
    gap = 0.0;
    stepCount = repeatsLeft = holdCounter = 0;
    heldL = heldR = 0.0f;
    stutterLevel = 1.0f;
    played.store(0, std::memory_order_relaxed);
    primed = wasActive = false;
    rng = 0x1B873593u; // fixed seed: renders and tests repeat exactly
}

float Shard::rand01()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float>(rng >> 8) * (1.0f / 16777216.0f);
}

void Shard::detectOnset(float dryL, float dryR)
{
    const float level = 0.5f * (std::abs(dryL) + std::abs(dryR));
    fast += (level - fast) * fastCoef;
    slow += (level - slow) * slowCoef;
    if (refractory > 0)
        --refractory;
    else if (absFrame >= refractoryFrames && fast > 2.5f * slow + 0.003f) // not in the first 60 ms: the followers start at 0, so any steady input looks like an onset; floor = -50 dBFS
    {
        onsets[static_cast<size_t>(onsetHead)] = std::max<std::int64_t>(0, absFrame - fade); // the follower lags by about a fade
        onsetHead = (onsetHead + 1) % numOnsets;
        numFound = std::min(numFound + 1, numOnsets);
        refractory = refractoryFrames;
    }
}

void Shard::startVoice(double pos, float step, bool lowpassed, float gain, const Params& p, int numCh, int sliceFrames)
{
    const float pan = numCh > 1 ? p.scatter * (rand01() * 2.0f - 1.0f) : 0.0f;
    for (auto& v : voices)
        if (! v.active)
        {
            v.active = true;
            v.lowpassed = lowpassed;
            v.pos = pos;
            v.step = step;
            v.gain = gain;
            v.panL = std::min(1.0f, 1.0f - pan);
            v.panR = std::min(1.0f, 1.0f + pan);
            v.age = 0;
            v.length = sliceFrames + fade;
            played.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    // all voices busy: this slice is skipped
}

void Shard::spawn(const Params& p, double now, double sliceFrames, int maxBack, int numCh)
{
    const int S = static_cast<int>(sliceFrames);
    switch (p.variant)
    {
        case 0: // Shuffle
        {
            const int reach = std::min(maxBack, 1 + static_cast<int>(std::lround(p.span * 7.0f)));
            const int m = std::min(reach, 1 + static_cast<int>(rand01() * static_cast<float>(reach)));
            const bool reversed = rand01() < p.scatter * 0.5f;
            if (reversed)
                startVoice(now - minAge - (m - 1) * sliceFrames, -1.0f, false, 1.0f, p, numCh, S);
            else
                startVoice(now - m * sliceFrames, 1.0f, false, 1.0f, p, numCh, S);
            break;
        }
        case 1: // Crush: always the newest slice
            startVoice(now - sliceFrames, 1.0f, false, 1.0f, p, numCh, S);
            break;
        case 2: // Arp
        {
            std::array<std::int64_t, numOnsets> valid {};
            int count = 0;
            for (int i = 0; i < numFound; ++i) // oldest first
            {
                const auto o = onsets[static_cast<size_t>((onsetHead - numFound + i + numOnsets) % numOnsets)];
                if (now - static_cast<double>(o) <= reachSeconds * sampleRate)
                    valid[static_cast<size_t>(count++)] = o;
            }
            if (count == 0)
                break;
            const double ratio = arpRatios[stepCount % 4];
            const double start = std::min(static_cast<double>(valid[static_cast<size_t>(stepCount % count)]),
                                          now - minAge - (S + fade) * (ratio - 1.0));
            startVoice(start, static_cast<float>(ratio), ratio > 1.4, 1.0f, p, numCh, S);
            break;
        }
        default: // Stutter
        {
            // re-freeze when the group is done, or when a longer slice (tempo or Swarm change) would push it out of reach
            if (repeatsLeft == 0 || now - frozenPos > reachSeconds * sampleRate - sliceFrames)
            {
                const int m = 1 + static_cast<int>(rand01() * static_cast<float>(std::min(2, maxBack)));
                frozenPos = now - std::min(m, 2) * sliceFrames;
                repeatsLeft = std::min(1 + static_cast<int>(std::lround(p.span * 7.0f)), std::max(1, maxBack - 2));
                stutterLevel = 1.0f;
            }
            startVoice(frozenPos, 1.0f, false, stutterLevel, p, numCh, S);
            stutterLevel *= stutterDecay;
            --repeatsLeft;
            break;
        }
    }
    stepCount = (stepCount + 1) % 840; // multiple of 4 (arp ratios) and of 1..4 onsets: wraps without a visible jump
    if (p.variant != 3)
        repeatsLeft = 0; // a Stutter group left half-way must not resume from an old frozen position
}

void Shard::process(float* const* ch, int numCh, int n, const Params& requested)
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

    double sliceSec = 60.0 / bpm * quarters[std::min(6, static_cast<int>(p.swarm * 7.0f))];
    while (sliceSec > 0.75) // fold into 30 ms .. 750 ms, staying on the beat grid
        sliceSec *= 0.5;
    while (sliceSec < 0.03)
        sliceSec *= 2.0;
    const double sliceFrames = std::max(1.0, std::floor(sliceSec * sampleRate)); // never 0: the spawn loop would not advance
    const int maxBack = std::max(1, std::min(8, static_cast<int>(reachSeconds * sampleRate / sliceFrames)));
    const float feedback = p.tail * maxFeedback;
    const float bitLevels = static_cast<float>(1 << (11 - static_cast<int>(std::lround(p.span * 9.0f)))); // 12 .. 3 bits
    const int hold = 1 + static_cast<int>(std::lround(p.span * 11.0f));
    const int mask = cap.mask;

    for (int pos = 0, m = 0; pos < n; pos += m)
    {
        m = std::min(chunk, n - pos);
        const bool wetActive = mixSm.isSmoothing() || mixSm.getTargetValue() > 0.0f;
        if (! wetActive)
        {
            if (wasActive)
                for (auto& v : voices)
                    v.active = false;
            wasActive = false;
            for (int f = 0; f < m; ++f, ++absFrame)
                detectOnset(ch[0][pos + f], ch[numCh > 1 ? 1 : 0][pos + f]);
            cap.write(ch[0] + pos, ch[numCh > 1 ? 1 : 0] + pos, m); // Mix 0: audio untouched, still recording
            continue;
        }
        if (! wasActive)
        {
            gap = 1.0; // the first slice starts on the first frame
            stepCount = repeatsLeft = holdCounter = 0;
            heldL = heldR = 0.0f;
        }
        wasActive = true;

        for (int f = 0; f < m; ++f, ++absFrame)
        {
            const float dryL = ch[0][pos + f];
            const float dryR = numCh > 1 ? ch[1][pos + f] : dryL;
            detectOnset(dryL, dryR);

            const double now = static_cast<double>(absFrame);
            gap -= 1.0;
            while (gap <= 0.0)
            {
                spawn(p, now, sliceFrames, maxBack, numCh);
                gap += sliceFrames;
            }

            float wetL = 0.0f, wetR = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active)
                    continue;
                // smoothstep fades: no corner where they end, and fade-in + fade-out of neighbours still sum to exactly 1
                const float u = std::min({ 1.0f, static_cast<float>(v.age) / static_cast<float>(fade),
                                           static_cast<float>(v.length - v.age) / static_cast<float>(fade) });
                const float w = u * u * (3.0f - 2.0f * u);
                const auto& src = cap.ring[v.lowpassed ? 1 : 0];
                const auto i0 = static_cast<std::int64_t>(std::floor(v.pos));
                const float frac = static_cast<float>(v.pos - static_cast<double>(i0));
                const auto a = static_cast<size_t>(i0 & mask), b = static_cast<size_t>((i0 + 1) & mask);
                const float gw = v.gain * w;
                wetL += gw * v.panL * (src[0][a] + frac * (src[0][b] - src[0][a]));
                wetR += gw * v.panR * (src[1][a] + frac * (src[1][b] - src[1][a]));

                v.pos += v.step;
                if (++v.age >= v.length)
                    v.active = false;
            }

            // feedback takes the signal before the crush: a quantiser inside a loop with gain 0.85 rounds a single step
            // back up to itself and never decays
            const float fbL = wetL, fbR = numCh > 1 ? wetR : wetL;

            if (p.variant == 1) // Crush: sample-and-hold, then quantise
            {
                if (holdCounter == 0)
                {
                    heldL = crush(wetL, bitLevels);
                    heldR = crush(wetR, bitLevels);
                }
                holdCounter = (holdCounter + 1) % hold;
                wetL = heldL;
                wetR = heldR;
            }

            rec[0][static_cast<size_t>(f)] = dryL + softLimit(feedback * fbL);
            rec[1][static_cast<size_t>(f)] = dryR + softLimit(feedback * fbR);

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
