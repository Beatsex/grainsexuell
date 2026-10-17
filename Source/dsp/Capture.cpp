#include "Capture.h"

#include <algorithm>

namespace grain::dsp
{
void Capture::prepare(double sr, float seconds)
{
    sampleRate = sr;
    size = 1;
    while (size < static_cast<int>(seconds * sr) + 16)
        size <<= 1;
    mask = size - 1;
    for (auto& r : ring)
        for (auto& c : r)
            c.assign(static_cast<size_t>(size), 0.0f);
    writePos = 0;
    lp2.prepare(sr);
    lp4.prepare(sr);
}

void Capture::write(const float* l, const float* r, int m)
{
    std::copy_n(l, m, raw2[0].data());
    std::copy_n(r, m, raw2[1].data());
    std::copy_n(l, m, raw4[0].data());
    std::copy_n(r, m, raw4[1].data());
    float* p2[] { raw2[0].data(), raw2[1].data() };
    float* p4[] { raw4[0].data(), raw4[1].data() };
    lp2.process(p2, 2, m, 0.225f * static_cast<float>(sampleRate));
    lp4.process(p4, 2, m, 0.1125f * static_cast<float>(sampleRate));

    for (int f = 0; f < m; ++f)
    {
        const auto w = static_cast<size_t>(writePos);
        ring[0][0][w] = l[f];
        ring[0][1][w] = r[f];
        for (size_t c = 0; c < 2; ++c)
        {
            ring[1][c][w] = raw2[c][static_cast<size_t>(f)];
            ring[2][c][w] = raw4[c][static_cast<size_t>(f)];
        }
        writePos = (writePos + 1) & mask;
    }
}
} // namespace grain::dsp
