# grainsexuell

[![build](https://github.com/Beatsex/grainsexuell/actions/workflows/build.yml/badge.svg)](https://github.com/Beatsex/grainsexuell/actions/workflows/build.yml) [![release](https://img.shields.io/github/v/release/Beatsex/grainsexuell?include_prereleases)](https://github.com/Beatsex/grainsexuell/releases/latest) [![license: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE) ![VST3 · AU · LV2](https://img.shields.io/badge/format-VST3%20%C2%B7%20AU%20%C2%B7%20LV2-pink)

**English** · [Deutsch](README.de.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md)

A granular / micro-loop multi-effect with two engines and a phrase looper, by **beatsexuell**.
VST3 on Windows, macOS and Linux, AU on macOS, LV2 on Linux, plus a standalone app.

> It listens to what you just played, and remembers it wrong.

[![grainsexuell tutorial](https://img.youtube.com/vi/63asHO567NA/maxresdefault.jpg)](https://www.youtube.com/watch?v=63asHO567NA)

▶ [Watch the tutorial (3 min)](https://www.youtube.com/watch?v=63asHO567NA)

## Features

- **Four engines, four variants each**
  - **Mosaic**: grabs the last moment as a micro-loop and stacks it at 0.5x / 1x / 2x / 4x
  - **Cloud**: granular cloud, 3 to 80 grains per second
  - **Shard**: tempo-synced glitch slicer (shuffle, crush, arp, stutter)
  - **Tap**: multitap delay (echo, ping-pong, reverse, cascade)
- **Two slots**: engine A plus an optional engine B, in **serial** (A then B) or **parallel** (A and B mixed)
- **Macro controls**: Swarm (how busy), Tail (repeats), Span (length), Scatter (randomness), Bend (pitch wobble)
- **Dusk** low-pass, **Haze** reverb (Room / Plate / Hall / Swell), Mix A and Mix B per engine, a master **Dry/Wet**, In and Out gain (±24 dB)
- **Hold**: freezes the last beat and loops it into the effect
- **Phrase looper**: Off / Rec / Play / Dub, speed ½ · 1x · 2x, pre- or post-FX, reverse, quantize, burst, undo
- **Auto Gain**: keeps the output close to the input level, with a safety ceiling
- **56 factory presets** in six categories (Ambient, Shimmer, Rhythmic, Echo, Lo-Fi, Drift), plus your own
- **MIDI learn**: right-click any control, then move a knob on your controller
- **Tempo sync**: Tap and Shard follow the host tempo
- Resizable window, live grain-activity display, update notice when a new release is out

## Install

Download the latest build from [Releases](https://github.com/Beatsex/grainsexuell/releases).

| System | How |
|---|---|
| Windows 10 / 11 (64-bit) | Run `grainsexuell-…-win64-setup.exe`. The beta is not code-signed yet: if SmartScreen appears, click *More info* → *Run anyway*. |
| macOS | Copy `grainsexuell.vst3` to `~/Library/Audio/Plug-Ins/VST3` and `grainsexuell.component` to `~/Library/Audio/Plug-Ins/Components`. See `INSTALL_mac.txt`. |
| Linux | Copy the VST3 / LV2 folder to `~/.vst3` / `~/.lv2`. See `INSTALL_linux.txt`. |

Then restart your DAW (or rescan plugins) and insert it as an audio effect: *beatsexuell → grainsexuell*.

## Quick start

1. Put grainsexuell on a track with something melodic or rhythmic.
2. Click **>** next to the preset name to step through presets, or click the name to open the categories.
3. Turn **Swarm** for more or fewer events and **Tail** for longer or shorter repeats.
4. Switch on engine **B** and choose **Serial** or **Parallel** for layered textures.
5. Press **Hold** to freeze a moment, or set the looper to **Rec** to capture a phrase.

The full user manual (`USER_MANUAL.html`) comes with every download.

## Build from source

Requirements: CMake 3.22+, a C++20 compiler (Visual Studio 2022, Xcode or GCC/Clang). JUCE 8 and Catch2 are fetched automatically.

```
cmake -B build -G "Visual Studio 17 2022" -A x64   # or: cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

The plugin ends up in `build/grainsexuell_artefacts/Release/`.

## Feedback

Open an [issue](https://github.com/Beatsex/grainsexuell/issues) with the preset or settings, what you played, what you heard, your DAW and sample rate. For a crash: which DAW and what you clicked last.

## License

[MIT](LICENSE) © beatsexuell
