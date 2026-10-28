# grainsexuell

[![build](https://github.com/Beatsex/grainsexuell/actions/workflows/build.yml/badge.svg)](https://github.com/Beatsex/grainsexuell/actions/workflows/build.yml) [![release](https://img.shields.io/github/v/release/Beatsex/grainsexuell?include_prereleases)](https://github.com/Beatsex/grainsexuell/releases/latest) [![license: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE) ![VST3 · AU · LV2](https://img.shields.io/badge/format-VST3%20%C2%B7%20AU%20%C2%B7%20LV2-pink)

[English](README.md) · [Deutsch](README.de.md) · [Español](README.es.md) · **Français** · [日本語](README.ja.md)

Un multi-effet granulaire à micro-boucles avec deux moteurs et un looper de phrases, par **beatsexuell**.
VST3 pour Windows, macOS et Linux, AU pour macOS, LV2 pour Linux, ainsi qu'une application autonome.

> Il écoute ce que tu viens de jouer, et s'en souvient de travers.

[![grainsexuell tutorial](https://img.youtube.com/vi/63asHO567NA/maxresdefault.jpg)](https://youtu.be/63asHO567NA)

▶ [Voir le tutoriel (3 min)](https://youtu.be/63asHO567NA)

## Fonctions

- **Quatre moteurs, quatre variantes chacun**
  - **Mosaic** : capture le dernier instant en micro-boucle et l'empile à 0,5x / 1x / 2x / 4x
  - **Cloud** : nuage granulaire, de 3 à 80 grains par seconde
  - **Shard** : découpeur glitch synchronisé au tempo (shuffle, crush, arp, stutter)
  - **Tap** : délai multitap (echo, ping-pong, reverse, cascade)
- **Deux emplacements** : moteur A et un moteur B optionnel, en **série** (A puis B) ou en **parallèle** (A et B mélangés)
- **Macros** : Swarm (densité), Tail (répétitions), Span (longueur), Scatter (hasard), Bend (flottement de hauteur)
- Filtre passe-bas **Dusk**, réverbération **Haze** (Room / Plate / Hall / Swell), Mix A et Mix B par moteur, un **Dry/Wet** général, gain d’entrée et de sortie (±24 dB)
- **Hold** : fige le dernier temps et le fait boucler dans l'effet
- **Looper de phrases** : Off / Rec / Play / Dub, vitesse ½ · 1x · 2x, avant ou après l'effet, reverse, quantize, burst, undo
- **Auto Gain** : garde la sortie proche du niveau d'entrée, avec un plafond de sécurité
- **56 presets d'usine** en six catégories (Ambient, Shimmer, Rhythmic, Echo, Lo-Fi, Drift), plus les tiens
- **MIDI learn** : clic droit sur n'importe quel contrôle, puis tourne un bouton de ton contrôleur
- **Synchro tempo** : Tap et Shard suivent le tempo de l'hôte
- Fenêtre redimensionnable, affichage en direct de l'activité granulaire, avis quand une nouvelle version sort

## Installation

Télécharge la dernière version dans [Releases](https://github.com/Beatsex/grainsexuell/releases).

| Système | Comment |
|---|---|
| Windows 10 / 11 (64 bits) | Lance `grainsexuell-…-win64-setup.exe`. La bêta n'est pas encore signée : si SmartScreen s'affiche, clique sur *Informations complémentaires* → *Exécuter quand même*. |
| macOS | Copie `grainsexuell.vst3` dans `~/Library/Audio/Plug-Ins/VST3` et `grainsexuell.component` dans `~/Library/Audio/Plug-Ins/Components`. Voir `INSTALL_mac.txt`. |
| Linux | Copie le dossier VST3 / LV2 dans `~/.vst3` / `~/.lv2`. Voir `INSTALL_linux.txt`. |

Redémarre ensuite ta DAW (ou relance le scan des plugins) et insère-le comme effet audio : *beatsexuell → grainsexuell*.

## Prise en main

1. Mets grainsexuell sur une piste avec quelque chose de mélodique ou de rythmique.
2. Clique sur **>** à côté du nom du preset pour les parcourir, ou sur le nom pour ouvrir les catégories.
3. Tourne **Swarm** pour plus ou moins d'événements et **Tail** pour des répétitions plus longues ou plus courtes.
4. Active le moteur **B** et choisis **Serial** ou **Parallel** pour des textures superposées.
5. Appuie sur **Hold** pour figer un instant, ou mets le looper sur **Rec** pour enregistrer une phrase.

Le manuel complet (`USER_MANUAL.html`, en anglais) est fourni avec chaque téléchargement.

## Compiler depuis les sources

Prérequis : CMake 3.22+, un compilateur C++20 (Visual Studio 2022, Xcode ou GCC/Clang). JUCE 8 et Catch2 sont récupérés automatiquement.

```
cmake -B build -G "Visual Studio 17 2022" -A x64   # ou : cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

Le plugin se trouve dans `build/grainsexuell_artefacts/Release/`.

## Retours

Ouvre une [issue](https://github.com/Beatsex/grainsexuell/issues) avec le preset ou les réglages, ce que tu as joué, ce que tu as entendu, ta DAW et ta fréquence d'échantillonnage. En cas de plantage : quelle DAW et ce sur quoi tu as cliqué en dernier.

## Licence

[MIT](LICENSE) © beatsexuell
