# grainsexuell

[![build](https://github.com/Beatsex/grainsexuell/actions/workflows/build.yml/badge.svg)](https://github.com/Beatsex/grainsexuell/actions/workflows/build.yml) [![release](https://img.shields.io/github/v/release/Beatsex/grainsexuell?include_prereleases)](https://github.com/Beatsex/grainsexuell/releases/latest) [![license: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE) ![VST3 · AU · LV2](https://img.shields.io/badge/format-VST3%20%C2%B7%20AU%20%C2%B7%20LV2-pink)

[English](README.md) · [Deutsch](README.de.md) · [Español](README.es.md) · [Français](README.fr.md) · **日本語**

**beatsexuell** によるグラニュラー／マイクロループ・マルチエフェクト。2 つのエンジンとフレーズルーパーを搭載。
Windows・macOS・Linux 用 VST3、macOS 用 AU、Linux 用 LV2、スタンドアロンアプリに対応。

> いま弾いたフレーズを聴いて、少しだけ間違えて思い出す。

[![grainsexuell tutorial](docs/tutorial.jpg)](https://youtu.be/63asHO567NA)

▶ [チュートリアルを見る（3 分）](https://youtu.be/63asHO567NA)

## 機能

- **4 種類のエンジン（各 4 バリエーション）**
  - **Mosaic**：直前の瞬間をマイクロループとして取り込み、0.5x / 1x / 2x / 4x で重ねる
  - **Cloud**：グラニュラー・クラウド（毎秒 3〜80 グレイン）
  - **Shard**：テンポ同期のグリッチ・スライサー（Shuffle、Crush、Arp、Stutter）
  - **Tap**：マルチタップ・ディレイ（Echo、Ping-pong、Reverse、Cascade）
- **2 スロット**：エンジン A に加えてエンジン B を任意で使用。**シリアル**（A → B）または**パラレル**（A と B をミックス）
- **マクロ**：Swarm（密度）、Tail（リピート）、Span（長さ）、Scatter（ランダム性）、Bend（ピッチの揺れ）
- **Dusk** ローパス、**Haze** リバーブ（Room / Plate / Hall / Swell）、エンジンごとの Mix A／Mix B、マスター **Dry/Wet**、入力・出力ゲイン（±24 dB）
- **Hold**：直前のビートをフリーズし、エフェクトの中でループさせる
- **フレーズルーパー**：Off / Rec / Play / Dub、速度 ½ · 1x · 2x、エフェクト前／後、Reverse、Quantize、Burst、Undo
- **Auto Gain**：出力を入力レベルに近づけ、セーフティ・シーリングを付加
- **56 個のファクトリープリセット**を 6 カテゴリー（Ambient、Shimmer、Rhythmic、Echo、Lo-Fi、Drift）で収録。自作プリセットも保存可能
- **MIDI ラーン**：コントロールを右クリックし、コントローラーのノブを動かすだけ
- **テンポ同期**：Tap と Shard はホストのテンポに追従
- サイズ変更できるウィンドウ、グレイン・アクティビティのライブ表示、新しいリリースのお知らせ

## インストール

最新版は [Releases](https://github.com/Beatsex/grainsexuell/releases) からダウンロードできます。

| システム | 方法 |
|---|---|
| Windows 10 / 11（64 ビット） | `grainsexuell-…-win64-setup.exe` を実行。ベータ版はまだコード署名されていないため、SmartScreen が表示されたら *詳細情報* → *実行* をクリック。 |
| macOS | `grainsexuell.vst3` を `~/Library/Audio/Plug-Ins/VST3` に、`grainsexuell.component` を `~/Library/Audio/Plug-Ins/Components` にコピー。詳しくは `INSTALL_mac.txt` を参照。 |
| Linux | VST3／LV2 フォルダーを `~/.vst3`／`~/.lv2` にコピー。詳しくは `INSTALL_linux.txt` を参照。 |

DAW を再起動（またはプラグインを再スキャン）し、オーディオエフェクトとして挿入します：*beatsexuell → grainsexuell*。

## クイックスタート

1. メロディーやリズムのあるトラックに grainsexuell を挿します。
2. プリセット名の横の **>** でプリセットを順に切り替え、名前をクリックするとカテゴリーが開きます。
3. **Swarm** でイベントの量を、**Tail** でリピートの長さを調整します。
4. エンジン **B** をオンにして **Serial** か **Parallel** を選ぶと、レイヤーされたテクスチャーになります。
5. **Hold** で瞬間をフリーズ、またはルーパーを **Rec** にしてフレーズを録音します。

完全なユーザーマニュアル（`USER_MANUAL.html`、英語）はダウンロードに同梱されています。

## ソースからビルド

必要なもの：CMake 3.22 以上、C++20 コンパイラー（Visual Studio 2022、Xcode、または GCC/Clang）。JUCE 8 と Catch2 は自動で取得されます。

```
cmake -B build -G "Visual Studio 17 2022" -A x64   # または: cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

プラグインは `build/grainsexuell_artefacts/Release/` に出力されます。

## フィードバック

[Issue](https://github.com/Beatsex/grainsexuell/issues) を作成し、プリセットまたは設定、演奏した内容、聞こえた音、使用 DAW とサンプルレートを書いてください。クラッシュの場合は、DAW と最後にクリックした箇所を教えてください。

## ライセンス

[MIT](LICENSE) © beatsexuell
