grainsexuell  -  beta 0.3.0 (Windows 10 / 11, 64-bit)
by beatsexuell
A granular / micro-loop multi-effect with two engines and a phrase looper.

INSTALL
  1. Double-click "grainsexuell-beta-0.3.0-win64-setup.exe".
  2. The beta is not code-signed yet, so Windows SmartScreen may say
     "Windows protected your PC". Click "More info", then "Run anyway".
  3. Allow the administrator prompt (the VST3 folder needs admin rights).
  4. Choose what to install:
       Full    VST3 plugin and standalone app (recommended)
       Custom  tick only the parts you want
  5. Click Install. The installer puts in place:
       VST3        C:\Program Files\Common Files\VST3\grainsexuell.vst3
       Standalone  C:\Program Files\beatsexuell\grainsexuell\grainsexuell.exe
                   (plus Start menu entries for the app and the user manual)
  6. Restart your DAW, or rescan plugins, so it finds grainsexuell.
  No extra runtime is needed.

UPDATING
  Run the new installer over the old version; there is no need to uninstall first.
  Your own presets are kept. The 40 extra presets from 0.2.0 are now built in as factory
  presets; copies you imported into the User folder are hidden from the menu (delete them
  from %APPDATA%\grainsexuell\Presets if you like).
  From 0.3.0 on, the plugin shows an UPDATE button in its header when a newer release is out.

WHERE TO FIND IT
  Ableton Live, Bitwig, Reaper, Cubase, FL Studio, Studio One and other VST3 hosts:
    insert it as an audio effect on a track: VST3 > beatsexuell > grainsexuell.
  Standalone: Start menu > grainsexuell. Choose your audio device under Options
    (ASIO gives the lowest latency if your interface has a driver for it).

NEW IN 0.3.0
  - 56 factory presets in six categories: Ambient, Shimmer, Rhythmic, Echo, Lo-Fi, Drift.
    Click the preset name to open them by category; < and > step through all of them.
  - Master DRY/WET (row 11): blends the whole effect with your clean input. 100 % = as before.
  - MIX is now MIX A; Mix A and Mix B are tinted, the master Dry/Wet is pink.
  - Update notice: an UPDATE button appears when GitHub has a newer release.
  - The first click on > or < in a fresh instance now loads a preset straight away.

THE BASICS
  Engines  Mosaic / Cloud / Shard / Tap, each with four variants.
  Rows     01 In, 02 Swarm (how busy), 03 Tail (repeats / feedback), 04 Span (length),
           05 Scatter (randomness), 06 Bend (pitch wobble), 07 Dusk (low-pass),
           08 Haze (reverb, 4 modes), 09 Mix A, 10 Mix B, 11 Dry/Wet (master), 12 Out.
           Double-click a slider to reset it.
  Hold     freezes the last beat and loops it into the effect.
  Looper   Off / Rec / Play / Dub, speed 1/2-1x-2x, Pre-FX / Post-FX, reverse, quantize, burst, undo.
  Auto Gain  keeps the output level close to the input and adds a safety ceiling.
  Presets  Init + 56 factory presets; Save writes your own to %APPDATA%\grainsexuell\Presets.
  MIDI     right-click any control -> "MIDI learn", then move a knob on your controller.
  Tempo    Tap and Shard follow the host tempo (120 BPM in the standalone).
  Window   drag the corner to resize; the size is remembered.
  The full user manual is installed next to this file (USER_MANUAL.html, also in the Start menu).

UNINSTALL
  Settings > Apps > grainsexuell > Uninstall, or Start menu > Uninstall grainsexuell.
  Your presets in %APPDATA%\grainsexuell are left alone; delete that folder by hand
  if you want them gone too.

KNOWN ISSUES / PLEASE WATCH FOR
  - Output can go above 0 dBFS with loud input when Auto Gain is off (some presets).
  - Each instance uses about 50 MB of memory for the looper (more at 192 kHz).
  - Very low bass can show a small DC offset in the wet signal.
  - Factory preset values are first guesses; engine B adds CPU.
  - Not code-signed yet (see INSTALL step 2).

FEEDBACK (plain words are great)
  github.com/Beatsex/grainsexuell/issues - or just tell me: which preset / settings, what you
  played, what you heard (clicky, metallic, too quiet, too washy...), your DAW and sample rate.
  A crash: which DAW and what you clicked last.
