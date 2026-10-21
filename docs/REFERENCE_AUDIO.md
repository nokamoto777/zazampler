# Zampler RX reference comparison — 2026-10-20

比較元はユーザー提供のRX録音4本（44.1 kHz、stereo、8秒FLAC）、同じFXB/SFZ/WAV、clip.midi.mid。
音色番号は1-based: 002 BS Agro Charge（ユーザー表記Argo）、042 KY Mercury、053 LD Dwarves Home、078 PD Ice Fairy。
0.9.2基準コミット: 36e9ba5778bc07e277e7446578d7e97ede441260。

Linuxで**実際のZaZamplerProcessor + sfizz + Effects**をオフライン実行。44.1 kHz、128-frame blocks、8秒、24-bit stereo WAV。MIDIにはtempoイベントがなく、120 BPM（4小節8秒）を使用。全音色でFXBのApproximate FXを有効化。試行用の手動パラメーター補正は最終比較に含めない。ユーザーの音源・原音録音はリポジトリへ同梱しない。

## 結果

無補正・無時間合わせで8秒全体を比較。相関は聴感上の一致率ではない。別実装のFXや位相差で値が変わるため、完全互換の合格判定には使用しない。

| Patch | RMS差 0.9.2 → 修正版 (dB) | 波形相関 0.9.2 → 修正版 | 相対波形誤差 0.9.2 → 修正版 |
|---|---:|---:|---:|
| 002 | +0.75 → +0.71 | 0.385 → 0.923 | 1.161 → 0.418 |
| 042 | −8.20 → −5.02 | −0.019 → 0.571 | 1.080 → 0.821 |
| 053 | −3.38 → +0.38 | −0.029 → 0.612 | 1.224 → 0.902 |
| 078 | −11.68 → −2.86 | 0.029 → 0.829 | 1.026 → 0.570 |

RMS差 = 20 log10(||ZaZampler|| / ||RX||)。誤差 = ||ZaZampler − RX|| / ||RX||。
出力をノーマライズしてゲイン差を隠すことはしていない。

## 修正根拠

- **Pitch Semi**: raw 0.4969697を小数の半音へ写すと約−7.27 centsが加算され、3音色の波形が継続的にずれた。半音単位へ量子化して改善。原音の全変調範囲を復元したものではなく、従来の±12半音近似を維持する。
- **Filter ENV**: raw 0を−5 octへ写していた。ゼロはゼロとして取り込み、不要な音のこもりを除去。
- **Mono**: 002の2秒(C4)・4秒(A5)の原音は、それぞれのキー用WAVよりも、最初のG#3サンプルを移調した波形と高い相関（冒頭0.2秒で約1.0）があった。7秒のMIDI休符後はG#4サンプルへ切り替わる。これに基づき、隙間のないMonoフレーズではサンプル選択を保持する。
- **ENV**: 078の立ち上がりは従来の3.08秒Linearと合わなかった。FXBに指数カーブと経験的な`0.001 * 10000^value`秒の時定数を導入（raw 0のみ瞬時）。この4音色による較正であり、全範囲の本家曲線を確定したものではない。
- **Release**: sfizzの既定に近い0–1ms releaseが、後段のPer voice ENVより先にvoiceを終わらせていた。Envelope-owned releaseでは、そのケースのみ内部releaseを延長し、外部ENVの終了でvoiceを回収。明示的な長いSFZ release、off-group/steal、サンプル末尾を保持する。
- 新規3パラメーターは既存リスト末尾に追加。旧状態はLinear / SFZ release / Each noteへ復元し、FXBを選び直した場合に新しい取り込みを適用する。

## 残る差・限界

- 042は約5 dB小さく、078も約3 dB小さい。音色ごとのゲイン補正のハードコードは行っていない。
- フィルター構造・応答、LFOの位相／波形、Delayの左右時間・色付け、Reverbのアルゴリズムは本家と異なる。残差の原因は分離したdry録音などで更に確認する必要がある。
- 後日、原音は120 BPM、Track/Main各0 dB、FXはパッチ設定のまま、44.1 kHz・16 bitと確認済み。正確なRXビルド番号は未確認。追加の16秒分離FX録音による検証は[ISOLATED_FX.md](ISOLATED_FX.md)。
- 原音は8秒で終わるため、最後のnote-offから0.125秒を超える残響はこの録音では比較できない。
- 非同期入力／Liveのアーム演奏遅延はこのオフライン比較では検証していない。
- 比較音声はLinuxレンダー。macOS arm64では別途ビルド・合成音の回帰テストを実行する。Mac版でこの4音色を数値比較した結果ではない。

## 再現方法

processor_testビルド後（Macでは通常build-macos/Release/processor_test）:

```sh
processor_test --render-bank "/absolute/path/Zampler Assault.fxb" "/absolute/path/library" 2 "/absolute/path/clip.midi.mid" "/absolute/path/002.wav" 8
python3 scripts/compare-reference-audio.py original-002.flac 002.wav
```

`--render-bank`は44.1 kHz、128フレーム、固定120 BPM相当の今回の比較用CLI。MIDIのタイムスタンプを読み込むが、ホストのtempo/transport mapを再現する汎用DAWではない。末尾に`id=value`で診断用のパラメーター上書きを指定できる。音声と同名のtxtへパラメーター／取り込み詳細を保存する。数値解析にはnumpy、soundfileが必要。

CIは配布不可のサンプルに依存しない合成テストで、ENVの時定数、既定releaseの延長と収束、明示SFZ releaseの保持、Monoサンプル保持／MIDI休符での解除、FXB変換を検証する。
