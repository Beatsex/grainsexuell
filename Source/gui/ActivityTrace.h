#pragma once

#include <algorithm>
#include <array>

namespace grain::gui
{
// History of engine events (grabs, grains, slices, tap repeats) for the activity display. The editor pushes the
// processor's running total 15 times a second; this keeps the per-tick differences for the last ~4 s.
// Plain C++, no GUI: tested offline.
class ActivityTrace
{
public:
    static constexpr int numBars = 63;
    static constexpr int ticksPerSecond = 15;

    void push(unsigned total)
    {
        // the counters restart at 0 when the host re-prepares: a smaller total is a restart, not a huge jump
        const unsigned delta = primed && total >= last ? std::min(total - last, 100000u) : 0u;
        last = total;
        primed = true;
        history[static_cast<size_t>(head)] = delta;
        head = (head + 1) % numBars;
    }

    // events in the last second
    unsigned perSecond() const
    {
        unsigned sum = 0;
        for (int i = 1; i <= ticksPerSecond; ++i)
            sum += history[static_cast<size_t>((head - i + numBars) % numBars)];
        return sum;
    }

    // bar i (0 = oldest), 0..1 relative to the busiest tick shown
    float bar(int i) const
    {
        const unsigned peak = *std::max_element(history.begin(), history.end());
        if (peak == 0 || i < 0 || i >= numBars)
            return 0.0f;
        return static_cast<float>(history[static_cast<size_t>((head + i) % numBars)]) / static_cast<float>(peak);
    }

private:
    std::array<unsigned, numBars> history {};
    int head = 0;
    unsigned last = 0;
    bool primed = false;
};
} // namespace grain::gui
