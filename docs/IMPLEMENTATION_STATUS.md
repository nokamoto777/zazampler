# 実装状況 — 0.9

| 項目 | 実装内容 |
|---|---|
| 音源 | SFZ、REX2、旧REX/RCYのスライス演奏 |
| FXB/FXP | 音色一覧・参照解決、Filter/FX、LFO/ADSR、対応Matrix経路、Mono/Glide/Keytrack、Arp基本設定の近似取り込み |
| 変調 | 3 LFO、3 ADSR、32 Matrix経路。Global / Per voice切替 |
| 演奏 | Mono Glide、Keytrack、Up/Down・8ステップ、指定16パターン、Euclidean、Swing、Tie、DAW PPQ位置同期 |
| UI | ライブラリブラウザー、波形表示、リサイズ、ロゴからAbout／デバッグログ |
| 保存 | パラメーター・埋め込みバンク・元音源参照・フォルダー権限。旧状態はGlobal / Note startを維持 |

## 残る互換上の制約

- FXBの本家固有マトリクス変調先、Random等の非対応ソース、voice selector、Arp/ArpFingerのステップ符号、元のTiesは未変換。IMPORT DETAILSに表示する。
- LFO波形順・同期区分を含む変換カーブは本家と未校正。同一DSP／出音の保証、FXB書き出し、既存Zamplerインスタンスの置換は対象外。
- 0.9.5: REX格納PPQによるHost loopとテンポ追従を実装。低速時に隙間を埋めるタイムストレッチは未実装。
- 0.9.4でLFO位相のPPQロックを追加。Host tempo + DAW phase、再生中かつPPQ提供時に対応。
- 0.9.3で既定0–1ms SFZ releaseを延長。明示SFZ release・サンプル末尾は維持。0.9.4で指数Releaseをホストの残響時間へ反映。
- 0.9.5: 生成MIDI出力、Arpラッチ、出力リミッター、20 ms音源クロスフェード、読込不可サンプル参照の収集を実装。
- macOSの各DAW・AUv3サンドボックス・実際の市販REX素材での検証は実機確認が必要。

提供されたColours FXBは読み込みを検証。対応SFZ/WAVがないため出音比較は未実施。


0.9.3: 提供されたRXの4音色に対する実音比較を追加。Filter ENVゼロ、Pitch Semiの量子化、指数ENV、Monoのフレーズ内サンプル保持、既定SFZ releaseの途中切れを修正。完全な音の一致ではない。残差と測定条件はREFERENCE_AUDIO.mdを参照。

0.9.4: Push登録補助と変調表示を追加。Liveデフォルト保存は初回にユーザーが実施。音源クロスフェードとREXループ再構成は0.9.5で対応。独自FXB経路は継続課題。RXとの音質差はREFERENCE_AUDIO.md参照。

0.9.5の制約: Limiterはサンプルピーク用。Crossfadeはdry音源を共通の新FXへ通すもので旧FX残響は保持しない。参照検査はsfizzで解析されたサンプルに限り、include構造・未知opcodeの全検証ではない。MIDI出力ルーティングと市販REXは実機検証が必要。
