grainsexuell  -  beta 0.2.0 (Windows 64-bit)
by beatsexuell
A granular / micro-loop multi-effect with two engines and a phrase looper.

INSTALL (VST3)
  Copy the folder "grainsexuell.vst3" to  C:\Program Files\Common Files\VST3\
  Rescan plugins in your DAW and put it on an audio track as an effect.
  No installer, no extra runtime needed.

STANDALONE
  Double-click grainsexuell.exe and choose your audio device in the options window.

NEW IN 0.2.0
  - New look: pink panel with live GRAIN ACTIVITY (events per second + the last seconds as bars).
  - Two engines: engine A, plus an optional engine B (B off / Mosaic / Cloud / Shard / Tap)
    in SERIAL (A then B) or PARALLEL (A and B mixed). Mix B sets B's dry/wet.
  - IN and OUT gain (-24 to +24 dB).
  - Init preset (all sound controls back to default), at the top of the preset list.
  - Switches (Auto Gain, Hold, Reverse, Quantize, Burst) show a square: filled = on.

THE BASICS
  Engines  Mosaic / Cloud / Shard / Tap, each with four variants.
  Rows     01 In, 02 Swarm (how busy), 03 Tail (repeats / feedback), 04 Span (length),
           05 Scatter (randomness), 06 Bend (pitch wobble), 07 Dusk (low-pass),
           08 Haze (reverb, 4 modes), 09 Mix, 10 Mix B, 11 Out. Double-click a slider to reset it.
  Hold     freezes the last beat and loops it into the effect.
  Looper   Off / Rec / Play / Dub, speed 1/2-1x-2x, Pre-FX / Post-FX, reverse, quantize, burst, undo.
  Auto Gain  keeps the output level close to the input and adds a safety ceiling.
  Presets  Init + 16 factory presets; Save writes your own to %APPDATA%\grainsexuell\Presets.
  MIDI     right-click any control -> "MIDI learn", then move a knob on your controller.
  Tempo    Tap and Shard follow the host tempo (120 BPM in the standalone).
  Window   drag the corner to resize; the size is remembered.

KNOWN ISSUES / PLEASE WATCH FOR
  - Output can go above 0 dBFS with loud input when Auto Gain is off (some presets).
  - Each instance uses about 50 MB of memory for the looper (more at 192 kHz).
  - Very low bass can show a small DC offset in the wet signal.
  - Factory preset values are first guesses; engine B adds CPU.
  - Windows only for now; unsigned, so Windows SmartScreen may warn about the .exe.

FEEDBACK (plain words are great)
  Which preset / settings, what you played, what you heard (clicky, metallic, too quiet,
  too washy...), your DAW and sample rate. A crash: which DAW and what you clicked last.
