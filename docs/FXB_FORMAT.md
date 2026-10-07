# Zampler FXB/FXP import — 0.3

ユーザー提供のColours FXBと公式配布FXPの構造を照合した読み込み実装。
公開された包括仕様ではないため、確認済みの寸法以外は拒否する。
.zrxという指定はユーザーにより.fxbへ訂正された。旧調査メモは履歴として残す。

## 構造

VSTコンテナーはbig-endian: CcnK、byteSize、FBCh/FPCh、version、ZMPL。
FXB chunkは160バイト目、FXP chunkは60バイト目から。chunkSizeと実サイズを照合。
Zampler chunkはlittle-endian: 先頭40バイト、parameter count 337、program count、元のprogram index、寸法12/2/2/20/0/20/0。
プログラムは12,780バイト固定。音色名+0(32)、コメント+32(480)、最初の128パラメーターfloat32 +512、旧ディレクトリ+11560(1000)、SFZ名+12560(220)。
残りの内容は不透明な生データとして保持し、意味を推測して適用しない。
slot 125 (ArpMode)には5/6の非正規化enum値があり、未適用のまま保持する。他は0〜1。
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
| Volume | 74 | カーブ不明のため取り込まず、出力は−6 dB |

Amp/filter EG、key tracking、LFO、mod matrix、glide、mono、arp/sequencerは適用しない。
マスターフィルターであり、元のボイス単位の動作とは異なる。
SFZ自体の定義はsfizzが対応する範囲で引き続き処理する。

## Colours検証対象

1,636,040 bytes、128スロット、24個の非空SFZ参照、104個の空スロット。
最初: BS Destructive MS-20 / BS Destructive MS-20.sfz。
最後の非空音色: SY Piano Pinta / SY Piano Pinta.sfz。
バンクの読み取りを検証したが、対応SFZ/WAVは未提供のためこのバンクの発音・本家との聴感比較は未検証。
提供ファイル、メーカーの実行ファイル、音源素材はソース配布に含めない。
