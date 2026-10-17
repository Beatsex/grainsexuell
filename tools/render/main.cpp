// Offline render harness: WAV (or generated signal) in, plugin processor with fixed parameters, WAV out,
// then the automatic checks (identity, finite, bounded). Exit code 0 = all checks passed, 1 = a check failed, 2 = bad usage.
//
//   grain_render <in.wav | --gen impulse|sweep|tone440|notes|silence> <out.wav>
//                [--set id=value]... [--sr 48000] [--block 512] [--seconds 2] [--tail 0] [--gain dB] [--expect-identity]

#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <cstdio>
#include <numbers>

namespace
{
struct Options
{
    juce::String in, gen, out;
    double sampleRate = 48000.0, seconds = 2.0;
    int block = 512;
    double tail = 0.0; // extra silent seconds appended so reverb decay is rendered and checked
    double gainDb = 0.0; // input gain applied before processing (quiet / hot test sources)
    bool expectIdentity = false;
    int program = -1; // --program N: start from factory preset N
    std::vector<std::pair<juce::String, float>> sets;
    struct Event { double seconds; juce::String id; float value; };
    std::vector<Event> events; // --at S id=value: set a parameter when the render reaches S seconds (block resolution)
};

int usage(const char* why)
{
    std::fprintf(stderr,
                 "%s\nusage: grain_render <in.wav | --gen impulse|sweep|tone440|notes|silence> <out.wav> "
                 "[--program N] [--set id=value]... [--at S id=value]... [--sr N] [--block N] [--seconds S] [--tail S] [--gain dB] [--expect-identity]\n",
                 why);
    return 2;
}

bool parse(int argc, char** argv, Options& o)
{
    juce::StringArray positional;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a(argv[i]);
        auto next = [&]() -> juce::String { return i + 1 < argc ? juce::String(argv[++i]) : juce::String(); };

        if (a == "--gen") o.gen = next();
        else if (a == "--sr") o.sampleRate = next().getDoubleValue();
        else if (a == "--block") o.block = next().getIntValue();
        else if (a == "--seconds") o.seconds = next().getDoubleValue();
        else if (a == "--tail") o.tail = next().getDoubleValue();
        else if (a == "--gain") o.gainDb = next().getDoubleValue();
        else if (a == "--expect-identity") o.expectIdentity = true;
        else if (a == "--program") o.program = next().getIntValue();
        else if (a == "--set")
        {
            const auto kv = next();
            if (! kv.contains("=")) return false;
            o.sets.emplace_back(kv.upToFirstOccurrenceOf("=", false, false), kv.fromFirstOccurrenceOf("=", false, false).getFloatValue());
        }
        else if (a == "--at")
        {
            const auto when = next(), kv = next();
            if (! kv.contains("=")) return false;
            o.events.push_back({ when.getDoubleValue(), kv.upToFirstOccurrenceOf("=", false, false),
                                 kv.fromFirstOccurrenceOf("=", false, false).getFloatValue() });
        }
        else positional.add(a);
    }

    if (o.gen.isEmpty()) { if (positional.size() != 2) return false; o.in = positional[0]; o.out = positional[1]; }
    else { if (positional.size() != 1) return false; o.out = positional[0]; }
    return o.block > 0 && o.sampleRate > 0 && o.seconds > 0;
}

bool generate(const juce::String& name, double sr, double seconds, juce::AudioBuffer<float>& b)
{
    const int n = static_cast<int>(sr * seconds);
    b.setSize(2, n);
    b.clear();
    const double twoPi = 2.0 * std::numbers::pi;

    if (name == "impulse")
        b.setSample(0, n / 10, 1.0f), b.setSample(1, n / 10, 1.0f);
    else if (name == "notes")
    {
        // decaying plucked tones, a new pitch every 0.5 s: a more realistic loop source than a steady sine
        constexpr double pitches[] { 220.0, 277.18, 329.63, 440.0, 329.63, 246.94 };
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr, k = std::floor(t / 0.5), tt = t - k * 0.5, f = pitches[static_cast<int>(k) % 6];
            const double env = std::exp(-tt / 0.2) * std::min(1.0, tt / 0.005) * std::min(1.0, (0.5 - tt) / 0.005); // 5 ms release: no step at the note change
            const double v = (std::sin(twoPi * f * tt) + 0.3 * std::sin(twoPi * 2 * f * tt) + 0.15 * std::sin(twoPi * 3 * f * tt)) / 1.45;
            b.setSample(0, i, static_cast<float>(0.5 * env * v));
            b.setSample(1, i, b.getSample(0, i));
        }
    }
    else if (name == "tone440" || name == "sweep")
    {
        // log sweep 20 Hz -> min(20 kHz, 0.45 sr); phase integral of f(t) = f0 * k^t
        const double f0 = 20.0, f1 = std::min(20000.0, 0.45 * sr), k = std::log(f1 / f0);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double phase = name == "tone440" ? twoPi * 440.0 * t
                                                   : twoPi * f0 * seconds / k * (std::exp(k * t / seconds) - 1.0);
            b.setSample(0, i, 0.5f * static_cast<float>(std::sin(phase)));
            b.setSample(1, i, b.getSample(0, i));
        }
    }
    else return name == "silence";
    return true;
}

struct Stats { float peak = 0, maxJump = 0; double dc = 0; bool finite = true; };

