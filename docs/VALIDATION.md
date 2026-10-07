# 検証記録 — 2026-10-06

## 実施環境

Linux x86_64 / GCC 13 / CMake 4.4.3 / Releaseビルド。
JUCE 8.0.6、sfizz 1.2.3の固定コミットで実施。
Linuxビルド用ヘッダー・ライブラリは検証環境内に展開して使用。
これらのLinux用依存物・バイナリはMac向けソースZIPには含めていない。

## 0.8 演奏機能・変調・オフライン同期の検証

Linux x86_64 / GCC 13、VST3 0.8.0のコンパイル・リンク・メタデータ生成成功。
最終CTest: engine_audio 0.23秒 / bank_format 0.00秒 / processor_state 0.96秒、3/3 PASS（合計1.19秒）。

processor_state内に以下の検査を追加：

- Monoの最後の鍵優先、Glideの初期音程と100 ms到達、保持鍵への復帰、ペダル、重複ノート。
- シーケンサーの250サンプル周期、125サンプルGate、休符、半音差、UpアルペジオとOctaves。
- Keytrack 1の1オクターブ追従、MatrixのPitch/Pan同時ルーティング・負極性・解除、直接LFO Pitch。
- sfizzから実際に波形を生成し、追加音程1200 centsで440 Hz→880 Hzとなること。
- 絶対Mono音程により、元のノート番号にかかわらず指定した音程となること。
- 発音37サンプル前の無音、128/257サンプルの分割で音程処理の音声が一致すること。
- AudioProcessor全体のMono Glide到達音程、シーケンサー／Glideの128/257分割一致、CC120停止。
- 新パラメーターの状態保存・復元、旧状態ではPoly/Sequence Off/Matrix 0へ移行。
- 非リアルタイム時、load直後に明示的waitなしでprocessBlockしてもロード後の冒頭MIDIが発音すること。

実際のC++ EditorからMAIN / FX / LFO / ENV / MATRIX / SEQUENCEの6画面をPNG化。
追加MATRIX・SEQUENCE・Voice mode欄を目視確認。Mac実機、AUv3署名・登録、DAWでの操作／オートメーション、負荷測定は未実施。

## 0.7 UI検証

Linux VST3 0.7.0のビルド・メタデータ生成成功。
engine_audio / bank_format / processor_state: 3/3 PASS（0.94秒）。
実際のC++ EditorからMAIN（鍵盤）/ EFFECTS / LFO ROUTING / ENV ROUTINGのPNGを生成。
MAINとEFFECTSの描画を目視確認し、ノブ・ラベル・パネルの配置を確認した。
ノブのホバー値表示と右クリック数値入力を追加し、入力時にはホストへの変更ジェスチャーを送る。
DAW内でのポップアップ操作、Retina表示、macOS arm64/AUv3実機動作は未検証。

## 0.6 ADSR検証

Linux VST3 0.6.0ビルド・メタデータ生成成功。3/3 CTest PASS（0.94秒）。
processor_stateに以下の数値検査を追加した。

- Attack/Decay/Sustain/Releaseの時間・レベル、0秒の即時遷移、再トリガー時の連続性。
- 和音、同音重複、velocity 0 note-on、CC64保持／解除、CC120/121/123。
- 同一サンプル位置にあるnote-on/offの順序。
- 定常入力をAmp ADSRで処理した際の発音前／Attack／Sustain／Release後の振幅。
- Filter envelopeで実際のフィルターが開くこと、Mod envelopeで左右バランスが変化すること。
- 48,000サンプル一括と127サンプル分割の一致。
- Amp/Filter/Modの独立性、設定保存・復元、旧状態のADSRバイパス。

C++ EditorのENV ROUTINGをPNG化して確認。Mac/AUv3実機確認は未実施。
ボイス単位のADSRや本家Zamplerと同一のカーブを実装したものではない。

## 0.5 LFO検証

Linux VST3 0.5.0のビルド・メタデータ生成成功。
engine_audio / bank_format / processor_state: 3/3 PASS。
processor_stateには次のLFO数値検査を追加した。

- 2 Hzのゼロ交差周期、BPM 60/120の四分音符周期。
- 1秒Fade-inの開始／中間／終了値、ノートオンでのFade再開始。
- 全6波形、3基同時、最大速度・非対称Skewでの有限性／範囲。
- ブロック途中のリトリガーを含む48,000サンプルを一括／127サンプル分割した際の一致。
- FX音声経路で矩形波トレモロが減衰区間／通過区間を生成すること。
- S&Hのリセット再現性と3基のランダム系列の独立性。
- LFOのDAW状態保存・復元、旧状態復元時のLFO無効化。

C++ EditorからMAIN / LFO ROUTINGのPNGを生成し、表示を確認。
本家のLFO/DSPとの一致、macOS arm64/AUv3実機動作は未検証。

## 0.4 UI / 改名の検証

- 製品名・CMakeターゲット・macOS生成物・インストールスクリプトをZaZamplerへ変更。
- AU/VSTのmanufacturer/plugin code、パラメーターID、保存状態のルートキーは維持。
- macOSスクリプト3本はbash構文検査済み。
- UI確認は実際のJUCE EditorをPNGへ描画して実施。画像は `ZaZampler-UI.png`。
- Linux VST3ビルド／メタデータ生成成功。engine_audio / bank_format / processor_stateの3テストがPASS。
- Mac実機／DAW内の操作、AUv3の改名後の登録は未検証。

