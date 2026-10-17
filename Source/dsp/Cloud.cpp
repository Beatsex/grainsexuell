#include "Cloud.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace grain::dsp
{
namespace
{
constexpr float ringSeconds = 3.0f;
constexpr float maxFeedback = 0.85f;

// Transparent below 0.9, bounded by 1.0: keeps the feedback path from ever running away.
float softLimit(float x)
{
    const float a = std::abs(x);
    if (a <= 0.9f)
        return x;
    return std::copysign(0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f), x);
}

struct Choice
{
    float speed, dir, weight;
};
// [variant]: up to two (speed, direction, probability) choices per grain
constexpr std::array<std::array<Choice, 2>, 4> variants { {
    { { { 1.0f, 1, 1.0f }, { 1.0f, 1, 0.0f } } },     // A Mist
    { { { 1.0f, 1, 0.5f }, { 2.0f, 1, 0.5f } } },     // B Shimmer
    { { { 0.5f, 1, 0.5f }, { 1.0f, 1, 0.5f } } },     // C Murk
    { { { 1.0f, -1, 0.7f }, { 0.5f, -1, 0.3f } } },   // D Backwash
} };
} // namespace

void Cloud::prepare(double sr)
{
    sampleRate = sr;
    cap.prepare(sr, ringSeconds);
    for (auto& g : grains)
        g = Grain {};
    for (int i = 0; i <= hannSize; ++i)
        hann[static_cast<size_t>(i)] = 0.5f - 0.5f * static_cast<float>(std::cos(2.0 * std::numbers::pi * i / hannSize));
    mixSm.reset(sr, 0.02);
    dryGain = 1.0f;
    wetGain = 0.0f;
    lastMix = -1.0f;
    gap = 0.0;
    spawned.store(0, std::memory_order_relaxed);
    primed = wasActive = false;
    rng = 0x2545F491u; // fixed seed: renders and tests repeat exactly
}

float Cloud::rand01()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float>(rng >> 8) * (1.0f / 16777216.0f);
}

void Cloud::spawn(const Params& p, int framesIntoChunk, float density, int lengthFrames, int numCh)
{
    Grain* g = nullptr;
    for (auto& cand : grains)
        if (! cand.active)
        {
            g = &cand;
            break;
        }
    if (g == nullptr)
        return; // all voices busy: this grain is skipped

    const auto& v = variants[static_cast<size_t>(p.variant)];
    const Choice& c = rand01() < v[0].weight ? v[0] : v[1];
    const float speed = c.speed * std::exp2((rand01() * 2.0f - 1.0f) * p.scatter * 0.025f); // +/-30 cents at Scatter 1
    // a base spray of up to 20 ms keeps overlapping grains different; with none they would all read the very
    // same samples and add up coherently (about 0.82*sqrt(N) too loud at high density)
    const double offset = static_cast<double>(rand01() * (0.02f + p.scatter * 1.2f)) * sampleRate;
    const double L = lengthFrames;

    // age of the grain's newest sample is never below minAge (see the class comment)
    const double now = static_cast<double>(cap.writePos) + framesIntoChunk;
    double pos;
    if (c.dir > 0)
        pos = now - (minAge + offset + L * speed);
    else
        pos = now - (minAge + offset);
    pos = std::fmod(pos + 8.0 * cap.size, static_cast<double>(cap.size));

    const float pan = p.scatter * (rand01() * 2.0f - 1.0f);
    g->active = true;
    g->lowpassed = speed > 1.4f;
    g->pos = pos;
    g->step = c.dir * speed;
    g->age = 0;
    g->length = lengthFrames;
    g->gain = 1.0f / std::sqrt(0.375f * std::max(1.0f, density * static_cast<float>(lengthFrames) / static_cast<float>(sampleRate)));
    g->panL = numCh > 1 ? std::min(1.0f, 1.0f - pan) : 1.0f;
    g->panR = std::min(1.0f, 1.0f + pan);
    spawned.fetch_add(1, std::memory_order_relaxed);
}

void Cloud::process(float* const* ch, int numCh, int n, const Params& requested)
{
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
        primed = true;
    }
    else
        mixSm.setTargetValue(mixTarget);

    const float density = 3.0f * std::pow(80.0f / 3.0f, p.swarm);
    const int lengthFrames = std::max(64, static_cast<int>(std::lround(0.01 * std::pow(40.0, p.span) * sampleRate)));
    // Grains are normalised as if they added up incoherently (1/sqrt(N)); a steady tone adds coherently, up to
    // 0.82*sqrt(N) times louder. Scaling the feedback by that worst case keeps the loop gain below Tail*0.85.
    const float overlap = density * static_cast<float>(lengthFrames) / static_cast<float>(sampleRate);
    const float feedback = p.tail * maxFeedback / std::max(1.0f, 0.82f * std::sqrt(overlap));
    const double meanGap = sampleRate / density;
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
            gap = 0.0;
        wasActive = true;

        for (int f = 0; f < m; ++f)
        {
            gap -= 1.0;
            while (gap <= 0.0)
            {
                spawn(p, f, density, lengthFrames, numCh);
                gap += std::max(1.0, meanGap * (1.0 + static_cast<double>(p.scatter * 0.8f * (rand01() * 2.0f - 1.0f))));
            }

            float wetL = 0.0f, wetR = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active)
                    continue;
                const float x = static_cast<float>(g.age) * (static_cast<float>(hannSize) / static_cast<float>(g.length));
                const int wi = static_cast<int>(x);
                const float w = hann[static_cast<size_t>(wi)] + (x - static_cast<float>(wi)) * (hann[static_cast<size_t>(wi) + 1] - hann[static_cast<size_t>(wi)]);

                const auto& src = cap.ring[g.lowpassed ? 1 : 0];
                const auto i0 = static_cast<int>(g.pos);
                const float frac = static_cast<float>(g.pos - i0);
                const auto a = static_cast<size_t>(i0 & mask), b = static_cast<size_t>((i0 + 1) & mask);
                const float gw = g.gain * w;
                wetL += gw * g.panL * (src[0][a] + frac * (src[0][b] - src[0][a]));
                wetR += gw * g.panR * (src[1][a] + frac * (src[1][b] - src[1][a]));

                g.pos += g.step;
                if (g.pos >= cap.size)
                    g.pos -= cap.size;
                else if (g.pos < 0.0)
                    g.pos += cap.size;
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

        // record after rendering: the grains above only read audio older than this chunk
        cap.write(rec[0].data(), rec[1].data(), m);
    }
}
} // namespace grain::dsp
