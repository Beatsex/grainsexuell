#include "DualEngine.h"

#include <algorithm>

namespace grain::dsp
{
void DualEngine::prepare(double sampleRate, const Params& initial)
{
    a.prepare(sampleRate);
    b.prepare(sampleRate);
    fade.reset(sampleRate, 0.02);
    fade.setCurrentAndTargetValue(initial.bOn ? 1.0f : 0.0f); // no fade on load
    activeRoute = std::clamp(initial.route, 0, 1);
    source.reset(sampleRate, 0.02);
    source.setCurrentAndTargetValue(static_cast<float>(activeRoute));
    lastB = initial.b;
}

void DualEngine::process(float* const* ch, int numCh, int n, const Params& p)
{
    const int wantRoute = std::clamp(p.route, 0, 1);
    if (p.bOn)
        lastB = p.b;

    // a route change fades B out first; once it is silent the new route takes over and B fades back in
    if (fade.getCurrentValue() <= 0.0f && ! fade.isSmoothing())
        activeRoute = wantRoute;
    fade.setTargetValue(p.bOn && wantRoute == activeRoute ? 1.0f : 0.0f);
    source.setTargetValue(static_cast<float>(activeRoute)); // what B records: 0 = A's output, 1 = the input

    const bool bSilent = fade.getCurrentValue() <= 0.0f && ! fade.isSmoothing();
    EngineSet::Params bp = lastB;
    if (bSilent)
        bp.mix = 0.0f; // B only records (cheap), so its memory has no gap when it comes back

    const bool parallel = activeRoute == 1;
    for (int pos = 0, m = 0; pos < n; pos += m)
    {
        m = std::min(sideFrames, n - pos);
        float* main[2] { ch[0] + pos, ch[numCh > 1 ? 1 : 0] + pos };
        float* aside[2] { side[0].data(), side[1].data() };

        for (int c = 0; c < numCh; ++c)
            std::copy(main[c], main[c] + m, aside[c]); // the input
        a.process(main, numCh, m, p.a);

        // B's input: A's output (serial) or the input (parallel), crossfaded after a route change
        if (source.isSmoothing())
            for (int i = 0; i < m; ++i)
            {
                const float s = source.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    aside[c][i] = main[c][i] + s * (aside[c][i] - main[c][i]);
            }
        else if (source.getTargetValue() < 0.5f)
            for (int c = 0; c < numCh; ++c)
                std::copy(main[c], main[c] + m, aside[c]);
        b.process(aside, numCh, m, bp);

        if (bSilent)
            continue; // A's output untouched: exactly a single slot

        if (! fade.isSmoothing() && fade.getTargetValue() >= 1.0f) // settled: exact formulas
        {
            for (int c = 0; c < numCh; ++c)
                for (int i = 0; i < m; ++i)
                    main[c][i] = parallel ? 0.5f * (main[c][i] + aside[c][i]) : aside[c][i];
            continue;
        }
        const float k = parallel ? 0.5f : 1.0f; // out = A + fade * (target - A)
        for (int i = 0; i < m; ++i)
        {
            const float f = fade.getNextValue() * k;
            for (int c = 0; c < numCh; ++c)
                main[c][i] += f * (aside[c][i] - main[c][i]);
        }
    }
}
} // namespace grain::dsp