## 0.3の追加検証

Linux VST3のビルド／メタデータ生成に成功。次のテストを実施。

- 実際の `Zampler Colours.fxb` をC++パーサーで読み込み、128スロット／24 SFZ参照、先頭・末尾の音色名、既知の設定を確認。
- ArpModeに非正規化enum値5/6があることを実データで確認し、そのまま保持するよう修正。
- 合成FXB/FXP、切り詰め、余剰バイト、他社ID、未知レイアウト、不正個数、NaNの検査。
- SFZの欠落通知、サブフォルダーからの再リンク、同名候補の曖昧さの検出。
- LP24／delay mixの近似変換、SFZ only時のFX既定値復元。
- 元のFXBファイルを削除した後も、DAW状態に埋め込んだバンクから復元できること。
- 手動で変更したcutoffが状態復元時にバンク初期値で上書きされないこと。

最終CTest: engine_audio / bank_format / processor_state **3/3 PASS (0.72 sec)**。
実ファイルを指定した `bank_test` もPASS。提供バンクは再配布しないため任意テスト入力：
`-DZAZAMPLER_FXB_TEST_FILE=/path/to/Zampler\ Colours.fxb`。

本家とのDSPカーブ／音質一致、GUI描画、macOSビルド、AUv3実行は未検証。
ColoursのSFZとWAVは未提供で、このバンク自体の発音確認は未実施。

## 0.2の追加検証

Linux版VST3を再ビルドし、以下を数値検証した。

- 全FXオフ、Output=0 dBで入力波形と出力が一致（ユニティゲイン）。
- LP/HPの通過・減衰、24 dBカスケードの傾斜、Band-passとNotchの特性。
- EQ1 +6 dBの中心周波数利得、EQ2 −6 dBの減衰。
- サチュレーションの大振幅圧縮。
- コーラスのセンター遅延、フェイザーによる出力変化。
- BPM120の四分音符ディレイが48 kHzで24,000サンプル後に出力されること。
- ディレイのフィードバック減衰、左右クロスフィードバック、リセット。
- リバーブ残響の発生。
- 44.1/48/96 kHzで高レゾナンス・Drive・Feedback・モード切替時に非有限値が出ないこと。
- 同じ波形を8,192サンプル一括／128サンプル分割で処理したときの一致。
- レゾナンスとディレイMixの状態復元、および0.1状態からの追加パラメーター初期値補完。

テンポ同期ディレイのテストでミリ秒→サンプル変換の浮動小数点丸めを検出し、
変換の中間計算をdoubleに修正してサンプル位置を合わせた。
また0.1の原音増幅バグ（JUCE reverb内部のdry倍率）を修正した。

.zrxに関する旧依頼は.fxbへ訂正され、0.3で上記の読み込みを追加。
Mac固有コード、M1での実行、GUI実描画・DAW実演奏も引き続き未検証。

## 音源／プラグインの検証範囲（0.1から継続）

| 検証対象 | 結果 |
|---|---|
| 音源エンジン C++ / sfizz | コンパイル・リンク成功 |
| Processor / Editor / Linux用FolderAccess | コンパイル・リンク成功 |
| Linux版VST3 | 共有ライブラリ生成・JUCEによるVST3メタデータ生成成功 |
| `engine_audio` | PASS |
| `processor_state` | PASS |
| Mac用3本のシェルスクリプト | `bash -n`による構文検証成功 |
| macOS Objective-C++ FolderAccess | 未コンパイル・未実行 |
| M1 arm64 / macOS Golden Gate | 未検証 |
| AUv3アプリ埋め込み・署名・登録・ホストからの利用 | 未検証 |
| 本物のZamplerバンク | Coloursの構造読み取りは検証済み。SFZ/WAV未提供で発音・本家音質比較は未検証 |

0.2の最終CTest出力：

```text
Test project build-plugin
    Start 1: engine_audio
1/2 Test #1: engine_audio .....................   Passed    0.16 sec
    Start 2: processor_state
2/2 Test #2: processor_state ..................   Passed    0.48 sec

100% tests passed out of 2

Total Test time (real) =   0.65 sec
```

`engine_audio`は実際のSFZ/WAVをロードして波形を生成し、以下を検査：

- リージョン数と読み込まれたサンプル数
- 128サンプル位置に指定したノートの発音前が無音であること
- ステレオ出力の左右バランス
- ノートオフ後のリリースとサステインペダルの保持／解除
- キー／ベロシティ範囲に応じた発音選別
- SFZのoffset/endに従う再生と停止
- 存在しないSFZの失敗と、その後の正常ロード
- 48 kHz音源を44.1 kHz出力に変換しても440 Hzを維持すること
- Panic後の無音と不完全MIDIメッセージの無視

`processor_state`はJUCE AudioProcessorを直接生成し、以下を検査：

- 最大ブロック128に対して1024サンプルを渡し、700サンプル位置のMIDIを正しく処理すること
- 音色状態を保存し、別インスタンス／別サンプルレートで復元して発音すること
- 空の初期状態を復元したら、ロード済み音色が停止すること
- 非同期の音色選択を連続したとき、最後の選択が有効になること

DAWを使ったVST3の実演奏テストやGUI描画確認は実施していない。
この結果をMacのAUv3／VST3動作保証と読み替えないこと。
Mac上の実行手順は `MAC_ACCEPTANCE.md` に記載。
