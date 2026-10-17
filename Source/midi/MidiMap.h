#pragma once

#include <array>
#include <atomic>
#include <optional>

namespace grain::midi
{
// MIDI CC -> parameter map with "learn". Threading: the table is written ONLY by the message thread (assign, clear,
// clearAll, pollLearn). The audio thread only reads it (onCc) and, while learning, hands the CC over through one atomic
// that the message thread turns into an assignment. Everything is atomic: nothing locks or allocates.
// A parameter is an index into ids::mappable; one CC drives at most one parameter and one parameter listens to at
// most one CC (assigning moves it).
class MidiMap
{
public:
    static constexpr int numCc = 128;

    struct Hit
    {
        int param;
        float normalised; // 0..1
    };

    MidiMap() { clearAll(); }

    // audio thread. While learning, the next CC is handed to the message thread (returns nothing); otherwise a mapped
    // CC returns its parameter.
    std::optional<Hit> onCc(int cc, int value)
    {
        if (cc < 0 || cc >= numCc)
            return std::nullopt;
        int learning = learnParam.load(std::memory_order_acquire);
        if (learning >= 0)
        {
            // only the thread that wins the exchange hands over (a cancel or restart in between wins instead)
            if (learnParam.compare_exchange_strong(learning, -1, std::memory_order_acq_rel))
                learned.store(((learning + 1) << 8) | cc, std::memory_order_release);
            return std::nullopt;
        }
        const int param = ccToParam[static_cast<size_t>(cc)].load(std::memory_order_relaxed);
        if (param < 0)
            return std::nullopt;
        const int v = value < 0 ? 0 : (value > 127 ? 127 : value);
        return Hit { param, static_cast<float>(v) / 127.0f };
    }

    void startLearn(int param) { learnParam.store(param, std::memory_order_release); }
    void cancelLearn() { learnParam.store(-1, std::memory_order_release); }

    // message thread: turns a CC the audio thread captured into an assignment. Returns true if one was assigned.
    bool pollLearn()
    {
        const int packed = learned.exchange(0, std::memory_order_acq_rel);
        if (packed == 0)
            return false;
        assign(packed & 0xff, (packed >> 8) - 1);
        return true;
    }
    int learning() const { return learnParam.load(std::memory_order_relaxed); } // -1 = not learning

    void assign(int cc, int param)
    {
        if (cc < 0 || cc >= numCc)
            return;
        for (auto& slot : ccToParam)
            if (slot.load(std::memory_order_relaxed) == param)
                slot.store(-1, std::memory_order_relaxed);
        ccToParam[static_cast<size_t>(cc)].store(param, std::memory_order_relaxed);
    }

    void clear(int param)
    {
        for (auto& slot : ccToParam)
            if (slot.load(std::memory_order_relaxed) == param)
                slot.store(-1, std::memory_order_relaxed);
    }

    void clearAll()
    {
        for (auto& slot : ccToParam)
            slot.store(-1, std::memory_order_relaxed);
        learnParam.store(-1, std::memory_order_relaxed);
        learned.store(0, std::memory_order_relaxed);
    }

    int ccFor(int param) const // -1 = none
    {
        for (int cc = 0; cc < numCc; ++cc)
            if (ccToParam[static_cast<size_t>(cc)].load(std::memory_order_relaxed) == param)
                return cc;
        return -1;
    }

private:
    std::array<std::atomic<int>, numCc> ccToParam;
    std::atomic<int> learnParam { -1 };
    std::atomic<int> learned { 0 }; // ((param + 1) << 8) | cc, 0 = nothing captured
};
} // namespace grain::midi
