# Zampler FXB/FXP import — 0.9

ユーザー提供のColours FXBと公式配布FXPの構造を照合した読み込み実装。
公開された包括仕様ではないため、確認済みの寸法以外は拒否する。
.zrxという指定はユーザーにより.fxbへ訂正された。旧調査メモは履歴として残す。

## 構造

VSTコンテナーはbig-endian: CcnK、byteSize、FBCh/FPCh、version、ZMPL。
FXB chunkは160バイト目、FXP chunkは60バイト目から。chunkSizeと実サイズを照合。
Zampler chunkはlittle-endian: 先頭40バイト、parameter count 337、program count、元のprogram index、寸法12/2/2/20/0/20/0。
プログラムは12,780バイト固定。音色名+0(32)、コメント+32(480)、337パラメーターfloat32 +512、旧ディレクトリ+11560(1000)、SFZ名+12560(220)。
残りの内容は不透明な生データとして保持し、意味を推測して適用しない。
slot 125 (ArpMode)には5/6の非正規化enum値がある。128–159の変調元、192–223の変調先、224–255のvoice selector、322–328のカテゴリ／FX順序も生の整数値。それ以外は0〜1。
入力上限4 MiB、最大128音色。サイズ、形式、寸法、floatの有限性／範囲を検査する。

## 設定変換

最初の128パラメーターの名称は公式Zampler 3配布バイナリのパラメーター記述と照合。
元のDSPや物理単位のカーブは復元・実測していない。以下はZaZampler側の近似である。

| 項目 | 入力slot（0始まり） | 処理 |
|---|---|---|
| Filter | 28,29,126 | 種類を対応付け、cutoffは20×1000^v Hz、Qは0.707+9.293v |
| Phaser/Chorus | 79–84,116 | 有効時のみ。chorus系はchorus、それ以外は6段phaser。flangerも近似 |
| Delay | 85–90,117 | 左時間20×100^v msを左右へ適用、feedback/mix/crossを変換。同期・右時間・colour未再現 |
| Reverb | 91–96,118 | Mix/size/dampingのみ。タイプ・predelay・lowcut未再現 |
| Distortion | 97–100,119 | Drive/mixをtanhへ。タイプとcolour未再現 |
| EQ1/2 | 101–110,120,121 | Peakingのみ。shelfは警告してバイパス |
| Volume | 74 | カーブ不明のため取り込まず、出力は0 dB |

### 追加した取り込み

- Amp ADSR 18–21、Filter ADSR 22–25、Mod ADSR 57–60。A/D/Rは10×v³秒、Sはv。
- Keytrack 26、Filter Env 30（±5 oct近似）、Glide 78（2×v²秒）、Mono 127。
- LFO 33–38 / 42–47 / 51–56：形状、速度、skew、fade、sync、reset。速度は0.01×3000^v Hz。形状順／同期区分の対応は暫定近似。
- 32行のmatrix: source 128–159、amount 160–191（2v−1）、destination 192–223、voice selector 224–255。
- 公式バイナリの名前テーブルから確認したsource ID: Velocity1, Key2, Wheel3, Bend4, AT5, Const7, Foot11, Expr12, LFO13–15, LFO×Wheel16–18, LFO×AT19–21, Amp22, Filter23, Mod25。
- 対応destination ID: PitchSemi1, PitchFine2, Volume3, Pan4, Cutoff5, Resonance26, Level88。本家の曲線ではなくZaZamplerの深さへ近似。未対応source/destinationおよびvoice selector≠0は経路番号付きで警告。
- Arp Range70 / Rate71 / Gate72 / Swing73 / Sync114 / On115 / Mode125の基本値を近似。Mode1をDown、その他をUpに置換したことを警告する。
- Arp256–287、ArpFinger288–319、Steps320の符号、元のTiesは未解読。生レコードは保持するが、勝手にステップへ変換しない。

変換時はPer voiceを選択。追加ADSRはSFZのADSRに乗算し、SFZのボイス／サンプル寿命を延長しない。独自DSPのため本家との出音一致は保証しない。

## Colours検証対象

1,636,040 bytes、128スロット、24個の非空SFZ参照、104個の空スロット。
最初: BS Destructive MS-20 / BS Destructive MS-20.sfz。
最後の非空音色: SY Piano Pinta / SY Piano Pinta.sfz。
バンクの読み取りを検証したが、対応SFZ/WAVは未提供のためこのバンクの発音・本家との聴感比較は未検証。
提供ファイル、メーカーの実行ファイル、音源素材はソース配布に含めない。


### 0.9.3 reference calibration

Filter ENV raw 0 now maps to 0 (positive 0–5 octaves), not −5. PitchSemi is quantized to integer semitones within the existing approximate ±12-semitone mapping; the full original modulation range is still not recovered. ADSR uses an empirical `0.001 * 10000^v` time constant (exact zero stays zero) and exponential response, based on supplied RX audio. LFO fade retains its previous approximation. Imported Mono holds the first region selection through contiguous notes, resetting at a sample with no held keys. This is selectable with `monoSampleHold`; pre-0.9.3 states default to the old behavior.
