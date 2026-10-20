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
- REXはスライスのMIDI演奏まで。スライス時刻からの自動ループ再構成・テンポ追従は未実装。
- LFOのHost tempoは周期の同期。LFO位相そのもののPPQロックは未実装。
- 追加ADSRはSFZに乗算するため、SFZボイス寿命より長くReleaseを延ばせない。
- 外部MIDI出力、ラッチ、出力リミッター、音色切替クロスフェード、欠落サンプル全件検査は未実装。
- macOSの各DAW・AUv3サンドボックス・実際の市販REX素材での検証は実機確認が必要。

提供されたColours FXBは読み込みを検証。対応SFZ/WAVがないため出音比較は未実施。


0.9.3: 提供されたRXの4音色に対する実音比較を追加。Filter ENVゼロ、Pitch Semiの量子化、指数ENV、Monoのフレーズ内サンプル保持、既定SFZ releaseの途中切れを修正。完全な音の一致ではない。残差と測定条件はREFERENCE_AUDIO.mdを参照。
