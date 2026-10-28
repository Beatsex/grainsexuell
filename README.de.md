# grainsexuell

[![build](https://github.com/Beatsex/grainsexuell/actions/workflows/build.yml/badge.svg)](https://github.com/Beatsex/grainsexuell/actions/workflows/build.yml) [![release](https://img.shields.io/github/v/release/Beatsex/grainsexuell?include_prereleases)](https://github.com/Beatsex/grainsexuell/releases/latest) [![license: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE) ![VST3 · AU · LV2](https://img.shields.io/badge/format-VST3%20%C2%B7%20AU%20%C2%B7%20LV2-pink)

[English](README.md) · **Deutsch** · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md)

Ein granularer Micro-Loop-Multieffekt mit zwei Engines und einem Phrasen-Looper, von **beatsexuell**.
VST3 für Windows, macOS und Linux, AU für macOS, LV2 für Linux, dazu eine Standalone-App.

> Er hört zu, was du gerade gespielt hast, und erinnert sich falsch daran.

[![grainsexuell tutorial](docs/tutorial.jpg)](https://youtu.be/63asHO567NA)

▶ [Tutorial ansehen (3 Min.)](https://youtu.be/63asHO567NA)

## Funktionen

- **Vier Engines mit je vier Varianten**
  - **Mosaic**: greift den letzten Moment als Micro-Loop und schichtet ihn in 0,5x / 1x / 2x / 4x
  - **Cloud**: Granularwolke, 3 bis 80 Grains pro Sekunde
  - **Shard**: tempo-synchroner Glitch-Slicer (Shuffle, Crush, Arp, Stutter)
  - **Tap**: Multitap-Delay (Echo, Ping-Pong, Reverse, Cascade)
- **Zwei Slots**: Engine A plus optional Engine B, **seriell** (erst A, dann B) oder **parallel** (A und B gemischt)
- **Makro-Regler**: Swarm (wie dicht), Tail (Wiederholungen), Span (Länge), Scatter (Zufall), Bend (Tonhöhen-Schwanken)
- **Dusk**-Tiefpass, **Haze**-Hall (Room / Plate / Hall / Swell), Mix A und Mix B pro Engine, ein Master-**Dry/Wet**, In- und Out-Gain (±24 dB)
- **Hold**: friert den letzten Beat ein und schickt ihn als Schleife in den Effekt
- **Phrasen-Looper**: Off / Rec / Play / Dub, Tempo ½ · 1x · 2x, vor oder nach dem Effekt, Reverse, Quantize, Burst, Undo
- **Auto Gain**: hält den Ausgang nah am Eingangspegel, mit Sicherheits-Limit
- **56 Werks-Presets** in sechs Kategorien (Ambient, Shimmer, Rhythmic, Echo, Lo-Fi, Drift), dazu deine eigenen
- **MIDI Learn**: Rechtsklick auf einen Regler, dann einen Knopf am Controller bewegen
- **Tempo-Sync**: Tap und Shard folgen dem Tempo des Hosts
- Skalierbares Fenster, Live-Anzeige der Grain-Aktivität, Hinweis wenn ein neues Release da ist

## Installation

Die neueste Version gibt es unter [Releases](https://github.com/Beatsex/grainsexuell/releases).

| System | So geht's |
|---|---|
| Windows 10 / 11 (64 Bit) | `grainsexuell-…-win64-setup.exe` starten. Die Beta ist noch nicht signiert: Wenn SmartScreen erscheint, auf *Weitere Informationen* → *Trotzdem ausführen* klicken. |
| macOS | `grainsexuell.vst3` nach `~/Library/Audio/Plug-Ins/VST3` und `grainsexuell.component` nach `~/Library/Audio/Plug-Ins/Components` kopieren. Siehe `INSTALL_mac.txt`. |
| Linux | Den VST3- bzw. LV2-Ordner nach `~/.vst3` bzw. `~/.lv2` kopieren. Siehe `INSTALL_linux.txt`. |

Danach die DAW neu starten (oder Plugins neu scannen) und als Audio-Effekt einfügen: *beatsexuell → grainsexuell*.

## Schnellstart

1. grainsexuell auf eine Spur mit etwas Melodischem oder Rhythmischem legen.
2. Mit **>** neben dem Preset-Namen durch die Presets klicken, oder auf den Namen klicken, um die Kategorien zu öffnen.
3. **Swarm** drehen für mehr oder weniger Ereignisse, **Tail** für längere oder kürzere Wiederholungen.
4. Engine **B** einschalten und **Serial** oder **Parallel** wählen, für geschichtete Texturen.
5. **Hold** drücken, um einen Moment einzufrieren, oder den Looper auf **Rec** stellen, um eine Phrase aufzunehmen.

Das vollständige Handbuch (`USER_MANUAL.html`) liegt jedem Download bei (auf Englisch).

## Selbst bauen

Voraussetzungen: CMake 3.22+, ein C++20-Compiler (Visual Studio 2022, Xcode oder GCC/Clang). JUCE 8 und Catch2 werden automatisch geladen.

```
cmake -B build -G "Visual Studio 17 2022" -A x64   # oder: cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

Das Plugin landet in `build/grainsexuell_artefacts/Release/`.

## Feedback

Eröffne ein [Issue](https://github.com/Beatsex/grainsexuell/issues) mit Preset oder Einstellungen, was du gespielt hast, was du gehört hast, deiner DAW und Samplerate. Bei einem Absturz: welche DAW und was du zuletzt angeklickt hast.

## Lizenz

[MIT](LICENSE) © beatsexuell
