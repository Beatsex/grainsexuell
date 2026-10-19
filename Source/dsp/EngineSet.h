#pragma once

#include "Cloud.h"
#include "Mosaic.h"
#include "Shard.h"
#include "Tap.h"

namespace grain::dsp
{
// One engine slot: the four families, of which exactly one is heard. The others keep recording, so a family switch
// has history to play from (each engine fades its own wet part in and out).
struct EngineSet
{
    struct Params
    {
        float swarm = 0.5f, tail = 0.5f, span = 0.5f, scatter = 0.0f, mix = 0.5f;
        int family = 0; // 0 Mosaic, 1 Cloud, 2 Shard, 3 Tap
        int variant = 0;
        double bpm = 120.0;
    };

    void prepare(double sampleRate)
    {
        mosaic.prepare(sampleRate);
        cloud.prepare(sampleRate);
        shard.prepare(sampleRate);
        tap.prepare(sampleRate);
    }

    void process(float* const* ch, int numCh, int n, const Params& p)
    {
        mosaic.process(ch, numCh, n, { p.swarm, p.tail, p.span, p.scatter, p.mix, p.variant, p.family == 0 });
        cloud.process(ch, numCh, n, { p.swarm, p.tail, p.span, p.scatter, p.mix, p.variant, p.family == 1 });
        shard.process(ch, numCh, n, { p.swarm, p.tail, p.span, p.scatter, p.mix, p.variant, p.family == 2, p.bpm });
        tap.process(ch, numCh, n, { p.swarm, p.tail, p.span, p.scatter, p.mix, p.variant, p.family == 3, p.bpm });
    }

    Mosaic mosaic;
    Cloud cloud;
    Shard shard;
    Tap tap;
};
} // namespace grain::dsp
