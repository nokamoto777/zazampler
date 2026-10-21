# 053 LD Dwarves Home: isolated FX comparison

2026-10-22。ユーザー提供のZampler RX録音5本を比較。120 BPM、Track/Main各0 dB、44.1 kHz・16 bit・stereo、16秒。順にFX ALL OFF / DRIVEのみ / PHASER・CHORUSのみ / DELAYのみ / REVERBのみ。各FXの設定はパッチのまま。正確なRXバージョン番号は未確認。

## 検証範囲

Linux上で**本番のSource/Effects.cpp**を実行し、#1のdry録音を入力、#2–5と比較した。JUCE 8.0.6、128-frame blocks、出力float32。時間合わせ・ゲイン補正・ノーマライズなし。音源サンプラー、MIDI、Filter、Voice ENV、FXBパーサーを通す全体比較ではない。使用値は以前読み取った053のFXB値を現在の取り込み式で変換したもの（丸めたraw値使用）。入力PCM16による量子化誤差も含む。

| FX | RMS差 Linear → Squared (dB) | 波形相関 Linear → Squared | 相対波形誤差 Linear → Squared |
|---|---:|---:|---:|
| Drive | −0.493（変更なし） | 0.9873 | 0.1646 |
| Phaser | −6.479（変更なし） | 0.4648 | 0.8854 |
| Delay | −1.797 → −0.071 | 0.8835 → 0.9844 | 0.4736 → 0.1764 |
| Reverb | −1.252 → +0.085 | 0.7163 → 0.8502 | 0.7136 → 0.5501 |

RMS差=20log10(||render||/||original||)、相対誤差=||render−original||/||original||。相関は聴感の一致率ではない。FXなしの対照は入力と完全一致（誤差0）。完全な数値と設定は[ISOLATED_FX_RESULTS.json](ISOLATED_FX_RESULTS.json)。音声・サンプルは同梱しない。

## Mixの仮説と実装

先頭8秒を4次Butterworth 10 kHz high-passに通し、FX音をdryに回帰した推定dry係数はDelay 0.890668、Reverb 0.733826。Linearの予測値は0.675 / 0.485、Squaredは0.894375 / 0.734775。遅延音や残響の高域成分が推定へ混入する可能性があり、この2点のみで全範囲を確定できない。

`spaceMixCurve` / **Delay/reverb mix curve** パラメーターを末尾に追加。0=Linear（既定）、1=Squared（wet=Amount²、dry=1−Amount²）。ホストのパラメーター一覧またはレンダーCLIから選べる。FXB取り込みも既定のLinearを維持。JUCEの内部wet×3 / dry×2は従来どおり補償。

全体レンダーでは未取り込みの原音Volume、サンプラー音量、フィルターなどの差が相殺されている可能性があるため、既存パッチへ自動適用しない。Delayの左右時間・音色、Reverb構造、Drive曲線、Phaser構造は依然異なる。特にPhaserは音量調整だけでは波形差を解消できない。

## 再現

JUCE 8.0.6のソース、LinuxのC++17コンパイラとzlib開発パッケージが必要。比較Pythonはnumpy、scipy、soundfileを使用。

```sh
bash scripts/build-fx-probe-linux.sh /absolute/path/JUCE /absolute/path/probe-build
python3 scripts/compare-isolated-fx-probe.py /absolute/path/probe-build/fx_probe /absolute/path/reference /absolute/path/output
```

referenceディレクトリには`zampler-rx-assult-053-#1.flac`〜`#5.flac`を置く。出力には音声とcomparison.jsonが生成される。別の音色にそのまま流用しない（053専用の設定を明記した実験）。

FXB取り込みも含める別経路:

```sh
processor_test --render-fx /absolute/path/bank.fxb 53 /absolute/path/dry.wav /absolute/path/delay.wav delay spaceMixCurve=1
```

このコマンドの全processorビルドはmacOS CIで別途検証する。上の数値は小型fx_probeによるもので、processor_testの実測値ではない。

## テスト

Linuxのfx_probe --self-testで既存Effects回帰（bypass、filter slopes、EQ、drive、chorus、phaser、delay同期/feedback/ping-pong/reset、reverb、stress、block invariance）と追加Mixテストが成功。追加テストはLinear/SquaredのDelay直達音と最初のecho、Reverb直達音を独立測定する。JUCEはMix変更を10 msで平滑化するため、Reverb定常値の測定前に無音を流す。
