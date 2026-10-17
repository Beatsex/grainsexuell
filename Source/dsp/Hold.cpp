#include "Hold.h"

#include <algorithm>
#include <cmath>

namespace grain::dsp
{
void Hold::prepare(double sr)
{
    sampleRate = sr;
    size = 1;
    while (size < static_cast<int>(2.5 * sr) + 16)
        size <<= 1;
    mask = size - 1;
    for (auto& r : ring)
        r.assign(static_cast<size_t>(size), 0.0f);
    writePos = loopPos = start = 0;
    period = seam = 1;
    held = false;
    gain = 0.0f;
    fadeIn = static_cast<float>(1.0 / (0.02 * sr));
    fadeOut = static_cast<float>(1.0 / (0.05 * sr));
}

void Hold::process(float* const* ch, int numCh, int n, bool hold, double bpm)
{
    if (size == 0)
        return; // process before prepare: leave the audio alone

    if (hold && ! held && gain <= 0.0f) // re-engaging during the fade-out keeps the running loop: no jump to a new window
    {
        double sec = 60.0 / (std::isfinite(bpm) && bpm > 0.0 ? std::clamp(bpm, 20.0, 400.0) : 120.0);
        while (sec > 2.0) // fold into 150 ms .. 2 s, staying on the beat grid
            sec *= 0.5;
        while (sec < 0.15)
            sec *= 2.0;
        // the loop is one beat plus the 10 ms seam, so the repeat period is exactly one beat (tempo-locked)
        seam = static_cast<int>(0.01 * sampleRate);
        const int length = std::max(8, static_cast<int>(sec * sampleRate)) + seam;
        period = length - seam;
        start = (writePos - length) & mask;
        loopPos = 0;
    }
    held = hold;
    const float target = held ? 1.0f : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float l = ch[0][i], r = numCh > 1 ? ch[1][i] : l;
        if (! held) // frozen while held; resumes when released
        {
            ring[0][static_cast<size_t>(writePos)] = l;
            ring[1][static_cast<size_t>(writePos)] = r;
            writePos = (writePos + 1) & mask;
        }

        gain = gain < target ? std::min(target, gain + fadeIn) : std::max(target, gain - fadeOut);
        if (gain <= 0.0f)
            continue;

        const auto a = static_cast<size_t>((start + loopPos) & mask);
        float lv = ring[0][a], rv = ring[1][a];
        if (loopPos < seam) // equal-power join with the audio one period further on
        {
            const auto b = static_cast<size_t>((start + loopPos + period) & mask);
            const float t = 1.5707963f * static_cast<float>(loopPos) / static_cast<float>(seam);
            const float up = std::sin(t), down = std::cos(t);
            lv = up * lv + down * ring[0][b];
            rv = up * rv + down * ring[1][b];
        }
        ch[0][i] = l + gain * lv;
        if (numCh > 1)
            ch[1][i] = r + gain * rv;
        if (++loopPos >= period)
            loopPos = 0;
    }
}
} // namespace grain::dsp
