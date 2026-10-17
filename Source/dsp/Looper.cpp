#include "Looper.h"

#include <algorithm>
#include <cmath>

namespace grain::dsp
{
void Looper::prepare(double sr, double maxSeconds)
{
    sampleRate = sr;
    cap = 0; // stays 0 (process() then does nothing) unless every allocation below succeeds
    const int frames = std::max(64, static_cast<int>(maxSeconds * sr));
    for (int c = 0; c < 2; ++c)
    {
        base[c].assign(static_cast<size_t>(frames), 0.0f);
        layer[c].assign(static_cast<size_t>(frames), 0.0f);
    }
    dirty.assign(static_cast<size_t>((frames >> chunkShift) + 1), 0);
    cap = frames;
    layerUsed = false;
    length = 0;
    period = seam = 1;
    recording = playing = false;
    recPos = 0;
    pendingFinish = -1;
    curMode = prevMode = Off;
    prevUndo = prevBurst = false;
    ph = 0.0;
    lastIdx = -1;
    resetBurst();
}

void Looper::resetBurst()
{
    bph = anchor = 0.0;
    burstCount = 0;
    burstMix = 0.0f;
    // a new or cleared loop starts silent and fades in; nothing is left to fade out
    loopGain = ovGain = 0.0f;
    layerGain = 1.0f;
    layerClearPending = loopClearPending = false;
}

void Looper::clearLayer()
{
    std::fill(dirty.begin(), dirty.end(), static_cast<std::uint8_t>(0));
    layerUsed = false;
}

void Looper::startRecord()
{
    recording = true;
    playing = false;
    recPos = 0;
    pendingFinish = -1;
    length = 0;
    period = seam = 1;
    clearLayer();
    resetBurst(); // a reader anchored in the old loop would sit past the new, shorter period
}

void Looper::finalize(int len, bool play)
{
    recording = false;
    pendingFinish = -1;
    clearLayer();
    ph = 0.0;
    lastIdx = -1;
    resetBurst();
    if (len < 16) // too short to be a loop
    {
        length = 0;
        period = seam = 1;
        playing = false;
        return;
    }
    length = len;
    seam = std::max(1, std::min(static_cast<int>(0.01 * sampleRate), len / 4));
    period = len - seam;
    playing = play;
}

void Looper::stopRecord(bool quantize, double beat)
{
    if (! quantize)
    {
        finalize(recPos, curMode != Off);
        return;
    }
    // whole beats plus the 10 ms seam, so the repeat period is exactly the beats (tempo-locked)
    const auto beats = std::max<long long>(1, std::llround(recPos / beat));
    const int target = static_cast<int>(
        std::min<long long>(cap, std::llround(static_cast<double>(beats) * beat) + static_cast<long long>(0.01 * sampleRate)));
    if (target <= recPos)
        finalize(target, curMode != Off);
    else
        pendingFinish = target; // keep recording up to the beat; finalize() then follows the mode of that moment
}

void Looper::addToLayer(int idx, float l, float r)
{
    const auto chunk = static_cast<size_t>(idx >> chunkShift);
    if (! dirty[chunk])
    {
        const auto from = static_cast<size_t>(chunk) << chunkShift;
        const auto to = std::min(static_cast<size_t>(cap), from + (size_t { 1 } << chunkShift));
        for (int c = 0; c < 2; ++c)
            std::fill(layer[c].begin() + static_cast<std::ptrdiff_t>(from), layer[c].begin() + static_cast<std::ptrdiff_t>(to), 0.0f);
        dirty[chunk] = 1;
    }
    layer[0][static_cast<size_t>(idx)] += l;
    layer[1][static_cast<size_t>(idx)] += r;
    layerUsed = true;
}

void Looper::frame(int i, float& l, float& r) const
{
    const auto u = static_cast<size_t>(i);
    l = base[0][u];
    r = base[1][u];
    if (i < seam) // equal-power join with the tail one period further on
    {
        const float t = 1.5707963f * static_cast<float>(i) / static_cast<float>(seam);
        const float up = std::sin(t), down = std::cos(t);
        const auto b = static_cast<size_t>(i + period);
        l = up * l + down * base[0][b];
        r = up * r + down * base[1][b];
    }
    if (layerUsed && dirty[static_cast<size_t>(i >> chunkShift)])
    {
        const float f = std::min({ 1.0f, static_cast<float>(i) / static_cast<float>(seam),
                                   static_cast<float>(period - i) / static_cast<float>(seam) });
        const float w = f * f * (3.0f - 2.0f * f) * layerGain;
        l += w * layer[0][u];
        r += w * layer[1][u];
    }
}

void Looper::read(double p, float& l, float& r) const
{
    const int i0 = std::clamp(static_cast<int>(p), 0, period - 1);
    const int i1 = i0 + 1 >= period ? 0 : i0 + 1;
    const float frac = static_cast<float>(std::clamp(p - i0, 0.0, 1.0));
    float l0, r0, l1, r1;
    frame(i0, l0, r0);
    frame(i1, l1, r1);
    l = l0 + frac * (l1 - l0);
    r = r0 + frac * (r1 - r0);
}

void Looper::advance(double& p, double step) const
{
    p += step;
    if (p >= period)
        p -= period;
    else if (p < 0.0)
        p += period;
}

void Looper::process(float* const* ch, int numCh, int n, const Params& req)
{
    if (cap == 0)
        return; // process before prepare: leave the audio alone

    const int mode = std::clamp(req.mode, 0, 3);
    const int speedIdx = std::clamp(req.speed, 0, 2);
    const double bpm = std::isfinite(req.bpm) && req.bpm > 0.0 ? std::clamp(req.bpm, 20.0, 400.0) : 120.0;
    const double beat = 60.0 / bpm * sampleRate;
    curMode = mode;

    if (mode != prevMode)
    {
        if (mode == Record)
            startRecord();
        else if (recording)
            stopRecord(req.quantize, beat);
        else if (mode == Off)
            playing = false;
        else if (length > 0 && ! playing)
        {
            playing = true;
            ph = 0.0;
        }
        lastIdx = -1; // overdub gap fill starts afresh
        prevMode = mode;
    }

    if (req.undo && ! prevUndo && ! recording)
    {
        if (layerUsed)
            layerClearPending = true; // the layer fades out, then its flags are cleared
        else if (length > 0)
        {
            loopClearPending = true; // the loop fades out, then it is cleared
            playing = false;
        }
    }
    prevUndo = req.undo;

    if (req.burst && ! prevBurst && playing)
    {
        anchor = bph = ph;
        burstCount = 0;
    }
    prevBurst = req.burst;

    const double rate = speedIdx == 0 ? 0.5 : speedIdx == 1 ? 1.0 : 2.0;
    const double step = req.reverse ? -rate : rate;
    const int slice = std::max(64, static_cast<int>(0.25 * beat)); // 1/16 note
    const float fadeLen = std::max(1.0f, static_cast<float>(0.003 * sampleRate));
    const float rampStep = 1.0f / fadeLen;
    const float fadeStep = 1.0f / std::max(1.0f, static_cast<float>(0.01 * sampleRate));
    const float burstTarget = req.burst && playing ? 1.0f : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float l = ch[0][i], r = numCh > 1 ? ch[1][i] : l;
        // Post-FX hears the engine output, which the input clamp never saw: a NaN must not get into the loop
        const float inL = std::isfinite(l) ? l : 0.0f, inR = std::isfinite(r) ? r : 0.0f;

        if (recording)
        {
            base[0][static_cast<size_t>(recPos)] = inL;
            base[1][static_cast<size_t>(recPos)] = inR;
            ++recPos;
            if (recPos >= cap)
                finalize(cap, curMode != Off); // full: carry on as Play (unless the transport was switched off)
            else if (pendingFinish >= 0 && recPos >= pendingFinish)
                finalize(pendingFinish, curMode != Off && curMode != Record);
        }
        // every abrupt change gets a 10 ms ramp: loop on/off, overdub on/off, Undo of the layer
        loopGain += std::clamp((playing ? 1.0f : 0.0f) - loopGain, -fadeStep, fadeStep);
        ovGain += std::clamp((curMode == Overdub && playing ? 1.0f : 0.0f) - ovGain, -fadeStep, fadeStep);
        layerGain += std::clamp((layerClearPending ? 0.0f : 1.0f) - layerGain, -fadeStep, fadeStep);
        if (layerClearPending && layerGain <= 0.0f)
        {
            clearLayer();
            layerClearPending = false;
            layerGain = 1.0f;
        }
        if (loopClearPending && loopGain <= 0.0f)
        {
            length = 0;
            period = seam = 1;
            resetBurst();
            clearLayer();
            loopClearPending = false;
        }
        if (length == 0 || (! playing && loopGain <= 0.0f))
            continue;

        float ol, orr;
        read(ph, ol, orr);
        ol *= loopGain;
        orr *= loopGain;

        burstMix += std::clamp(burstTarget - burstMix, -rampStep, rampStep);
        const bool burstRunning = burstMix > 0.0f || burstTarget > 0.0f;
        if (burstMix > 0.0f)
        {
            float bl, br;
            read(bph, bl, br);
            const float u = std::min({ 1.0f, static_cast<float>(burstCount) / fadeLen,
                                       static_cast<float>(slice - burstCount) / fadeLen });
            const float g = burstMix * u * u * (3.0f - 2.0f * u);
            ol = (1.0f - burstMix) * ol + g * bl;
            orr = (1.0f - burstMix) * orr + g * br;
        }

        ch[0][i] = l + ol;
        if (numCh > 1)
            ch[1][i] = r + orr;

        if (ovGain > 0.0f) // record what is heard at the playhead; fill positions a fast speed skips
        {
            const int idx = std::min(static_cast<int>(ph), period - 1);
            if (idx != lastIdx)
            {
                int k = lastIdx >= 0 ? lastIdx : idx;
                const int dir = step < 0.0 ? -1 : 1;
                for (int guard = 0; guard < 4; ++guard)
                {
                    if (k != idx)
                        k = (k + dir + period) % period;
                    addToLayer(k, inL * ovGain, inR * ovGain);
                    if (k == idx)
                        break;
                }
                lastIdx = idx;
            }
        }

        advance(ph, step);
        if (burstRunning)
        {
            advance(bph, step);
            if (++burstCount >= slice)
            {
                burstCount = 0;
                bph = anchor;
            }
        }
    }
}
} // namespace grain::dsp
