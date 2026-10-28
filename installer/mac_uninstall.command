#!/bin/bash
# Removes grainsexuell installed by the .pkg (VST3, Audio Unit, app folder) and the installer receipts.
# Your presets in ~/Library/grainsexuell are kept; delete that folder by hand if you want them gone too.
echo "This removes grainsexuell (VST3, Audio Unit and the app). Your presets are kept."
read -r -p "Continue? [y/N] " answer
[[ "$answer" =~ ^[Yy]$ ]] || exit 0

sudo rm -rf /Library/Audio/Plug-Ins/VST3/grainsexuell.vst3 \
            /Library/Audio/Plug-Ins/Components/grainsexuell.component \
            /Applications/grainsexuell \
            /Applications/grainsexuell.app
for id in vst3 au app; do
  sudo pkgutil --forget "com.beatsexuell.grainsexuell.$id" >/dev/null 2>&1
done
echo "grainsexuell is removed. Restart your DAW."
