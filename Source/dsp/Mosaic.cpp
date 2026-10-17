#include "Mosaic.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace grain::dsp
{
namespace
{
constexpr float ringSeconds = 2.0f;
constexpr float maxSpanSeconds = 0.75f;
constexpr float maxEdgeSeconds = 0.02f; // loop-seam crossfade (pre-roll / post-roll length)
constexpr float slotFadeSeconds = 0.03f;

float smooth(float x, float a, float b)
{
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// copies `total` frames starting at ring index `start` (wrapping) into dst
void copyFromRing(const std::vector<float>& ring, int start, int total, float* dst)
{
    const int size = static_cast<int>(ring.size());
    const int first = std::min(total, size - start);
    std::memcpy(dst, ring.data() + start, static_cast<size_t>(first) * sizeof(float));
    if (first < total)
        std::memcpy(dst + first, ring.data(), static_cast<size_t>(total - first) * sizeof(float));
}
} // namespace

void Mosaic::prepare(double sr)
{
    sampleRate = sr;
    cap.prepare(sr, ringSeconds);

    const int slotFrames = static_cast<int>((maxSpanSeconds + 2.0f * maxEdgeSeconds) * sr) + 16;
    for (auto& s : slots)
    {
        for (auto& src : s.src)
            for (auto& c : src)
                c.assign(static_cast<size_t>(slotFrames), 0.0f);
        s.length = s.pre = 0;
        s.fade = s.fadeTarget = 0.0f;
        s.env = 1.0f;
    }
    cur = 0;

    mixSm.reset(sr, 0.02);
    swarmSm.reset(sr, 0.05);
    dryGain = 1.0f;
    wetGain = 0.0f;
    lastMix = -1.0f;
    countdown = static_cast<int>(0.25 * sr);
    grabs.store(0, std::memory_order_relaxed);
    primed = wasActive = forceGrab = false;
    startup = true;
    rng = 0x9E3779B9u; // fixed seed: renders and tests repeat exactly
}

float Mosaic::rand01()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float>(rng >> 8) * (1.0f / 16777216.0f);
}

// Sample at fractional loop position i0 + frac. Forward playback blends the last X frames of the loop with the
// pre-roll, backward playback blends the first X frames with the post-roll, so the seam is continuous.
float Mosaic::read(const float* b, int X, int L, int i0, float frac, bool reverse)
{
    const auto at = [&](int i)
    {
        if (! reverse)
        {
            if (i >= L - X)
            {
                const float t = static_cast<float>(i - (L - X)) / static_cast<float>(X);
                return b[X + i] * (1.0f - t) + b[X + i - L] * t;
            }
        }
        else if (i < X)
        {
            const float t = std::min(1.0f, static_cast<float>(X - i) / static_cast<float>(X));
            return b[X + i] * (1.0f - t) + b[X + i + L] * t;
        }
        return b[X + i];
    };
    const float a = at(i0);
    return a + frac * (at(i0 + 1) - a);
}

void Mosaic::grab(const Params& p)
{
    Slot& s = slots[static_cast<size_t>(1 - cur)];
    const int L = std::max(64, static_cast<int>(std::lround(0.025 * std::pow(30.0, p.span) * sampleRate)));
    const int X = std::max(1, std::min(static_cast<int>(maxEdgeSeconds * sampleRate), L / 4));
    const int total = L + 2 * X;
    const int jitter = p.scatter > 0.0f ? static_cast<int>(rand01() * p.scatter * 0.25f * static_cast<float>(sampleRate)) : 0;
    const int start = ((cap.writePos - jitter - total) % cap.size + cap.size) % cap.size;

    // ponytail: copies all 3 sources x 2 channels at once (about 0.1 ms at 48 kHz, an estimated 0.5-1.5 ms at
    // 192 kHz with Span 1: too long for 64-128 sample buffers there). Upgrade: spread the copy over several
    // chunks (the 2 s ring is not overwritten for ~0.9 s) and swap `cur` when it is done.
    for (size_t a = 0; a < 3; ++a)
        for (size_t c = 0; c < 2; ++c)
            copyFromRing(cap.ring[a][c], start, total, s.src[a][c].data());

    s.length = L;
    s.pre = X;
    s.cfg = tables[static_cast<size_t>(std::clamp(p.variant, 0, 3))];
    for (size_t i = 0; i < numLayers; ++i)
    {
        s.phase[i] = s.cfg[i].dir > 0 ? 0.0f : static_cast<float>(L) - 0.5f;
        s.pan[i] = p.scatter * (rand01() * 2.0f - 1.0f);
    }
    s.env = 1.0f;
    s.fade = 0.0f;
    s.fadeTarget = 1.0f;
    slots[static_cast<size_t>(cur)].fadeTarget = 0.0f;
    cur = 1 - cur;
    grabs.fetch_add(1, std::memory_order_relaxed);
}

void Mosaic::process(float* const* ch, int numCh, int n, const Params& requested)
{
    if (cap.size == 0)
        return; // process before prepare: leave the audio alone

    // Slot and ring sizes assume 0..1 (Span 1 is exactly the slot size), so never trust the caller.
    const auto unit = [](float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; };
    Params p = requested;
    p.swarm = unit(requested.swarm);
    p.tail = unit(requested.tail);
    p.span = unit(requested.span);
    p.scatter = unit(requested.scatter);
    p.mix = unit(requested.mix);
    p.variant = std::clamp(requested.variant, 0, 3);

    const float mixTarget = p.enabled ? p.mix : 0.0f;
    if (! primed)
    {
        mixSm.setCurrentAndTargetValue(mixTarget);
        swarmSm.setCurrentAndTargetValue(p.swarm);
        lastVariant = p.variant;
        primed = true;
    }
    else
    {
        mixSm.setTargetValue(mixTarget);
        swarmSm.setTargetValue(p.swarm);
    }
    if (p.variant != lastVariant)
    {
        forceGrab = true; // new variant takes effect through an immediate new grab (crossfaded)
        lastVariant = p.variant;
    }

    const float fadeStep = 1.0f / (slotFadeSeconds * static_cast<float>(sampleRate));
    const float tailRate = (1.0f - smooth(p.tail, 0.9f, 1.0f)) / (0.15f * std::pow(80.0f, p.tail)); // 1/s
    const float decayMul = std::exp(-tailRate / static_cast<float>(sampleRate));

    for (int pos = 0, m = 0; pos < n; pos += m)
    {
        const bool wetActive = mixSm.isSmoothing() || mixSm.getTargetValue() > 0.0f;
        if (wetActive && ! wasActive)
            forceGrab = true;
        wasActive = wetActive;

        // A chunk never crosses the next grab time, so grabs land on the exact frame at any block size.
        m = std::min(chunk, n - pos);
        if (wetActive && forceGrab)
            m = 1; // a forced grab (start, variant change) lands on the first frame, not at a chunk end
        else if (wetActive && countdown > 0)
            m = std::min(m, countdown);

        cap.write(ch[0] + pos, ch[numCh > 1 ? 1 : 0] + pos, m); // raw plus the two low-passed copies

        if (! wetActive)
            continue; // Mix 0: audio untouched (bit-exact); the ring keeps recording

        for (int f = 0; f < m; ++f)
        {
            const float swarm = swarmSm.getNextValue();
            const std::array<float, numLayers> act { 1.0f, 1.0f, smooth(swarm, 0.2f, 0.6f), smooth(swarm, 0.4f, 0.9f) };

            float wetL = 0.0f, wetR = 0.0f;
            for (auto& s : slots)
            {
                if (s.fade <= 0.0f && s.fadeTarget <= 0.0f)
                    continue;
                s.fade += std::clamp(s.fadeTarget - s.fade, -fadeStep, fadeStep);
                s.env *= decayMul;
                if (s.env < 1.0e-6f)
                    s.env = 0.0f;

                for (size_t i = 0; i < numLayers; ++i)
                {
                    const float g = s.cfg[i].gain * act[i] * s.env * s.fade;
                    if (g > 0.0f)
                    {
                        const int i0 = static_cast<int>(s.phase[i]);
                        const float frac = s.phase[i] - static_cast<float>(i0);
                        const bool rev = s.cfg[i].dir < 0;
                        const auto& src = s.src[static_cast<size_t>(sourceOf[i])];
                        const float panL = numCh > 1 ? std::min(1.0f, 1.0f - s.pan[i]) : 1.0f;
                        const float panR = std::min(1.0f, 1.0f + s.pan[i]);
                        wetL += g * panL * read(src[0].data(), s.pre, s.length, i0, frac, rev);
                        wetR += g * panR * read(src[1].data(), s.pre, s.length, i0, frac, rev);
                    }
                    float& ph = s.phase[i];
                    ph += s.cfg[i].dir * speeds[i];
                    const auto L = static_cast<float>(s.length);
                    if (ph >= L)
                        ph -= L;
                    else if (ph < 0.0f)
                        ph += L;
                }
            }

            const float mix = mixSm.getNextValue();
            if (mix != lastMix)
            {
                dryGain = std::cos(mix * 1.57079633f);
                wetGain = std::sin(mix * 1.57079633f);
                lastMix = mix;
            }
            float* l = ch[0] + pos + f;
            *l = *l * dryGain + wetL * wetGain;
            if (numCh > 1)
            {
                float* r = ch[1] + pos + f;
                *r = *r * dryGain + wetR * wetGain;
            }
        }

        // Grab after the chunk is rendered: the new loop starts on the next frame, and the chunk ended exactly
        // when the countdown ran out, so timing does not depend on the host block size.
        if (forceGrab || (countdown -= m) <= 0)
        {
            if (slots[static_cast<size_t>(1 - cur)].fade <= 0.0f)
            {
                grab(p);
                forceGrab = false;
                const float jitter = 1.0f + p.scatter * 0.4f * (rand01() * 2.0f - 1.0f);
                // A loop must get at least two passes, or at high Swarm + long Span it never loops (at 1x it
                // would just be a delayed copy of the input, the slower layers restart before completing).
                const double interval = std::max(3.0 * std::pow(0.04, p.swarm) * sampleRate, 2.0 * slots[static_cast<size_t>(cur)].length);
                countdown = static_cast<int>(interval * jitter);
                if (startup) // the very first loop is made of an empty ring: replace it as soon as there is audio
                {
                    countdown = static_cast<int>(0.08 * sampleRate);
                    startup = false;
                }
            }
            else
                forceGrab = true; // other slot still fading out: retry next chunk
        }
    }
}
} // namespace grain::dsp
