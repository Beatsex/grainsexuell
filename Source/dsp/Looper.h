#pragma once

#include <cstdint>
#include <vector>

namespace grain::dsp
{
// Looper: phrase looper with overdub, undo, reverse, speed, quantize and burst.
//
//  - Record writes the input into a preallocated stereo buffer (up to maxSeconds) from frame 0. Leaving Record fixes
//    the length; with Quantize the length is rounded to whole beats (at least one), recording on until the next beat
//    boundary when rounding up. A full buffer switches to Play by itself.
//  - Seam: the last 10 ms of the recording are blended into the start with an equal-power crossfade, so the loop's
//    period is length - 10 ms and it joins itself without a click.
//  - Overdub adds the input into a second layer at the playhead. A dirty flag per 4096-frame chunk makes Undo
//    instant: it clears the flags (all overdubs since the base recording); with no overdubs it clears the loop.
//    The layer has 10 ms smoothstep fades at the period edges.
//  - Speed 0.5 / 1 / 2 (linear interpolation, pitch follows), Reverse plays backwards.
//  - Burst: while on, a second reader restarts from the playhead position at the moment of engaging every 1/16 note
//    (3 ms fades); the normal playhead keeps running underneath, so releasing rejoins the loop in time.
//  - The loop is added to the audio; with nothing recorded, or Off, the audio is bit-exact.
class Looper
{
public:
    enum Mode { Off, Record, Play, Overdub };

    struct Params
    {
        int mode = Off;
        int speed = 1; // 0 half, 1 normal, 2 double
        bool reverse = false, quantize = false, burst = false, undo = false;
        double bpm = 120.0;
    };

    void prepare(double sampleRate, double maxSeconds = 60.0);
    void process(float* const* ch, int numCh, int n, const Params& p);

    int loopLength() const { return length; }
    int loopPeriod() const { return period; }
    bool isRecording() const { return recording; }
    bool isPlaying() const { return playing; }
    bool hasOverdub() const { return layerUsed; }

private:
    static constexpr int chunkShift = 12;

    void startRecord();
    void stopRecord(bool quantize, double beat);
    void finalize(int len, bool play);
    void clearLayer();
    void resetBurst();
    void addToLayer(int idx, float l, float r);
    void frame(int i, float& l, float& r) const;
    void read(double p, float& l, float& r) const;
    void advance(double& p, double step) const;

    double sampleRate = 48000.0;
    int cap = 0;
    std::vector<float> base[2], layer[2];
    std::vector<std::uint8_t> dirty;
    bool layerUsed = false;

    int length = 0, period = 1, seam = 1;
    bool recording = false, playing = false;
    int recPos = 0, pendingFinish = -1;

    int curMode = Off, prevMode = Off;
    bool prevUndo = false, prevBurst = false;

    double ph = 0.0; // playhead, [0, period)
    int lastIdx = -1;

    double bph = 0.0, anchor = 0.0; // burst reader
    int burstCount = 0;
    float burstMix = 0.0f;

    // 10 ms ramps: loop level, overdub write level, layer level (Undo fades the layer out before clearing it)
    float loopGain = 0.0f, ovGain = 0.0f, layerGain = 1.0f;
    bool layerClearPending = false, loopClearPending = false;
};
} // namespace grain::dsp
