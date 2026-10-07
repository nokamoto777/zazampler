# ADSR envelopes — 0.6

Amp / Filter / Modの3基はSFZ合成後に作用する共通エンベロープ。
各音独立のADSRではない。新しいノートオンで3基とも現在値からAttackへ移行する。
最後の押鍵が解除され、サステインペダルも離れるとReleaseへ移行する。

| 設定 | 範囲／意味 |
|---|---|
| Attack | 0～10秒。0→1の線形上昇時間 |
| Decay | 0～10秒。1→Sustainの線形減衰時間 |
| Sustain | 0～1。ゲートが開いている間の保持レベル |
| Release | 0～20秒。離鍵時の値→0の線形減衰時間 |
| Amp On/Off | AMP ENVELOPEの見出し右。既定Offで旧音色への影響を防ぐ |
| Filter Env / oct | FILTER内。−5～+5オクターブをカットオフへ加算。既定0 |
| Mod target / Depth | ENV ROUTING内。Off / Cutoff / Volume / Pan / Resonance / Drive。Depth −1～+1 |

再トリガーは現在値から始まるため、Attackは現在値によって短くなる。
Attack/Decay/Releaseの0秒は即時遷移。Sustain値の手動変更には短い平滑化を適用する。
Modの最大効果量: Cutoff ±5 oct、Volume ±12 dB、Pan 左右端、Resonance ±4 Q、Drive ±24 dB。
FilterとModの周波数変調はLFOと加算。最終値はフィルター／Driveの処理可能範囲へ制限する。

## 操作例

- Amp: AMP ENVELOPEをOn、Attack=0.5、Decay=0.2、Sustain=0.7、Release=0.3として演奏。
- Filter: Filter typeをLP12、Cutoff=500 Hz、Env / oct=3、FILTER ENVELOPEのAttack=0.1、Decay=0.6、Sustain=0として演奏。
- Mod: ENV ROUTINGのTarget=Pan、Depth=1、MOD ENVELOPEのAttack=1、Sustain=1として左右変化を確認。

## ゲートと互換性

MIDIノートオン、ノートオフ、velocity 0のノートオン、CC64、CC120、CC121、CC123を処理。
同時押鍵と同じキーの重複を追跡し、途中のノートオフで全体を早期リリースしない。
CC123はペダル保持を尊重、CC121はペダル保持を解除、CC120/Panicはリセット。
Omniは既存音源と同じく1パートに統合し、コントローラーも共通に作用する。

音量とパンはFX前に作用するため、ディレイ／リバーブの残響はそのまま残る。
SFZにあるampeg/filterの定義はsfizzが別途処理し、今回のADSRはそれに重ねる。
**SFZ側のサンプルやボイスが先に止まると、今回のReleaseを長くしても音は延長されない。**
例えばDemo/Sine.sfzはampeg_release=0.05なので、長いReleaseの聴感比較には同梱Demo/Envelope.sfz（SFZ側Release 30秒）を選ぶ。

全設定はDAW状態／オートメーションに保存。進行中のADSR位置は保存しない。
旧状態やFXB選択時は追加Amp Off、Filter amount 0、Mod Offとなる。
ZamplerのFXB内ADSRの単位カーブは未確認のため自動変換は行わない。

0.8: Mod TargetにPitchを追加（Depth 1で12半音）。MatrixのSourceにも3基のEnvelopeを使用できる。共通ADSRである点は同じ。
