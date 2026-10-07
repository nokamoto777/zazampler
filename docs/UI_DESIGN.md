# ZaZampler 0.8 UI

Zampler RX / 3系の「左LFO・中央LCD・右Filter/Output/Envelope・下部Keyboard/Effects」を参考に再配置。
青いLCD、チャコール色のパネル、金属風ノブ、アンバーの指標をJUCEのベクター描画で実装。
元製品のロゴ・背景画像・ノブ画像は同梱しない。製品表示はZaZampler。

参照した画面：
- 公式サイト https://www.zampler.de/
- 公式Zampler 3画像 https://www.zampler.de/img/Zampler-3-Package.png
- 公式音色販売ページ https://zamplersounds.sellfy.store/p/BXp3/

## 操作

中央: LOAD BANK / FXP、SAMPLE FOLDER、SFZ MODE、音色リスト、前後移動、近似FX設定、IMPORT DETAILS。
右: カットオフ、レゾナンス、フィルター種類、MIDIチャンネル、出力音量。
下: KEYBOARD / EFFECTS切替、Panic、サチュレーション、2 EQ、Phaser/Chorus、Delay、Reverb。
連続値はノブの上下ドラッグ／右クリックで数値入力／ダブルクリック初期化。常時数値欄をなくし、ホバー／ドラッグ時のポップアップで値を確認。ホストへのparameter attachmentを使用。

KeytrackとGlideを実装。GlideはENV ROUTINGのVoice mode = Monoで動作。ADSRとFilter Env量も実装済み。
中央のLFO ROUTINGは3基の変調先・深さ・同期・リトリガーを設定するページ。MATRIXページに4系統の変調、SEQUENCEページにUp/Downアルペジオと8ステップ編集を追加。本家FXB設定の再現とは別機能。
0.5では共通LFO 3基を音声処理へ追加し、左側の波形／Rate／Skew／Fadeノブを有効化。

## スクリーンショット再生成

GUI対応のLinuxビルドでも、ウィンドウを開かずに同じEditorをPNG化できる。

```bash
./build/processor_test --snapshot /absolute/path/ZaZampler-UI.png
```

同梱PNGは実際のC++ Editorの描画。イメージ案やHTMLの代替画面ではない。
Macのネイティブダイアログ、ホスト内操作、Retinaでの描画は別途実機確認が必要。

0.6: 右側3基のADSRとAmp On/Off、中央ENV ROUTINGのMod target/depthを追加。

0.8: Zampler//RXの画面を参考に、角型パネル／淡い青の液晶ボタン／金属風ノブとアンバー指標へ変更。鍵盤を初期表示にした。機能とパラメーターIDは0.6から変更していない。
