grainsexuell  -  beta 0.2.0 (Windows 10 / 11, 64-bit)
by beatsexuell
A granular / micro-loop multi-effect with two engines and a phrase looper.

INSTALL
  1. Double-click "grainsexuell-beta-0.2.0-win64-setup.exe".
  2. The beta is not code-signed yet, so Windows SmartScreen may say
     "Windows protected your PC". Click "More info", then "Run anyway".
  3. Allow the administrator prompt (the VST3 folder needs admin rights).
  4. Choose what to install:
       Full    VST3 plugin, standalone app and 40 extra presets (recommended)
       Custom  tick only the parts you want
  5. Click Install. The installer puts in place:
       VST3        C:\Program Files\Common Files\VST3\grainsexuell.vst3
       Standalone  C:\Program Files\beatsexuell\grainsexuell\grainsexuell.exe
                   (plus Start menu entries for the app and the user manual)
       Presets     %APPDATA%\grainsexuell\Presets  (40 extra presets, under "User")
  6. Restart your DAW, or rescan plugins, so it finds grainsexuell.
  No extra runtime is needed.

UPDATING
  Run the new installer over the old version; there is no need to uninstall first.
  If you installed beta 0.1.0 by copying the folder by hand, the installer simply replaces it.
  Your own presets are kept.

WHERE TO FIND IT
  Ableton Live, Bitwig, Reaper, Cubase, FL Studio, Studio One and other VST3 hosts:
    insert it as an audio effect on a track: VST3 > beatsexuell > grainsexuell.
  Standalone: Start menu > grainsexuell. Choose your audio device under Options
    (ASIO gives the lowest latency if your interface has a driver for it).

EXTRA PRESETS
  The installer adds 40 new presets (not built into the plugin yet). Click the preset name
  in the plugin: they are under "User". A preset you already have with the same name is
  never overwritten.

NEW IN 0.2.0
  - Windows installer (VST3 + standalone + 40 extra presets + Start menu entries + uninstaller).
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
  The full user manual is installed next to this file (USER_MANUAL.html, also in the Start menu).

UNINSTALL
  Settings > Apps > grainsexuell > Uninstall, or Start menu > Uninstall grainsexuell.
  Presets in %APPDATA%\grainsexuell (yours and the 40 extras) are left alone; delete that folder by hand
  if you want them gone too.

KNOWN ISSUES / PLEASE WATCH FOR
  - Output can go above 0 dBFS with loud input when Auto Gain is off (some presets).
  - Each instance uses about 50 MB of memory for the looper (more at 192 kHz).
  - Very low bass can show a small DC offset in the wet signal.
  - Factory preset values are first guesses; engine B adds CPU.
  - Not code-signed yet (see INSTALL step 2).

FEEDBACK (plain words are great)
  Which preset / settings, what you played, what you heard (clicky, metallic, too quiet,
  too washy...), your DAW and sample rate. A crash: which DAW and what you clicked last.