Stats measure(const juce::AudioBuffer<float>& b, int channels)
{
    Stats s;
    for (int ch = 0; ch < channels; ++ch)
    {
        const float* x = b.getReadPointer(ch);
        double sum = 0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            if (! std::isfinite(x[i])) { s.finite = false; continue; }
            s.peak = std::max(s.peak, std::abs(x[i]));
            sum += x[i];
            if (i > 0) s.maxJump = std::max(s.maxJump, std::abs(x[i] - x[i - 1]));
        }
        s.dc = std::max(s.dc, std::abs(sum / std::max(1, b.getNumSamples())));
    }
    return s;
}

bool report(bool ok, const char* name, const juce::String& detail)
{
    std::printf("%s  %-18s %s\n", ok ? "PASS" : "FAIL", name, detail.toRawUTF8());
    return ok;
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    Options o;
    if (! parse(argc, argv, o)) return usage("bad arguments");

    juce::AudioBuffer<float> input;
    int inChannels = 2;
    if (o.gen.isNotEmpty())
    {
        if (! generate(o.gen, o.sampleRate, o.seconds, input)) return usage("unknown generator");
    }
    else
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(o.in)));
        if (reader == nullptr || reader->numChannels > 2) return usage("cannot read input (need mono/stereo WAV)");
        inChannels = static_cast<int>(reader->numChannels);
        o.sampleRate = reader->sampleRate;
        input.setSize(2, static_cast<int>(reader->lengthInSamples));
        input.clear();
        reader->read(&input, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    }

    input.applyGain(static_cast<float>(juce::Decibels::decibelsToGain(o.gainDb)));
    if (o.tail > 0.0)
        input.setSize(2, input.getNumSamples() + static_cast<int>(o.tail * o.sampleRate), true, true);

    PluginProcessor p;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(inChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    if (! p.setBusesLayout(layout)) return usage("layout rejected");

    if (o.program >= 0)
    {
        if (o.program >= p.getNumPrograms()) return usage("no such program");
        p.setCurrentProgram(o.program); // factory preset first, so --set can still override single values
    }

    for (auto& [id, value] : o.sets)
    {
        auto* param = p.apvts.getParameter(id);
        if (param == nullptr) return usage(("unknown parameter: " + id).toRawUTF8());
        param->setValueNotifyingHost(param->convertTo0to1(value));
    }

    for (auto& e : o.events)
        if (p.apvts.getParameter(e.id) == nullptr) return usage(("unknown parameter: " + e.id).toRawUTF8());

    p.setRateAndBufferSizeDetails(o.sampleRate, o.block);
    p.prepareToPlay(o.sampleRate, o.block);

    const int total = input.getNumSamples();
    juce::AudioBuffer<float> output(2, total), work(2, o.block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += o.block)
    {
        const int n = std::min(o.block, total - pos);
        for (auto& e : o.events)
            if (pos >= static_cast<int>(e.seconds * o.sampleRate) && pos - o.block < static_cast<int>(e.seconds * o.sampleRate))
            {
                auto* param = p.apvts.getParameter(e.id);
                param->setValueNotifyingHost(param->convertTo0to1(e.value));
            }
        work.clear();
        for (int ch = 0; ch < inChannels; ++ch) work.copyFrom(ch, 0, input, ch, pos, n);
        juce::AudioBuffer<float> view(work.getArrayOfWritePointers(), 2, n);
        p.processBlock(view, midi);
        for (int ch = 0; ch < 2; ++ch) output.copyFrom(ch, pos, view, ch, 0, n);
    }
    p.releaseResources();

    // write 32-bit float so levels above 0 dBFS stay visible
    juce::File outFile(o.out);
    outFile.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = outFile.createOutputStream();
    if (stream == nullptr) return usage("cannot open output file");
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(o.sampleRate).withNumChannels(2).withBitsPerSample(32));
    if (writer == nullptr) return usage("cannot create WAV writer");
    writer->writeFromAudioSampleBuffer(output, 0, total);
    writer.reset();

    // automatic checks
    const auto in = measure(input, inChannels), out = measure(output, 2);
    bool ok = true;
    ok &= report(out.finite, "finite", "no NaN/Inf");
    ok &= report(out.peak <= 1.0f + 1e-6f, "peak <= 0 dBFS", juce::String(juce::Decibels::gainToDecibels(out.peak), 2) + " dBFS");
    ok &= report(std::abs(out.dc - in.dc) < 1e-3, "no added DC", "out " + juce::String(out.dc, 6) + ", in " + juce::String(in.dc, 6));
    const float jumpLimit = std::max(0.5f, 2.0f * in.maxJump); // coarse: misses cuts at zero crossings
    ok &= report(out.maxJump <= jumpLimit, "no clicks", "max jump " + juce::String(out.maxJump, 4) + ", limit " + juce::String(jumpLimit, 4));
    ok &= report(in.peak > 0.0f || out.peak == 0.0f, "silence in = out", "out peak " + juce::String(out.peak, 9));

    if (o.expectIdentity)
    {
        float maxDiff = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < total; ++i)
                maxDiff = std::max(maxDiff, std::abs(output.getSample(ch, i) - input.getSample(inChannels == 1 ? 0 : ch, i)));
        ok &= report(maxDiff <= 1e-7f, "identity", "max diff " + juce::String(maxDiff, 9));
    }

    std::printf("wrote %s (%d samples, %.0f Hz)\n", o.out.toRawUTF8(), total, o.sampleRate);
    return ok ? 0 : 1;
}
