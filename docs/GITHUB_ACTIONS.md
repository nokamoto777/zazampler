# GitHub ActionsでmacOSバイナリを作る

`macOS arm64 build` はmacos-15（Apple Silicon）で実行します。mainへのpush、main向けPR、vで始まるタグ、手動実行が対象です。自動でReleaseの公開やPRのマージはしません。

## ダウンロードと実行

1. GitHubのActionsから成功した `macOS arm64 build` を開く。
2. Artifactsの `ZaZampler-macOS-arm64-実行番号` をダウンロード（GitHubへのログインが必要）。保存期間は14日です。
3. ダウンロードしたZIPの中の `ZaZampler-macOS-arm64.zip` も展開する。
4. 展開先へターミナルで移動し、DAWを終了して `bash scripts/install-macos.sh` を実行する。
5. 起動したStandaloneを終了し、DAWを再起動してプラグインを再スキャンする。

VST3 / AUv2 / AUv3入りStandaloneアプリ、インストール・検証スクリプト、Demo、説明書を含みます。AUv3はアプリ内に保持してください。

## ビルド内容

既存のbuild-macos.shでarm64 Releaseビルド、3件のCTest、AUv3埋め込みとアプリ署名を確認します。パッケージ時にはVST3/AUv2/Standaloneの署名とarm64アーキテクチャも検査します。ファイル権限とバンドル構造を保つため、macOSのdittoでZIP化してからアップロードします。
失敗時にもビルド・テスト・CMakeログを別Artifactへ保存します。手動実行のRun workflowボタンは、このworkflowがmainへマージされた後に使用できます。

## 署名と検証範囲

Apple Developerの証明書・Secretsは不要なアドホック署名の開発用ビルドです。Developer ID署名・Apple公証済みの一般配布物ではなく、ダウンロードしたアプリやプラグインをmacOSのセキュリティ機構がブロックする場合があります。これはビルド成功とは別の問題です。一般配布向けの署名・公証は未実装です。
自動テストの成功は、Ableton等でのプラグイン認識、AUv3登録、オーディオ機器を使った演奏を保証しません。Mac実機ではMAC_ACCEPTANCE.mdも確認してください。
